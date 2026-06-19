# B01 MS-DOS Partition Table Timeline

This is the same B01 demo split into smaller diagrams so each step stays readable without deep zoom.

## 1. Kernel Disk Setup

```mermaid
sequenceDiagram
    title B01 setup: choose the disk object

    participant K as kernel.cpp<br/>kernelMain()
    participant ATAObj as AdvancedTechnologyAttachment<br/>ata0m / ata0s
    participant MBR as msdospart.cpp<br/>ReadPartitions()

    K->>K: HARDDRIVE_DEMO path runs
    K->>ATAObj: create ata0m(true, ATA_PRIMARY_IO_BASE)
    Note over K,ATAObj: primary master disk object exists for tutorial/demo parity

    K->>ATAObj: create ata0s(false, ATA_PRIMARY_IO_BASE)
    Note over K,ATAObj: primary slave disk object points at the B01 test disk

    K->>MBR: MSDOSPartitionTable::ReadPartitions(&ata0s)
```

## 2. Partition Reader Requests Sector 0

```mermaid
sequenceDiagram
    title B01 handoff: prepare an MBR buffer

    participant MBR as msdospart.cpp<br/>ReadPartitions()
    participant ATA as ata.cpp<br/>Read28()
    participant Screen as VGA text output<br/>printf()

    MBR->>MBR: create MasterBootRecord mbr
    MBR->>MBR: treat &mbr as raw uint8_t* bytes
    MBR->>Screen: printf("MBR: ")
    MBR->>ATA: hd->Read28(0, mbrBytes, sizeof(MasterBootRecord))
    Note over MBR,ATA: sector 0 is the MBR sector
```

## 3. ATA Reads The MBR Bytes

```mermaid
sequenceDiagram
    title B01 ATA read: pull sector 0 from the disk

    participant MBR as msdospart.cpp<br/>ReadPartitions()
    participant ATA as ata.cpp<br/>Read28()
    participant Disk as VirtualBox ATA disk<br/>primary slave VDI
    participant Screen as VGA text output<br/>printf()

    ATA->>ATA: validate sector number and buffer
    ATA->>Disk: select primary slave and LBA sector 0
    ATA->>Disk: commandPort.Write(ATA_CMD_READ_SECTORS)
    ATA->>ATA: wait until disk is not busy and data is ready
    ATA->>Screen: printf("Reading from ATA: ")

    loop every 16-bit word in requested bytes
        Disk-->>ATA: dataPort.Read()
        ATA->>Screen: print raw byte characters
        ATA->>MBR: copy bytes into mbrBytes
    end

    ATA-->>MBR: Read28 returns with mbr filled
```

## 4. MBR Magic Check And Partition Output

```mermaid
sequenceDiagram
    title B01 parser: validate and print partition entries

    participant MBR as msdospart.cpp<br/>ReadPartitions()
    participant Screen as VGA text output<br/>printf()

    MBR->>MBR: check mbr.magicnumber == 0xAA55
    alt invalid MBR
        MBR->>Screen: printf("illegal MBR")
    else valid MBR
        loop four MBR primary partition entries
            MBR->>MBR: partition = &mbr.primaryPartition[i]
            MBR->>Screen: print Partition XX
            MBR->>Screen: print bootable / not bootable
            MBR->>Screen: print Type YY
        end
    end
```

## 5. Return To The Kernel

```mermaid
sequenceDiagram
    title B01 finish: partition scan is done

    participant MBR as msdospart.cpp<br/>ReadPartitions()
    participant K as kernel.cpp<br/>kernelMain()

    MBR-->>K: ReadPartitions returns
    K->>K: kernel stays in while(1)
```

## Function Order

1. `kernelMain()` in `src/kernel.cpp`
2. `AdvancedTechnologyAttachment::AdvancedTechnologyAttachment(...)` in `src/drivers/ata.cpp`
3. `MSDOSPartitionTable::ReadPartitions(...)` in `src/filesystem/msdospart.cpp`
4. `AdvancedTechnologyAttachment::Read28(...)` in `src/drivers/ata.cpp`
5. `Port8Bit::Write(...)` / `Port16Bit::Read(...)` through the ATA ports
6. back to `ReadPartitions(...)` to parse `mbr.primaryPartition[0..3]`

## What Each Piece Means

`ata0m` is the primary master disk object.

`ata0s` is the primary slave disk object. The B01 test disk is attached here, so this is the disk passed to `ReadPartitions()`.

`Read28(0, ...)` reads sector `0`. On an MBR-partitioned disk, sector `0` contains the Master Boot Record.

`MasterBootRecord` is the struct layout used to interpret the 512 bytes from sector `0`.

`primaryPartition[4]` is the four-entry MS-DOS/MBR partition table.

The raw weird characters come from `Read28()` printing the MBR bytes as text before `ReadPartitions()` prints the same bytes in a structured way.
