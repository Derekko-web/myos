
#ifndef __MYOS__SYSCALLS_H
#define __MYOS__SYSCALLS_H

#include <common/types.h>
#include <hardwarecommunication/interrupts.h>

namespace myos
{
    static const common::uint32_t SYSCALL_WRITE = 4;

    class SyscallHandler : public hardwarecommunication::InterruptHandler
    {
    public:
        SyscallHandler(hardwarecommunication::InterruptManager* interruptManager,
                       myos::common::uint8_t InterruptNumber);
        ~SyscallHandler();

        virtual myos::common::uint32_t HandleInterrupt(myos::common::uint32_t esp);
    };

}

#endif
