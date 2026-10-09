
#include <hardwarecommunication/interrupts.h>
using namespace myos;
using namespace myos::common;
using namespace myos::hardwarecommunication;


void printf(const char* str);
void printfHex(uint8_t);

static const uint16_t IDT_HANDLER_ADDRESS_MASK = 0xFFFF;
static const uint8_t IDT_DESCRIPTOR_PRESENT = 0x80;
static const uint8_t IDT_INTERRUPT_GATE = 0x0E;
static const uint8_t IDT_PRIVILEGE_LEVEL_MASK = 0x03;

static const uint16_t PIC_MASTER_COMMAND_PORT = 0x20;
static const uint16_t PIC_MASTER_DATA_PORT = 0x21;
static const uint16_t PIC_SLAVE_COMMAND_PORT = 0xA0;
static const uint16_t PIC_SLAVE_DATA_PORT = 0xA1;
static const uint8_t PIC_INITIALIZE = 0x11;
static const uint8_t PIC_MASTER_HAS_SLAVE_AT_IRQ2 = 0x04;
static const uint8_t PIC_SLAVE_CASCADE_ID = 0x02;
static const uint8_t PIC_8086_MODE = 0x01;
static const uint8_t PIC_UNMASK_ALL_IRQS = 0x00;
static const uint8_t PIC_END_OF_INTERRUPT = 0x20;
static const uint8_t PIC_SLAVE_IRQ_START = 8;
static const uint8_t PIC_IRQ_COUNT = 16;

static const uint8_t EXCEPTION_DIVIDE_BY_ZERO = 0x00;
static const uint8_t EXCEPTION_DEBUG = 0x01;
static const uint8_t EXCEPTION_NON_MASKABLE_INTERRUPT = 0x02;
static const uint8_t EXCEPTION_BREAKPOINT = 0x03;
static const uint8_t EXCEPTION_OVERFLOW = 0x04;
static const uint8_t EXCEPTION_BOUND_RANGE_EXCEEDED = 0x05;
static const uint8_t EXCEPTION_INVALID_OPCODE = 0x06;
static const uint8_t EXCEPTION_DEVICE_NOT_AVAILABLE = 0x07;
static const uint8_t EXCEPTION_DOUBLE_FAULT = 0x08;
static const uint8_t EXCEPTION_COPROCESSOR_SEGMENT_OVERRUN = 0x09;
static const uint8_t EXCEPTION_INVALID_TSS = 0x0A;
static const uint8_t EXCEPTION_SEGMENT_NOT_PRESENT = 0x0B;
static const uint8_t EXCEPTION_STACK_SEGMENT_FAULT = 0x0C;
static const uint8_t EXCEPTION_GENERAL_PROTECTION_FAULT = 0x0D;
static const uint8_t EXCEPTION_PAGE_FAULT = 0x0E;
static const uint8_t EXCEPTION_RESERVED = 0x0F;
static const uint8_t EXCEPTION_X87_FLOATING_POINT = 0x10;
static const uint8_t EXCEPTION_ALIGNMENT_CHECK = 0x11;
static const uint8_t EXCEPTION_MACHINE_CHECK = 0x12;
static const uint8_t EXCEPTION_SIMD_FLOATING_POINT = 0x13;

static const uint8_t IRQ_TIMER = 0x00;
static const uint8_t IRQ_KEYBOARD = 0x01;
static const uint8_t IRQ_CASCADE = 0x02;
static const uint8_t IRQ_COM2 = 0x03;
static const uint8_t IRQ_COM1 = 0x04;
static const uint8_t IRQ_LPT2 = 0x05;
static const uint8_t IRQ_FLOPPY = 0x06;
static const uint8_t IRQ_LPT1 = 0x07;
static const uint8_t IRQ_RTC = 0x08;
static const uint8_t IRQ_FREE_9 = 0x09;
static const uint8_t IRQ_FREE_10 = 0x0A;
static const uint8_t IRQ_FREE_11 = 0x0B;
static const uint8_t IRQ_MOUSE = 0x0C;
static const uint8_t IRQ_FPU = 0x0D;
static const uint8_t IRQ_PRIMARY_ATA = 0x0E;
static const uint8_t IRQ_SECONDARY_ATA = 0x0F;




InterruptHandler::InterruptHandler(InterruptManager* interruptManager, uint8_t InterruptNumber)
{
    this->InterruptNumber = InterruptNumber;
    this->interruptManager = interruptManager;
    interruptManager->handlers[InterruptNumber] = this;
}

InterruptHandler::~InterruptHandler()
{
    if(interruptManager->handlers[InterruptNumber] == this)
        interruptManager->handlers[InterruptNumber] = 0;
}

uint32_t InterruptHandler::HandleInterrupt(uint32_t esp)
{
    return esp;
}










InterruptManager::GateDescriptor InterruptManager::interruptDescriptorTable[256];
InterruptManager* InterruptManager::ActiveInterruptManager = 0;




