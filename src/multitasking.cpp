#include <multitasking.h>

using namespace myos;
using namespace myos::common;

static const uint32_t CPU_EFLAGS_INTERRUPT_ENABLE = 0x202;

Task::Task(GlobalDescriptorTable* gdt, void entrypoint())
{
    cpustate = (CPUState*)(stack + 4096 - sizeof(CPUState));

    cpustate->eax = 0;
    cpustate->ebx = 0;
    cpustate->ecx = 0;
    cpustate->edx = 0;

    cpustate->esi = 0;
    cpustate->edi = 0;
    cpustate->ebp = 0;

    cpustate->error = 0;
    cpustate->eip = (uint32_t)entrypoint;
    cpustate->cs = gdt->CodeSegmentSelector();
    cpustate->eflags = CPU_EFLAGS_INTERRUPT_ENABLE;
    cpustate->esp = 0;
    cpustate->ss = gdt->DataSegmentSelector();
}

Task::~Task()
{
}

TaskManager::TaskManager()
{
    numTasks = 0;
    currentTask = -1;
}

TaskManager::~TaskManager()
{
}

bool TaskManager::AddTask(Task* task)
{
    if(numTasks >= 256)
        return false;

    tasks[numTasks++] = task;
    return true;
}

CPUState* TaskManager::Schedule(CPUState* cpustate)
{
    if(numTasks <= 0)
        return cpustate;

    if(currentTask >= 0)
        tasks[currentTask]->cpustate = cpustate;

    if(++currentTask >= numTasks)
        currentTask %= numTasks;

    return tasks[currentTask]->cpustate;
}
