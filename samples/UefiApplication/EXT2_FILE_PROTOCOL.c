#include "EXT2_FILE_PROTOCOL.h"
#include <Uefi.h>
#include <Protocol\BlockIo.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/ShellLib.h>
#include "FindExt2BlockIoProtocol.h"

typedef struct
{
	UINT32 BlockBitmap;
	UINT32 InodeBitmap;
	UINT32 InodeTable;
	UINT16 FreeBlocksCount;
	UINT16 FreeInodesCount;
	UINT16 UsedDirsCount;
	UINT16 Pad;
	CHAR8 reserved[12];
} BLOCK_GROUP_DESCRIPTOR_TABLE;


typedef struct
{
	UINT16 Mode;
	UINT16 Uid;
	UINT32 Size;
	UINT32 Atime;
	UINT32 Ctime;
	UINT32 Mtime;
	UINT32 Dtime;
	UINT16 Gid;
	UINT16 LinksCount;
	UINT32 Blocks;
	UINT32 Flags;
	UINT32 Osd1;
	UINT32 BlockTable[15];
	UINT32 Generation;
	UINT32 FileAcl;
	UINT32 DirAcl;
	UINT32 Faddr;
	CHAR8 Osd2[12];
} INODE;


typedef struct
{
	CHAR8 SuperBlock[1024];
	INODE RootInode;
	EFI_BLOCK_IO_PROTOCOL *blockIoProtocol;
} EXT2;


typedef struct
{
	UINT32 InodeId;
	UINT16 RecordLength;
	UINT8 NameLength;
	UINT8 FileType;
	CHAR8 Name[257];
} DIRECTORY_ENTRY;


typedef struct
{
	EFI_FILE_PROTOCOL FileProtocol;
	EFI_BOOT_SERVICES *BootServices;
	EXT2 Ext2;
	INODE Inode;
	UINTN Offset;
} EXT2_FILE_PROTOCOL;
EXT2_FILE_PROTOCOL protocol;

EFI_STATUS EXT2_FILE_OPEN(IN EXT2_FILE_PROTOCOL *This,
	OUT EXT2_FILE_PROTOCOL **NewHandle,
	IN CHAR16 *FileName,
	IN UINT64 OpenMode,
	IN UINT64 Attributes
);


EFI_STATUS EXT2_FILE_READ(IN EXT2_FILE_PROTOCOL *This,
	IN OUT UINTN *BufferSize,
	OUT VOID *Buffer
);


EFI_STATUS ReadExt2Block(IN EXT2_FILE_PROTOCOL *This,
	IN UINTN BlockId,
	IN OUT VOID *Buffer
);


EFI_STATUS ReadDisk(IN EXT2_FILE_PROTOCOL *This,
	IN UINTN Offset,
	IN UINTN BufferSize,
	IN OUT VOID *Buffer
);


EFI_STATUS FindNextInode(IN EXT2_FILE_PROTOCOL *This,
	IN CHAR16 *FileName,
	IN OUT INODE *Inode
);


EFI_STATUS LoadBlockFromInode(IN EXT2_FILE_PROTOCOL *This,
	IN UINTN BlockId,
	IN OUT UINT32 *IndBlock,
	IN OUT UINT32 *DoubleIndBlock,
	IN OUT UINT32 *TriIndBlock,
	IN OUT VOID *block
);


EFI_STATUS ReadExt2Inode(IN EXT2_FILE_PROTOCOL *This,
	IN UINT32 InodeId,
	IN OUT INODE *Inode
);


EFI_STATUS REGISTER_EXT2_FILE_PROTOCOL(IN EFI_HANDLE ImageHandle,
	IN EFI_BOOT_SERVICES *BootServices,
	OUT EFI_FILE_PROTOCOL **NewProtocol
)
{
	EFI_STATUS efiStatus;
	EFI_BLOCK_IO_PROTOCOL *ext2BlockIoProtocol = NULL;
	efiStatus = FindExt2BlockIoProtocol(ImageHandle, BootServices, &ext2BlockIoProtocol);
	if (EFI_ERROR(efiStatus))
	{
		return efiStatus;
	}

	protocol.FileProtocol.Open = (EFI_FILE_OPEN)EXT2_FILE_OPEN;
	protocol.FileProtocol.Read = (EFI_FILE_READ)EXT2_FILE_READ;
	protocol.Ext2.blockIoProtocol = ext2BlockIoProtocol;
	protocol.BootServices = BootServices;

	efiStatus = ReadDisk(&protocol, 1024, 1024, protocol.Ext2.SuperBlock);
	if (EFI_ERROR(efiStatus))
	{
		return efiStatus;
	}

	*NewProtocol = &protocol.FileProtocol;

	return ReadExt2Inode(&protocol, 2, &protocol.Ext2.RootInode);
}


