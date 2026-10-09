
#ifndef __MYOS__FILESYSTEM__MSDOSPART_H
#define __MYOS__FILESYSTEM__MSDOSPART_H

#include <common/types.h>
#include <drivers/ata.h>

namespace myos
{
    namespace filesystem
    {
        struct PartitionTableEntry
        {
            common::uint8_t bootable;

            common::uint8_t start_head;
            common::uint8_t start_sector_cylinder;
            common::uint8_t start_cylinder;

            common::uint8_t partition_id;

            common::uint8_t end_head;
            common::uint8_t end_sector_cylinder;
            common::uint8_t end_cylinder;

            common::uint32_t start_lba;
            common::uint32_t length;
        } __attribute__((packed));

        struct MasterBootRecord
        {
            common::uint8_t bootloader[440];
            common::uint32_t signature;
            common::uint16_t unused;

            PartitionTableEntry primaryPartition[4];

            common::uint16_t magicnumber;
        } __attribute__((packed));

        class MSDOSPartitionTable
        {
        public:
            static bool ReadMasterBootRecord(drivers::AdvancedTechnologyAttachment* hd,
                                             MasterBootRecord* mbr,
                                             bool trace = true);
            static bool ReadPartition(drivers::AdvancedTechnologyAttachment* hd,
                                      common::uint8_t partitionIndex,
                                      PartitionTableEntry* partition);
            static void ReadPartitions(drivers::AdvancedTechnologyAttachment* hd);
        };
    }
}

#endif
