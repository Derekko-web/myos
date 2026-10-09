#include <hardwarecommunication/pci.h>
#include <drivers/amd_am79c973.h>

using namespace myos::common;
using namespace myos::drivers;
using namespace myos::hardwarecommunication;

static const uint16_t PCI_CONFIG_DATA_PORT = 0xCFC;
static const uint16_t PCI_CONFIG_COMMAND_PORT = 0xCF8;

static const uint32_t PCI_CONFIG_ENABLE = 0x1 << 31;
static const uint32_t PCI_CONFIG_BUS_MASK = 0xFF;
static const uint32_t PCI_CONFIG_DEVICE_MASK = 0x1F;
static const uint32_t PCI_CONFIG_FUNCTION_MASK = 0x07;
static const uint32_t PCI_CONFIG_REGISTER_ALIGN_MASK = 0xFC;
static const uint32_t PCI_BYTE_MASK = 0xFF;
static const uint32_t PCI_WORD_HIGH_BYTE_MASK = 0xFF00;

static const uint32_t PCI_CONFIG_VENDOR_ID_OFFSET = 0x00;
static const uint32_t PCI_CONFIG_DEVICE_ID_OFFSET = 0x02;
static const uint32_t PCI_CONFIG_COMMAND_OFFSET = 0x04;
static const uint32_t PCI_CONFIG_REVISION_ID_OFFSET = 0x08;
static const uint32_t PCI_CONFIG_INTERFACE_ID_OFFSET = 0x09;
static const uint32_t PCI_CONFIG_SUBCLASS_ID_OFFSET = 0x0A;
static const uint32_t PCI_CONFIG_CLASS_ID_OFFSET = 0x0B;
static const uint32_t PCI_CONFIG_HEADER_TYPE_OFFSET = 0x0E;
static const uint32_t PCI_CONFIG_BAR0_OFFSET = 0x10;
static const uint32_t PCI_CONFIG_INTERRUPT_OFFSET = 0x3C;

static const uint32_t PCI_HEADER_MULTIFUNCTION = 1 << 7;
static const uint32_t PCI_HEADER_TYPE_MASK = 0x7F;
static const uint32_t PCI_BAR_IO_SPACE = 0x1;
static const uint32_t PCI_BAR_MEMORY_TYPE_MASK = 0x3;
static const uint32_t PCI_BAR_IO_ADDRESS_MASK = 0x3;
static const uint32_t PCI_COMMAND_IO_SPACE = 0x1;
static const uint32_t PCI_COMMAND_BUS_MASTER = 0x4;

static const uint16_t PCI_VENDOR_ID_NONE = 0x0000;
static const uint16_t PCI_VENDOR_ID_INVALID = 0xFFFF;
static const uint16_t PCI_VENDOR_ID_AMD = 0x1022;
static const uint16_t PCI_VENDOR_ID_INTEL = 0x8086;
static const uint16_t PCI_DEVICE_ID_AMD_AM79C973 = 0x2000;
static const uint8_t PCI_CLASS_DISPLAY = 0x03;
static const uint8_t PCI_DISPLAY_SUBCLASS_VGA = 0x00;




PeripheralComponentInterconnectDeviceDescriptor::PeripheralComponentInterconnectDeviceDescriptor()
{
    portBase = 0;
    interrupt = 0;

    bus = 0;
    device = 0;
    function = 0;

    vendor_id = 0;
    device_id = 0;

    class_id = 0;
    subclass_id = 0;
    interface_id = 0;

    revision = 0;
}

PeripheralComponentInterconnectDeviceDescriptor::~PeripheralComponentInterconnectDeviceDescriptor()
{
}







PeripheralComponentInterconnectController::PeripheralComponentInterconnectController()
: dataPort(PCI_CONFIG_DATA_PORT),
  commandPort(PCI_CONFIG_COMMAND_PORT)
{
}

PeripheralComponentInterconnectController::~PeripheralComponentInterconnectController()
{
}

uint32_t PeripheralComponentInterconnectController::Read(uint16_t bus, uint16_t device, uint16_t function, uint32_t registeroffset)
{
    uint32_t id =
        PCI_CONFIG_ENABLE
        | ((bus & PCI_CONFIG_BUS_MASK) << 16)
        | ((device & PCI_CONFIG_DEVICE_MASK) << 11)
        | ((function & PCI_CONFIG_FUNCTION_MASK) << 8)
        | (registeroffset & PCI_CONFIG_REGISTER_ALIGN_MASK);
    commandPort.Write(id);
    uint32_t result = dataPort.Read();
    return result >> (8* (registeroffset % 4));
}

void PeripheralComponentInterconnectController::Write(uint16_t bus, uint16_t device, uint16_t function, uint32_t registeroffset, uint32_t value)
{
    uint32_t id =
        PCI_CONFIG_ENABLE
        | ((bus & PCI_CONFIG_BUS_MASK) << 16)
        | ((device & PCI_CONFIG_DEVICE_MASK) << 11)
        | ((function & PCI_CONFIG_FUNCTION_MASK) << 8)
        | (registeroffset & PCI_CONFIG_REGISTER_ALIGN_MASK);
    commandPort.Write(id);
    dataPort.Write(value); 
}

bool PeripheralComponentInterconnectController::DeviceHasFunctions(common::uint16_t bus, common::uint16_t device)
{
    return Read(bus, device, 0, PCI_CONFIG_HEADER_TYPE_OFFSET) & PCI_HEADER_MULTIFUNCTION;
}


void printf(const char* str);
void printfHex(uint8_t);

