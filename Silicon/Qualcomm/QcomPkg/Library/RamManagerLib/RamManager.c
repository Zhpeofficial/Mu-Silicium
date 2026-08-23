#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/RamManagerLib.h>

#include <Protocol/EFISmem.h>
#include <Protocol/EFIRamPartition.h>

//
// SMEM RAM Partition Table Defines
//
#define SMEM_USABLE_RAM_PARTITION_TABLE 402

#define RAM_NUM_PART_ENTRIES            32
#define RAM_PART_MAGIC1                 0x9DA5E0A8
#define RAM_PART_MAGIC2                 0xAF9EC4E2
#define RAM_PART_SYS_MEMORY             1
#define RAM_PART_SDRAM                  14

//
// SMEM RAM Partition Entry & Table (matches XBL's RamPartitionTable from SMEM)
//
#pragma pack(push, 1)
typedef struct {
  CHAR8  Name[16];
  UINT64 Base;
  UINT64 Length;
  UINT32 Attribute;
  UINT32 Category;
  UINT32 Domain;
  UINT32 Type;
  UINT32 NumPartitions;
  UINT32 HWInfo;
  UINT8  HighestBankBit;
  UINT8  Reserved0;
  UINT8  Reserved1;
  UINT8  Reserved2;
  UINT32 MinPasrSize;
  UINT64 AvailableLength;
} SMEM_RAM_PARTITION_ENTRY;

typedef struct {
  UINT32                   Magic1;
  UINT32                   Magic2;
  UINT32                   Version;
  UINT32                   Reserved1;
  UINT32                   NumPartitions;
  UINT32                   Reserved2;
  SMEM_RAM_PARTITION_ENTRY RamPartitionEntry[RAM_NUM_PART_ENTRIES];
} SMEM_RAM_PARTITION_TABLE;
#pragma pack(pop)

//
// Global Variables
//
STATIC EFI_SMEM_PROTOCOL *mSmemProtocol;
STATIC RamPartitionEntry *RamPartition;
STATIC UINT32             RamPartitionCount;

EFI_STATUS
GetUsableMemoryRanges (
  OUT EFI_MEMORY_RANGE **Range,
  OUT UINT8             *RangeCount)
{
  EFI_STATUS        Status     = EFI_SUCCESS;
  EFI_MEMORY_RANGE *LocalRange = NULL;

  if (RamPartitionCount == 0) {
    DEBUG ((EFI_D_ERROR, "RAM Partition Count is 0, no usable RAM!\n"));
    return EFI_NOT_READY;
  }

  // Allocate Memory
  LocalRange = AllocateZeroPool (sizeof (EFI_MEMORY_RANGE) * RamPartitionCount);
  if (LocalRange == NULL) {
    DEBUG ((EFI_D_ERROR, "Failed to Allocate Memory for usable Memory Ranges!\n"));
    Status = EFI_OUT_OF_RESOURCES;
    goto cleanup;
  }

  // Populate Memory Ranges
  for (UINT32 i = 0; i < RamPartitionCount; i++) {
    LocalRange[i].Address = RamPartition[i].Base;
    LocalRange[i].Length  = RamPartition[i].AvailableLength;
  }

  // Pass Memory Range Details
  *Range      = LocalRange;
  *RangeCount = (UINT8)RamPartitionCount;

cleanup:
  // Free Buffer
  FreePool (RamPartition);

  return Status;
}

EFI_STATUS
EFIAPI
GetRamPartitions (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable)
{
  EFI_STATUS                Status;
  SMEM_RAM_PARTITION_TABLE *RamPartTable = NULL;
  UINT32                    SmemSize      = 0;
  UINT32                    Count         = 0;
  UINT32                    Index;

  // Locate SMEM Protocol (provided by SmemDxe)
  Status = gBS->LocateProtocol (&gEfiSMEMProtocolGuid, NULL, (VOID *)&mSmemProtocol);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "Failed to Locate SMEM Protocol!\n"));
    return Status;
  }

  // Get RAM Partition Table from SMEM
  Status = mSmemProtocol->GetFunc (SMEM_USABLE_RAM_PARTITION_TABLE, &SmemSize, (VOID *)&RamPartTable);
  if (EFI_ERROR (Status) || RamPartTable == NULL) {
    DEBUG ((EFI_D_ERROR, "Failed to get RAM Partition Table from SMEM! Status = %r\n", Status));
    return EFI_NOT_FOUND;
  }

  // Verify RAM Partition Magic
  if (RamPartTable->Magic1 != RAM_PART_MAGIC1 || RamPartTable->Magic2 != RAM_PART_MAGIC2) {
    DEBUG ((EFI_D_ERROR, "Invalid RAM Partition Table Magic!\n"));
    return EFI_NOT_FOUND;
  }

  // Count valid SDRAM System Memory Partitions
  for (Index = 0; Index < RamPartTable->NumPartitions && Index < RAM_NUM_PART_ENTRIES; Index++) {
    SMEM_RAM_PARTITION_ENTRY *Entry = &RamPartTable->RamPartitionEntry[Index];
    if (Entry->Type == RAM_PART_SYS_MEMORY && Entry->Category == RAM_PART_SDRAM && Entry->AvailableLength != 0) {
      Count++;
    }
  }

  if (Count == 0) {
    DEBUG ((EFI_D_ERROR, "No valid SDRAM System Memory Partitions found!\n"));
    return EFI_NOT_FOUND;
  }

  // Allocate Memory
  RamPartition = AllocateZeroPool (sizeof (RamPartitionEntry) * Count);
  if (RamPartition == NULL) {
    DEBUG ((EFI_D_ERROR, "Failed to Allocate Memory for RAM Partitions!\n"));
    return EFI_OUT_OF_RESOURCES;
  }

  // Populate RAM Partitions
  Index = 0;
  for (UINT32 i = 0; i < RamPartTable->NumPartitions && i < RAM_NUM_PART_ENTRIES; i++) {
    SMEM_RAM_PARTITION_ENTRY *Entry = &RamPartTable->RamPartitionEntry[i];
    if (Entry->Type == RAM_PART_SYS_MEMORY && Entry->Category == RAM_PART_SDRAM && Entry->AvailableLength != 0) {
      RamPartition[Index].Base            = Entry->Base;
      RamPartition[Index].AvailableLength = Entry->AvailableLength;
      Index++;
    }
  }

  RamPartitionCount = Count;

  return EFI_SUCCESS;
}
