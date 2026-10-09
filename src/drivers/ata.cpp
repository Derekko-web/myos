#include <drivers/ata.h>

using namespace myos;
using namespace myos::common;
using namespace myos::drivers;

void printf(const char* str);

static const uint16_t ATA_REG_ERROR_OFFSET = 0x1;
static const uint16_t ATA_REG_SECTOR_COUNT_OFFSET = 0x2;
static const uint16_t ATA_REG_LBA_LOW_OFFSET = 0x3;
static const uint16_t ATA_REG_LBA_MID_OFFSET = 0x4;
static const uint16_t ATA_REG_LBA_HIGH_OFFSET = 0x5;
static const uint16_t ATA_REG_DEVICE_OFFSET = 0x6;
static const uint16_t ATA_REG_COMMAND_OFFSET = 0x7;
static const uint16_t ATA_REG_CONTROL_OFFSET = 0x206;

static const uint8_t ATA_DEVICE_MASTER = 0xA0;
static const uint8_t ATA_DEVICE_SLAVE = 0xB0;
static const uint8_t ATA_DEVICE_MASTER_LBA28 = 0xE0;
static const uint8_t ATA_DEVICE_SLAVE_LBA28 = 0xF0;

static const uint8_t ATA_CMD_IDENTIFY = 0xEC;
static const uint8_t ATA_CMD_READ_SECTORS = 0x20;
static const uint8_t ATA_CMD_WRITE_SECTORS = 0x30;
static const uint8_t ATA_CMD_CACHE_FLUSH = 0xE7;

static const uint8_t ATA_STATUS_NO_DRIVE = 0x00;
static const uint8_t ATA_STATUS_FLOATING_BUS = 0xFF;
static const uint8_t ATA_STATUS_ERROR = 0x01;
static const uint8_t ATA_STATUS_DATA_REQUEST = 0x08;
static const uint8_t ATA_STATUS_BUSY = 0x80;

static const uint32_t ATA_LBA28_MAX_SECTOR = 0x0FFFFFFF;
static const uint32_t ATA_LBA28_HEAD_MASK = 0x0F000000;
static const uint32_t ATA_LBA28_LOW_BYTE_MASK = 0x000000FF;
static const uint32_t ATA_LBA28_MID_BYTE_MASK = 0x0000FF00;
static const uint32_t ATA_LBA28_HIGH_BYTE_MASK = 0x00FF0000;
static const uint8_t BYTE_MASK = 0xFF;
static const uint16_t ATA_PADDING_WORD = 0x0000;

AdvancedTechnologyAttachment::AdvancedTechnologyAttachment(bool master, common::uint16_t portBase)
:   dataPort(portBase),
    errorPort(portBase + ATA_REG_ERROR_OFFSET),
    sectorCountPort(portBase + ATA_REG_SECTOR_COUNT_OFFSET),
    lbaLowPort(portBase + ATA_REG_LBA_LOW_OFFSET),
    lbaMidPort(portBase + ATA_REG_LBA_MID_OFFSET),
    lbaHiPort(portBase + ATA_REG_LBA_HIGH_OFFSET),
    devicePort(portBase + ATA_REG_DEVICE_OFFSET),
    commandPort(portBase + ATA_REG_COMMAND_OFFSET),
    controlPort(portBase + ATA_REG_CONTROL_OFFSET)
{
    this->master = master;
    bytesPerSector = 512;
}

AdvancedTechnologyAttachment::~AdvancedTechnologyAttachment()
{
}

void AdvancedTechnologyAttachment::Identify()
{
    devicePort.Write(master ? ATA_DEVICE_MASTER : ATA_DEVICE_SLAVE);
    controlPort.Write(0);

    devicePort.Write(ATA_DEVICE_MASTER);
    uint8_t status = commandPort.Read();
    if(status == ATA_STATUS_FLOATING_BUS)
    {
        printf("NO DRIVE\n");
        return;
    }

    devicePort.Write(master ? ATA_DEVICE_MASTER : ATA_DEVICE_SLAVE);
    sectorCountPort.Write(0);
    lbaLowPort.Write(0);
    lbaMidPort.Write(0);
    lbaHiPort.Write(0);
    commandPort.Write(ATA_CMD_IDENTIFY);

    status = commandPort.Read();
    if(status == ATA_STATUS_NO_DRIVE)
    {
        printf("NO DRIVE\n");
        return;
    }

    while(((status & ATA_STATUS_BUSY) == ATA_STATUS_BUSY)
       && ((status & ATA_STATUS_ERROR) != ATA_STATUS_ERROR))
        status = commandPort.Read();

    if(status & ATA_STATUS_ERROR)
    {
        printf("ERROR");
        return;
    }

    printf("MODEL: ");
    for(int i = 0; i < 256; i++)
    {
        uint16_t data = dataPort.Read();

        if(27 <= i && i <= 46)
        {
            char text[] = "  ";
            text[0] = (data >> 8) & BYTE_MASK;
            text[1] = data & BYTE_MASK;
            printf(text);
        }
    }
    printf("\n");
}

