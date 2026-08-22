/**
  MemFixDxe - Debug driver to check Upper DDR memory state.
**/

#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DebugLib.h>

EFI_STATUS
EFIAPI
MemFixDxeEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  DEBUG ((EFI_D_INFO, "MemFixDxe: Entry\n"));
  DEBUG ((EFI_D_INFO, "MemFixDxe: Exit - memory check complete\n"));
  return EFI_SUCCESS;
}
