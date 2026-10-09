#include <net/tcp.h>
#include <memorymanagement.h>

using namespace myos;
using namespace myos::common;
using namespace myos::net;

static const uint16_t TCP_FIRST_DYN_PORT = 1024;
static const uint16_t TCP_LOW_BYTE_MASK = 0x00FF;
static const uint16_t TCP_HIGH_BYTE_MASK = 0xFF00;
static const uint32_t TCP_INIT_SEQ_NUM = 0xBEEFCAFE;
static const uint16_t TCP_WND_SIZE = 0xFFFF;
static const uint16_t TCP_PROTO_PHDR_BE = 0x0600;
static const uint32_t TCP_SYN_OPTS = 0xB4050402;
static const uint8_t TCP_WORD_BYTES = 4;
static const uint8_t TCP_MIN_HDR_BYTES = 20;

static uint16_t bigEndian16(uint16_t x)
{
    return ((x & TCP_HIGH_BYTE_MASK) >> 8) | ((x & TCP_LOW_BYTE_MASK) << 8);
}

static uint32_t bigEndian32(uint32_t x)
{
    return ((x & 0xFF000000) >> 24)
         | ((x & 0x00FF0000) >> 8)
         | ((x & 0x0000FF00) << 8)
         | ((x & 0x000000FF) << 24);
}

TCPHandler::TCPHandler()
{
}

TCPHandler::~TCPHandler()
{
}

bool TCPHandler::HandleTCPMessage(
    TCPSocket* socket,
    uint8_t* data,
    uint16_t size)
{
    return true;
}

TCPSocket::TCPSocket(TCPProvider* backend)
{
    this->backend = backend;
    handler = 0;
    state = CLOSED;
}

TCPSocket::~TCPSocket()
{
}

bool TCPSocket::HandleTCPMessage(uint8_t* data, uint16_t size)
{
    if(handler != 0)
        return handler->HandleTCPMessage(this, data, size);
    return false;
}

void TCPSocket::Send(uint8_t* data, uint16_t size)
{
    while(state != ESTABLISHED)
    {
    }

    backend->Send(this, data, size, PSH | ACK);
}

void TCPSocket::Disconnect()
{
    backend->Disconnect(this);
}

TCPProvider::TCPProvider(IPProvider* backend)
: IPHandler(backend, IP_PROTOCOL_TCP)
{
    for(int i = 0; i < 65535; i++)
        sockets[i] = 0;

    numSockets = 0;
    freePort = TCP_FIRST_DYN_PORT;
}

TCPProvider::~TCPProvider()
{
}

