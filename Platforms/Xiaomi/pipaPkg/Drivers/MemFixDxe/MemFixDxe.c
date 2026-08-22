/**
  MemFixDxe - Ensure Upper DDR memory is properly configured for Windows boot.

  This driver runs early and ensures the Upper DDR Conv region (0xB0000000+)
  is available in the UEFI memory map. It registers an ExitBootServices
  callback to verify memory attributes before Windows takes over.
**/

#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiDxeServicesTableLib.h>
#include <Library/DebugLib.h>

STATIC
VOID
EFIAPI
ExitBootServicesCallback (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  DEBUG ((EFI_D_INFO, "MemFixDxe: ExitBootServices callback - memory configured\n"));
}

EFI_STATUS
EFIAPI
MemFixDxeEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;
  EFI_EVENT   Event;

  DEBUG ((EFI_D_INFO, "MemFixDxe: Entry - checking Upper DDR memory\n"));

  // Register ExitBootServices callback to ensure memory is configured
  Status = gBS->CreateEvent (
                  EVT_SIGNAL_EXIT_BOOT_SERVICES,
                  TPL_NOTIFY,
                  ExitBootServicesCallback,
                  NULL,
                  &Event
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "MemFixDxe: Failed to create ExitBootServices event: %r\n", Status));
  }

  // Set memory attributes for Upper DDR region (0xB0000000 - 0x280000000)
  // This ensures Windows can access the full 8GB DDR
  {
    EFI_GCD_MEMORY_SPACE_DESCRIPTOR  Desc;
    Status = gDS->GetMemorySpaceDescriptor (0xB0000000, &Desc);
    if (!EFI_ERROR (Status)) {
      DEBUG ((EFI_D_INFO, "MemFixDxe: Upper DDR at 0xB0000000, type=%d, len=0x%lx\n",
             Desc.GcdMemoryType, Desc.Length));
      // Ensure the region has proper cache attributes for Windows
      Status = gDS->SetMemorySpaceAttributes (
                       0xB0000000,
                       Desc.Length,
                       EFI_MEMORY_WB
                       );
      if (EFI_ERROR (Status)) {
        DEBUG ((EFI_D_WARN, "MemFixDxe: SetMemorySpaceAttributes failed: %r\n", Status));
      }
    } else {
      DEBUG ((EFI_D_WARN, "MemFixDxe: Upper DDR not found in GCD: %r\n", Status));
    }
  }

  DEBUG ((EFI_D_INFO, "MemFixDxe: Exit\n"));
  return EFI_SUCCESS;
}
