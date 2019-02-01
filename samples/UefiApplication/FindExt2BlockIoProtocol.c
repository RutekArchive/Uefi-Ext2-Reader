#include "FindExt2BlockIoProtocol.h"
#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/ShellLib.h>
#define EXT2_SUPER_MAGIC 0xEF53

extern EFI_GUID gEfiBlockIoProtocolGuid = EFI_BLOCK_IO_PROTOCOL_GUID;


EFI_STATUS OpenProtocols(IN EFI_HANDLE ImageHandle,
	IN EFI_BOOT_SERVICES *BootServices,
	IN EFI_GUID *Protocol,
	IN OUT UINTN *BufferSize,
	OUT VOID **Buffer
);


EFI_STATUS SelectExt2BlockIoProtocols(IN EFI_HANDLE ImageHandle,
	IN EFI_BOOT_SERVICES *BootServices,
	IN OUT EFI_BLOCK_IO_PROTOCOL **BlockIoProtocols,
	IN OUT UINTN *Size
);


EFI_STATUS OpenProtocols(IN EFI_HANDLE ImageHandle,
	IN EFI_BOOT_SERVICES *BootServices,
	IN EFI_GUID *Protocol,
	IN OUT UINTN *BufferSize,
	OUT VOID **Buffer
)
{
	EFI_STATUS efiStatus;
	EFI_HANDLE *handles = NULL;
	UINTN size2;
	BOOLEAN isBufferTooSmall;

	size2 = 0;
	efiStatus = BootServices->LocateHandle(ByProtocol, &gEfiBlockIoProtocolGuid, NULL, &size2, NULL);
	isBufferTooSmall = (*BufferSize < size2 * sizeof(VOID*) / sizeof(EFI_HANDLE));
	*BufferSize = size2 * sizeof(VOID*) / sizeof(EFI_HANDLE);
	if (isBufferTooSmall)
	{
		return EFI_BUFFER_TOO_SMALL;
	}

	efiStatus = BootServices->AllocatePool(EfiBootServicesData, size2, (VOID**)&handles);
	if (EFI_ERROR(efiStatus))
	{
		goto Exit;
	}

	efiStatus = BootServices->LocateHandle(ByProtocol, &gEfiBlockIoProtocolGuid, NULL, &size2, handles);
	if (EFI_ERROR(efiStatus))
	{
		goto Exit;
	}

	size2 /= sizeof(EFI_HANDLE*);
	for (int i = 0; i < size2; i++)
	{
		efiStatus = BootServices->OpenProtocol(handles[i],
			&gEfiBlockIoProtocolGuid,
			Buffer + i,
			ImageHandle,
			NULL,
			EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL
		);

		if (EFI_ERROR(efiStatus))
		{
			goto Exit;
		}
	}

Exit:
	BootServices->FreePool(handles);
	return efiStatus;
}


EFI_STATUS SelectExt2BlockIoProtocols(IN EFI_HANDLE ImageHandle,
	IN EFI_BOOT_SERVICES *BootServices,
	IN OUT EFI_BLOCK_IO_PROTOCOL **BlockIoProtocols,
	IN OUT UINTN *Size
)
{
	EFI_STATUS efiStatus;

	for (int i = 0; i < *Size; i++)
	{
		char buffer[1024];
		EFI_BLOCK_IO_MEDIA *media = BlockIoProtocols[i]->Media;
		efiStatus = BlockIoProtocols[i]->ReadBlocks(BlockIoProtocols[i], media->MediaId, 2, 1024, buffer);
		if (EFI_ERROR(efiStatus))
		{
			continue;
		}

		UINT16 ext2SuperMagic = *(UINT16*)(buffer + 56);
		if (ext2SuperMagic == EXT2_SUPER_MAGIC)
		{
			*Size = i;
			return EFI_SUCCESS;
		}
	}

	return EFI_NOT_FOUND;
}


EFI_STATUS FindExt2BlockIoProtocol(IN EFI_HANDLE ImageHandle,
	IN EFI_BOOT_SERVICES *BootServices,
	IN OUT EFI_BLOCK_IO_PROTOCOL **PExt2BlockIoProtocol
)
{
	if (PExt2BlockIoProtocol == NULL || BootServices == NULL)
	{
		return EFI_INVALID_PARAMETER;
	}

	EFI_STATUS efiStatus;
	EFI_BLOCK_IO_PROTOCOL **blockIoProtocols = NULL;
	UINTN size = 0;

	efiStatus = OpenProtocols(ImageHandle, BootServices, &gEfiBlockIoProtocolGuid, &size, (VOID**)blockIoProtocols);
	if (efiStatus == EFI_BUFFER_TOO_SMALL)
	{
		efiStatus = BootServices->AllocatePool(EfiBootServicesData, size, (VOID**)&blockIoProtocols);
		if (EFI_ERROR(efiStatus))
		{
			Print(L"Failed to allocate page for block io protocol handles!\n");
			goto Exit;
		}

		efiStatus = OpenProtocols(ImageHandle, BootServices, &gEfiBlockIoProtocolGuid, &size, (VOID**)blockIoProtocols);
		size /= sizeof(VOID*);
	}

	if (EFI_ERROR(efiStatus))
	{
		Print(L"Failed to open protocols!\n");
		goto Exit;
	}

	efiStatus = SelectExt2BlockIoProtocols(ImageHandle, BootServices, blockIoProtocols, &size);
	if (EFI_ERROR(efiStatus))
	{
		Print(L"Failed to select ext2 block io protocols!\n");
	}

	*PExt2BlockIoProtocol = blockIoProtocols[size];
Exit:
	BootServices->FreePool(blockIoProtocols);
	return efiStatus;
}