EFI_STATUS EXT2_FILE_OPEN(IN EXT2_FILE_PROTOCOL *This,
	OUT EXT2_FILE_PROTOCOL **NewHandle,
	IN CHAR16 *FileName,
	IN UINT64 OpenMode,
	IN UINT64 Attributes
)
{
	EFI_STATUS efiStatus;
	INT32 i = 1;
	for (; FileName[i - 1] != '\0'; i++)
	{ }

	CHAR16 *fileNameCopy;
	efiStatus = This->BootServices->AllocatePool(EfiBootServicesData, i * sizeof(CHAR16), (VOID**)&fileNameCopy);
	if (EFI_ERROR(efiStatus))
	{
		goto Error;
	}
	This->BootServices->CopyMem(fileNameCopy, FileName, i * sizeof(CHAR16));
	
	INODE inode = This->Ext2.RootInode;
	INT32 begin = 0;

	for (i = 0; fileNameCopy[i] != '\0'; i++)
	{
		if (fileNameCopy[i] == '\\' || fileNameCopy[i] == '/')
		{
			CHAR16 tmp = fileNameCopy[i];
			fileNameCopy[i] = '\0';
			efiStatus = FindNextInode(This, fileNameCopy + begin, &inode);
			fileNameCopy[i] = tmp;
			if (EFI_ERROR(efiStatus))
			{
				goto Error;
			}
			begin =  i + 1;
		}

	}

	efiStatus = FindNextInode(This, fileNameCopy + begin, &inode);
	if (EFI_ERROR(efiStatus))
	{
		goto Error;
	}

	This->BootServices->AllocatePool(EfiBootServicesData, sizeof(EXT2_FILE_PROTOCOL), (VOID**)NewHandle);
	(*NewHandle)->Ext2 = This->Ext2;
	(*NewHandle)->FileProtocol = This->FileProtocol;
	(*NewHandle)->BootServices = This->BootServices;
	(*NewHandle)->Inode = inode;
	(*NewHandle)->Offset = 0;

Error:
	This->BootServices->FreePool(fileNameCopy);
	return efiStatus;
}


EFI_STATUS EXT2_FILE_READ(IN EXT2_FILE_PROTOCOL *This,
	IN OUT UINTN *BufferSize,
	OUT VOID *Buffer
)
{	
	EFI_STATUS efiStatus;
	UINTN blockSize = 1024 << *(UINT32*)(This->Ext2.SuperBlock + 24);
	CHAR8 *block;
	UINT32 *IndBlock, *DoubleIndBlock, *TriIndBlock;
	efiStatus = This->BootServices->AllocatePool(EfiBootServicesData, 4 * blockSize, (VOID**)&block);
	if (EFI_ERROR(efiStatus))
	{
		return efiStatus;
	}
	IndBlock = (UINT32*)(block + blockSize);
	DoubleIndBlock = IndBlock + blockSize;
	TriIndBlock = DoubleIndBlock + blockSize;

	UINTN blockId = This->Offset / blockSize;
	UINTN offset = This->Offset % blockSize;
	UINTN IdsPerBlock = blockSize / sizeof(UINT32);
	UINTN read = 0;
	if (blockId < 12) { }
	else if (blockId < 12 + IdsPerBlock)
	{
		efiStatus = ReadExt2Block(This, 12, IndBlock);
		if (EFI_ERROR(efiStatus))
		{
			goto Error;
		}
	}
	else if (blockId < 12 + IdsPerBlock * (IdsPerBlock + 1))
	{
		efiStatus = ReadExt2Block(This, 13, DoubleIndBlock);
		if (EFI_ERROR(efiStatus))
		{
			goto Error;
		}
		UINTN id = (blockId - 12 - IdsPerBlock) / IdsPerBlock;
		efiStatus = ReadExt2Block(This, DoubleIndBlock[id], IndBlock);
		if (EFI_ERROR(efiStatus))
		{
			goto Error;
		}
	}
	else
	{
		efiStatus = ReadExt2Block(This, 14, TriIndBlock);
		if (EFI_ERROR(efiStatus))
		{
			goto Error;
		}
		UINTN id = (blockId - 12 - IdsPerBlock * (IdsPerBlock + 1)) / (IdsPerBlock * IdsPerBlock);
		efiStatus = ReadExt2Block(This, TriIndBlock[id], DoubleIndBlock);
		if (EFI_ERROR(efiStatus))
		{
			goto Error;
		}
		efiStatus = ReadExt2Block(This, DoubleIndBlock[((blockId - 12 - IdsPerBlock * (IdsPerBlock + 1)) % (IdsPerBlock * IdsPerBlock)) / IdsPerBlock], IndBlock);
		if (EFI_ERROR(efiStatus))
		{
			goto Error;
		}
	}

	if (This->Offset + *BufferSize > This->Inode.Size)
	{
		*BufferSize = This->Inode.Size - This->Offset;
	}

	while (read != *BufferSize)
	{
		efiStatus = LoadBlockFromInode(This, blockId, IndBlock, DoubleIndBlock, TriIndBlock, block);
		if (EFI_ERROR(efiStatus))
		{
			goto Error;
		}
		if (blockSize - offset + read > *BufferSize)
		{
			blockSize = *BufferSize - read + offset;
		}
		This->BootServices->CopyMem((CHAR8*)Buffer + read, block + offset, blockSize - offset);
		read += blockSize - offset;
		This->Offset += blockSize - offset;
		offset = 0;
		blockId++;
	}

Error:
	This->BootServices->FreePool(block);
	return efiStatus;
}


