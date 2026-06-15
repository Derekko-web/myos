#include <drivers/amd_am79c973.h>

using namespace myos;
using namespace myos::common;
using namespace myos::drivers;
using namespace myos::hardwarecommunication;

static const uint16_t AMD_PORT_MAC2_OFFSET = 0x02;
static const uint16_t AMD_PORT_MAC4_OFFSET = 0x04;
static const uint16_t AMD_PORT_REGISTER_DATA_OFFSET = 0x10;
static const uint16_t AMD_PORT_REGISTER_ADDRESS_OFFSET = 0x12;
static const uint16_t AMD_PORT_RESET_OFFSET = 0x14;
static const uint16_t AMD_PORT_BUS_CONTROL_DATA_OFFSET = 0x16;

static const uint16_t AMD_BCR_SOFTWARE_STYLE = 20;
static const uint16_t AMD_BCR_SOFTWARE_STYLE_32BIT = 0x0102;
static const uint16_t AMD_CSR_STATUS_AND_CONTROL = 0;
static const uint16_t AMD_CSR_INIT_BLOCK_LOW = 1;
static const uint16_t AMD_CSR_INIT_BLOCK_HIGH = 2;
static const uint16_t AMD_CSR_FEATURE_CONTROL = 4;

static const uint16_t AMD_CSR0_INIT = 0x0001;
static const uint16_t AMD_CSR0_START = 0x0002;
static const uint16_t AMD_CSR0_STOP = 0x0004;
static const uint16_t AMD_CSR0_TRANSMIT_DEMAND = 0x0008;
static const uint16_t AMD_CSR0_INTERRUPT_ENABLE = 0x0040;
static const uint16_t AMD_CSR0_INIT_DONE = 0x0100;
static const uint16_t AMD_CSR0_TRANSMIT_INTERRUPT = 0x0200;
static const uint16_t AMD_CSR0_RECEIVE_INTERRUPT = 0x0400;
static const uint16_t AMD_CSR0_MEMORY_ERROR = 0x0800;
static const uint16_t AMD_CSR0_MISSED_FRAME = 0x1000;
static const uint16_t AMD_CSR0_COLLISION_ERROR = 0x2000;
static const uint16_t AMD_CSR0_ERROR = 0x8000;
static const uint16_t AMD_CSR4_TUTORIAL_FEATURE_BITS = 0x0C00;

static const uint16_t AMD_INIT_MODE_NORMAL = 0x0000;
static const uint32_t AMD_ALIGNMENT_MASK = 0xF;
static const uint32_t AMD_ADDRESS_LOW_WORD_MASK = 0xFFFF;
static const uint32_t AMD_DESCRIPTOR_INIT_FLAGS = 0xF7FF;
static const uint32_t AMD_DESCRIPTOR_OWN = 0x80000000;
static const uint32_t AMD_DESCRIPTOR_ERROR = 0x40000000;
static const uint32_t AMD_RECV_DESCRIPTOR_START_END = 0x03000000;
static const uint32_t AMD_BUFFER_BYTE_COUNT_MASK = 0x0FFF;
static const uint32_t AMD_SEND_DESCRIPTOR_READY_FLAGS = 0x8300F000;
static const uint32_t AMD_RECV_DESCRIPTOR_EMPTY_FLAGS = AMD_DESCRIPTOR_OWN | AMD_DESCRIPTOR_INIT_FLAGS;

RawDataHandler::RawDataHandler(amd_am79c973* backend)
{
    this->backend = backend;
    backend->SetHandler(this);
}

RawDataHandler::~RawDataHandler()
{
    backend->SetHandler(0);
}

bool RawDataHandler::OnRawDataReceived(uint8_t* buffer, uint32_t size)
{
    return false;
}

void RawDataHandler::Send(uint8_t* buffer, uint32_t size)
{
    backend->Send(buffer, size);
}

