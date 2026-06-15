#include <net/ipv4.h>

using namespace myos;
using namespace myos::common;
using namespace myos::net;

static const uint16_t IPV4_LOW_BYTE_MASK = 0x00FF;
static const uint16_t IPV4_HIGH_BYTE_MASK = 0xFF00;
static const uint32_t IPV4_CHECKSUM_CARRY_MASK = 0xFFFF0000;
static const uint32_t IPV4_CHECKSUM_WORD_MASK = 0xFFFF;
static const uint8_t IPV4_VERSION = 4;
static const uint8_t IPV4_WORD_BYTES = 4;
static const uint8_t IPV4_DEFAULT_TTL = 0x40;
static const uint16_t IPV4_DEFAULT_IDENT_BE = 0x0100;
static const uint16_t IPV4_DONT_FRAGMENT_BE = 0x0040;

static uint16_t bigEndian16(uint16_t x)
{
    return ((x & IPV4_HIGH_BYTE_MASK) >> 8) | ((x & IPV4_LOW_BYTE_MASK) << 8);
}

InternetProtocolHandler::InternetProtocolHandler(InternetProtocolProvider* backend, uint8_t protocol)
{
    this->backend = backend;
    this->ip_protocol = protocol;
    backend->handlers[protocol] = this;
}

InternetProtocolHandler::~InternetProtocolHandler()
{
    if(backend->handlers[ip_protocol] == this)
        backend->handlers[ip_protocol] = 0;
}

bool InternetProtocolHandler::OnInternetProtocolReceived(uint32_t srcIP_BE, uint32_t dstIP_BE,
                                                         uint8_t* internetprotocolPayload, uint32_t size)
{
    return false;
}

void InternetProtocolHandler::Send(uint32_t dstIP_BE, uint8_t* internetprotocolPayload, uint32_t size)
{
    backend->Send(dstIP_BE, ip_protocol, internetprotocolPayload, size);
}

InternetProtocolProvider::InternetProtocolProvider(EtherFrameProvider* backend,
                                                   AddressResolutionProtocol* arp,
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

InternetProtocolProvider::~InternetProtocolProvider()
{
}

bool InternetProtocolProvider::OnEtherFrameReceived(uint8_t* etherframePayload, uint32_t size)
{
    if(size < sizeof(InternetProtocolV4Message))
        return false;

    InternetProtocolV4Message* ipmessage = (InternetProtocolV4Message*)etherframePayload;
    bool sendBack = false;

    if(ipmessage->dstIP == backend->GetIPAddress())
    {
        uint32_t length = bigEndian16(ipmessage->totalLength);
        if(length > size)
            length = size;

        uint32_t headerLength = IPV4_WORD_BYTES * ipmessage->headerLength;
        if(headerLength <= length && handlers[ipmessage->protocol] != 0)
            sendBack = handlers[ipmessage->protocol]->OnInternetProtocolReceived(
                ipmessage->srcIP, ipmessage->dstIP,
                etherframePayload + headerLength, length - headerLength);
    }

    if(sendBack)
    {
        uint32_t temp = ipmessage->dstIP;
        ipmessage->dstIP = ipmessage->srcIP;
        ipmessage->srcIP = temp;

        ipmessage->timeToLive = IPV4_DEFAULT_TTL;
        ipmessage->checksum = 0;
        ipmessage->checksum = Checksum((uint16_t*)ipmessage, IPV4_WORD_BYTES*ipmessage->headerLength);
    }

    return sendBack;
}

void InternetProtocolProvider::Send(uint32_t dstIP_BE, uint8_t protocol, uint8_t* data, uint32_t size)
{
    uint8_t* buffer = (uint8_t*)MemoryManager::activeMemoryManager->malloc(sizeof(InternetProtocolV4Message) + size);
    if(buffer == 0)
        return;

    InternetProtocolV4Message* message = (InternetProtocolV4Message*)buffer;
    message->version = IPV4_VERSION;
    message->headerLength = sizeof(InternetProtocolV4Message)/IPV4_WORD_BYTES;
    message->tos = 0;
    message->totalLength = bigEndian16(size + sizeof(InternetProtocolV4Message));
    message->ident = IPV4_DEFAULT_IDENT_BE;
    message->flagsAndOffset = IPV4_DONT_FRAGMENT_BE;
    message->timeToLive = IPV4_DEFAULT_TTL;
    message->protocol = protocol;

    message->dstIP = dstIP_BE;
    message->srcIP = backend->GetIPAddress();

    message->checksum = 0;
    message->checksum = Checksum((uint16_t*)message, sizeof(InternetProtocolV4Message));

    uint8_t* databuffer = buffer + sizeof(InternetProtocolV4Message);
    for(uint32_t i = 0; i < size; i++)
        databuffer[i] = data[i];

    uint32_t route = dstIP_BE;
    if((dstIP_BE & subnetMask) != (message->srcIP & subnetMask))
        route = gatewayIP;

    backend->Send(arp->Resolve(route), this->etherType_BE, buffer, sizeof(InternetProtocolV4Message) + size);
    MemoryManager::activeMemoryManager->free(buffer);
}

uint16_t InternetProtocolProvider::Checksum(uint16_t* data, uint32_t lengthInBytes)
{
    uint32_t temp = 0;

    for(uint32_t i = 0; i < lengthInBytes/2; i++)
        temp += ((data[i] & IPV4_HIGH_BYTE_MASK) >> 8) | ((data[i] & IPV4_LOW_BYTE_MASK) << 8);

    if(lengthInBytes % 2)
        temp += ((uint16_t)((char*)data)[lengthInBytes-1]) << 8;

    while(temp & IPV4_CHECKSUM_CARRY_MASK)
        temp = (temp & IPV4_CHECKSUM_WORD_MASK) + (temp >> 16);

    return ((~temp & IPV4_HIGH_BYTE_MASK) >> 8) | ((~temp & IPV4_LOW_BYTE_MASK) << 8);
}