void PeripheralComponentInterconnectController::SelectDrivers(DriverManager* driverManager, myos::hardwarecommunication::InterruptManager* interrupts)
{
    for(int bus = 0; bus < 8; bus++)
    {
        for(int device = 0; device < 32; device++)
        {
            int numFunctions = DeviceHasFunctions(bus, device) ? 8 : 1;
            for(int function = 0; function < numFunctions; function++)
            {
                PeripheralComponentInterconnectDeviceDescriptor dev = GetDeviceDescriptor(bus, device, function);
                
                if(dev.vendor_id == PCI_VENDOR_ID_NONE || dev.vendor_id == PCI_VENDOR_ID_INVALID)
                    continue;
                
                
                for(int barNum = 0; barNum < 6; barNum++)
                {
                    BaseAddressRegister bar = GetBaseAddressRegister(bus, device, function, barNum);
                    if(bar.address && (bar.type == InputOutput))
                        dev.portBase = (uint32_t)bar.address;
                }

                Driver* driver = GetDriver(dev, interrupts);
                if(driver != 0 && dev.vendor_id == PCI_VENDOR_ID_AMD
                && dev.device_id == PCI_DEVICE_ID_AMD_AM79C973)
                {
                    uint32_t command = Read(bus, device, function, PCI_CONFIG_COMMAND_OFFSET) & 0xFFFF;
                    Write(bus, device, function, PCI_CONFIG_COMMAND_OFFSET,
                          command | PCI_COMMAND_IO_SPACE | PCI_COMMAND_BUS_MASTER);
                }

                if(driver != 0)
                    driverManager->AddDriver(driver);
                
                
                printf("PCI BUS ");
                printfHex(bus & PCI_BYTE_MASK);
                
                printf(", DEVICE ");
                printfHex(device & PCI_BYTE_MASK);

                printf(", FUNCTION ");
                printfHex(function & PCI_BYTE_MASK);
                
                printf(" = VENDOR ");
                printfHex((dev.vendor_id & PCI_WORD_HIGH_BYTE_MASK) >> 8);
                printfHex(dev.vendor_id & PCI_BYTE_MASK);
                printf(", DEVICE ");
                printfHex((dev.device_id & PCI_WORD_HIGH_BYTE_MASK) >> 8);
                printfHex(dev.device_id & PCI_BYTE_MASK);
                printf("\n");
            }
        }
    }
}


BaseAddressRegister PeripheralComponentInterconnectController::GetBaseAddressRegister(uint16_t bus, uint16_t device, uint16_t function, uint16_t bar)
{
    BaseAddressRegister result;
    result.address = 0;
    result.size = 0;
    result.prefetchable = false;
    result.type = MemoryMapping;
    
    
    uint32_t headertype = Read(bus, device, function, PCI_CONFIG_HEADER_TYPE_OFFSET) & PCI_HEADER_TYPE_MASK;
    int maxBARs = 6 - (4*headertype);
    if(bar >= maxBARs)
        return result;
    
    
    uint32_t bar_value = Read(bus, device, function, PCI_CONFIG_BAR0_OFFSET + 4*bar);
    result.type = (bar_value & PCI_BAR_IO_SPACE) ? InputOutput : MemoryMapping;
    uint32_t temp;
    
    
    
    if(result.type == MemoryMapping)
    {
        
        switch((bar_value >> 1) & PCI_BAR_MEMORY_TYPE_MASK)
        {
            
            case 0: // 32 Bit Mode
            case 1: // 20 Bit Mode
            case 2: // 64 Bit Mode
                break;
        }
        
    }
    else // InputOutput
    {
        result.address = (uint8_t*)(bar_value & ~PCI_BAR_IO_ADDRESS_MASK);
        result.prefetchable = false;
    }
    
    
    return result;
}



Driver* PeripheralComponentInterconnectController::GetDriver(PeripheralComponentInterconnectDeviceDescriptor dev, InterruptManager* interrupts)
{
    Driver* driver = 0;

    switch(dev.vendor_id)
    {
        case PCI_VENDOR_ID_AMD:
            switch(dev.device_id)
            {
                case PCI_DEVICE_ID_AMD_AM79C973:
                    printf("AMD am79c973 ");
                    driver = (amd_am79c973*)MemoryManager::activeMemoryManager->malloc(sizeof(amd_am79c973));
                    if(driver != 0)
                        new (driver) amd_am79c973(&dev, interrupts);
                    else
                        printf("instantiation failed");
                    return driver;
                    break;
            }
            break;

        case PCI_VENDOR_ID_INTEL:
            break;
    }
    
    
    switch(dev.class_id)
    {
        case PCI_CLASS_DISPLAY:
            switch(dev.subclass_id)
            {
                case PCI_DISPLAY_SUBCLASS_VGA:
                    printf("VGA ");
                    break;
            }
            break;
    }
    
    
    return driver;
}



PeripheralComponentInterconnectDeviceDescriptor PeripheralComponentInterconnectController::GetDeviceDescriptor(uint16_t bus, uint16_t device, uint16_t function)
{
    PeripheralComponentInterconnectDeviceDescriptor result;
    
    result.bus = bus;
    result.device = device;
    result.function = function;
    
    result.vendor_id = Read(bus, device, function, PCI_CONFIG_VENDOR_ID_OFFSET);
    result.device_id = Read(bus, device, function, PCI_CONFIG_DEVICE_ID_OFFSET);

    result.class_id = Read(bus, device, function, PCI_CONFIG_CLASS_ID_OFFSET);
    result.subclass_id = Read(bus, device, function, PCI_CONFIG_SUBCLASS_ID_OFFSET);
    result.interface_id = Read(bus, device, function, PCI_CONFIG_INTERFACE_ID_OFFSET);

    result.revision = Read(bus, device, function, PCI_CONFIG_REVISION_ID_OFFSET);
    result.interrupt = Read(bus, device, function, PCI_CONFIG_INTERRUPT_OFFSET);
    
    return result;
}


