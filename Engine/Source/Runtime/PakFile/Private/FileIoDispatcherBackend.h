// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IO/IoDispatcherBackend.h"
#include "IO/IoStatus.h"
#include "Misc/AES.h"
#include "Misc/EnumClassFlags.h"

struct FIoContainerHeader;

namespace UE::IoStore
{

enum class ETocMountOptions
{
	None = 0,
	/** Make soft references available */
	WithSoftReferences = 1 << 0,
};
ENUM_CLASS_FLAGS(ETocMountOptions);

class IFileIoDispatcherBackend
	: public IIoDispatcherBackend
{
public:
	virtual ~IFileIoDispatcherBackend() = default;

	virtual TIoStatusOr<FIoContainerHeader> Mount(
		const TCHAR* TocPath, 
		int32 Order, 
		const FGuid& EncryptionKeyGuid, 
		const FAES::FAESKey& EncryptionKey, 
		ETocMountOptions Options = ETocMountOptions::None) = 0;
	virtual bool							Unmount(const TCHAR* TocPath) = 0;
	virtual void							ReopenAllFileHandles() = 0;
};

TSharedPtr<IFileIoDispatcherBackend> MakeFileIoDispatcherBackend();

} // namespace UE::IoStore
