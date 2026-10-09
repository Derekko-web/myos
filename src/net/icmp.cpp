#include <net/icmp.h>

using namespace myos;
using namespace myos::common;
using namespace myos::net;

static const uint8_t ICMP_TYPE_ECHO_REPLY = 0;
static const uint8_t ICMP_TYPE_ECHO_REQUEST = 8;
static const uint8_t ICMP_CODE_ECHO = 0;
static const uint16_t ICMP_ECHO_DEMO_DATA = 0x3713;
static const uint8_t IP_OCTET_MASK = 0xFF;

ICMP::ICMP(IPProvider* backend)
: IPHandler(backend, IP_PROTOCOL_ICMP)
{
}

ICMP::~ICMP()
{
}

void printf(const char*);
void printfHex(uint8_t);

bool ICMP::OnIPReceived(uint32_t srcIP_BE, uint32_t dstIP_BE,
                        uint8_t* ipPayload,
                        uint32_t size)
{
    if(size < sizeof(ICMPMessage))
        return false;

    ICMPMessage* msg = (ICMPMessage*)ipPayload;

    switch(msg->type)
    {
        case ICMP_TYPE_ECHO_REPLY:
            printf("ping response from ");
            printfHex(srcIP_BE & IP_OCTET_MASK);
            printf(".");
            printfHex((srcIP_BE >> 8) & IP_OCTET_MASK);
            printf(".");
            printfHex((srcIP_BE >> 16) & IP_OCTET_MASK);
            printf(".");
            printfHex((srcIP_BE >> 24) & IP_OCTET_MASK);
            printf("\n");
            break;

        case ICMP_TYPE_ECHO_REQUEST:
            msg->type = ICMP_TYPE_ECHO_REPLY;
            msg->csum = 0;
            msg->csum = IPProvider::Csum((uint16_t*)msg,
                sizeof(ICMPMessage));
            return true;
    }

    return false;
}

void ICMP::RequestEchoReply(uint32_t ip_be)
{
    ICMPMessage icmp;
    icmp.type = ICMP_TYPE_ECHO_REQUEST;
    icmp.code = ICMP_CODE_ECHO;
    icmp.data = ICMP_ECHO_DEMO_DATA;
    icmp.csum = 0;
    icmp.csum = IPProvider::Csum((uint16_t*)&icmp,
        sizeof(ICMPMessage));

    IPHandler::Send(ip_be, (uint8_t*)&icmp, sizeof(ICMPMessage));
}