amd_am79c973::amd_am79c973(PeripheralComponentInterconnectDeviceDescriptor* dev, InterruptManager* interrupts)
:   Driver(),
    InterruptHandler(interrupts, dev->interrupt + interrupts->HardwareInterruptOffset()),
    MACAddress0Port(dev->portBase),
    MACAddress2Port(dev->portBase + AMD_PORT_MAC2_OFFSET),
    MACAddress4Port(dev->portBase + AMD_PORT_MAC4_OFFSET),
    registerDataPort(dev->portBase + AMD_PORT_REGISTER_DATA_OFFSET),
    registerAddressPort(dev->portBase + AMD_PORT_REGISTER_ADDRESS_OFFSET),
    resetPort(dev->portBase + AMD_PORT_RESET_OFFSET),
    busControlRegisterDataPort(dev->portBase + AMD_PORT_BUS_CONTROL_DATA_OFFSET)
{
    handler = 0;
    currentSendBuffer = 0;
    currentRecvBuffer = 0;

    uint64_t MAC0 = MACAddress0Port.Read() % 256;
    uint64_t MAC1 = MACAddress0Port.Read() / 256;
    uint64_t MAC2 = MACAddress2Port.Read() % 256;
    uint64_t MAC3 = MACAddress2Port.Read() / 256;
    uint64_t MAC4 = MACAddress4Port.Read() % 256;
    uint64_t MAC5 = MACAddress4Port.Read() / 256;

    uint64_t MAC = MAC5 << 40
                 | MAC4 << 32
                 | MAC3 << 24
                 | MAC2 << 16
                 | MAC1 << 8
                 | MAC0;

    registerAddressPort.Write(AMD_BCR_SOFTWARE_STYLE);
    busControlRegisterDataPort.Write(AMD_BCR_SOFTWARE_STYLE_32BIT);

    registerAddressPort.Write(AMD_CSR_STATUS_AND_CONTROL);
    registerDataPort.Write(AMD_CSR0_STOP);

    initBlock.mode = AMD_INIT_MODE_NORMAL;
    initBlock.reserved1 = 0;
    initBlock.numSendBuffers = 3;
    initBlock.reserved2 = 0;
    initBlock.numRecvBuffers = 3;
    initBlock.physicalAddress = MAC;
    initBlock.reserved3 = 0;
    initBlock.logicalAddress = 0;

    sendBufferDescr = (BufferDescriptor*)((((uint32_t)&sendBufferDescrMemory[0]) + 15) & ~AMD_ALIGNMENT_MASK);
    initBlock.sendBufferDescrAddress = (uint32_t)sendBufferDescr;
    recvBufferDescr = (BufferDescriptor*)((((uint32_t)&recvBufferDescrMemory[0]) + 15) & ~AMD_ALIGNMENT_MASK);
    initBlock.recvBufferDescrAddress = (uint32_t)recvBufferDescr;

    for(uint8_t i = 0; i < 8; i++)
    {
        sendBufferDescr[i].address = (((uint32_t)&sendBuffers[i][0]) + 15) & ~AMD_ALIGNMENT_MASK;
        sendBufferDescr[i].flags = AMD_DESCRIPTOR_INIT_FLAGS;
        sendBufferDescr[i].flags2 = 0;
        sendBufferDescr[i].avail = 0;

        recvBufferDescr[i].address = (((uint32_t)&recvBuffers[i][0]) + 15) & ~AMD_ALIGNMENT_MASK;
        recvBufferDescr[i].flags = AMD_RECV_DESCRIPTOR_EMPTY_FLAGS;
        recvBufferDescr[i].flags2 = 0;
        recvBufferDescr[i].avail = 0;
    }

    registerAddressPort.Write(AMD_CSR_INIT_BLOCK_LOW);
    registerDataPort.Write((uint32_t)(&initBlock) & AMD_ADDRESS_LOW_WORD_MASK);
    registerAddressPort.Write(AMD_CSR_INIT_BLOCK_HIGH);
    registerDataPort.Write(((uint32_t)(&initBlock) >> 16) & AMD_ADDRESS_LOW_WORD_MASK);
}

amd_am79c973::~amd_am79c973()
{
}

void amd_am79c973::Activate()
{
    registerAddressPort.Write(AMD_CSR_STATUS_AND_CONTROL);
    registerDataPort.Write(AMD_CSR0_INIT | AMD_CSR0_INTERRUPT_ENABLE);

    registerAddressPort.Write(AMD_CSR_FEATURE_CONTROL);
    uint32_t temp = registerDataPort.Read();
    registerAddressPort.Write(AMD_CSR_FEATURE_CONTROL);
    registerDataPort.Write(temp | AMD_CSR4_TUTORIAL_FEATURE_BITS);

    registerAddressPort.Write(AMD_CSR_STATUS_AND_CONTROL);
    registerDataPort.Write(AMD_CSR0_START | AMD_CSR0_INTERRUPT_ENABLE);
}

int amd_am79c973::Reset()
{
    resetPort.Read();
    resetPort.Write(0);
    return 10;
}

void printf(const char*);
void printfHex(uint8_t);

static bool traceOutputStarted = false;
static bool traceSentPending = false;

static void FlushTraceSent()
{
    if(traceSentPending)
    {
        printf(" SENT");
        traceSentPending = false;
    }
}

static void PrintTracePrefix(const char* prefix, bool flushSentBeforePrefix)
{
    if(flushSentBeforePrefix)
        FlushTraceSent();

    if(traceOutputStarted)
        printf("\n");

    printf(prefix);
    traceOutputStarted = true;
}

void amd_am79c973::ResetTraceOutput()
{
    traceOutputStarted = false;
    traceSentPending = false;
}

