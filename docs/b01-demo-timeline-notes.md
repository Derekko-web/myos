# B01 Demo Timeline Explanation Notes

## 1. Kernel Disk Setup

Main point: B01 starts by choosing which disk object MyOS will read.

Say this:

The kernel has already booted far enough to run `kernelMain()`. For the B01 partition-table demo, the important compile-time path is `B01_PARTITION_DEMO`, which enables `HARDDRIVE_DEMO`.

That matters because `HARDDRIVE_DEMO` sends the kernel into the simple hard-drive demo path instead of the normal graphics or network path. The kernel still sets up basics like the GDT, memory manager, task manager, interrupts, syscalls, keyboard, and mouse, but the disk story begins when it reaches the hard-drive block.

Inside that block, the kernel creates two ATA disk objects on the primary ATA bus:

```cpp
AdvancedTechnologyAttachment ata0m(true, ATA_PRIMARY_IO_BASE);
AdvancedTechnologyAttachment ata0s(false, ATA_PRIMARY_IO_BASE);
```

`ata0m` means primary master. `ata0s` means primary slave.

For this demo, the extra VirtualBox test disk is attached as the primary slave disk. That is why the kernel passes `&ata0s` into the partition reader:

```cpp
MSDOSPartitionTable::ReadPartitions(&ata0s);
```

In plain English: "Kernel, use the primary slave disk, read its partition table, and print what you find."

Good thing to emphasize:

B01 is not reading files yet. It is only trying to answer one question: where do the partitions start on this disk?

Current-code note:

Right now `src/kernel.cpp` has `B02_FAT32_DEMO` enabled, not `B01_PARTITION_DEMO`. B02 reuses the same entry point, so the setup still looks the same, but after B01 finds each real partition, the current code continues into FAT32 reading.

## 2. Partition Reader Requests Sector 0

Main point: the partition reader prepares a 512-byte MBR-shaped buffer and asks ATA to read sector 0.

Say this:

`ReadPartitions()` does not magically know the partition layout. It first needs the Master Boot Record, or MBR. On a traditional MS-DOS/MBR disk, the MBR lives in absolute disk sector `0`.

The code creates a `MasterBootRecord mbr` struct, then treats its address like a raw byte buffer:

```cpp
MasterBootRecord mbr;
uint8_t* mbrBytes = (uint8_t*)&mbr;
hd->Read28(0, mbrBytes, sizeof(MasterBootRecord), trace);
```

That line is the core of B01:

```text
Read28(0, ...)
```

means "read LBA sector 0 from this disk."

The destination is `mbrBytes`, which points at the `MasterBootRecord` struct. After the ATA read returns, the bytes from sector 0 are sitting inside that struct, so the code can inspect fields like `primaryPartition[0]` and `magicnumber`.

Good thing to emphasize:

`ReadPartitions()` depends on the disk format. It is assuming sector 0 has the MBR layout defined in `include/filesystem/msdospart.h`.

## 3. ATA Reads The MBR Bytes

Main point: ATA `Read28()` talks to the IDE ports and fills the buffer with bytes from the disk.

Say this:

The partition code asks for a sector, but the ATA driver does the hardware-level work.

`Read28()` uses LBA28 addressing. First it selects the right device:

```cpp
devicePort.Write(master ? ATA_DEVICE_MASTER_LBA28 : ATA_DEVICE_SLAVE_LBA28);
```

Because the kernel passed `ata0s`, `master` is false, so the driver selects the primary slave disk.

Then it writes the sector number into the ATA LBA registers. For B01, the sector number is `0`, so all the LBA byte registers receive zero.

After that, it sends the read command:

```cpp
commandPort.Write(ATA_CMD_READ_SECTORS);
```

Then it waits until the drive is ready. The important status bits are:

```text
BUSY clear
DATA_REQUEST set
ERROR not set
```

Once the disk is ready, the driver reads 16-bit words from `dataPort`. Each word contains two bytes. The driver splits each word into low byte and high byte and copies those bytes into the buffer that `ReadPartitions()` provided.

Good thing to emphasize:

The ATA driver does not understand partitions. It only understands sectors. B01's partition logic exists one layer above this.

Current-code note:

The timeline mentions trace output like `MBR:` and `Reading from ATA:`. In the current code, `Read28()` prints the label when tracing is enabled, but it no longer prints every raw MBR byte as a character.

## 4. MBR Magic Check And Partition Output

