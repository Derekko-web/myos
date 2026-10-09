# Network Stack Timeline: Seven Part View

This is the same network stack flow split into smaller diagrams so each piece stays readable without deep zoom.

## 1. Startup Wiring

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

## 2. Gateway ARP Bootstrap

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

## 3. Receive Entry

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

## 4. IPv4 Handler Dispatch

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

## 5. In-Place Reply Path

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

## 6. TCP Generated Response

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

## 7. App Send Path

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
