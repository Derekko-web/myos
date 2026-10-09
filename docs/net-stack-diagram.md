# UDP, TCP, and HTTP Server Diagram

This is an editable Mermaid diagram set for the UDP/TCP pieces and the small HTTP demo server wired up in `src/kernel.cpp`.

Focused timeline versions:

- `docs/net-stack-timeline-all-in-one.md`: one large continuous timeline
- `docs/net-stack-timeline-7-part.md`: the same flow split into seven smaller diagrams

## Network Stack Timeline

The timeline is split into smaller slices so each piece stays readable without deep zoom.

### Startup Wiring

```mermaid
flowchart TB
    K["kernelMain"]
    NIC["amd_am79c973<br/>SetIP(10.0.2.15)"]
    EF["EtherFrameProvider"]
    ARP["ARP"]
    IP["IPProvider<br/>gateway + subnet"]
    ICMP["ICMP"]
    UDP["UDPProvider<br/>Listen/Bind 1234"]
    TCP["TCPProvider<br/>Listen/Bind 1234"]

    K --> NIC
    K --> EF
    EF --> NIC
    K --> ARP
    ARP --> EF
    K --> IP
    IP --> EF
    IP --> ARP
    K --> ICMP
    ICMP --> IP
    K --> UDP
    UDP --> IP
    K --> TCP
    TCP --> IP
```

### Gateway ARP Bootstrap

```mermaid
sequenceDiagram
    title Initial gateway MAC lookup

    participant App as Kernel setup
    participant ARP as ARP
    participant EF as EtherFrameProvider
    participant NIC as amd_am79c973
    participant Wire as Network Wire

    App->>ARP: RequestMAC(gateway)
    ARP->>EF: Send(broadcast MAC, ARPMessage)
    EF->>NIC: Send(ETHERTYPE_ARP)
    NIC->>Wire: Ethernet frame: ARP request
```

### Receive Entry

```mermaid
sequenceDiagram
    title Frame receive and EtherType split

    participant Wire as Network Wire
    participant NIC as amd_am79c973
    participant EF as EtherFrameProvider
    participant ARP as ARP
    participant IP as IPProvider

    Wire-->>NIC: Ethernet frame bytes
    NIC->>EF: Receive() -> OnRawDataReceived(buffer, size)
    EF->>EF: check destination MAC and etherType

    alt etherType == ARP
        EF->>ARP: OnEtherFrameReceived(payload)
        alt request for our IP
            ARP->>ARP: mutate packet into ARP reply
            ARP-->>EF: return true
            EF->>EF: swap Ethernet src/dst MAC
            EF-->>NIC: return true
            NIC->>Wire: Send(buffer, size): ARP reply
        else reply for us
            ARP->>ARP: cache srcIP -> srcMAC
            ARP-->>EF: return false
        end
    else etherType == IPv4
        EF->>IP: OnEtherFrameReceived(ip packet)
    end
```

### IPv4 Handler Dispatch

```mermaid
sequenceDiagram
    title IPv4 protocol handlers

    participant IP as IPProvider
    participant ICMP as ICMP
    participant UDP as UDPProvider
    participant TCP as TCPProvider
    participant App as Kernel Handler / Socket

    IP->>IP: validate dstIP, totalLen, hdrLen
    IP->>IP: dispatch by ipMessage->proto

    alt proto == ICMP
        IP->>ICMP: OnIPReceived(...)
        alt echo request
            ICMP->>ICMP: change to echo reply, recompute checksum
            ICMP-->>IP: return true for in-place reply
        else echo reply
            ICMP->>App: printf("ping response ...")
            ICMP-->>IP: return false
        end
    else proto == UDP
        IP->>UDP: OnIPReceived(...)
        UDP->>UDP: find socket by port/IP tuple
        UDP->>App: UDPSocket::HandleUDPMessage(data)
        UDP-->>IP: return false
    else proto == TCP
        IP->>TCP: OnIPReceived(...)
        TCP->>TCP: find/listen socket, update TCP state
        TCP->>App: TCPSocket::HandleTCPMessage(data)
        TCP-->>IP: return false
    end
```

### In-Place Reply Path

```mermaid
sequenceDiagram
    title ICMP replies reuse the receive buffer

    participant Handler as ICMP handler
    participant IP as IPProvider
    participant EF as EtherFrameProvider
    participant NIC as amd_am79c973
    participant Wire as Network Wire

    Handler-->>IP: return true after mutating payload
    IP->>IP: swap src/dst IP, reset TTL, recompute checksum
    IP-->>EF: return true
    EF->>EF: swap Ethernet src/dst MAC
    EF-->>NIC: return true
    NIC->>Wire: Send(buffer, size): reply
```

### TCP Generated Response