EFI_STATUS ReadExt2Block(IN EXT2_FILE_PROTOCOL *This,
	IN UINTN BlockId,
	IN OUT VOID *Buffer
)
{
	UINTN blockSize = 1024 << *(UINT32*)(This->Ext2.SuperBlock + 24);

	if (BlockId == 0 || BlockId == 1)
	{
		return ReadDisk(This, BlockId * 1024, 1024, Buffer);
	}
	else
	{
		return ReadDisk(This, 2048 + (BlockId - 2) * blockSize, blockSize, Buffer);
	}
}


EFI_STATUS ReadDisk(IN EXT2_FILE_PROTOCOL *This,
	IN UINTN Offset,
	IN UINTN BufferSize,
	IN OUT VOID *Buffer
)
{
	CHAR8 *block;
	EFI_STATUS efiStatus;
	EFI_BLOCK_IO_PROTOCOL *blockIoProtocol = This->Ext2.blockIoProtocol;
	UINTN blockSize = blockIoProtocol->Media->BlockSize;
	efiStatus = This->BootServices->AllocatePool(EfiBootServicesData, blockSize, (VOID**)&block);
	if (EFI_ERROR(efiStatus))
	{
		return efiStatus;
	}

	EFI_LBA lba = Offset / blockSize;
	UINTN offset = Offset % blockSize;
	UINTN read = 0;
	while (read != BufferSize)
	{
		efiStatus = blockIoProtocol->ReadBlocks(blockIoProtocol, blockIoProtocol->Media->MediaId, lba, blockSize, block);
		if (EFI_ERROR(efiStatus))
		{
			goto Error;
		}

		if (blockSize - offset + read > BufferSize)
		{
			blockSize = BufferSize - read + offset;
		}

		This->BootServices->CopyMem((CHAR8*)Buffer + read, block + offset, blockSize - offset);
		read += blockSize - offset;
		offset = 0;
		lba++;
	}

Error:
	This->BootServices->FreePool(block);
	return efiStatus;
}


EFI_STATUS FindNextInode(IN EXT2_FILE_PROTOCOL *This,
	IN CHAR16 *FileName,
	IN OUT INODE *Inode
)
{
	EFI_STATUS efiStatus;
	EXT2_FILE_PROTOCOL tmp;
	tmp.BootServices = This->BootServices;
	tmp.Ext2 = This->Ext2;
	tmp.FileProtocol = This->FileProtocol;
	tmp.Inode = *Inode;
	tmp.Offset = 0;

	UINTN read = 0;
	while (read != Inode->Size)
	{
		DIRECTORY_ENTRY entry;
		entry.Name[256] = '\0';
		UINTN size = 8;
		efiStatus = EXT2_FILE_READ(&tmp, &size, &entry);
		if (EFI_ERROR(efiStatus))
		{
			return efiStatus;
		}
		read += size;
		size = entry.NameLength;
		if (entry.InodeId != 0)
		{
			efiStatus = EXT2_FILE_READ(&tmp, &size, entry.Name);
			if (EFI_ERROR(efiStatus))
			{
				return efiStatus;
			}
			read += size;
			UINT32 i = 0;
			for (; FileName[i] != '\0'; i++)
			{
				if (FileName[i] != entry.Name[i])
					break;
			}
			if (FileName[i] == '\0' && i == entry.NameLength)
			{
				return ReadExt2Inode(&tmp, entry.InodeId, Inode);
			}
			tmp.Offset += entry.RecordLength - 8 - entry.NameLength;
			read += entry.RecordLength - 8 - entry.NameLength;
		}
		else
		{
			tmp.Offset += size;
		}
	}
	
	return EFI_NOT_FOUND;
}


