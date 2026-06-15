
#include <common/types.h>
#include <gdt.h>
#include <memorymanagement.h>
#include <hardwarecommunication/interrupts.h>
#include <syscalls.h>
#include <hardwarecommunication/pci.h>
#include <drivers/driver.h>
#include <drivers/amd_am79c973.h>
#include <drivers/keyboard.h>
#include <drivers/mouse.h>
#include <drivers/vga.h>
#include <drivers/ata.h>
#include <gui/desktop.h>
#include <gui/window.h>
#include <multitasking.h>
#include <net/etherframe.h>
#include <net/arp.h>
#include <net/ipv4.h>
#include <net/icmp.h>

#define NETWORK_DEMO
// #define HARDDRIVE_DEMO

#if !defined(HARDDRIVE_DEMO) && !defined(NETWORK_DEMO)
#define GRAPHICSMODE
#endif

using namespace myos;
using namespace myos::common;
using namespace myos::drivers;
using namespace myos::hardwarecommunication;
using namespace myos::gui;
using namespace myos::net;

static const uint16_t VGA_TEXT_WIDTH = 80;
static const uint16_t VGA_TEXT_HEIGHT = 25;
static const uint16_t VGA_TEXT_ATTRIBUTE_MASK = 0xFF00;
static const uint16_t VGA_TEXT_CHARACTER_MASK = 0x00FF;
static const uint16_t VGA_TEXT_FOREGROUND_MASK = 0x0F00;
static const uint16_t VGA_TEXT_BACKGROUND_MASK = 0xF000;
static const uint8_t HEX_NIBBLE_MASK = 0x0F;
static const uint8_t BYTE_MASK = 0xFF;
static const uint8_t RGB_CHANNEL_OFF = 0x00;
static const uint8_t RGB_CHANNEL_MID = 0xA8;

static uint16_t* VideoMemory = (uint16_t*)VGA_TEXT_MEMORY;
static uint8_t textCursorX = 0;
static uint8_t textCursorY = 0;

void clearScreen()
{
    for(uint16_t y = 0; y < VGA_TEXT_HEIGHT; y++)
        for(uint16_t x = 0; x < VGA_TEXT_WIDTH; x++)
            VideoMemory[VGA_TEXT_WIDTH*y+x] =
                (VideoMemory[VGA_TEXT_WIDTH*y+x] & VGA_TEXT_ATTRIBUTE_MASK) | ' ';

    textCursorX = 0;
    textCursorY = 0;
}

static void scrollTextScreen()
{
    for(uint16_t y = 1; y < VGA_TEXT_HEIGHT; y++)
        for(uint16_t x = 0; x < VGA_TEXT_WIDTH; x++)
            VideoMemory[VGA_TEXT_WIDTH*(y-1)+x] = VideoMemory[VGA_TEXT_WIDTH*y+x];

    for(uint16_t x = 0; x < VGA_TEXT_WIDTH; x++)
        VideoMemory[VGA_TEXT_WIDTH*(VGA_TEXT_HEIGHT-1)+x] =
            (VideoMemory[VGA_TEXT_WIDTH*(VGA_TEXT_HEIGHT-1)+x] & VGA_TEXT_ATTRIBUTE_MASK) | ' ';

    textCursorY = VGA_TEXT_HEIGHT - 1;
}


void printf(const char* str)
{
    for(int i = 0; str[i] != '\0'; ++i)
    {
        switch(str[i])
        {
            case '\n':
                textCursorX = 0;
                textCursorY++;
                break;
            default:
                VideoMemory[VGA_TEXT_WIDTH*textCursorY+textCursorX] =
                    (VideoMemory[VGA_TEXT_WIDTH*textCursorY+textCursorX] & VGA_TEXT_ATTRIBUTE_MASK) | str[i];
                textCursorX++;
                break;
        }

        if(textCursorX >= VGA_TEXT_WIDTH)
        {
            textCursorX = 0;
            textCursorY++;
        }

        while(textCursorY >= VGA_TEXT_HEIGHT)
            scrollTextScreen();
    }
}

