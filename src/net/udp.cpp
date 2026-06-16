#include <net/udp.h>
#include <memorymanagement.h>

using namespace myos;
using namespace myos::common;
using namespace myos::net;

static const uint16_t UDP_FIRST_DYNAMIC_PORT = 1024;
static const uint16_t UDP_LOW_BYTE_MASK = 0x00FF;
static const uint16_t UDP_HIGH_BYTE_MASK = 0xFF00;

static uint16_t bigEndian16(uint16_t x)
{
    return ((x & UDP_HIGH_BYTE_MASK) >> 8) | ((x & UDP_LOW_BYTE_MASK) << 8);
}

UserDatagramProtocolHandler::UserDatagramProtocolHandler()
{
}

UserDatagramProtocolHandler::~UserDatagramProtocolHandler()
{
}

void UserDatagramProtocolHandler::HandleUserDatagramProtocolMessage(UserDatagramProtocolSocket* socket,
                                                                    uint8_t* data,
                                                                    uint16_t size)
{
}

UserDatagramProtocolSocket::UserDatagramProtocolSocket(UserDatagramProtocolProvider* backend)
{
    this->backend = backend;
    handler = 0;
    listening = false;
}

UserDatagramProtocolSocket::~UserDatagramProtocolSocket()
{
}

void UserDatagramProtocolSocket::HandleUserDatagramProtocolMessage(uint8_t* data, uint16_t size)
{
    if(handler != 0)
        handler->HandleUserDatagramProtocolMessage(this, data, size);
}

void UserDatagramProtocolSocket::Send(uint8_t* data, uint16_t size)
{
    backend->Send(this, data, size);
}

void UserDatagramProtocolSocket::Disconnect()
{
    backend->Disconnect(this);
}

UserDatagramProtocolProvider::UserDatagramProtocolProvider(InternetProtocolProvider* backend)
: InternetProtocolHandler(backend, IP_PROTOCOL_UDP)
{
    for(int i = 0; i < 65535; i++)
        sockets[i] = 0;

    numSockets = 0;
    freePort = UDP_FIRST_DYNAMIC_PORT;
}

UserDatagramProtocolProvider::~UserDatagramProtocolProvider()
{
}

bool UserDatagramProtocolProvider::OnInternetProtocolReceived(uint32_t srcIP_BE,
                                                              uint32_t dstIP_BE,
                                                              uint8_t* internetprotocolPayload,
                                                              uint32_t size)
{
    if(size < sizeof(UserDatagramProtocolHeader))
        return false;

    UserDatagramProtocolHeader* msg = (UserDatagramProtocolHeader*)internetprotocolPayload;
    uint16_t udpLength = bigEndian16(msg->length);
    if(udpLength < sizeof(UserDatagramProtocolHeader) || udpLength > size)
        udpLength = size;

    UserDatagramProtocolSocket* socket = 0;
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
        socket->HandleUserDatagramProtocolMessage(
            internetprotocolPayload + sizeof(UserDatagramProtocolHeader),
            udpLength - sizeof(UserDatagramProtocolHeader));

    return false;
}

UserDatagramProtocolSocket* UserDatagramProtocolProvider::Connect(uint32_t ip, uint16_t port)
{
    UserDatagramProtocolSocket* socket =
        (UserDatagramProtocolSocket*)MemoryManager::activeMemoryManager->malloc(
            sizeof(UserDatagramProtocolSocket));

    if(socket == 0)
        return 0;

    new (socket) UserDatagramProtocolSocket(this);

    socket->remotePort = bigEndian16(port);
    socket->remoteIP = ip;
    socket->localPort = bigEndian16(freePort++);
    socket->localIP = backend->GetIPAddress();

    sockets[numSockets++] = socket;
    return socket;
}

UserDatagramProtocolSocket* UserDatagramProtocolProvider::Listen(uint16_t port)
{
    UserDatagramProtocolSocket* socket =
        (UserDatagramProtocolSocket*)MemoryManager::activeMemoryManager->malloc(
            sizeof(UserDatagramProtocolSocket));

    if(socket == 0)
        return 0;

    new (socket) UserDatagramProtocolSocket(this);

    socket->listening = true;
    socket->localPort = bigEndian16(port);
    socket->localIP = backend->GetIPAddress();

    sockets[numSockets++] = socket;
    return socket;
}

void UserDatagramProtocolProvider::Disconnect(UserDatagramProtocolSocket* socket)
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

void UserDatagramProtocolProvider::Send(UserDatagramProtocolSocket* socket,
                                        uint8_t* data,
                                        uint16_t size)
{
    uint16_t totalLength = size + sizeof(UserDatagramProtocolHeader);
    uint8_t* buffer = (uint8_t*)MemoryManager::activeMemoryManager->malloc(totalLength);
    if(buffer == 0)
        return;

    UserDatagramProtocolHeader* msg = (UserDatagramProtocolHeader*)buffer;
    msg->srcPort = socket->localPort;
    msg->dstPort = socket->remotePort;
    msg->length = bigEndian16(totalLength);
    msg->checksum = 0;

    uint8_t* payload = buffer + sizeof(UserDatagramProtocolHeader);
    for(uint16_t i = 0; i < size; i++)
        payload[i] = data[i];

    InternetProtocolHandler::Send(socket->remoteIP, buffer, totalLength);
    MemoryManager::activeMemoryManager->free(buffer);
}

void UserDatagramProtocolProvider::Bind(UserDatagramProtocolSocket* socket,
                                        UserDatagramProtocolHandler* handler)
{
    if(socket != 0)
        socket->handler = handler;
}