EFI_STATUS LoadBlockFromInode(IN EXT2_FILE_PROTOCOL *This,
	IN UINTN BlockId,
	IN OUT UINT32 *IndBlock,
	IN OUT UINT32 *DoubleIndBlock,
	IN OUT UINT32 *TriIndBlock,
	IN OUT VOID *block
)
{
	EFI_STATUS efiStatus;
	UINTN BlockSize = 1024 << *(INT32*)(This->Ext2.SuperBlock + 24);
	UINTN IdsPerBlock = BlockSize / sizeof(UINT32);
	if (BlockId < 12)
	{
		return ReadExt2Block(This, This->Inode.BlockTable[BlockId], block);
	}
	if (BlockId < 12 + IdsPerBlock)
	{
		UINTN id = BlockId - 12;
		if (id == 0)
		{
			efiStatus = ReadExt2Block(This, This->Inode.BlockTable[12], IndBlock);
			if (EFI_ERROR(efiStatus))
			{
				return efiStatus;
			}
		}
		return ReadExt2Block(This, IndBlock[id], block);
	}
	if (BlockId < 12 + IdsPerBlock * (IdsPerBlock + 1))
	{
		UINTN id = BlockId - IdsPerBlock - 12;
		if (id == 0)
		{
			efiStatus = ReadExt2Block(This, This->Inode.BlockTable[13], DoubleIndBlock);
			if (EFI_ERROR(efiStatus))
			{
				return efiStatus;
			}
		}

		if (id % IdsPerBlock == 0)
		{
			efiStatus = ReadExt2Block(This, DoubleIndBlock[id / IdsPerBlock], IndBlock);
			if (EFI_ERROR(efiStatus))
			{
				return efiStatus;
			}
		}

		return ReadExt2Block(This, IndBlock[id % IdsPerBlock], block);
	}

	UINTN id = BlockId - IdsPerBlock * (IdsPerBlock + 1) - 12;
	if (id == 0)
	{
		efiStatus = ReadExt2Block(This, This->Inode.BlockTable[14], TriIndBlock);
		if (EFI_ERROR(efiStatus))
		{
			return efiStatus;
		}
	}

	if (id % IdsPerBlock == 0)
	{
		efiStatus = ReadExt2Block(This, TriIndBlock[id / IdsPerBlock], DoubleIndBlock);
		if (EFI_ERROR(efiStatus))
		{
			return efiStatus;
		}
	}

	id %= IdsPerBlock;
	if (id % IdsPerBlock == 0)
	{
		efiStatus = ReadExt2Block(This, DoubleIndBlock[id / IdsPerBlock], IndBlock);
		if (EFI_ERROR(efiStatus))
		{
			return efiStatus;
		}
	}

	return ReadExt2Block(This, IndBlock[id % IdsPerBlock], block);
}


EFI_STATUS ReadExt2Inode(IN EXT2_FILE_PROTOCOL *This,
	IN UINT32 InodeId,
	IN OUT INODE *Inode
)
{
	EFI_STATUS efiStatus;
	BLOCK_GROUP_DESCRIPTOR_TABLE blockGroupDescriptorTable;
	efiStatus = ReadDisk(This, 2048, sizeof(BLOCK_GROUP_DESCRIPTOR_TABLE), &blockGroupDescriptorTable);
	if (EFI_ERROR(efiStatus))
	{
		return efiStatus;
	}

	InodeId--;
	UINT32 firstInodeBlockId = blockGroupDescriptorTable.InodeTable;
	UINT32 blockSize = 1024 << *(UINT32*)(This->Ext2.SuperBlock + 24);
	UINT32 offset = 2048 + (firstInodeBlockId - 2) * blockSize;
	offset += InodeId * sizeof(INODE);
	return ReadDisk(This, offset, sizeof(INODE), Inode);
}