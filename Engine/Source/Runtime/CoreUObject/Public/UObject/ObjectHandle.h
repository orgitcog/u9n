// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Map.h"
#include "Containers/ScriptArray.h"
#include "CoreTypes.h"
#include "HAL/CriticalSection.h"
#include "HAL/Platform.h"
#include "Misc/AssertionMacros.h"
#include "Templates/TypeHash.h"
#include "UObject/NameTypes.h"
#include "UObject/ObjectHandleTracking.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ObjectRef.h"
#include "UObject/PackedObjectRef.h"
#include "UObject/RemoteObject.h"

class UClass;
class UPackage;

/**
 * FObjectHandle is either a packed object ref or the resolved pointer to an object.  Depending on configuration
 * when you create a handle, it may immediately be resolved to a pointer.
 */
#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE

namespace UE::CoreUObject::Private
{
	struct FObjectHandlePrivate;
}

using FObjectHandle = UE::CoreUObject::Private::FObjectHandlePrivate;

#elif UE_WITH_REMOTE_OBJECT_HANDLE

namespace UE::CoreUObject::Private
{
	struct FRemoteObjectHandlePrivate;
}

using FObjectHandle = UE::CoreUObject::Private::FRemoteObjectHandlePrivate;

#else

using FObjectHandle = UObject*;
//NOTE: operator==, operator!=, GetTypeHash fall back to the default on UObject* or void* through coercion.

#endif

inline bool IsObjectHandleNull(FObjectHandle Handle);
inline bool IsObjectHandleResolved(FObjectHandle Handle);
inline bool IsObjectHandleTypeSafe(FObjectHandle Handle);

//Private functions that forced public due to inlining.
namespace UE::CoreUObject::Private
{
#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE

	struct FObjectHandlePrivate
	{
		//Stores either FPackedObjectRef or a UObject*
		UPTRINT PointerOrRef;

		explicit inline operator bool() const
		{
			return PointerOrRef != 0;
		}
	};

	/* Returns the packed object ref for this object IF one exists otherwise returns a null PackedObjectRef */
	COREUOBJECT_API FPackedObjectRef FindExistingPackedObjectRef(const UObject* Object);

	/* Creates and ObjectRef from a packed object ref*/
	COREUOBJECT_API FObjectRef MakeObjectRef(FPackedObjectRef Handle);

#elif UE_WITH_REMOTE_OBJECT_HANDLE

	struct FRemoteObjectHandlePrivate
	{
		UPTRINT PointerOrHandle;

		FRemoteObjectHandlePrivate() = default;
		explicit FRemoteObjectHandlePrivate(UObject* Object)
		{
			PointerOrHandle = UPTRINT(Object);
		}
		explicit FRemoteObjectHandlePrivate(UE::RemoteObject::Handle::FRemoteObjectStub* RemoteInfo)
		{
			PointerOrHandle = UPTRINT(RemoteInfo) | 1;
		}

		FORCEINLINE const UE::RemoteObject::Handle::FRemoteObjectStub* ToStub() const
		{
			return reinterpret_cast<UE::RemoteObject::Handle::FRemoteObjectStub*>(PointerOrHandle & ~UPTRINT(1));
		}

		COREUOBJECT_API FRemoteObjectId GetRemoteId() const;

		COREUOBJECT_API static FRemoteObjectHandlePrivate ConvertToRemoteHandle(UObject* Object);
		COREUOBJECT_API static FRemoteObjectHandlePrivate FromIdNoResolve(FRemoteObjectId ObjectId);
	};

#endif

	///these functions are always defined regardless of UE_WITH_OBJECT_HANDLE_LATE_RESOLVE value

	/* Makes a resolved FObjectHandle from an UObject. */
	inline FObjectHandle MakeObjectHandle(UObject* Object);

	/* Returns the UObject from Handle and the handle is updated cache the resolved UObject */
	inline UObject* ResolveObjectHandle(FObjectHandle& Handle);

	/* Returns the UClass for UObject store in Handle. Handle is not resolved */
	inline UClass* ResolveObjectHandleClass(FObjectHandle Handle);

