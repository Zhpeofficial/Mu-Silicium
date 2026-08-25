#ifndef _DEVICE_MEMORY_MAP_LIB_H_
#define _DEVICE_MEMORY_MAP_LIB_H_

// v2.7 interop: the v2.7 DeviceMemoryMapLib is a thin wrapper around zhpe's
// MemoryMapLib. The region structure/macros are provided by MemoryMapLib.h and
// ARM_MEMORY_REGION_DESCRIPTOR_EX is aliased there to zhpe's
// EFI_MEMORY_REGION_DESCRIPTOR (identical layout).
#include <Library/MemoryMapLib.h>

/**
  This Function returns the Device Memory Map.

  @param[out] MemoryDescriptor - The Memory Map.

  @return A pointer to the array of ARM_MEMORY_REGION_DESCRIPTOR_EX.
**/
ARM_MEMORY_REGION_DESCRIPTOR_EX*
GetDeviceMemoryMap (
  VOID
  );

#endif /* _DEVICE_MEMORY_MAP_LIB_H_ */
