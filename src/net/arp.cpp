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
static const uint64_t ARP_UNKNOWN_MAC = 0;

ARP::ARP(EtherFrameProvider* backend)
: EtherFrameHandler(backend, ETHERTYPE_ARP)
{
    numCacheEntries = 0;
}

ARP::~ARP()
{
}

bool ARP::OnEtherFrameReceived(uint8_t* etherframePayload, uint32_t size)
{
    if(size < sizeof(ARPMessage))
        return false;

    ARPMessage* arp = (ARPMessage*)etherframePayload;
    if(arp->hardwareType == ARP_HARDWARE_ETHERNET_BE
    && arp->proto == ARP_PROTOCOL_IPV4_BE
    && arp->hardwareAddressSize == ARP_HARDWARE_ADDRESS_SIZE_ETHERNET
    && arp->protocolAddressSize == ARP_PROTOCOL_ADDRESS_SIZE_IPV4
    && arp->dstIP == backend->GetIP())
    {
        switch(arp->command)
        {
            case ARP_COMMAND_REQUEST_BE:
                arp->command = ARP_COMMAND_REPLY_BE;
                arp->dstIP = arp->srcIP;
                arp->dstMAC = arp->srcMAC;
                arp->srcIP = backend->GetIP();
                arp->srcMAC = backend->GetMAC();
                return true;

            case ARP_COMMAND_REPLY_BE:
                if(numCacheEntries < ARP_CACHE_SIZE)
                {
                    ipCache[numCacheEntries] = arp->srcIP;
                    macCache[numCacheEntries] = arp->srcMAC;
                    numCacheEntries++;
                }
                break;
        }
    }

    return false;
}

void ARP::BroadcastMAC(uint32_t ip_BE)
{
    ARPMessage arp;
    arp.hardwareType = ARP_HARDWARE_ETHERNET_BE;
    arp.proto = ARP_PROTOCOL_IPV4_BE;
    arp.hardwareAddressSize = ARP_HARDWARE_ADDRESS_SIZE_ETHERNET;
    arp.protocolAddressSize = ARP_PROTOCOL_ADDRESS_SIZE_IPV4;
    arp.command = ARP_COMMAND_REPLY_BE;

    arp.srcMAC = backend->GetMAC();
    arp.srcIP = backend->GetIP();
    arp.dstMAC = Resolve(ip_BE);
    arp.dstIP = ip_BE;

    Send(arp.dstMAC, (uint8_t*)&arp, sizeof(ARPMessage));
}

void ARP::RequestMAC(uint32_t ip_BE)
{
    ARPMessage arp;
    arp.hardwareType = ARP_HARDWARE_ETHERNET_BE;
    arp.proto = ARP_PROTOCOL_IPV4_BE;
    arp.hardwareAddressSize = ARP_HARDWARE_ADDRESS_SIZE_ETHERNET;
    arp.protocolAddressSize = ARP_PROTOCOL_ADDRESS_SIZE_IPV4;
    arp.command = ARP_COMMAND_REQUEST_BE;

    arp.srcMAC = backend->GetMAC();
    arp.srcIP = backend->GetIP();
    arp.dstMAC = ARP_UNKNOWN_MAC;
    arp.dstIP = ip_BE;

    Send(ETHERNET_BROADCAST_MAC, (uint8_t*)&arp, sizeof(ARPMessage));
}

uint64_t ARP::GetMACFromCache(uint32_t ip_BE)
{
    for(int i = 0; i < numCacheEntries; i++)
        if(ipCache[i] == ip_BE)
            return macCache[i];
    return ETHERNET_BROADCAST_MAC;
}

uint64_t ARP::Resolve(uint32_t ip_BE)
{
    uint64_t result = GetMACFromCache(ip_BE);
    if(result == ETHERNET_BROADCAST_MAC)
        RequestMAC(ip_BE);

    return result;
}