```mermaid
sequenceDiagram
    title TCP ACK/SYN-ACK/RST/FIN responses

    participant TCP as TCPProvider
    participant IP as IPProvider
    participant ARP as ARP
    participant EF as EtherFrameProvider
    participant NIC as amd_am79c973
    participant Wire as Network Wire

    TCP->>IP: IPHandler::Send(remoteIP, TCP segment)
    IP->>IP: choose route or gateway
    IP->>ARP: Resolve(next hop)
    ARP-->>IP: destination MAC or broadcast while resolving
    IP->>EF: Send(dstMAC, ETHERTYPE_IPV4, IPv4 packet)
    EF->>NIC: Send(Ethernet frame)
    NIC->>Wire: Ethernet frame: IPv4/TCP response
```

### App Send Path

```mermaid
flowchart TB
    App["Kernel Handler / Socket"]
    UDP["UDPSocket::Send<br/>build UDPHeader + payload"]
    TCP["TCPSocket::Send<br/>build TCPHeader + checksum"]
    IP["IPProvider::Send<br/>build IPv4Message<br/>choose route/gateway"]
    ARP["ARP::Resolve(next hop)"]
    EF["EtherFrameProvider::Send<br/>ETHERTYPE_IPV4"]
    NIC["amd_am79c973::Send"]
    Wire["Network Wire"]

    App --> UDP
    App --> TCP
    UDP --> IP
    TCP --> IP
    IP --> ARP
    ARP --> IP
    IP --> EF
    EF --> NIC
    NIC --> Wire
```

## Structs, Classes, and Ownership

```mermaid
classDiagram
    direction TB

    class IPHandler {
        -IPProvider* backend
        -uint8_t proto
        +OnIPReceived(srcIP_BE, dstIP_BE, ipPayload, size) bool
        +Send(dstIP_BE, ipPayload, size) void
    }

    class IPProvider {
        -IPHandler* handlers[255]
        -ARP* arp
        -uint32_t gatewayIP
        -uint32_t subnetMask
        +OnEtherFrameReceived(payload, size) bool
        +Send(dstIP_BE, proto, buffer, size) void
        +Csum(data, lenBytes) uint16_t
    }

    class UDPHeader {
        <<struct>>
        uint16_t srcPort
        uint16_t dstPort
        uint16_t len
        uint16_t csum
    }

    class UDPHandler {
        +HandleUDPMessage(socket, data, size) void
    }

    class UDPSocket {
        -uint16_t remotePort
        -uint32_t remoteIP
        -uint16_t localPort
        -uint32_t localIP
        -UDPProvider* backend
        -UDPHandler* handler
        -bool listening
        +HandleUDPMessage(data, size) void
        +Send(data, size) void
        +Disconnect() void
    }

    class UDPProvider {
        -UDPSocket* sockets[65535]
        -uint16_t numSockets
        -uint16_t freePort
        +OnIPReceived(srcIP_BE, dstIP_BE, ipPayload, size) bool
        +Connect(ip, port) UDPSocket*
        +Listen(port) UDPSocket*
        +Bind(socket, handler) void
        +Send(socket, data, size) void
        +Disconnect(socket) void
    }

    class TCPHeader {
        <<struct>>
        uint16_t srcPort
        uint16_t dstPort
        uint32_t seqNum
        uint32_t ackNum
        uint8_t hdrSize32
        uint8_t flags
        uint16_t wndSize
        uint16_t csum
        uint16_t urgPtr
        uint32_t opts
    }

    class TCPPseudoHeader {
        <<struct>>
        uint32_t srcIP
        uint32_t dstIP
        uint16_t proto
        uint16_t totalLen
    }

    class TCPHandler {
        +HandleTCPMessage(socket, data, size) bool
    }

    class TCPSocket {
        -uint16_t remotePort
        -uint32_t remoteIP
        -uint16_t localPort
        -uint32_t localIP
        -uint32_t seqNum
        -uint32_t ackNum
        -TCPProvider* backend
        -TCPHandler* handler
        -TCPSocketState state
        +HandleTCPMessage(data, size) bool
        +Send(data, size) void
        +Disconnect() void
    }

    class TCPProvider {
        -TCPSocket* sockets[65535]
        -uint16_t numSockets
        -uint16_t freePort
        +OnIPReceived(srcIP_BE, dstIP_BE, ipPayload, size) bool
        +Connect(ip, port) TCPSocket*
        +Listen(port) TCPSocket*
        +Bind(socket, handler) void
        +Send(socket, data, size, flags) void
        +Disconnect(socket) void
    }

    class PrintfUDPHandler {
        +HandleUDPMessage(socket, data, size) void
    }

    class PrintfTCPHandler {
        +HandleTCPMessage(socket, data, size) bool
    }

    UDPProvider --|> IPHandler : IP_PROTOCOL_UDP
    TCPProvider --|> IPHandler : IP_PROTOCOL_TCP
    IPHandler --> IPProvider : backend
    IPProvider o-- IPHandler : handlers[proto]

    UDPProvider o-- UDPSocket : sockets[]
    UDPSocket --> UDPProvider : backend
    UDPSocket --> UDPHandler : handler
    UDPProvider ..> UDPHeader : parses/builds
    PrintfUDPHandler --|> UDPHandler

    TCPProvider o-- TCPSocket : sockets[]
    TCPSocket --> TCPProvider : backend
    TCPSocket --> TCPHandler : handler
    TCPProvider ..> TCPHeader : parses/builds
    TCPProvider ..> TCPPseudoHeader : checksum
    PrintfTCPHandler --|> TCPHandler
```