uint32_t amd_am79c973::HandleInterrupt(uint32_t esp)
{
    registerAddressPort.Write(AMD_CSR_STATUS_AND_CONTROL);
    uint32_t temp = registerDataPort.Read();
    bool received = (temp & AMD_CSR0_RECEIVE_INTERRUPT) == AMD_CSR0_RECEIVE_INTERRUPT;
    bool sent = (temp & AMD_CSR0_TRANSMIT_INTERRUPT) == AMD_CSR0_TRANSMIT_INTERRUPT;

    if((temp & AMD_CSR0_ERROR) == AMD_CSR0_ERROR) printf("AMD am79c973 ERROR\n");
    if((temp & AMD_CSR0_COLLISION_ERROR) == AMD_CSR0_COLLISION_ERROR) printf("AMD am79c973 COLLISION ERROR\n");
    if((temp & AMD_CSR0_MISSED_FRAME) == AMD_CSR0_MISSED_FRAME) printf("AMD am79c973 MISSED FRAME\n");
    if((temp & AMD_CSR0_MEMORY_ERROR) == AMD_CSR0_MEMORY_ERROR) printf("AMD am79c973 MEMORY ERROR\n");
    if(received) Receive();
    if(sent) traceSentPending = true;
    if(received) FlushTraceSent();

    registerAddressPort.Write(AMD_CSR_STATUS_AND_CONTROL);
    registerDataPort.Write(temp);

    if((temp & AMD_CSR0_INIT_DONE) == AMD_CSR0_INIT_DONE) printf("AMD am79c973 INIT DONE\n");

    return esp;
}

void amd_am79c973::Send(uint8_t* buffer, int size)
{
    if(buffer == 0 || size <= 0)
        return;

    int sendDescriptor = currentSendBuffer;
    currentSendBuffer = (currentSendBuffer + 1) % 8;

    if(size > 1518)
        size = 1518;

    for(uint8_t* src = buffer + size - 1,
                *dst = (uint8_t*)(sendBufferDescr[sendDescriptor].address + size - 1);
        src >= buffer; src--, dst--)
    {
        *dst = *src;
    }

    PrintTracePrefix("SENDING: ", true);
    for(int i = 0; i < (size > 64 ? 64 : size); i++)
    {
        printfHex(buffer[i]);
        printf(" ");
    }

    sendBufferDescr[sendDescriptor].avail = 0;
    sendBufferDescr[sendDescriptor].flags2 = 0;
    sendBufferDescr[sendDescriptor].flags = AMD_SEND_DESCRIPTOR_READY_FLAGS
                                          | ((uint16_t)((-size) & AMD_BUFFER_BYTE_COUNT_MASK));
    registerAddressPort.Write(AMD_CSR_STATUS_AND_CONTROL);
    registerDataPort.Write(AMD_CSR0_TRANSMIT_DEMAND | AMD_CSR0_INTERRUPT_ENABLE);
}

void amd_am79c973::Receive()
{
    PrintTracePrefix("RECEIVING: ", false);

    for(; (recvBufferDescr[currentRecvBuffer].flags & AMD_DESCRIPTOR_OWN) == 0;
        currentRecvBuffer = (currentRecvBuffer + 1) % 8)
    {
        if(!(recvBufferDescr[currentRecvBuffer].flags & AMD_DESCRIPTOR_ERROR)
            && (recvBufferDescr[currentRecvBuffer].flags & AMD_RECV_DESCRIPTOR_START_END) == AMD_RECV_DESCRIPTOR_START_END)
        {
            uint32_t size = recvBufferDescr[currentRecvBuffer].flags & AMD_BUFFER_BYTE_COUNT_MASK;
            if(size > 64)
                size -= 4;

            uint8_t* buffer = (uint8_t*)(recvBufferDescr[currentRecvBuffer].address);

            for(uint32_t i = 0; i < (size > 64 ? 64 : size); i++)
            {
                printfHex(buffer[i]);
                printf(" ");
            }

            if(handler != 0 && handler->OnRawDataReceived(buffer, size))
                Send(buffer, size);
        }

        recvBufferDescr[currentRecvBuffer].flags2 = 0;
        recvBufferDescr[currentRecvBuffer].flags = AMD_RECV_DESCRIPTOR_EMPTY_FLAGS;
    }
}

void amd_am79c973::SetHandler(RawDataHandler* handler)
{
    this->handler = handler;
}

uint64_t amd_am79c973::GetMACAddress()
{
    return initBlock.physicalAddress;
}

void amd_am79c973::SetIPAddress(uint32_t ip)
{
    initBlock.logicalAddress = ip;
}

uint32_t amd_am79c973::GetIPAddress()
{
    return initBlock.logicalAddress;
}
