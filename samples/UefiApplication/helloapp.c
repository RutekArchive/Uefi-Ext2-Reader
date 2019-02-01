#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/ShellLib.h>
#include "..\UefiDriver\drvproto.h"

#include <Protocol\BlockIo.h>
#include <Protocol\DiskIo.h>
#include <Protocol\SimpleFileSystem.h>

#include "EXT2_FILE_PROTOCOL.h"

CHAR8 *gEfiCallerBaseName = "ShellSample";
extern CONST UINT32 _gUefiDriverRevision = 0;

EFI_STATUS EFIAPI UefiUnload(IN EFI_HANDLE ImageHandle)
{
	ASSERT(FALSE);
}


VOID AsciiToUtf16(CHAR8 *Ascii, CHAR16 *Utf16, UINTN Size)
{
	for (int i = 0; i < Size; i++)
	{
		Utf16[i] = Ascii[i];
	}
}


EFI_STATUS EFIAPI UefiMain(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE* SystemTable)
{
	EFI_STATUS efiStatus;
	EFI_FILE_PROTOCOL *fileProtocol;
	efiStatus = ShellInitialize();

	if (EFI_ERROR(efiStatus))
	{
		Print(L"Failed to initialize shell!\n");
		goto Error;
	}

	Print(L"Shell is initialized!\n");

	efiStatus = REGISTER_EXT2_FILE_PROTOCOL(ImageHandle, SystemTable->BootServices, &fileProtocol);
	if (EFI_ERROR(efiStatus))
	{
		Print(L"Failed to register protocol!\n");
		goto Error;
	}

	Print(L"Registration of my file protocol is success!\n");

	EFI_FILE_PROTOCOL *file;
	CHAR16 fileName[] = L"dir1/file in dir";
	efiStatus = fileProtocol->Open(fileProtocol, &file, fileName, 0, 0);
	if (EFI_ERROR(efiStatus))
	{
		Print(L"Failed to open file %s!\n", fileName);
		goto Error;
	}
	Print(L"File %s is opened!\n", fileName);
	UINTN bufferSize = 1024;
	CHAR8 buffer[1024];
	efiStatus = file->Read(file, &bufferSize, buffer);
	if (EFI_ERROR(efiStatus))
	{
		Print(L"Failed to read buffer!\n");
		goto Error;
	}

	Print(L"File %s was opened and protocol read %d bytes from it!\n", fileName, bufferSize);

	CHAR16 buffer2[1024];
	AsciiToUtf16(buffer, buffer2, 1024);

	SystemTable->ConOut->OutputString(SystemTable->ConOut, buffer2);
	Print(L"\n");
	Print(L"Efi success!\n");
Error:
	return efiStatus;
}