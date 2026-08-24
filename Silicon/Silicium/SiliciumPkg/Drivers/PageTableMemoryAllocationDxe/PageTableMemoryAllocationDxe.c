/** @file PageTableMemoryAllocationDxe.c

  Pre-allocate page table memory in low memory and install the
  gArmPageTableMemoryAllocationProtocol. ArmMmuLib uses this protocol to
  get page table pages from a low-memory pool, avoiding the
  chicken-and-egg where page tables would be allocated from the still
  unmapped Upper DDR.

  Copyright (C) Microsoft Corporation. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/ArmPageTableMemoryAllocation.h>

#pragma pack(1)

typedef struct {
  UINT32        Signature;
  UINTN         Offset;
  UINTN         FreePages;
  LIST_ENTRY    NextPool;
} PAGE_TABLE_POOL;

#pragma pack()

#define MIN_PAGES_AVAILABLE        5
#define PAGE_TABLE_POOL_SIGNATURE  SIGNATURE_32 ('P','T','P','L')

UINTN       TotalFreePages     = 0;
BOOLEAN     mPageTablePoolLock = FALSE;
LIST_ENTRY  mPageTablePoolList = INITIALIZE_LIST_HEAD_VARIABLE (mPageTablePoolList);

EFI_STATUS
GetMorePages (
  IN  UINTN  PoolPages
  )
{
  PAGE_TABLE_POOL  *Buffer;

  if (PoolPages == 0) {
    return EFI_INVALID_PARAMETER;
  }

  PoolPages++;
  PoolPages = ALIGN_VALUE (PoolPages, EFI_SIZE_TO_PAGES (SIZE_2MB));
  Buffer    = (PAGE_TABLE_POOL *)AllocateAlignedPages (PoolPages, SIZE_2MB);
  if (Buffer == NULL) {
    DEBUG ((DEBUG_ERROR, "ERROR: Out of aligned pages\n"));
    return EFI_OUT_OF_RESOURCES;
  }

  Buffer->FreePages = PoolPages - 1;
  Buffer->Offset    = EFI_PAGE_SIZE;
  Buffer->Signature = PAGE_TABLE_POOL_SIGNATURE;
  TotalFreePages   += Buffer->FreePages;
  InsertHeadList (&mPageTablePoolList, &Buffer->NextPool);

  return EFI_SUCCESS;
}

PAGE_TABLE_POOL *
FindPoolToAllocateFrom (
  IN UINTN  Pages
  )
{
  PAGE_TABLE_POOL  *PoolToAllocateFrom = NULL;
  LIST_ENTRY       *CurrentEntry       = mPageTablePoolList.ForwardLink;

  while (CurrentEntry != &mPageTablePoolList) {
    PoolToAllocateFrom = CR (CurrentEntry, PAGE_TABLE_POOL, NextPool, PAGE_TABLE_POOL_SIGNATURE);
    if (Pages <= PoolToAllocateFrom->FreePages) {
      break;
    }

    CurrentEntry = CurrentEntry->ForwardLink;
  }

  return PoolToAllocateFrom;
}

VOID *
AllocatePageTableMemory (
  IN UINTN  Pages
  )
{
  VOID             *Buffer;
  PAGE_TABLE_POOL  *PoolToAllocateFrom = NULL;
  EFI_STATUS       Status;

  if (Pages == 0) {
    return NULL;
  }

  PoolToAllocateFrom = FindPoolToAllocateFrom (Pages);

  if (((PoolToAllocateFrom == NULL) ||
       (TotalFreePages < Pages) ||
       ((TotalFreePages - Pages) <= MIN_PAGES_AVAILABLE)) &&
      !mPageTablePoolLock)
  {
    DEBUG ((DEBUG_INFO, "%a - The reserve of translation table memory is being refilled!\n", __func__));
    mPageTablePoolLock = TRUE;
    Status             = GetMorePages (1);
    mPageTablePoolLock = FALSE;
    if (EFI_ERROR (Status)) {
      return NULL;
    }

    PoolToAllocateFrom = FindPoolToAllocateFrom (Pages);
  }

  if (PoolToAllocateFrom == NULL) {
    return NULL;
  }

  Buffer                         = (UINT8 *)PoolToAllocateFrom + PoolToAllocateFrom->Offset;
  PoolToAllocateFrom->Offset    += EFI_PAGES_TO_SIZE (Pages);
  PoolToAllocateFrom->FreePages -= Pages;
  TotalFreePages                -= Pages;

  return Buffer;
}

PAGE_TABLE_MEM_ALLOC_PROTOCOL  mPageTableMemAllocProtocol = {
  AllocatePageTableMemory
};

EFI_STATUS
EFIAPI
InitializePageTableMemory (
  IN EFI_HANDLE  ImageHandle
  )
{
  EFI_STATUS       Status;
  PAGE_TABLE_POOL  *Pages;

  Status = GetMorePages (1);

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "ERROR: Failed to allocate initial page table pool\n"));
    return Status;
  }

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &ImageHandle,
                  &gArmPageTableMemoryAllocationProtocolGuid,
                  &mPageTableMemAllocProtocol,
                  NULL
                  );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "ERROR: Failed to install ARM page table memory allocation protocol!\n"));
    Pages = CR (mPageTablePoolList.ForwardLink, PAGE_TABLE_POOL, NextPool, PAGE_TABLE_POOL_SIGNATURE);
    FreePages (Pages, EFI_SIZE_TO_PAGES (Pages->FreePages) + 1);
  }

  return Status;
}

EFI_STATUS
EFIAPI
PageTableMemoryAllocationDxeEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable)
{
  return InitializePageTableMemory (ImageHandle);
}
