#include <syscalls.h>
#include <multitasking.h>

using namespace myos;
using namespace myos::common;
using namespace myos::hardwarecommunication;

SyscallHandler::SyscallHandler(InterruptManager* interruptManager, uint8_t InterruptNumber)
:   InterruptHandler(interruptManager, InterruptNumber + interruptManager->HardwareInterruptOffset())
{
}

SyscallHandler::~SyscallHandler()
{
}

void printf(const char*);

uint32_t SyscallHandler::HandleInterrupt(uint32_t esp)
{
    CPUState* cpu = (CPUState*)esp;

    switch(cpu->eax)
    {
        case SYSCALL_WRITE:
            printf((const char*)cpu->ebx);
            break;
        default:
            break;
    }

    return esp;
}
