#ifndef EXT2_DISK_IO_PROTOCOL_H
#define EXT2_DISK_IO_PROTOCOL_H
#include <Uefi.h>
#include <Protocol\BlockIo.h>

/*
EFI_STATUS OpenExt2(IN EFI_BLOCK_IO_PROTOCOL *This,
	IN EFI_BOOT_SERVICES *BootServices,
	IN OUT EXT2 *Ext2
)
{
	EFI_STATUS efiStatus;
	efiStatus = ReadDisk(This, BootServices, 1024, 1024, Ext2->SuperBlock);
	if (EFI_ERROR(efiStatus))
	{
		return efiStatus;
	}
	efiStatus = ReadDisk(This, BootServices, 2048, sizeof(BLOCK_GROUP_DESCRIPTOR_TABLE), &Ext2->BlockGroupDescriptorTable);
	return efiStatus;
}*/

/*
EFI_STATUS ReadExt2Inode(IN EFI_BLOCK_IO_PROTOCOL *This,
	IN EFI_BOOT_SERVICES *BootServices,
	IN EXT2 *Ext2,
	IN UINT32 InodeId,
	IN OUT INODE *Inode
)
{
	UINT32 firstInodeBlockId = Ext2->BlockGroupDescriptorTable.InodeTable;
	UINT32 blockSize = 1024 << *(UINT32*)(Ext2->SuperBlock + 24);
	UINT32 offset = 2048 + (firstInodeBlockId - 2) * blockSize;
	offset += InodeId * sizeof(INODE);
	Print(L"OffsetF: %d\n", offset);
	return ReadDisk(This, BootServices, offset, sizeof(INODE), Inode);
}*/

/*
EFI_STATUS ReadDataFromInode(IN EFI_BLOCK_IO_PROTOCOL *This,
	IN EFI_BOOT_SERVICES *BootServices,
	IN INODE *Inode,
	IN EXT2 *Ext2,
	IN UINT32 Offset,
	IN OUT UINT32 *Size,
	OUT VOID *Buffer
)
{
	if (Offset + *Size > Inode->Size)
	{
		*Size = Inode->Size - Offset;
	}
	
	EFI_STATUS efiStatus;
	CHAR8 *block = NULL;
	UINT32 blockSize = 1024 << *(UINT32*)(Ext2->SuperBlock + 24);
	efiStatus = BootServices->AllocatePool(EfiBootServicesData, blockSize, (VOID**)&block);

	if (EFI_ERROR(efiStatus))
	{
		return efiStatus;
	}

	UINT32 blockId = Offset / blockSize;
	UINT32 offset = Offset % blockSize;
	UINT32 read = 0;
	while (blockId < 12 && read != *Size)
	{
		Print(L"DEBUG: blockSize: %d, blockID: %d, offset %d, read %d\n", blockSize, blockId, offset, read);

		efiStatus = ReadExt2Block(This, BootServices, Ext2, Inode->BlockTable[blockId], &blockSize, block);
		if (EFI_ERROR(efiStatus))
		{
			goto Error;
		}
		if (blockSize - offset + read > *Size)
		{
			blockSize = *Size - read + offset;
		}
		BootServices->CopyMem((CHAR8*)Buffer + read, block + offset, blockSize - offset);
		read += blockSize - offset;
		offset = 0;
		blockId++;
	}

	if (read == *Size)
	{
		goto Error;
	}

	UINT32 *IndBlock = NULL;
	efiStatus = BootServices->AllocatePool(EfiBootServicesData, blockSize, (VOID**)&IndBlock);

	if (EFI_ERROR(efiStatus))
	{
		goto Error;
	}

	efiStatus = ReadExt2Block(This, BootServices, Ext2, Inode->BlockTable[12], &blockSize, IndBlock);
	if (EFI_ERROR(efiStatus))
	{
		goto Error2;
	}

	while (blockId < 12 + blockSize / sizeof(UINT32) && read != *Size)
	{
		if (blockSize - offset + read > *Size)
		{
			blockSize = *Size - read + offset;
		}

		efiStatus = ReadExt2Block(This, BootServices, Ext2, IndBlock[blockId - 12], &blockSize, block);
		if (EFI_ERROR(efiStatus))
		{
			goto Error2;
		}

		BootServices->CopyMem((CHAR8*)Buffer + read, block + offset, blockSize - offset);
		read += blockSize - offset;
		offset = 0;
		blockId++;
	}

Error2:
	BootServices->FreePool(IndBlock);
Error:
	BootServices->FreePool(block);
	return efiStatus;
}*/
#endif //EXT2_DISK_IO_PROTOCOL_H