bool TCPProvider::OnIPReceived(uint32_t srcIP_BE,
                               uint32_t dstIP_BE,
                               uint8_t* ipPayload,
                               uint32_t size)
{
    if(size < TCP_MIN_HDR_BYTES)
        return false;

    TCPHeader* msg =
        (TCPHeader*)ipPayload;
    uint16_t hdrSize = msg->hdrSize32 * TCP_WORD_BYTES;
    if(hdrSize < TCP_MIN_HDR_BYTES || hdrSize > size)
        return false;

    TCPSocket* socket = 0;
    for(uint16_t i = 0; i < numSockets && socket == 0; i++)
    {
        if(sockets[i] == 0)
            continue;

        if(sockets[i]->localPort == msg->dstPort
        && sockets[i]->localIP == dstIP_BE
        && sockets[i]->state == LISTEN
        && ((msg->flags & (SYN | ACK)) == SYN))
        {
            TCPSocket* listener = sockets[i];
            socket = (TCPSocket*)MemoryManager::activeMemoryManager->malloc(
                sizeof(TCPSocket));

            if(socket != 0)
            {
                new (socket) TCPSocket(this);
                socket->localPort = listener->localPort;
                socket->localIP = listener->localIP;
                socket->handler = listener->handler;
                socket->state = LISTEN;
                sockets[numSockets++] = socket;
            }
        }
        else if(sockets[i]->localPort == msg->dstPort
             && sockets[i]->localIP == dstIP_BE
             && sockets[i]->remotePort == msg->srcPort
             && sockets[i]->remoteIP == srcIP_BE)
        {
            socket = sockets[i];
        }
    }

    bool reset = false;

    if(socket != 0 && (msg->flags & RST))
        socket->state = CLOSED;

    if(socket != 0 && socket->state != CLOSED)
    {
        switch(msg->flags & (SYN | ACK | FIN))
        {
            case SYN:
                if(socket->state == LISTEN)
                {
                    socket->state = SYN_RECEIVED;
                    socket->remotePort = msg->srcPort;
                    socket->remoteIP = srcIP_BE;
                    socket->ackNum = bigEndian32(msg->seqNum) + 1;
                    socket->seqNum = TCP_INIT_SEQ_NUM;
                    Send(socket, 0, 0, SYN | ACK);
                    socket->seqNum++;
                }
                else
                {
                    reset = true;
                }
                break;

            case SYN | ACK:
                if(socket->state == SYN_SENT)
                {
                    socket->state = ESTABLISHED;
                    socket->ackNum = bigEndian32(msg->seqNum) + 1;
                    socket->seqNum++;
                    Send(socket, 0, 0, ACK);
                }
                else
                {
                    reset = true;
                }
                break;

            case SYN | FIN:
            case SYN | FIN | ACK:
                reset = true;
                break;

            case FIN:
            case FIN | ACK:
                if(socket->state == ESTABLISHED)
                {
                    socket->state = CLOSE_WAIT;
                    socket->ackNum++;
                    Send(socket, 0, 0, ACK);
                    Send(socket, 0, 0, FIN | ACK);
                }
                else if(socket->state == CLOSE_WAIT)
                {
                    socket->state = CLOSED;
                }
                else if(socket->state == FIN_WAIT1 || socket->state == FIN_WAIT2)
                {
                    socket->state = CLOSED;
                    socket->ackNum++;
                    Send(socket, 0, 0, ACK);
                }
                else
                {
                    reset = true;
                }
                break;

            case ACK:
                if(socket->state == SYN_RECEIVED)
                {
                    socket->state = ESTABLISHED;
                    return false;
                }
                else if(socket->state == FIN_WAIT1)
                {
                    socket->state = FIN_WAIT2;
                    return false;
                }
                else if(socket->state == CLOSE_WAIT)
                {
                    socket->state = CLOSED;
                    break;
                }

                if(msg->flags == ACK)
                    break;

            default:
                if(bigEndian32(msg->seqNum) == socket->ackNum)
                {
                    uint16_t payloadLen = size - hdrSize;
                    uint16_t ackPayloadLen = payloadLen;

                    if(payloadLen > 0)
                    {
                        ackPayloadLen = 0;
                        for(uint32_t i = hdrSize; i < size; i++)
                            if(ipPayload[i] != 0)
                                ackPayloadLen = i - hdrSize + 1;

                        if(ackPayloadLen == 0)
                            ackPayloadLen = payloadLen;
                    }

                    socket->ackNum += ackPayloadLen;
                    reset = !socket->HandleTCPMessage(
                        ipPayload + hdrSize,
                        payloadLen);

                    if(!reset)
                        Send(socket, 0, 0, ACK);
                }
                else
                {
                    reset = true;
                }
                break;
        }
    }

    if(reset)
    {
        if(socket != 0)
        {
            Send(socket, 0, 0, RST);
        }
        else
        {
            TCPSocket resetSocket(this);
            resetSocket.remotePort = msg->srcPort;
            resetSocket.remoteIP = srcIP_BE;
            resetSocket.localPort = msg->dstPort;
            resetSocket.localIP = dstIP_BE;
            resetSocket.seqNum = bigEndian32(msg->ackNum);
            resetSocket.ackNum = bigEndian32(msg->seqNum) + 1;
            Send(&resetSocket, 0, 0, RST);
        }
    }

    if(socket != 0 && socket->state == CLOSED)
    {
        for(uint16_t i = 0; i < numSockets; i++)
        {
            if(sockets[i] == socket)
            {
                sockets[i] = sockets[--numSockets];
                MemoryManager::activeMemoryManager->free(socket);
                break;
            }
        }
    }

    return false;
}