void AdvancedTechnologyAttachment::Read28(common::uint32_t sectorNum,
                                          common::uint8_t* data,
                                          common::uint32_t count,
                                          bool trace)
{
    if(sectorNum > ATA_LBA28_MAX_SECTOR || data == 0 || count == 0)
        return;
    if(count > bytesPerSector)
        count = bytesPerSector;

    devicePort.Write((master ? ATA_DEVICE_MASTER_LBA28 : ATA_DEVICE_SLAVE_LBA28)
        | ((sectorNum & ATA_LBA28_HEAD_MASK) >> 24));
    errorPort.Write(0);
    sectorCountPort.Write(1);
    lbaLowPort.Write(sectorNum & ATA_LBA28_LOW_BYTE_MASK);
    lbaMidPort.Write((sectorNum & ATA_LBA28_MID_BYTE_MASK) >> 8);
    lbaHiPort.Write((sectorNum & ATA_LBA28_HIGH_BYTE_MASK) >> 16);
    commandPort.Write(ATA_CMD_READ_SECTORS);

    uint8_t status = commandPort.Read();
    if(status == ATA_STATUS_NO_DRIVE || status == ATA_STATUS_FLOATING_BUS)
    {
        printf("NO DRIVE\n");
        return;
    }

    uint32_t timeout = 0x100000;
    while(((status & ATA_STATUS_BUSY) == ATA_STATUS_BUSY
        || (status & ATA_STATUS_DATA_REQUEST) != ATA_STATUS_DATA_REQUEST)
       && ((status & ATA_STATUS_ERROR) != ATA_STATUS_ERROR)
       && timeout > 0)
    {
        status = commandPort.Read();
        timeout--;
    }

    if(timeout == 0
    && (((status & ATA_STATUS_BUSY) == ATA_STATUS_BUSY)
     || ((status & ATA_STATUS_DATA_REQUEST) != ATA_STATUS_DATA_REQUEST)))
    {
        printf("ATA READ TIMEOUT\n");
        return;
    }

    if(status & ATA_STATUS_ERROR)
    {
        printf("ERROR");
        return;
    }

    if(trace)
        printf("Reading from ATA: ");

    for(common::uint32_t i = 0; i < count; i += 2)
    {
        uint16_t wdata = dataPort.Read();

        data[i] = wdata & BYTE_MASK;
        if(i+1 < count)
            data[i+1] = (wdata >> 8) & BYTE_MASK;
    }

    for(common::uint32_t i = count + (count%2); i < bytesPerSector; i += 2)
        dataPort.Read();
}

void AdvancedTechnologyAttachment::Write28(common::uint32_t sectorNum, common::uint8_t* data, common::uint32_t count)
{
    if(sectorNum > ATA_LBA28_MAX_SECTOR || data == 0 || count > 512)
        return;

    devicePort.Write((master ? ATA_DEVICE_MASTER_LBA28 : ATA_DEVICE_SLAVE_LBA28)
        | ((sectorNum & ATA_LBA28_HEAD_MASK) >> 24));
    errorPort.Write(0);
    sectorCountPort.Write(1);
    lbaLowPort.Write(sectorNum & ATA_LBA28_LOW_BYTE_MASK);
    lbaMidPort.Write((sectorNum & ATA_LBA28_MID_BYTE_MASK) >> 8);
    lbaHiPort.Write((sectorNum & ATA_LBA28_HIGH_BYTE_MASK) >> 16);
    commandPort.Write(ATA_CMD_WRITE_SECTORS);

    uint8_t status = commandPort.Read();
    while(((status & ATA_STATUS_BUSY) == ATA_STATUS_BUSY
        || (status & ATA_STATUS_DATA_REQUEST) != ATA_STATUS_DATA_REQUEST)
       && ((status & ATA_STATUS_ERROR) != ATA_STATUS_ERROR))
        status = commandPort.Read();

    if(status & ATA_STATUS_ERROR)
    {
        printf("ERROR");
        return;
    }

    printf("Wrote: ");

    for(common::uint32_t i = 0; i < count; i += 2)
    {
        uint16_t wdata = data[i];
        if(i+1 < count)
            wdata |= ((uint16_t)data[i+1]) << 8;
        dataPort.Write(wdata);

        char text[] = "  ";
        text[0] = wdata & BYTE_MASK;
        text[1] = (i+1 < count) ? ((wdata >> 8) & BYTE_MASK) : '\0';
        printf(text);
    }

    for(common::uint32_t i = count + (count%2); i < 512; i += 2)
        dataPort.Write(ATA_PADDING_WORD);
}

void AdvancedTechnologyAttachment::Flush()
{
    devicePort.Write(master ? ATA_DEVICE_MASTER_LBA28 : ATA_DEVICE_SLAVE_LBA28);
    commandPort.Write(ATA_CMD_CACHE_FLUSH);

    uint8_t status = commandPort.Read();
    if(status == ATA_STATUS_NO_DRIVE)
        return;

    while(((status & ATA_STATUS_BUSY) == ATA_STATUS_BUSY)
       && ((status & ATA_STATUS_ERROR) != ATA_STATUS_ERROR))
        status = commandPort.Read();

    if(status & ATA_STATUS_ERROR)
    {
        printf("ERROR");
        return;
    }
}