void printfHex(uint8_t key)
{
    char foo[] = "00";
    const char* hex = "0123456789ABCDEF";
    foo[0] = hex[(key >> 4) & HEX_NIBBLE_MASK];
    foo[1] = hex[key & HEX_NIBBLE_MASK];
    printf(foo);
}

void printfHex16(uint16_t key)
{
    printfHex((key >> 8) & BYTE_MASK);
    printfHex(key & BYTE_MASK);
}

void printfHex32(uint32_t key)
{
    printfHex((key >> 24) & BYTE_MASK);
    printfHex((key >> 16) & BYTE_MASK);
    printfHex((key >> 8) & BYTE_MASK);
    printfHex(key & BYTE_MASK);
}




class PrintfKeyboardEventHandler : public KeyboardEventHandler
{
public:
    void OnKeyDown(char c)
    {
        char foo[] = " ";
        foo[0] = c;
        printf(foo);
    }
};

class MouseToConsole : public MouseEventHandler
{
    int8_t x, y;
public:
    
    MouseToConsole()
    {
        uint16_t* VideoMemory = (uint16_t*)VGA_TEXT_MEMORY;
        x = 40;
        y = 12;
        VideoMemory[VGA_TEXT_WIDTH*y+x] =
            (VideoMemory[VGA_TEXT_WIDTH*y+x] & VGA_TEXT_FOREGROUND_MASK) << 4
          | (VideoMemory[VGA_TEXT_WIDTH*y+x] & VGA_TEXT_BACKGROUND_MASK) >> 4
          | (VideoMemory[VGA_TEXT_WIDTH*y+x] & VGA_TEXT_CHARACTER_MASK);        
    }
    
    virtual void OnMouseMove(int xoffset, int yoffset)
    {
        static uint16_t* VideoMemory = (uint16_t*)VGA_TEXT_MEMORY;
        VideoMemory[VGA_TEXT_WIDTH*y+x] =
            (VideoMemory[VGA_TEXT_WIDTH*y+x] & VGA_TEXT_FOREGROUND_MASK) << 4
          | (VideoMemory[VGA_TEXT_WIDTH*y+x] & VGA_TEXT_BACKGROUND_MASK) >> 4
          | (VideoMemory[VGA_TEXT_WIDTH*y+x] & VGA_TEXT_CHARACTER_MASK);

        x += xoffset;
        if(x >= VGA_TEXT_WIDTH) x = VGA_TEXT_WIDTH - 1;
        if(x < 0) x = 0;
        y += yoffset;
        if(y >= VGA_TEXT_HEIGHT) y = VGA_TEXT_HEIGHT - 1;
        if(y < 0) y = 0;

        VideoMemory[VGA_TEXT_WIDTH*y+x] =
            (VideoMemory[VGA_TEXT_WIDTH*y+x] & VGA_TEXT_FOREGROUND_MASK) << 4
          | (VideoMemory[VGA_TEXT_WIDTH*y+x] & VGA_TEXT_BACKGROUND_MASK) >> 4
          | (VideoMemory[VGA_TEXT_WIDTH*y+x] & VGA_TEXT_CHARACTER_MASK);
    }

    virtual void OnMouseDown(uint8_t button)
    {
        printf("MOUSE ");
        printfHex(button);
        printf(" DOWN\n");
    }

    virtual void OnMouseUp(uint8_t button)
    {
        printf("MOUSE ");
        printfHex(button);
        printf(" UP\n");
    }
    
};


void sysprintf(const char* str)
{
    asm volatile("int $%c0" : : "i" ((int)SYSCALL_INTERRUPT), "a" (SYSCALL_WRITE), "b" (str));
}

void taskA()
{
    while(true)
        sysprintf("A");
}