void TCPProvider::Send(TCPSocket* socket,
                       uint8_t* data,
                       uint16_t size,
                       uint16_t flags)
{
    uint16_t totalLen = size + sizeof(TCPHeader);
    uint16_t lenInclPHdr = totalLen + sizeof(TCPPseudoHeader);

    uint8_t* buffer = (uint8_t*)MemoryManager::activeMemoryManager->malloc(lenInclPHdr);
    if(buffer == 0)
        return;

    TCPPseudoHeader* phdr =
        (TCPPseudoHeader*)buffer;
    TCPHeader* msg =
        (TCPHeader*)(buffer + sizeof(TCPPseudoHeader));
    uint8_t* payload = buffer + sizeof(TCPPseudoHeader)
                            + sizeof(TCPHeader);

    msg->srcPort = socket->localPort;
    msg->dstPort = socket->remotePort;
    msg->seqNum = bigEndian32(socket->seqNum);
    msg->ackNum = bigEndian32(socket->ackNum);
    msg->rsvd = 0;
    msg->hdrSize32 = sizeof(TCPHeader) / TCP_WORD_BYTES;
    msg->flags = flags;
    msg->wndSize = TCP_WND_SIZE;
    msg->urgPtr = 0;
    msg->opts = (flags & SYN) ? TCP_SYN_OPTS : 0;

    socket->seqNum += size;

    for(uint16_t i = 0; i < size; i++)
        payload[i] = data[i];

    phdr->srcIP = socket->localIP;
    phdr->dstIP = socket->remoteIP;
    phdr->proto = TCP_PROTO_PHDR_BE;
    phdr->totalLen = bigEndian16(totalLen);

    msg->csum = 0;
    msg->csum = IPProvider::Csum((uint16_t*)buffer, lenInclPHdr);

    IPHandler::Send(socket->remoteIP, (uint8_t*)msg, totalLen);
    MemoryManager::activeMemoryManager->free(buffer);
}

TCPSocket* TCPProvider::Connect(uint32_t ip, uint16_t port)
{
    TCPSocket* socket =
        (TCPSocket*)MemoryManager::activeMemoryManager->malloc(
            sizeof(TCPSocket));

    if(socket == 0)
        return 0;

    new (socket) TCPSocket(this);

    socket->remotePort = bigEndian16(port);
    socket->remoteIP = ip;
    socket->localPort = bigEndian16(freePort++);
    socket->localIP = backend->GetIP();
    socket->state = SYN_SENT;
    socket->seqNum = TCP_INIT_SEQ_NUM;

    sockets[numSockets++] = socket;
    Send(socket, 0, 0, SYN);

    return socket;
}

TCPSocket* TCPProvider::Listen(uint16_t port)
{
    TCPSocket* socket =
        (TCPSocket*)MemoryManager::activeMemoryManager->malloc(
            sizeof(TCPSocket));

    if(socket == 0)
        return 0;

    new (socket) TCPSocket(this);

    socket->state = LISTEN;
    socket->localIP = backend->GetIP();
    socket->localPort = bigEndian16(port);

    sockets[numSockets++] = socket;
    return socket;
}

void TCPProvider::Disconnect(TCPSocket* socket)
{
    if(socket == 0)
        return;

    socket->state = FIN_WAIT1;
    Send(socket, 0, 0, FIN | ACK);
    socket->seqNum++;
}

void TCPProvider::Bind(TCPSocket* socket, TCPHandler* handler)
{
    if(socket != 0)
        socket->handler = handler;
}