void InterruptManager::SetInterruptDescriptorTableEntry(uint8_t interrupt,
    uint16_t CodeSegment, void (*handler)(), uint8_t DescriptorPrivilegeLevel, uint8_t DescriptorType)
{
    // address of pointer to code segment (relative to global descriptor table)
    // and address of the handler (relative to segment)
    interruptDescriptorTable[interrupt].handlerAddressLowBits = ((uint32_t) handler) & IDT_HANDLER_ADDRESS_MASK;
    interruptDescriptorTable[interrupt].handlerAddressHighBits = (((uint32_t) handler) >> 16) & IDT_HANDLER_ADDRESS_MASK;
    interruptDescriptorTable[interrupt].gdt_codeSegmentSelector = CodeSegment;

    interruptDescriptorTable[interrupt].access =
        IDT_DESCRIPTOR_PRESENT | ((DescriptorPrivilegeLevel & IDT_PRIVILEGE_LEVEL_MASK) << 5) | DescriptorType;
    interruptDescriptorTable[interrupt].reserved = 0;
}


InterruptManager::InterruptManager(uint16_t hardwareInterruptOffset, GlobalDescriptorTable* globalDescriptorTable, TaskManager* taskManager)
    : programmableInterruptControllerMasterCommandPort(PIC_MASTER_COMMAND_PORT),
      programmableInterruptControllerMasterDataPort(PIC_MASTER_DATA_PORT),
      programmableInterruptControllerSlaveCommandPort(PIC_SLAVE_COMMAND_PORT),
      programmableInterruptControllerSlaveDataPort(PIC_SLAVE_DATA_PORT)
{
    this->taskManager = taskManager;
    this->hardwareInterruptOffset = hardwareInterruptOffset;
    uint32_t CodeSegment = globalDescriptorTable->CodeSegmentSelector();

    for(uint8_t i = 255; i > 0; --i)
    {
        SetInterruptDescriptorTableEntry(i, CodeSegment, &InterruptIgnore, 0, IDT_INTERRUPT_GATE);
        handlers[i] = 0;
    }
    SetInterruptDescriptorTableEntry(0, CodeSegment, &InterruptIgnore, 0, IDT_INTERRUPT_GATE);
    handlers[0] = 0;

    SetInterruptDescriptorTableEntry(EXCEPTION_DIVIDE_BY_ZERO, CodeSegment, &HandleException0x00, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(EXCEPTION_DEBUG, CodeSegment, &HandleException0x01, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(EXCEPTION_NON_MASKABLE_INTERRUPT, CodeSegment, &HandleException0x02, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(EXCEPTION_BREAKPOINT, CodeSegment, &HandleException0x03, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(EXCEPTION_OVERFLOW, CodeSegment, &HandleException0x04, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(EXCEPTION_BOUND_RANGE_EXCEEDED, CodeSegment, &HandleException0x05, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(EXCEPTION_INVALID_OPCODE, CodeSegment, &HandleException0x06, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(EXCEPTION_DEVICE_NOT_AVAILABLE, CodeSegment, &HandleException0x07, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(EXCEPTION_DOUBLE_FAULT, CodeSegment, &HandleException0x08, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(EXCEPTION_COPROCESSOR_SEGMENT_OVERRUN, CodeSegment, &HandleException0x09, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(EXCEPTION_INVALID_TSS, CodeSegment, &HandleException0x0A, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(EXCEPTION_SEGMENT_NOT_PRESENT, CodeSegment, &HandleException0x0B, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(EXCEPTION_STACK_SEGMENT_FAULT, CodeSegment, &HandleException0x0C, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(EXCEPTION_GENERAL_PROTECTION_FAULT, CodeSegment, &HandleException0x0D, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(EXCEPTION_PAGE_FAULT, CodeSegment, &HandleException0x0E, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(EXCEPTION_RESERVED, CodeSegment, &HandleException0x0F, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(EXCEPTION_X87_FLOATING_POINT, CodeSegment, &HandleException0x10, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(EXCEPTION_ALIGNMENT_CHECK, CodeSegment, &HandleException0x11, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(EXCEPTION_MACHINE_CHECK, CodeSegment, &HandleException0x12, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(EXCEPTION_SIMD_FLOATING_POINT, CodeSegment, &HandleException0x13, 0, IDT_INTERRUPT_GATE);

    SetInterruptDescriptorTableEntry(hardwareInterruptOffset + IRQ_TIMER, CodeSegment, &HandleInterruptRequest0x00, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(hardwareInterruptOffset + IRQ_KEYBOARD, CodeSegment, &HandleInterruptRequest0x01, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(hardwareInterruptOffset + IRQ_CASCADE, CodeSegment, &HandleInterruptRequest0x02, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(hardwareInterruptOffset + IRQ_COM2, CodeSegment, &HandleInterruptRequest0x03, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(hardwareInterruptOffset + IRQ_COM1, CodeSegment, &HandleInterruptRequest0x04, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(hardwareInterruptOffset + IRQ_LPT2, CodeSegment, &HandleInterruptRequest0x05, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(hardwareInterruptOffset + IRQ_FLOPPY, CodeSegment, &HandleInterruptRequest0x06, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(hardwareInterruptOffset + IRQ_LPT1, CodeSegment, &HandleInterruptRequest0x07, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(hardwareInterruptOffset + IRQ_RTC, CodeSegment, &HandleInterruptRequest0x08, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(hardwareInterruptOffset + IRQ_FREE_9, CodeSegment, &HandleInterruptRequest0x09, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(hardwareInterruptOffset + IRQ_FREE_10, CodeSegment, &HandleInterruptRequest0x0A, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(hardwareInterruptOffset + IRQ_FREE_11, CodeSegment, &HandleInterruptRequest0x0B, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(hardwareInterruptOffset + IRQ_MOUSE, CodeSegment, &HandleInterruptRequest0x0C, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(hardwareInterruptOffset + IRQ_FPU, CodeSegment, &HandleInterruptRequest0x0D, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(hardwareInterruptOffset + IRQ_PRIMARY_ATA, CodeSegment, &HandleInterruptRequest0x0E, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(hardwareInterruptOffset + IRQ_SECONDARY_ATA, CodeSegment, &HandleInterruptRequest0x0F, 0, IDT_INTERRUPT_GATE);

    SetInterruptDescriptorTableEntry(SYSCALL_INTERRUPT, CodeSegment, &HandleInterruptRequest0x80, 0, IDT_INTERRUPT_GATE);

    programmableInterruptControllerMasterCommandPort.Write(PIC_INITIALIZE);
    programmableInterruptControllerSlaveCommandPort.Write(PIC_INITIALIZE);

    // remap
    programmableInterruptControllerMasterDataPort.Write(hardwareInterruptOffset);
    programmableInterruptControllerSlaveDataPort.Write(hardwareInterruptOffset + PIC_SLAVE_IRQ_START);

    programmableInterruptControllerMasterDataPort.Write(PIC_MASTER_HAS_SLAVE_AT_IRQ2);
    programmableInterruptControllerSlaveDataPort.Write(PIC_SLAVE_CASCADE_ID);

    programmableInterruptControllerMasterDataPort.Write(PIC_8086_MODE);
    programmableInterruptControllerSlaveDataPort.Write(PIC_8086_MODE);

    programmableInterruptControllerMasterDataPort.Write(PIC_UNMASK_ALL_IRQS);
    programmableInterruptControllerSlaveDataPort.Write(PIC_UNMASK_ALL_IRQS);

    InterruptDescriptorTablePointer idt_pointer;
    idt_pointer.size  = 256*sizeof(GateDescriptor) - 1;
    idt_pointer.base  = (uint32_t)interruptDescriptorTable;
    asm volatile("lidt %0" : : "m" (idt_pointer));
}

InterruptManager::~InterruptManager()
{
    Deactivate();
}

uint16_t InterruptManager::HardwareInterruptOffset()
{
    return hardwareInterruptOffset;
}

void InterruptManager::Activate()
{
    if(ActiveInterruptManager != 0)
        ActiveInterruptManager->Deactivate();

    ActiveInterruptManager = this;
    asm("sti");
}

void InterruptManager::Deactivate()
{
    if(ActiveInterruptManager == this)
    {
        ActiveInterruptManager = 0;
        asm("cli");
    }
}

uint32_t InterruptManager::HandleInterrupt(uint8_t interrupt, uint32_t esp)
{
    if(ActiveInterruptManager != 0)
        return ActiveInterruptManager->DoHandleInterrupt(interrupt, esp);
    return esp;
}


uint32_t InterruptManager::DoHandleInterrupt(uint8_t interrupt, uint32_t esp)
{
    if(handlers[interrupt] != 0)
    {
        esp = handlers[interrupt]->HandleInterrupt(esp);
    }
    else if(interrupt != hardwareInterruptOffset)
    {
        printf("UNHANDLED INTERRUPT 0x");
        printfHex(interrupt);
    }

    if(interrupt == hardwareInterruptOffset && taskManager != 0)
        esp = (uint32_t)taskManager->Schedule((CPUState*)esp);

    // hardware interrupts must be acknowledged
    if(hardwareInterruptOffset <= interrupt && interrupt < hardwareInterruptOffset + PIC_IRQ_COUNT)
    {
        programmableInterruptControllerMasterCommandPort.Write(PIC_END_OF_INTERRUPT);
        if(hardwareInterruptOffset + PIC_SLAVE_IRQ_START <= interrupt)
            programmableInterruptControllerSlaveCommandPort.Write(PIC_END_OF_INTERRUPT);
    }

    return esp;
}