## Runtime Wiring

```mermaid
flowchart TB
    subgraph kernel["kernel.cpp demo setup"]
        K["kernelMain network setup"]
        UH["PrintfUDPHandler<br/>prints UDP payload bytes"]
        TH["PrintfTCPHandler<br/>minimal HTTP server<br/>GET / -> 200 OK<br/>other GET -> 204<br/>then closes TCP socket"]
    end

    subgraph transport["Transport layer"]
        UDP["UDPProvider<br/>Listen(1234)<br/>Bind(socket, PrintfUDPHandler)"]
        US["UDPSocket<br/>local port 1234<br/>listening = true"]
        TCP["TCPProvider<br/>Listen(1234)<br/>Bind(socket, PrintfTCPHandler)"]
        TLS["listening TCPSocket<br/>local port 1234<br/>state = LISTEN"]
        TCS["accepted TCPSocket<br/>remote IP/port set<br/>seq/ack tracked<br/>state moves through handshake"]
    end

    subgraph ip["IPv4 layer"]
        IP["IPProvider<br/>handlers[0x11] = UDPProvider<br/>handlers[0x06] = TCPProvider"]
        ARP["ARP<br/>Resolve(next hop MAC)"]
        ETH["EtherFrameProvider<br/>send/receive Ethernet frames"]
    end

    K --> IP
    K --> UDP
    K --> TCP
    K --> UH
    K --> TH

    UDP --> US
    UDP --> UH
    TCP --> TLS
    TCP --> TH
    TLS -. "SYN creates connection socket" .-> TCS
    TCS --> TH

    UDP -- "IPHandler::Send(proto=UDP)" --> IP
    TCP -- "IPHandler::Send(proto=TCP)" --> IP
    IP --> ARP
    IP --> ETH
```

## Incoming UDP Flow

```mermaid
sequenceDiagram
    participant Client
    participant ETH as EtherFrameProvider
    participant IP as IPProvider
    participant UDP as UDPProvider
    participant Sock as UDPSocket:1234
    participant Handler as PrintfUDPHandler

    Client->>ETH: Ethernet frame containing IPv4/UDP
    ETH->>IP: OnEtherFrameReceived(payload, size)
    IP->>UDP: handlers[0x11]->OnIPReceived(src, dst, udpPayload, len)
    UDP->>UDP: Parse UDPHeader and find matching socket
    UDP->>Sock: Set remote IP/port when listener matched
    UDP->>Sock: HandleUDPMessage(payload, payloadLen)
    Sock->>Handler: HandleUDPMessage(socket, data, size)
    Handler->>Handler: printf each received byte
```

## Incoming TCP / HTTP Flow

```mermaid
sequenceDiagram
    participant Client
    participant IP as IPProvider
    participant TCP as TCPProvider
    participant Listen as Listening TCPSocket:1234
    participant Conn as Accepted TCPSocket
    participant HTTP as PrintfTCPHandler

    Client->>IP: IPv4 packet with proto 0x06
    IP->>TCP: handlers[0x06]->OnIPReceived(src, dst, tcpSegment, len)

    alt SYN to listening socket
        TCP->>Listen: Match local IP/port and state LISTEN
        TCP->>Conn: Allocate connection socket and copy handler
        TCP->>Conn: state = SYN_RECEIVED, remote IP/port set
        TCP->>IP: Send SYN|ACK
    else ACK completes handshake
        TCP->>Conn: state = ESTABLISHED
    else Data segment in sequence
        TCP->>Conn: Advance ackNum by payload length
        Conn->>HTTP: HandleTCPMessage(data, size)
        HTTP->>HTTP: Print request bytes
        alt Request begins with GET /
            HTTP->>Conn: Send("HTTP/1.1 200 OK ...")
        else Other GET request
            HTTP->>Conn: Send("HTTP/1.1 204 No Content ...")
        end
        Conn->>TCP: backend->Send(PSH|ACK)
        HTTP->>Conn: Disconnect()
        Conn->>TCP: backend->Disconnect()
        TCP->>IP: Send FIN|ACK
    else FIN or reset-worthy segment
        TCP->>Conn: Move close state or send RST
    end
```

## Key Source Locations

- `include/net/udp.h`: `UDPHeader`, `UDPHandler`, `UDPSocket`, `UDPProvider`
- `src/net/udp.cpp`: UDP receive dispatch, listen/connect/bind/send/disconnect behavior
- `include/net/tcp.h`: `TCPHeader`, `TCPPseudoHeader`, `TCPHandler`, `TCPSocket`, `TCPProvider`, TCP flags/states
- `src/net/tcp.cpp`: TCP state handling, accepted socket allocation, ACK/FIN/RST behavior, packet send/checksum behavior
- `src/kernel.cpp`: `PrintfUDPHandler`, `PrintfTCPHandler`, and port `1234` setup for both UDP printing and TCP HTTP responses