void taskB()
{
    while(true)
        sysprintf("B");
}








typedef void (*constructor)();
extern "C" constructor start_ctors;
extern "C" constructor end_ctors;
extern "C" void callConstructors()
{
    for(constructor* i = &start_ctors; i != &end_ctors; i++)
        (*i)();
}



extern "C" void kernelMain(const void* multiboot_structure, uint32_t /*multiboot_magic*/)
{
    printf("Hello World! --- http://www.AlgorithMan.de\n");

    GlobalDescriptorTable gdt;

    uint32_t* memupper = (uint32_t*)(((size_t)multiboot_structure) + 8);
    size_t heap = 10*1024*1024;
    size_t memorySize = (*memupper) * 1024;
    size_t heapSize = memorySize > heap + 10*1024 ? memorySize - heap - 10*1024 : 0;
    MemoryManager memoryManager(heap, heapSize);

    printf("heap: 0x");
    printfHex32(heap);

    void* allocated = memoryManager.malloc(1024);
    printf("\nallocated: 0x");
    printfHex32((size_t)allocated);
    printf("\n");

    TaskManager taskManager;

    #if !defined(HARDDRIVE_DEMO) && !defined(NETWORK_DEMO)
    Task task1(&gdt, taskA);
    Task task2(&gdt, taskB);
    taskManager.AddTask(&task1);
    taskManager.AddTask(&task2);
    #endif

    InterruptManager interrupts(IRQ_BASE, &gdt, &taskManager);
    SyscallHandler syscalls(&interrupts, SYSCALL_INTERRUPT);
    
    printf("Initializing Hardware, Stage 1\n");

    #ifdef GRAPHICSMODE
        Desktop desktop(320,200, RGB_CHANNEL_OFF, RGB_CHANNEL_OFF, RGB_CHANNEL_MID);
    #endif
    
    DriverManager drvManager;
    
        #ifdef GRAPHICSMODE
            KeyboardDriver keyboard(&interrupts, &desktop);
        #else
            PrintfKeyboardEventHandler kbhandler;
            KeyboardDriver keyboard(&interrupts, &kbhandler);
        #endif
        drvManager.AddDriver(&keyboard);
    
        #ifdef GRAPHICSMODE
            MouseDriver mouse(&interrupts, &desktop);
        #else
            MouseToConsole mousehandler;
            MouseDriver mouse(&interrupts, &mousehandler);
        #endif
        drvManager.AddDriver(&mouse);
        
        #ifndef HARDDRIVE_DEMO
            PeripheralComponentInterconnectController PCIController;
            PCIController.SelectDrivers(&drvManager, &interrupts);
        #endif

        #ifdef GRAPHICSMODE
            VideoGraphicsArray vga;
        #endif
        
    printf("Initializing Hardware, Stage 2\n");
        drvManager.ActivateAll();
        
    printf("Initializing Hardware, Stage 3\n");

    #ifdef GRAPHICSMODE
        vga.SetMode(320,200,8);
        Window win1(&desktop, 10,10,20,20, RGB_CHANNEL_MID, RGB_CHANNEL_OFF, RGB_CHANNEL_OFF);
        desktop.AddChild(&win1);
        Window win2(&desktop, 40,15,30,30, RGB_CHANNEL_OFF, RGB_CHANNEL_MID, RGB_CHANNEL_OFF);
        desktop.AddChild(&win2);
    #endif

    #ifdef HARDDRIVE_DEMO
    interrupts.Activate();

    printf("\nPOSIX-style System Call Demo\n");
    printf("Calling int 0x80 with syscall number 4\n");
    sysprintf("Syscall write says: hello from int 0x80\n");
    printf("Returned from system call\n");

    printf("\nHard Drive Demo\n");
    printf("\nS-ATA primary master: ");
    AdvancedTechnologyAttachment ata0m(true, ATA_PRIMARY_IO_BASE);
    ata0m.Identify();

    uint8_t hardDriveDemoMessage[] = "myos hard drive demo";
    uint32_t hardDriveDemoSector = 1;

    printf("\nWriting demo message to primary master sector 0x");
    printfHex32(hardDriveDemoSector);
    printf("\n");
    ata0m.Write28(hardDriveDemoSector, hardDriveDemoMessage, sizeof(hardDriveDemoMessage)-1);
    ata0m.Flush();

    printf("\nReading it back from sector 0x");
    printfHex32(hardDriveDemoSector);
    printf("\n");
    ata0m.Read28(hardDriveDemoSector, sizeof(hardDriveDemoMessage)-1);
    printf("\n");
    #endif

    #ifdef NETWORK_DEMO
    printf("\nNetwork Demo A01-A04\n");

    amd_am79c973* eth0 = 0;
    if(drvManager.numDrivers > 2)
        eth0 = (amd_am79c973*)drvManager.drivers[2];

    if(eth0 != 0)
    {
        uint8_t ip1 = 10, ip2 = 0, ip3 = 2, ip4 = 15;
        uint32_t ip_be = ((uint32_t)ip4 << 24)
                       | ((uint32_t)ip3 << 16)
                       | ((uint32_t)ip2 << 8)
                       | (uint32_t)ip1;
        eth0->SetIPAddress(ip_be);

        EtherFrameProvider* etherframe = new EtherFrameProvider(eth0);
        AddressResolutionProtocol* arp = new AddressResolutionProtocol(etherframe);

        uint8_t gip1 = 10, gip2 = 0, gip3 = 2, gip4 = 2;
        uint32_t gip_be = ((uint32_t)gip4 << 24)
                        | ((uint32_t)gip3 << 16)
                        | ((uint32_t)gip2 << 8)
                        | (uint32_t)gip1;

        uint8_t subnet1 = 255, subnet2 = 255, subnet3 = 255, subnet4 = 0;
        uint32_t subnet_be = ((uint32_t)subnet4 << 24)
                           | ((uint32_t)subnet3 << 16)
                           | ((uint32_t)subnet2 << 8)
                           | (uint32_t)subnet1;

        InternetProtocolProvider* ipv4 = new InternetProtocolProvider(etherframe, arp, gip_be, subnet_be);
        InternetControlMessageProtocol* icmp = new InternetControlMessageProtocol(ipv4);

        interrupts.Activate();

        printf("Local IP: 10.0.2.15\n");
        printf("Gateway: 10.0.2.2\n");
        printf("Resolving gateway with ARP and sending ICMP echo request\n");

        clearScreen();
        eth0->ResetTraceOutput();
        arp->BroadcastMACAddress(gip_be);
        icmp->RequestEchoReply(gip_be);
    }
    else
    {
        printf("No AMD am79c973 network card found.\n");
        printf("Use a VirtualBox PCnet/AMD network adapter for this demo.\n");
    }
    #endif

    #ifndef HARDDRIVE_DEMO
    #ifndef NETWORK_DEMO
    if(drvManager.numDrivers > 2)
    {
        amd_am79c973* eth0 = (amd_am79c973*)drvManager.drivers[2];
        uint8_t networkMessage[60] = {
            // Destination MAC: broadcast
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,

            // Source MAC: locally administered demo address
            0x02, 0x12, 0x34, 0x56, 0x78, 0x9A,

            // EtherType: custom experimental value
            0x88, 0xB5,

            'H', 'e', 'l', 'l', 'o', ' ',
            'N', 'e', 't', 'w', 'o', 'r', 'k'
        };
        eth0->Send(networkMessage, 60);
    }
    #endif
    #endif

    #ifndef NETWORK_DEMO
    interrupts.Activate();
    #endif

    while(1)
    {
        #ifdef GRAPHICSMODE
            desktop.Draw(&vga);
        #endif
    }
}
