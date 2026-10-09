#include <net/ipv4.h>

using namespace myos;
using namespace myos::common;
using namespace myos::net;

static const uint16_t IPV4_LOW_BYTE_MASK = 0x00FF;
static const uint16_t IPV4_HIGH_BYTE_MASK = 0xFF00;
static const uint32_t IPV4_CSUM_CARRY_MASK = 0xFFFF0000;
static const uint32_t IPV4_CSUM_WORD_MASK = 0xFFFF;
static const uint8_t IPV4_VERSION = 4;
static const uint8_t IPV4_WORD_BYTES = 4;
static const uint8_t IPV4_DEFAULT_TTL = 0x40;
static const uint16_t IPV4_IDENT_BE = 0x0100;
static const uint16_t IPV4_DF_BE = 0x0040;

static uint16_t bigEndian16(uint16_t x)
{
    return ((x & IPV4_HIGH_BYTE_MASK) >> 8) | ((x & IPV4_LOW_BYTE_MASK) << 8);
}

IPHandler::IPHandler(IPProvider* backend, uint8_t proto)
{
    this->backend = backend;
    this->proto = proto;
    backend->handlers[proto] = this;
}

IPHandler::~IPHandler()
{
    if(backend->handlers[proto] == this)
        backend->handlers[proto] = 0;
}

bool IPHandler::OnIPReceived(uint32_t srcIP_BE, uint32_t dstIP_BE,
                             uint8_t* ipPayload, uint32_t size)
{
    return false;
}

void IPHandler::Send(uint32_t dstIP_BE, uint8_t* ipPayload, uint32_t size)
{
    backend->Send(dstIP_BE, proto, ipPayload, size);
}

IPProvider::IPProvider(EtherFrameProvider* backend,
                       ARP* arp,
                       uint32_t gatewayIP,
                       uint32_t subnetMask)
: EtherFrameHandler(backend, ETHERTYPE_IPV4)
{
    for(int i = 0; i < 255; i++)
        handlers[i] = 0;
    this->arp = arp;
    this->gatewayIP = gatewayIP;
    this->subnetMask = subnetMask;
}

IPProvider::~IPProvider()
{
}

bool IPProvider::OnEtherFrameReceived(uint8_t* etherframePayload, uint32_t size)
{
    if(size < sizeof(IPv4Message))
        return false;

    IPv4Message* ipMessage = (IPv4Message*)etherframePayload;
    bool sendBack = false;

    if(ipMessage->dstIP == backend->GetIP())
    {
        uint32_t len = bigEndian16(ipMessage->totalLen);
        if(len > size)
            len = size;

        uint32_t hdrLen = IPV4_WORD_BYTES * ipMessage->hdrLen;
        if(hdrLen <= len && handlers[ipMessage->proto] != 0)
            sendBack = handlers[ipMessage->proto]->OnIPReceived(
                ipMessage->srcIP, ipMessage->dstIP,
                etherframePayload + hdrLen, len - hdrLen);
    }

    if(sendBack)
    {
        uint32_t temp = ipMessage->dstIP;
        ipMessage->dstIP = ipMessage->srcIP;
        ipMessage->srcIP = temp;

        ipMessage->ttl = IPV4_DEFAULT_TTL;
        ipMessage->csum = 0;
        ipMessage->csum = Csum((uint16_t*)ipMessage, IPV4_WORD_BYTES*ipMessage->hdrLen);
    }

    return sendBack;
}

void IPProvider::Send(uint32_t dstIP_BE, uint8_t proto, uint8_t* data, uint32_t size)
{
    uint8_t* buffer = (uint8_t*)MemoryManager::activeMemoryManager->malloc(sizeof(IPv4Message) + size);
    if(buffer == 0)
        return;

    IPv4Message* message = (IPv4Message*)buffer;
    message->version = IPV4_VERSION;
    message->hdrLen = sizeof(IPv4Message)/IPV4_WORD_BYTES;
    message->tos = 0;
    message->totalLen = bigEndian16(size + sizeof(IPv4Message));
    message->ident = IPV4_IDENT_BE;
    message->flagsOffset = IPV4_DF_BE;
    message->ttl = IPV4_DEFAULT_TTL;
    message->proto = proto;

    message->dstIP = dstIP_BE;
    message->srcIP = backend->GetIP();

    message->csum = 0;
    message->csum = Csum((uint16_t*)message, sizeof(IPv4Message));

    uint8_t* dataBuf = buffer + sizeof(IPv4Message);
    for(uint32_t i = 0; i < size; i++)
        dataBuf[i] = data[i];

    uint32_t route = dstIP_BE;
    if((dstIP_BE & subnetMask) != (message->srcIP & subnetMask))
        route = gatewayIP;

    backend->Send(arp->Resolve(route), this->etherType_BE, buffer, sizeof(IPv4Message) + size);
    MemoryManager::activeMemoryManager->free(buffer);
}

uint16_t IPProvider::Csum(uint16_t* data, uint32_t lenBytes)
{
    uint32_t temp = 0;

    for(uint32_t i = 0; i < lenBytes/2; i++)
        temp += ((data[i] & IPV4_HIGH_BYTE_MASK) >> 8) | ((data[i] & IPV4_LOW_BYTE_MASK) << 8);

    if(lenBytes % 2)
        temp += ((uint16_t)((char*)data)[lenBytes-1]) << 8;

    while(temp & IPV4_CSUM_CARRY_MASK)
        temp = (temp & IPV4_CSUM_WORD_MASK) + (temp >> 16);

    return ((~temp & IPV4_HIGH_BYTE_MASK) >> 8) | ((~temp & IPV4_LOW_BYTE_MASK) << 8);
}
