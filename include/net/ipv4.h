#ifndef __MYOS__NET__IPV4_H
#define __MYOS__NET__IPV4_H

#include <common/types.h>
#include <net/etherframe.h>
#include <net/arp.h>

namespace myos
{
    namespace net
    {
        static const common::uint8_t IP_PROTOCOL_ICMP = 0x01;
        static const common::uint8_t IP_PROTOCOL_TCP = 0x06;
        static const common::uint8_t IP_PROTOCOL_UDP = 0x11;

        struct IPv4Message
        {
            common::uint8_t hdrLen : 4;
            common::uint8_t version : 4;
            common::uint8_t tos;
            common::uint16_t totalLen;

            common::uint16_t ident;
            common::uint16_t flagsOffset;

            common::uint8_t ttl;
            common::uint8_t proto;
            common::uint16_t csum;

            common::uint32_t srcIP;
            common::uint32_t dstIP;
        } __attribute__((packed));

        class IPProvider;

        class IPHandler
        {
        protected:
            IPProvider* backend;
            common::uint8_t proto;

        public:
            IPHandler(IPProvider* backend, common::uint8_t proto);
            ~IPHandler();

            virtual bool OnIPReceived(common::uint32_t srcIP_BE, common::uint32_t dstIP_BE,
                                      common::uint8_t* ipPayload,
                                      common::uint32_t size);
            void Send(common::uint32_t dstIP_BE, common::uint8_t* ipPayload, common::uint32_t size);
        };

        class IPProvider : public EtherFrameHandler
        {
            friend class IPHandler;

        protected:
            IPHandler* handlers[255];
            ARP* arp;
            common::uint32_t gatewayIP;
            common::uint32_t subnetMask;

        public:
            IPProvider(EtherFrameProvider* backend,
                       ARP* arp,
                       common::uint32_t gatewayIP,
                       common::uint32_t subnetMask);
            ~IPProvider();

            bool OnEtherFrameReceived(common::uint8_t* etherframePayload, common::uint32_t size);

            void Send(common::uint32_t dstIP_BE,
                      common::uint8_t proto,
                      common::uint8_t* buffer,
                      common::uint32_t size);

            static common::uint16_t Csum(common::uint16_t* data, common::uint32_t lenBytes);
        };
    }
}

#endif
