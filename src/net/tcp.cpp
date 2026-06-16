#include <net/tcp.h>
#include <memorymanagement.h>

using namespace myos;
using namespace myos::common;
using namespace myos::net;

static const uint16_t TCP_FIRST_DYNAMIC_PORT = 1024;
static const uint16_t TCP_LOW_BYTE_MASK = 0x00FF;
static const uint16_t TCP_HIGH_BYTE_MASK = 0xFF00;
static const uint32_t TCP_INITIAL_SEQUENCE_NUMBER = 0xBEEFCAFE;
static const uint16_t TCP_WINDOW_SIZE = 0xFFFF;
static const uint16_t TCP_PROTOCOL_PSEUDOHEADER_BE = 0x0600;
static const uint32_t TCP_SYN_OPTIONS = 0xB4050402;
static const uint8_t TCP_WORD_BYTES = 4;
static const uint8_t TCP_MIN_HEADER_BYTES = 20;

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

TransmissionControlProtocolHandler::TransmissionControlProtocolHandler()
{
}

TransmissionControlProtocolHandler::~TransmissionControlProtocolHandler()
{
}

bool TransmissionControlProtocolHandler::HandleTransmissionControlProtocolMessage(
    TransmissionControlProtocolSocket* socket,
    uint8_t* data,
    uint16_t size)
{
    return true;
}

TransmissionControlProtocolSocket::TransmissionControlProtocolSocket(
    TransmissionControlProtocolProvider* backend)
{
    this->backend = backend;
    handler = 0;
    state = CLOSED;
}

TransmissionControlProtocolSocket::~TransmissionControlProtocolSocket()
{
}

bool TransmissionControlProtocolSocket::HandleTransmissionControlProtocolMessage(uint8_t* data,
                                                                                uint16_t size)
{
    if(handler != 0)
        return handler->HandleTransmissionControlProtocolMessage(this, data, size);
    return false;
}

void TransmissionControlProtocolSocket::Send(uint8_t* data, uint16_t size)
{
    while(state != ESTABLISHED)
    {
    }

    backend->Send(this, data, size, PSH | ACK);
}

void TransmissionControlProtocolSocket::Disconnect()
{
    backend->Disconnect(this);
}

TransmissionControlProtocolProvider::TransmissionControlProtocolProvider(
    InternetProtocolProvider* backend)
: InternetProtocolHandler(backend, IP_PROTOCOL_TCP)
{
    for(int i = 0; i < 65535; i++)
        sockets[i] = 0;

    numSockets = 0;
    freePort = TCP_FIRST_DYNAMIC_PORT;
}

TransmissionControlProtocolProvider::~TransmissionControlProtocolProvider()
{
}