Main point: once sector 0 is in memory, B01 validates it and loops over the four MBR partition entries.

Say this:

After the sector read, the code checks the MBR signature:

```cpp
if(mbr->magicnumber != 0xAA55)
```

The last two bytes of a valid MBR should be `55 AA` on disk, which appears as `0xAA55` in this little-endian struct field.

If the magic number is wrong, the code prints:

```text
illegal MBR
```

and stops.

If the magic number is valid, B01 looks at the four primary partition slots:

```cpp
for(uint8_t i = 0; i < 4; i++)
```

Each slot is a `PartitionTableEntry`. The fields B01 cares about most are:

```text
bootable
partition_id
start_lba
length
```

`bootable` tells whether the boot flag is set.

`partition_id` is the MBR partition type byte.

`start_lba` tells where that partition begins on the whole disk.

`length` tells how many sectors the partition covers.

Good thing to emphasize:

The most valuable B01 result is `start_lba`. Later FAT32 code needs that number because a filesystem boot sector is not at disk sector 0. It is at the first sector of its partition.

Current-code note:

The current `ReadPartitions()` skips empty partition entries:

```cpp
if(partition->partition_id == 0x00)
    continue;
```

Then, for each non-empty partition, the current code calls:

```cpp
ReadBiosBlock(hd, partition->start_lba);
```

That call belongs to the B02 FAT32 demo. When explaining B01, you can say: "B01 stops conceptually after learning and printing the partition entries. B02 takes the `start_lba` value and enters the filesystem."

## 5. Return To The Kernel

Main point: after the partition scan finishes, control returns to `kernelMain()`.

Say this:

When `ReadPartitions()` finishes checking the partition entries, it returns to the kernel. The kernel prints a newline, activates interrupts, and then enters the infinite loop at the bottom of `kernelMain()`.

For the hard-drive demo, the useful work is already done by that point. The printed screen output is the demo result.

Good thing to emphasize:

This is normal for a tiny teaching kernel. There is no shell or process manager asking what to do next. The kernel runs the demo code, prints what it learned, and stays alive.

## Function Order To Explain Out Loud

Use this as the short call-stack version:

1. `kernelMain()` reaches the hard-drive demo block.
2. It creates `ata0m` for primary master.
3. It creates `ata0s` for primary slave.
4. It calls `MSDOSPartitionTable::ReadPartitions(&ata0s)`.
5. `ReadPartitions()` calls `ReadMasterBootRecord()`.
6. `ReadMasterBootRecord()` calls `hd->Read28(0, ...)`.
7. `Read28()` reads sector 0 through ATA ports.
8. Control returns to `ReadPartitions()`.
9. `ReadPartitions()` checks `0xAA55` and prints non-empty partition entries.
10. Control returns to `kernelMain()`.

## One-Minute Version

B01 is the "find the partitions" demo.

The kernel creates an ATA object for the primary slave disk because that is where the VirtualBox test disk is attached. Then it passes that disk object to `MSDOSPartitionTable::ReadPartitions()`.

`ReadPartitions()` asks the ATA driver to read sector 0. Sector 0 is special on an MBR disk because it contains the boot code area, the four primary partition entries, and the `0xAA55` signature.

After the read, MyOS checks the signature. If it is valid, it loops over the four partition entries and prints the partition type and bootable flag. The key field it learns is `start_lba`, because that tells later filesystem code where a partition begins.

So the whole B01 story is: kernel picks the primary slave disk, ATA reads sector 0, the MBR parser validates it, and MyOS learns where the partitions are.

## Common Confusions To Head Off

`/dev/hdb` is the whole disk in Tiny Core. `ata0s` is MyOS's object for that same disk.

`/dev/hdb1` and `/dev/hdb2` are partitions inside the disk. MyOS discovers their starting sectors by reading the MBR.

`fdisk` creates the partition table in sector 0. `mkfs.vfat` creates FAT32 structures inside a partition. B01 is about the `fdisk` part.

The MBR partition type byte is not the same thing as fully parsing a filesystem. Seeing a partition entry only tells MyOS where a region starts and roughly what type it claims to be.

Sector 0 is an absolute disk sector. `partition->start_lba` is also an absolute disk sector, but it points to the first sector inside one partition.

`Read28()` reads disk sectors. It does not know what an MBR, FAT32 boot sector, directory entry, or file is. Those meanings come from the higher-level code.
