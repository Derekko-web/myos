
#include <filesystem/msdospart.h>
#include <filesystem/fat.h>

using namespace myos;
using namespace myos::common;
using namespace myos::drivers;
using namespace myos::filesystem;

void printf(const char* str);
void printfHex(uint8_t key);

bool MSDOSPartitionTable::ReadMasterBootRecord(AdvancedTechnologyAttachment* hd,
                                               MasterBootRecord* mbr,
                                               bool trace)
{
    if(hd == 0 || mbr == 0)
        return false;

    uint8_t* mbrBytes = (uint8_t*)mbr;

    for(uint16_t i = 0; i < sizeof(MasterBootRecord); i++)
        mbrBytes[i] = 0;

    if(trace)
        printf("MBR: ");
    hd->Read28(0, mbrBytes, sizeof(MasterBootRecord), trace);

    if(mbr->magicnumber != 0xAA55)
    {
        printf("illegal MBR\n");
        return false;
    }

    return true;
}

bool MSDOSPartitionTable::ReadPartition(AdvancedTechnologyAttachment* hd,
                                        uint8_t partitionIndex,
                                        PartitionTableEntry* partition)
{
    if(partition == 0 || partitionIndex >= 4)
        return false;

    MasterBootRecord mbr;
    if(!ReadMasterBootRecord(hd, &mbr, false))
        return false;

    *partition = mbr.primaryPartition[partitionIndex];
    return partition->partition_id != 0 && partition->length != 0;
}

void MSDOSPartitionTable::ReadPartitions(AdvancedTechnologyAttachment* hd)
{
    MasterBootRecord mbr;
    if(!ReadMasterBootRecord(hd, &mbr))
        return;

    for(uint8_t i = 0; i < 4; i++)
    {
        PartitionTableEntry* partition = &mbr.primaryPartition[i];
        if(partition->partition_id == 0x00)
            continue;

        printf(" Partition ");
        printfHex(i);

        if(partition->bootable == 0x80)
            printf(" bootable. Type ");
        else
            printf(" not bootable. Type ");

        printfHex(partition->partition_id);

        ReadBiosBlock(hd, partition->start_lba);
    }
}
