/**
  SchedulerInstallDxe - Installs EFI_KERNEL_PROTOCOL from XBL HOB

  The XBL firmware passes the LK Scheduler Interface address via a HOB
  (gEfiSchedulerInterfaceHobGuid). UFSDxe expects this as an installed
  protocol (gEfiKernelProtocolGuid) for sleep callback registration.
  This driver bridges the gap by reading the HOB and installing the protocol.

**/

#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/HobLib.h>
#include <Library/DebugLib.h>
#include <Protocol/EFIKernelInterface.h>

EFI_STATUS
EFIAPI
SchedulerInstallDxeEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_HOB_GUID_TYPE    *GuidHob;
  EFI_PHYSICAL_ADDRESS *SchedulerAddr;

  // Check if protocol is already installed
  EFI_KERNEL_PROTOCOL *Existing = NULL;
  EFI_STATUS Status = gBS->LocateProtocol (
    &gEfiKernelProtocolGuid,
    NULL,
    (VOID **)&Existing
    );
  if (!EFI_ERROR (Status) && Existing != NULL) {
    DEBUG ((EFI_D_INFO, "SchedulerInstallDxe: EFI_KERNEL_PROTOCOL already installed\n"));
    return EFI_SUCCESS;
  }

  // Find the Scheduler Interface HOB built by PlatformPei
  GuidHob = GetFirstGuidHob (&gEfiSchedulerInterfaceHobGuid);
  if (GuidHob == NULL) {
    DEBUG ((EFI_D_ERROR, "SchedulerInstallDxe: Scheduler Interface HOB not found\n"));
    return EFI_NOT_FOUND;
  }

  // HOB data contains the physical address of the scheduler interface
  SchedulerAddr = (EFI_PHYSICAL_ADDRESS *)(GuidHob + 1);
  if (*SchedulerAddr == 0) {
    DEBUG ((EFI_D_ERROR, "SchedulerInstallDxe: Scheduler Interface address is NULL\n"));
    return EFI_INVALID_PARAMETER;
  }

  EFI_KERNEL_PROTOCOL *SchedulerProtocol = (EFI_KERNEL_PROTOCOL *)(UINTN)*SchedulerAddr;

  DEBUG ((
    EFI_D_INFO,
    "SchedulerInstallDxe: Installing EFI_KERNEL_PROTOCOL at 0x%lx (Version=0x%lx)\n",
    *SchedulerAddr,
    SchedulerProtocol->Version
    ));

  // Install as EFI_KERNEL_PROTOCOL
  Status = gBS->InstallProtocolInterface (
    &ImageHandle,
    &gEfiKernelProtocolGuid,
    EFI_NATIVE_INTERFACE,
    SchedulerProtocol
    );
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "SchedulerInstallDxe: Failed to install protocol: %r\n", Status));
    return Status;
  }

  DEBUG ((EFI_D_INFO, "SchedulerInstallDxe: EFI_KERNEL_PROTOCOL installed successfully\n"));
  return EFI_SUCCESS;
}
