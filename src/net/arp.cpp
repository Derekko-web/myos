#include <net/arp.h>

using namespace myos;
using namespace myos::common;
using namespace myos::net;

static const uint16_t ARP_HARDWARE_ETHERNET_BE = 0x0100;
static const uint16_t ARP_PROTOCOL_IPV4_BE = 0x0008;
static const uint8_t ARP_HARDWARE_ADDRESS_SIZE_ETHERNET = 6;
static const uint8_t ARP_PROTOCOL_ADDRESS_SIZE_IPV4 = 4;
static const uint16_t ARP_COMMAND_REQUEST_BE = 0x0100;
static const uint16_t ARP_COMMAND_REPLY_BE = 0x0200;
static const int ARP_CACHE_SIZE = 128;

AddressResolutionProtocol::AddressResolutionProtocol(EtherFrameProvider* backend)
: EtherFrameHandler(backend, ETHERTYPE_ARP)
{
    numCacheEntries = 0;
}

AddressResolutionProtocol::~AddressResolutionProtocol()
{
}

bool AddressResolutionProtocol::OnEtherFrameReceived(uint8_t* etherframePayload, uint32_t size)
{
    if(size < sizeof(AddressResolutionProtocolMessage))
        return false;

    AddressResolutionProtocolMessage* arp = (AddressResolutionProtocolMessage*)etherframePayload;
    if(arp->hardwareType == ARP_HARDWARE_ETHERNET_BE
    && arp->protocol == ARP_PROTOCOL_IPV4_BE
    && arp->hardwareAddressSize == ARP_HARDWARE_ADDRESS_SIZE_ETHERNET
    && arp->protocolAddressSize == ARP_PROTOCOL_ADDRESS_SIZE_IPV4
    && arp->dstIP == backend->GetIPAddress())
    {
        switch(arp->command)
        {
            case ARP_COMMAND_REQUEST_BE:
                arp->command = ARP_COMMAND_REPLY_BE;
                arp->dstIP = arp->srcIP;
                arp->dstMAC = arp->srcMAC;
                arp->srcIP = backend->GetIPAddress();
                arp->srcMAC = backend->GetMACAddress();
                return true;

            case ARP_COMMAND_REPLY_BE:
                if(numCacheEntries < ARP_CACHE_SIZE)
                {
                    IPcache[numCacheEntries] = arp->srcIP;
                    MACcache[numCacheEntries] = arp->srcMAC;
                    numCacheEntries++;
                }
                break;
        }
    }

    return false;
}

void AddressResolutionProtocol::BroadcastMACAddress(uint32_t IP_BE)
{
    AddressResolutionProtocolMessage arp;
    arp.hardwareType = ARP_HARDWARE_ETHERNET_BE;
    arp.protocol = ARP_PROTOCOL_IPV4_BE;
    arp.hardwareAddressSize = ARP_HARDWARE_ADDRESS_SIZE_ETHERNET;
    arp.protocolAddressSize = ARP_PROTOCOL_ADDRESS_SIZE_IPV4;
    arp.command = ARP_COMMAND_REPLY_BE;

    arp.srcMAC = backend->GetMACAddress();
    arp.srcIP = backend->GetIPAddress();
    arp.dstMAC = Resolve(IP_BE);
    arp.dstIP = IP_BE;

    Send(arp.dstMAC, (uint8_t*)&arp, sizeof(AddressResolutionProtocolMessage));
}

void AddressResolutionProtocol::RequestMACAddress(uint32_t IP_BE)
{
    AddressResolutionProtocolMessage arp;
    arp.hardwareType = ARP_HARDWARE_ETHERNET_BE;
    arp.protocol = ARP_PROTOCOL_IPV4_BE;
    arp.hardwareAddressSize = ARP_HARDWARE_ADDRESS_SIZE_ETHERNET;
    arp.protocolAddressSize = ARP_PROTOCOL_ADDRESS_SIZE_IPV4;
    arp.command = ARP_COMMAND_REQUEST_BE;

    arp.srcMAC = backend->GetMACAddress();
    arp.srcIP = backend->GetIPAddress();
    arp.dstMAC = ETHERNET_BROADCAST_MAC;
    arp.dstIP = IP_BE;

    Send(arp.dstMAC, (uint8_t*)&arp, sizeof(AddressResolutionProtocolMessage));
}

uint64_t AddressResolutionProtocol::GetMACFromCache(uint32_t IP_BE)
{
    for(int i = 0; i < numCacheEntries; i++)
        if(IPcache[i] == IP_BE)
            return MACcache[i];
    return ETHERNET_BROADCAST_MAC;
}

uint64_t AddressResolutionProtocol::Resolve(uint32_t IP_BE)
{
    uint64_t result = GetMACFromCache(IP_BE);
    if(result == ETHERNET_BROADCAST_MAC)
        RequestMACAddress(IP_BE);

    while(result == ETHERNET_BROADCAST_MAC)
        result = GetMACFromCache(IP_BE);

    return result;
}