bool TransmissionControlProtocolProvider::OnInternetProtocolReceived(uint32_t srcIP_BE,
                                                                    uint32_t dstIP_BE,
                                                                    uint8_t* internetprotocolPayload,
                                                                    uint32_t size)
{
    if(size < TCP_MIN_HEADER_BYTES)
        return false;

    TransmissionControlProtocolHeader* msg =
        (TransmissionControlProtocolHeader*)internetprotocolPayload;
    uint16_t headerSize = msg->headerSize32 * TCP_WORD_BYTES;
    if(headerSize < TCP_MIN_HEADER_BYTES || headerSize > size)
        return false;

    TransmissionControlProtocolSocket* socket = 0;
    for(uint16_t i = 0; i < numSockets && socket == 0; i++)
    {
        if(sockets[i] == 0)
            continue;

        if(sockets[i]->localPort == msg->dstPort
        && sockets[i]->localIP == dstIP_BE
        && sockets[i]->state == LISTEN
        && ((msg->flags & (SYN | ACK)) == SYN))
        {
            TransmissionControlProtocolSocket* listener = sockets[i];
            socket = (TransmissionControlProtocolSocket*)MemoryManager::activeMemoryManager->malloc(
                sizeof(TransmissionControlProtocolSocket));

            if(socket != 0)
            {
                new (socket) TransmissionControlProtocolSocket(this);
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
                    socket->acknowledgementNumber = bigEndian32(msg->sequenceNumber) + 1;
                    socket->sequenceNumber = TCP_INITIAL_SEQUENCE_NUMBER;
                    Send(socket, 0, 0, SYN | ACK);
                    socket->sequenceNumber++;
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
                    socket->acknowledgementNumber = bigEndian32(msg->sequenceNumber) + 1;
                    socket->sequenceNumber++;
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
                    socket->acknowledgementNumber++;
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
                    socket->acknowledgementNumber++;
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
                if(bigEndian32(msg->sequenceNumber) == socket->acknowledgementNumber)
                {
                    uint16_t payloadSize = size - headerSize;
                    uint16_t acknowledgedPayloadSize = payloadSize;

                    if(payloadSize > 0)
                    {
                        acknowledgedPayloadSize = 0;
                        for(uint32_t i = headerSize; i < size; i++)
                            if(internetprotocolPayload[i] != 0)
                                acknowledgedPayloadSize = i - headerSize + 1;

                        if(acknowledgedPayloadSize == 0)
                            acknowledgedPayloadSize = payloadSize;
                    }

                    socket->acknowledgementNumber += acknowledgedPayloadSize;
                    reset = !socket->HandleTransmissionControlProtocolMessage(
                        internetprotocolPayload + headerSize,
                        payloadSize);

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
            TransmissionControlProtocolSocket resetSocket(this);
            resetSocket.remotePort = msg->srcPort;
            resetSocket.remoteIP = srcIP_BE;
            resetSocket.localPort = msg->dstPort;
            resetSocket.localIP = dstIP_BE;
            resetSocket.sequenceNumber = bigEndian32(msg->acknowledgementNumber);
            resetSocket.acknowledgementNumber = bigEndian32(msg->sequenceNumber) + 1;
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

void TransmissionControlProtocolProvider::Send(TransmissionControlProtocolSocket* socket,
                                               uint8_t* data,
                                               uint16_t size,
                                               uint16_t flags)
{
    uint16_t totalLength = size + sizeof(TransmissionControlProtocolHeader);
    uint16_t lengthInclPHdr = totalLength + sizeof(TransmissionControlProtocolPseudoHeader);

    uint8_t* buffer = (uint8_t*)MemoryManager::activeMemoryManager->malloc(lengthInclPHdr);
    if(buffer == 0)
        return;

    TransmissionControlProtocolPseudoHeader* phdr =
        (TransmissionControlProtocolPseudoHeader*)buffer;
    TransmissionControlProtocolHeader* msg =
        (TransmissionControlProtocolHeader*)(buffer + sizeof(TransmissionControlProtocolPseudoHeader));
    uint8_t* payload = buffer + sizeof(TransmissionControlProtocolPseudoHeader)
                            + sizeof(TransmissionControlProtocolHeader);

    msg->srcPort = socket->localPort;
    msg->dstPort = socket->remotePort;
    msg->sequenceNumber = bigEndian32(socket->sequenceNumber);
    msg->acknowledgementNumber = bigEndian32(socket->acknowledgementNumber);
    msg->reserved = 0;
    msg->headerSize32 = sizeof(TransmissionControlProtocolHeader) / TCP_WORD_BYTES;
    msg->flags = flags;
    msg->windowSize = TCP_WINDOW_SIZE;
    msg->urgentPtr = 0;
    msg->options = (flags & SYN) ? TCP_SYN_OPTIONS : 0;

    socket->sequenceNumber += size;

    for(uint16_t i = 0; i < size; i++)
        payload[i] = data[i];

    phdr->srcIP = socket->localIP;
    phdr->dstIP = socket->remoteIP;
    phdr->protocol = TCP_PROTOCOL_PSEUDOHEADER_BE;
    phdr->totalLength = bigEndian16(totalLength);

    msg->checksum = 0;
    msg->checksum = InternetProtocolProvider::Checksum((uint16_t*)buffer, lengthInclPHdr);

    InternetProtocolHandler::Send(socket->remoteIP, (uint8_t*)msg, totalLength);
    MemoryManager::activeMemoryManager->free(buffer);
}

TransmissionControlProtocolSocket* TransmissionControlProtocolProvider::Connect(uint32_t ip,
                                                                                uint16_t port)
{
    TransmissionControlProtocolSocket* socket =
        (TransmissionControlProtocolSocket*)MemoryManager::activeMemoryManager->malloc(
            sizeof(TransmissionControlProtocolSocket));

    if(socket == 0)
        return 0;

    new (socket) TransmissionControlProtocolSocket(this);

    socket->remotePort = bigEndian16(port);
    socket->remoteIP = ip;
    socket->localPort = bigEndian16(freePort++);
    socket->localIP = backend->GetIPAddress();
    socket->state = SYN_SENT;
    socket->sequenceNumber = TCP_INITIAL_SEQUENCE_NUMBER;

    sockets[numSockets++] = socket;
    Send(socket, 0, 0, SYN);

    return socket;
}

TransmissionControlProtocolSocket* TransmissionControlProtocolProvider::Listen(uint16_t port)
{
    TransmissionControlProtocolSocket* socket =
        (TransmissionControlProtocolSocket*)MemoryManager::activeMemoryManager->malloc(
            sizeof(TransmissionControlProtocolSocket));

    if(socket == 0)
        return 0;

    new (socket) TransmissionControlProtocolSocket(this);

    socket->state = LISTEN;
    socket->localIP = backend->GetIPAddress();
    socket->localPort = bigEndian16(port);

    sockets[numSockets++] = socket;
    return socket;
}

void TransmissionControlProtocolProvider::Disconnect(TransmissionControlProtocolSocket* socket)
{
    if(socket == 0)
        return;

    socket->state = FIN_WAIT1;
    Send(socket, 0, 0, FIN | ACK);
    socket->sequenceNumber++;
}

void TransmissionControlProtocolProvider::Bind(TransmissionControlProtocolSocket* socket,
                                               TransmissionControlProtocolHandler* handler)
{
    if(socket != 0)
        socket->handler = handler;
}
