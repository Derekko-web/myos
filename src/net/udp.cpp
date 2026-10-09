#include <net/udp.h>
#include <memorymanagement.h>

using namespace myos;
using namespace myos::common;
using namespace myos::net;

static const uint16_t UDP_FIRST_DYN_PORT = 1024;
static const uint16_t UDP_LOW_BYTE_MASK = 0x00FF;
static const uint16_t UDP_HIGH_BYTE_MASK = 0xFF00;

static uint16_t bigEndian16(uint16_t x)
{
    return ((x & UDP_HIGH_BYTE_MASK) >> 8) | ((x & UDP_LOW_BYTE_MASK) << 8);
}

UDPHandler::UDPHandler()
{
}

UDPHandler::~UDPHandler()
{
}

void UDPHandler::HandleUDPMessage(UDPSocket* socket,
                                  uint8_t* data,
                                  uint16_t size)
{
}

UDPSocket::UDPSocket(UDPProvider* backend)
{
    this->backend = backend;
    handler = 0;
    listening = false;
}

UDPSocket::~UDPSocket()
{
}

void UDPSocket::HandleUDPMessage(uint8_t* data, uint16_t size)
{
    if(handler != 0)
        handler->HandleUDPMessage(this, data, size);
}

void UDPSocket::Send(uint8_t* data, uint16_t size)
{
    backend->Send(this, data, size);
}

void UDPSocket::Disconnect()
{
    backend->Disconnect(this);
}

UDPProvider::UDPProvider(IPProvider* backend)
: IPHandler(backend, IP_PROTOCOL_UDP)
{
    for(int i = 0; i < 65535; i++)
        sockets[i] = 0;

    numSockets = 0;
    freePort = UDP_FIRST_DYN_PORT;
}

UDPProvider::~UDPProvider()
{
}

bool UDPProvider::OnIPReceived(uint32_t srcIP_BE,
                               uint32_t dstIP_BE,
                               uint8_t* ipPayload,
                               uint32_t size)
{
    if(size < sizeof(UDPHeader))
        return false;

    UDPHeader* msg = (UDPHeader*)ipPayload;
    uint16_t udpLen = bigEndian16(msg->len);
    if(udpLen < sizeof(UDPHeader) || udpLen > size)
        udpLen = size;

    UDPSocket* socket = 0;
    for(uint16_t i = 0; i < numSockets && socket == 0; i++)
    {
        if(sockets[i] == 0)
            continue;

        if(sockets[i]->localPort == msg->dstPort
        && sockets[i]->localIP == dstIP_BE
        && sockets[i]->listening)
        {
            socket = sockets[i];
            socket->remotePort = msg->srcPort;
            socket->remoteIP = srcIP_BE;
        }
        else if(sockets[i]->localPort == msg->dstPort
             && sockets[i]->localIP == dstIP_BE
             && sockets[i]->remotePort == msg->srcPort
             && sockets[i]->remoteIP == srcIP_BE)
        {
            socket = sockets[i];
        }
    }

    if(socket != 0)
        socket->HandleUDPMessage(
            ipPayload + sizeof(UDPHeader),
            udpLen - sizeof(UDPHeader));

    return false;
}

UDPSocket* UDPProvider::Connect(uint32_t ip, uint16_t port)
{
    UDPSocket* socket =
        (UDPSocket*)MemoryManager::activeMemoryManager->malloc(
            sizeof(UDPSocket));

    if(socket == 0)
        return 0;

    new (socket) UDPSocket(this);

    socket->remotePort = bigEndian16(port);
    socket->remoteIP = ip;
    socket->localPort = bigEndian16(freePort++);
    socket->localIP = backend->GetIP();

    sockets[numSockets++] = socket;
    return socket;
}

UDPSocket* UDPProvider::Listen(uint16_t port)
{
    UDPSocket* socket =
        (UDPSocket*)MemoryManager::activeMemoryManager->malloc(
            sizeof(UDPSocket));

    if(socket == 0)
        return 0;

    new (socket) UDPSocket(this);

    socket->listening = true;
    socket->localPort = bigEndian16(port);
    socket->localIP = backend->GetIP();

    sockets[numSockets++] = socket;
    return socket;
}

void UDPProvider::Disconnect(UDPSocket* socket)
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

void UDPProvider::Send(UDPSocket* socket, uint8_t* data, uint16_t size)
{
    uint16_t totalLen = size + sizeof(UDPHeader);
    uint8_t* buffer = (uint8_t*)MemoryManager::activeMemoryManager->malloc(totalLen);
    if(buffer == 0)
        return;

    UDPHeader* msg = (UDPHeader*)buffer;
    msg->srcPort = socket->localPort;
    msg->dstPort = socket->remotePort;
    msg->len = bigEndian16(totalLen);
    msg->csum = 0;

    uint8_t* payload = buffer + sizeof(UDPHeader);
    for(uint16_t i = 0; i < size; i++)
        payload[i] = data[i];

    IPHandler::Send(socket->remoteIP, buffer, totalLen);
    MemoryManager::activeMemoryManager->free(buffer);
}

void UDPProvider::Bind(UDPSocket* socket, UDPHandler* handler)
{
    if(socket != 0)
        socket->handler = handler;
}