	/* Returns the UObject from Handle and the handle is updated cache the resolved UObject.
	 * Does not cause ObjectHandleTracking to fire a read event
	 */
	inline UObject* ResolveObjectHandleNoRead(FObjectHandle& Handle);

	/** Resolves an ObjectHandle without checking if already resolved. Invalid to call for resolved handles */
	inline UObject* ResolveObjectHandleNoReadNoCheck(FObjectHandle& Handle);

	/** Read the handle as a pointer without checking if it is resolved. Invalid to call for unresolved handles. */
	inline UObject* ReadObjectHandlePointerNoCheck(FObjectHandle Handle);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/* return true if handle is null */
inline bool IsObjectHandleNull(FObjectHandle Handle)
{
#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE
	return !Handle.PointerOrRef;
#elif UE_WITH_REMOTE_OBJECT_HANDLE
	return !Handle.PointerOrHandle;
#else
	return !Handle;
#endif
}

/* checks if a handle is resolved. 
 * nullptr counts as resolved
 * all handles are resolved when late resolved is off
 */
inline bool IsObjectHandleResolved(FObjectHandle Handle)
{
#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE
	return !(Handle.PointerOrRef & 1);
#elif UE_WITH_REMOTE_OBJECT_HANDLE
	if (!!(Handle.PointerOrHandle & 1))
	{
		return false;
	}
	else if (!IsObjectHandleNull(Handle))
	{
		return !UE::RemoteObject::Handle::IsRemote(ReadObjectHandlePointerNoCheck(Handle));
	}
	return true;
#else
	return true;
#endif
}

/* checks if a handle is resolved.
 * nullptr counts as resolved
 * all handles are resolved when late resolved is off
 */
inline bool IsObjectHandleResolved_ForGC(FObjectHandle Handle)
{
#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE
	return !(Handle.PointerOrRef & 1);
#elif UE_WITH_REMOTE_OBJECT_HANDLE
	return !(Handle.PointerOrHandle & 1);
#else
	return true;
#endif
}

/* return true if a handle is type safe.
 * null and resolved handles are considered type safe
 */ 
inline bool IsObjectHandleTypeSafe(FObjectHandle Handle)
{
#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE && UE_WITH_OBJECT_HANDLE_TYPE_SAFETY
	return !((Handle.PointerOrRef & 3) == 3);
#elif UE_WITH_REMOTE_OBJECT_HANDLE
	return true;
#else
	return true;
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE
inline bool operator==(UE::CoreUObject::Private::FObjectHandlePrivate LHS, UE::CoreUObject::Private::FObjectHandlePrivate RHS)
{
	using namespace UE::CoreUObject::Private;

	bool LhsResolved = IsObjectHandleResolved(LHS);
	bool RhsResolved = IsObjectHandleResolved(RHS);

	//if both resolved or both unresolved compare the uintptr
	if (LhsResolved == RhsResolved)
	{
		return LHS.PointerOrRef == RHS.PointerOrRef;
	}

	//only one side can be resolved
	if (LhsResolved)
	{
		//both sides can't be null as resolved status would have be true for both
		const UObject* Obj = ReadObjectHandlePointerNoCheck(LHS);
		if (!Obj)
		{
			return false;
		}

		//if packed ref empty then can't be equal as RHS is an unresolved pointer
		FPackedObjectRef PackedLhs = FindExistingPackedObjectRef(Obj);
		if (PackedLhs.EncodedRef == 0)
		{
			return false;
		}
		return PackedLhs.EncodedRef == RHS.PointerOrRef;

	}
	else
	{
		//both sides can't be null as resolved status would have be true for both
		const UObject* Obj = ReadObjectHandlePointerNoCheck(RHS);
		if (!Obj)
		{
			return false;
		}

		//if packed ref empty then can't be equal as RHS is an unresolved pointer
		FPackedObjectRef PackedRhs = FindExistingPackedObjectRef(Obj);
		if (PackedRhs.EncodedRef == 0)
		{
			return false;
		}
		return PackedRhs.EncodedRef == LHS.PointerOrRef;
	}

}

inline bool operator!=(UE::CoreUObject::Private::FObjectHandlePrivate LHS, UE::CoreUObject::Private::FObjectHandlePrivate RHS)
{
	return !(LHS == RHS);
}

inline uint32 GetTypeHash(UE::CoreUObject::Private::FObjectHandlePrivate Handle)
{
	using namespace UE::CoreUObject::Private;

	if (Handle.PointerOrRef == 0)
	{
		return 0;
	}

	if (IsObjectHandleResolved(Handle))
	{
		const UObject* Obj = ReadObjectHandlePointerNoCheck(Handle);

		FPackedObjectRef PackedObjectRef = FindExistingPackedObjectRef(Obj);
		if (PackedObjectRef.EncodedRef == 0)
		{
			return GetTypeHash(Obj);
		}
		return GetTypeHash(PackedObjectRef.EncodedRef);
	}
	return GetTypeHash(Handle.PointerOrRef);
}
#elif UE_WITH_REMOTE_OBJECT_HANDLE
inline bool operator==(UE::CoreUObject::Private::FRemoteObjectHandlePrivate LHS, UE::CoreUObject::Private::FRemoteObjectHandlePrivate RHS)
{
	using namespace UE::CoreUObject::Private;
	using namespace UE::RemoteObject::Handle;

	const bool bLhsResolved = IsObjectHandleResolved_ForGC(LHS);
	const bool bRhsResolved = IsObjectHandleResolved_ForGC(RHS);

	//if both resolved or both unresolved compare the uintptr
	if (bLhsResolved == bRhsResolved)
	{
		return LHS.PointerOrHandle == RHS.PointerOrHandle;
	}

	//only one side can be resolved
	if (bLhsResolved)
	{
		//both sides can't be null as resolved status would have be true for both
		const UObject* Obj = ReadObjectHandlePointerNoCheck(LHS);
		if (!Obj)
		{
			return false;
		}

		const FRemoteObjectStub* RhsStub = RHS.ToStub();
		return RhsStub->Id == FRemoteObjectId((const UObjectBase*)Obj);
	}
	else
	{
		//both sides can't be null as resolved status would have be true for both
		const UObject* Obj = ReadObjectHandlePointerNoCheck(RHS);
		if (!Obj)
		{
			return false;
		}

		const FRemoteObjectStub* LhsStub = LHS.ToStub();
		return LhsStub->Id == FRemoteObjectId((const UObjectBase*)Obj);
	}

}

inline bool operator!=(UE::CoreUObject::Private::FRemoteObjectHandlePrivate LHS, UE::CoreUObject::Private::FRemoteObjectHandlePrivate RHS)
{
	return !(LHS == RHS);
}

inline FRemoteObjectId GetRemoteObjectId(UE::CoreUObject::Private::FRemoteObjectHandlePrivate Handle)
{
	using namespace UE::CoreUObject::Private;
	using namespace UE::RemoteObject::Handle;

	if (Handle.PointerOrHandle == 0)
	{
		return FRemoteObjectId();
	}

	if (IsObjectHandleResolved_ForGC(Handle))
	{
		const UObject* Obj = ReadObjectHandlePointerNoCheck(Handle);
		return FRemoteObjectId((const UObjectBase*)Obj);
	}
	const FRemoteObjectStub* Stub = Handle.ToStub();
	return Stub->Id;
}

/**
* GetTypeHash for FRemoteObjectHandlePrivate is guaranteed to return the same value whether a non-null handle is resolved or not (GetTypeHash of FRemoteObjectId)
*/
inline uint32 GetTypeHash(UE::CoreUObject::Private::FRemoteObjectHandlePrivate Handle)
{
	return GetTypeHash(GetRemoteObjectId(Handle));
}
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace UE::CoreUObject::Private
{
	inline FObjectHandle MakeObjectHandle(UObject* Object)
	{
#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE
		return { UPTRINT(Object) };
#elif UE_WITH_REMOTE_OBJECT_HANDLE
		return FObjectHandle(Object);
#else
		return Object;
#endif
	}

	inline UObject* ResolveObjectHandle(FObjectHandle& Handle)
	{
#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE || UE_WITH_OBJECT_HANDLE_TRACKING
		UObject* ResolvedObject = ResolveObjectHandleNoRead(Handle);
		UE::CoreUObject::Private::OnHandleRead(ResolvedObject);
		return ResolvedObject;
#elif UE_WITH_REMOTE_OBJECT_HANDLE
		UObject* ResolvedObject = ResolveObjectHandleNoRead(Handle);
		return ResolvedObject;
#else
		return ReadObjectHandlePointerNoCheck(Handle);
#endif
	}

#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE
	inline FPackedObjectRef ReadObjectHandlePackedObjectRefNoCheck(FObjectHandle Handle)
	{
		return { Handle.PointerOrRef };
	}
#endif

	inline UClass* ResolveObjectHandleClass(FObjectHandle Handle)
	{
#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE
		if (IsObjectHandleResolved(Handle))
		{
			UObject* Obj = ReadObjectHandlePointerNoCheck(Handle);
			return Obj != nullptr ? UE::CoreUObject::Private::GetClass(Obj) : nullptr;
		}
		else
		{
			// @TODO: OBJPTR: This should be cached somewhere instead of resolving on every call
			FPackedObjectRef PackedObjectRef = ReadObjectHandlePackedObjectRefNoCheck(Handle);
			FObjectRef ObjectRef = MakeObjectRef(PackedObjectRef);
			return ObjectRef.ResolveObjectRefClass();
		}
#elif UE_WITH_REMOTE_OBJECT_HANDLE
		UObject* Obj = nullptr;
		if (IsObjectHandleResolved(Handle))
		{
			Obj = ReadObjectHandlePointerNoCheck(Handle);
		}
		else
		{
			Obj = ResolveObjectHandle(Handle);
		}
		return Obj != nullptr ? UE::CoreUObject::Private::GetClass(Obj) : nullptr;
#else
		UObject* Obj = ReadObjectHandlePointerNoCheck(Handle);
		return Obj != nullptr ? UE::CoreUObject::Private::GetClass(Obj) : nullptr;
#endif
	}

	inline UObject* ResolveObjectHandleNoRead(FObjectHandle& Handle)
	{
#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE
		FObjectHandle LocalHandle = Handle;
		if (IsObjectHandleResolved(LocalHandle))
		{
			UObject* ResolvedObject = ReadObjectHandlePointerNoCheck(LocalHandle);
			return ResolvedObject;
		}
		else
		{
			FPackedObjectRef PackedObjectRef = ReadObjectHandlePackedObjectRefNoCheck(LocalHandle);
			FObjectRef ObjectRef = MakeObjectRef(PackedObjectRef);
			UObject* ResolvedObject = ObjectRef.Resolve();
#if UE_WITH_OBJECT_HANDLE_TYPE_SAFETY
			if (IsObjectHandleTypeSafe(LocalHandle))
#endif
			{
				Handle = MakeObjectHandle(ResolvedObject);
			}
			return ResolvedObject;
		}
#elif UE_WITH_REMOTE_OBJECT_HANDLE
		FObjectHandle LocalHandle = Handle;
		UObject* ResolvedObject = nullptr;
		if (!!(LocalHandle.PointerOrHandle & 1))
		{
			ResolvedObject = UE::RemoteObject::Handle::ResolveObject(LocalHandle.ToStub());
			
		}
		else if (!IsObjectHandleNull(Handle) && UE::RemoteObject::Handle::IsRemote(ReadObjectHandlePointerNoCheck(LocalHandle)))
		{
			ResolvedObject = UE::RemoteObject::Handle::ResolveObject(ReadObjectHandlePointerNoCheck(LocalHandle));
		}
		else
		{
			ResolvedObject = ReadObjectHandlePointerNoCheck(LocalHandle);
			UE::RemoteObject::Handle::TouchResidentObject(ResolvedObject);
			return ResolvedObject;
		}
		Handle = MakeObjectHandle(ResolvedObject);
		return ResolvedObject;
#else
		return ReadObjectHandlePointerNoCheck(Handle);
#endif
	}


	inline UObject* NoResolveObjectHandleNoRead(const FObjectHandle& Handle)
	{
		FObjectHandle LocalHandle = Handle;
		if (IsObjectHandleResolved_ForGC(LocalHandle))
		{
			UObject* ResolvedObject = ReadObjectHandlePointerNoCheck(LocalHandle);
			return ResolvedObject;
		}
		else
		{
			return nullptr;
		}
	}

	inline UObject* ResolveObjectHandleNoReadNoCheck(FObjectHandle& Handle)
	{
#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE
		FObjectHandle LocalHandle = Handle;
		FPackedObjectRef PackedObjectRef = ReadObjectHandlePackedObjectRefNoCheck(LocalHandle);
		FObjectRef ObjectRef = MakeObjectRef(PackedObjectRef);
		UObject* ResolvedObject = ObjectRef.Resolve();
#if UE_WITH_OBJECT_HANDLE_TYPE_SAFETY
		if (IsObjectHandleTypeSafe(LocalHandle))
#endif
		{
			LocalHandle = MakeObjectHandle(ResolvedObject);
			Handle = LocalHandle;
		}
		return ResolvedObject;
#elif UE_WITH_REMOTE_OBJECT_HANDLE
		FObjectHandle LocalHandle = Handle;
		// Unresolved handle may mean two things: we still have the remote object memory (it hasn't been GC'd yet) or we only have a stub
		UObject* ResolvedObject = nullptr;
		if (IsObjectHandleResolved_ForGC(LocalHandle))
		{
			ResolvedObject = UE::RemoteObject::Handle::ResolveObject(ReadObjectHandlePointerNoCheck(LocalHandle));
		}
		else
		{
			ResolvedObject = UE::RemoteObject::Handle::ResolveObject(LocalHandle.ToStub());
		}
		LocalHandle = MakeObjectHandle(ResolvedObject);
		Handle = LocalHandle;
		return ResolvedObject;	
#else
		return ReadObjectHandlePointerNoCheck(Handle);
#endif
	}

	inline UObject* ReadObjectHandlePointerNoCheck(FObjectHandle Handle)
	{
#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE
		return reinterpret_cast<UObject*>(Handle.PointerOrRef);
#elif UE_WITH_REMOTE_OBJECT_HANDLE
		return reinterpret_cast<UObject*>(Handle.PointerOrHandle);
#else
		return Handle;
#endif
	}

	//Natvis structs
	struct FObjectHandlePackageDebugData
	{
		FMinimalName PackageName;
		FScriptArray ObjectDescriptors;
		uint8 _Padding[sizeof(FRWLock)];
	};

	struct FObjectHandleDataClassDescriptor
	{
		FMinimalName PackageName;
		FMinimalName ClassName;
	};

	struct FObjectPathIdDebug
	{
		uint32 Index = 0;
		uint32 Number = 0;

		static constexpr uint32 WeakObjectMask = ~((~0u) >> 1);       //most significant bit
		static constexpr uint32 SimpleNameMask = WeakObjectMask >> 1; //second most significant bits
	};

	struct FObjectDescriptorDebug
	{
		FObjectPathIdDebug ObjectPath;
		FObjectHandleDataClassDescriptor ClassDescriptor;
	};

	struct FStoredObjectPathDebug
	{
		static constexpr const int32 NumInlineElements = 3;
		int32 NumElements;

		union
		{
			FMinimalName Short[NumInlineElements];
			FMinimalName* Long;
		};
	};

	inline constexpr uint32 TypeIdShift = 1;
	inline constexpr uint32 ObjectIdShift = 2;
	inline constexpr uint32 PackageIdShift = 34;
	inline constexpr uint32 PackageIdMask = 0x3FFF'FFFF;

#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE
	//forward declarations
	void InitObjectHandles(int32 Size);
	void FreeObjectHandle(const UObjectBase* Object);
	void UpdateRenamedObject(const UObject* Obj, FName NewName, UObject* NewOuter);
	UE::CoreUObject::Private::FPackedObjectRef MakePackedObjectRef(const UObject* Object);
#endif
}
