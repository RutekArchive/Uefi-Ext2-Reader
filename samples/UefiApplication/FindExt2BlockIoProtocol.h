#ifndef FIND_EXT2_BLOCK_IO_PROTOCOL_H
#define FIND_EXT2_BLOCK_IO_PROTOCOL_H
#include <Protocol\BlockIo.h>
EFI_STATUS FindExt2BlockIoProtocol(IN EFI_HANDLE ImageHandle,
	IN EFI_BOOT_SERVICES *BootServices,
	IN OUT EFI_BLOCK_IO_PROTOCOL **PExt2BlockIoProtocol
);
#endif //FIND_EXT2_BLOCK_IO_PROTOCOL_H