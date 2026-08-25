#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/RamManagerLib.h>

#include <Protocol/EFISmem.h>

//
// SMEM RAM Partition Table (read directly from SMEM, matching v2.7 RamPartitionTableLib)
//
#define SMEM_USABLE_RAM_PARTITION_TABLE 402
#define RAM_PART_MAGIC1                 0x9DA5E0A8
#define RAM_PART_MAGIC2                 0xAF9EC4E2
#define RAM_NUM_PART_ENTRIES            32
#define RAM_PART_SYS_MEMORY             1

typedef struct {
  CHAR8   Name[16];
  UINT64  Base;
  UINT64  Length;
  UINT32  Attribute;
  UINT32  Category;
  UINT32  Domain;
  UINT32  Type;
  UINT32  NumPartitions;
  UINT32  HWInfo;
  UINT8   HighestBankBit;
  UINT8   Reserved0;
  UINT8   Reserved1;
  UINT8   Reserved2;
  UINT32  MinPasrSize;
  UINT64  AvailableLength;
} RAM_PARTITION_ENTRY;

typedef struct {
  UINT32               Magic1;
  UINT32               Magic2;
  UINT32               Version;
  UINT32               Reserved1;
  UINT32               NumPartitions;
  UINT32               Reserved2;
  RAM_PARTITION_ENTRY  RamPartitionEntry[RAM_NUM_PART_ENTRIES];
} RAM_PARTITION_TABLE;

//
// Simplified RAM partition entry (Base + AvailableLength) for the output list
//
typedef struct {
  UINT64  Base;
  UINT64  AvailableLength;
  UINT32  Type;
} RamPartitionEntry;

//
// Global Variables
//
STATIC RamPartitionEntry         *RamPartition;
STATIC UINT32                     RamPartitionCount;

EFI_STATUS
GetUsableMemoryRanges (
  OUT EFI_MEMORY_RANGE **Range,
  OUT UINT8             *RangeCount)
{
  EFI_STATUS        Status     = EFI_SUCCESS;
  EFI_MEMORY_RANGE *LocalRange = NULL;

  // Allocate Memory
  LocalRange = AllocateZeroPool (sizeof (EFI_MEMORY_RANGE) * RamPartitionCount);
  if (LocalRange == NULL) {
    DEBUG ((EFI_D_ERROR, "Failed to Allocate Memory for usable Memory Ranges!\n"));
    Status = EFI_OUT_OF_RESOURCES;
    goto cleanup;
  }

  // Populate Memory Ranges (only RAM_PART_SYS_MEMORY partitions with space)
  UINT32 Count = 0;
  for (UINT32 i = 0; i < RamPartitionCount; i++) {
    if (RamPartition[i].Type != RAM_PART_SYS_MEMORY || RamPartition[i].AvailableLength == 0) {
      continue;
    }
    LocalRange[Count].Address = RamPartition[i].Base;
    LocalRange[Count].Length  = RamPartition[i].AvailableLength;
    Count++;
  }

  // Pass Memory Range Details
  *Range      = LocalRange;
  *RangeCount = (UINT8)Count;

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
  EFI_STATUS           Status;
  EFI_SMEM_PROTOCOL   *SMEMProtocol = NULL;
  RAM_PARTITION_TABLE *Table        = NULL;
  UINT32               SmemSize     = 0;
  UINT32               i;

  // Locate SMEM Protocol
  Status = gBS->LocateProtocol (&gEfiSMEMProtocolGuid, NULL, (VOID *)&SMEMProtocol);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "Failed to Locate SMEM Protocol! Status = %r\n", Status));
    return Status;
  }

  // Get the RAM Partition Table directly from SMEM (item 402)
  Status = SMEMProtocol->GetFunc (SMEM_USABLE_RAM_PARTITION_TABLE, &SmemSize, (VOID *)&Table);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "Failed to get RAM Partition Table! Status = %r\n", Status));
    return Status;
  }

  // Validate the table magic values
  if (Table->Magic1 != RAM_PART_MAGIC1 || Table->Magic2 != RAM_PART_MAGIC2) {
    DEBUG ((EFI_D_ERROR, "RAM Partition Table Magic mismatch!\n"));
    return EFI_NOT_FOUND;
  }

  // Record the number of partitions
  RamPartitionCount = Table->NumPartitions;

  // Allocate Memory
  RamPartition = AllocateZeroPool (sizeof (RamPartitionEntry) * RamPartitionCount);
  if (RamPartition == NULL) {
    DEBUG ((EFI_D_ERROR, "Failed to Allocate Memory for RAM Partitions!\n"));
    return EFI_OUT_OF_RESOURCES;
  }

  // Copy partition base and available length (this catches the Upper DDR too)
  for (i = 0; i < RamPartitionCount; i++) {
    RamPartition[i].Base           = Table->RamPartitionEntry[i].Base;
    RamPartition[i].AvailableLength = Table->RamPartitionEntry[i].AvailableLength;
    RamPartition[i].Type           = Table->RamPartitionEntry[i].Type;
    // DIAG: dump each partition (Type/Base/Length/AvailableLength)
    DEBUG ((EFI_D_WARN,
      "RamManager: Part[%d] Type=%d Base=0x%llx Len=0x%llx Avail=0x%llx Name=%.16a\n",
      i,
      Table->RamPartitionEntry[i].Type,
      (unsigned long long)Table->RamPartitionEntry[i].Base,
      (unsigned long long)Table->RamPartitionEntry[i].Length,
      (unsigned long long)Table->RamPartitionEntry[i].AvailableLength,
      Table->RamPartitionEntry[i].Name));
  }
  DEBUG ((EFI_D_WARN, "RamManager: NumPartitions=%d\n", RamPartitionCount));

  return EFI_SUCCESS;
}
