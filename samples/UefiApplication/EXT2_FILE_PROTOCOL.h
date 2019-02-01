#pragma once
#include "Ext2DiskIoProtocol.h"
#include <Protocol\SimpleFileSystem.h>


EFI_STATUS REGISTER_EXT2_FILE_PROTOCOL(IN EFI_HANDLE ImageHandle,
	IN EFI_BOOT_SERVICES *BootServices,
	OUT EFI_FILE_PROTOCOL **NewProtocol
);