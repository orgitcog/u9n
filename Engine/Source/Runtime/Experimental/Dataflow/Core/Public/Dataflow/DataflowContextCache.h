// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "StructUtils/InstancedStruct.h"
#include "StructUtils/StructArrayView.h"
#include "Templates/Decay.h"
#include "Templates/UniquePtr.h"
#include "UObject/UnrealType.h"

namespace UE::Dataflow
{
	class FContext;
	struct FContextCacheElementBase;
	struct FContextCacheElementNull;

	typedef uint32 FContextCacheKey;

	/** Trait used to select the UObject* or TObjectPtr cache element path code. */
	template <typename T>
	struct TIsUObjectPtrElement
	{
		typedef typename TDecay<T>::Type Type;
		static constexpr bool Value = (std::is_pointer_v<Type> && std::is_convertible_v<Type, const UObjectBase*>) || TIsTObjectPtr_V<Type>;
	};

	/** Trait used to select the UStruct cache element path code. */
	template <typename T, typename = void, typename = void>
	struct TIsReflectedStruct { static constexpr bool Value = false; };
	template <typename T>
	struct TIsReflectedStruct<T, std::void_t<decltype(&TBaseStructure<typename TDecay<T>::Type>::Get)>, std::void_t<decltype(&T::StaticStruct)>> { static constexpr bool Value = true; };

	struct FTimestamp
	{
		typedef uint64 Type;
		Type Value = Type(0);

		FTimestamp(Type InValue) : Value(InValue) {}
		bool operator>=(const FTimestamp& InTimestamp) const { return Value >= InTimestamp.Value; }
		bool operator<(const FTimestamp& InTimestamp) const { return Value < InTimestamp.Value; }
		bool operator==(const FTimestamp& InTimestamp) const { return Value == InTimestamp.Value; }
		bool IsInvalid() { return Value == Invalid; }
		bool IsInvalid() const { return Value == Invalid; }

		static DATAFLOWCORE_API Type Current();
		static DATAFLOWCORE_API Type Invalid; // 0
	};

	struct IContextCacheStore
	{
		virtual const TUniquePtr<FContextCacheElementBase>* FindCacheElement(FContextCacheKey Key) const = 0;
		virtual bool HasCacheElement(FContextCacheKey Key, FTimestamp InTimestamp = FTimestamp::Invalid) const = 0;
	};

	//--------------------------------------------------------------------
	// base class for all context cache entries 
	//--------------------------------------------------------------------
	struct FContextCacheElementBase 
	{
		enum EType
		{
			CacheElementTyped,
			CacheElementReference,
			CacheElementNull,
			CacheElementUObject, // UObjectPtr
			CacheElementUObjectArray, // TArray of UObjectPtr
			CacheElementUStruct,
			CacheElementUStructArray
		};

		FContextCacheElementBase(EType CacheElementType, FGuid InNodeGuid = FGuid(), const FProperty* InProperty = nullptr, uint32 InNodeHash = 0, FTimestamp InTimestamp = FTimestamp::Invalid)
			: Type(CacheElementType)
			, NodeGuid(InNodeGuid)
			, Property(InProperty)
			, NodeHash(InNodeHash)
			, Timestamp(InTimestamp)
		{}
		virtual ~FContextCacheElementBase() {}

		// InReferenceDataKey is the key of the cache element this function is called on 
		inline TUniquePtr<FContextCacheElementBase> CreateReference(FContextCacheKey InReferenceDataKey) const;

		// clone the cache entry
		virtual TUniquePtr<FContextCacheElementBase> Clone(const IContextCacheStore& Context) const = 0;

		template<typename T>
		inline const T& GetTypedData(const IContextCacheStore& Context, const FProperty* PropertyIn, const T& Default) const;

		virtual const void* GetUntypedData(const IContextCacheStore& Context, const FProperty* PropertyIn) const = 0;

		virtual int32 GetNumArrayElements(const IContextCacheStore& Context) const = 0;
		virtual TUniquePtr<FContextCacheElementBase> CreateFromArrayElement(const IContextCacheStore& Context, int32 Index, const FProperty* InProperty, const FGuid& InNodeGuid, uint32 InNodeHash, const FTimestamp& InTimestamp) const = 0;

		EType GetType() const {	return Type; }

		const FProperty* GetProperty() const { return Property; }
		const FTimestamp& GetTimestamp() const { return Timestamp; }
		void SetTimestamp(const FTimestamp& InTimestamp) { Timestamp = InTimestamp; }

		const FGuid& GetNodeGuid() const { return NodeGuid; }
		const uint32 GetNodeHash() const { return NodeHash; }

		// use this with caution: setting the property of a wrong type may cause problems
		void SetProperty(const FProperty* NewProperty) { Property = NewProperty; }

		// use this with caution: setting the property of a wrong type may cause problems
		void UpdatePropertyAndNodeData(const FProperty* InProperty, const FGuid& InNodeGuid, uint32 InNodeHash, const FTimestamp& InTimestamp)
		{ 
			Property = InProperty;
			NodeGuid = InNodeGuid;
			NodeHash = InNodeHash;
			Timestamp = InTimestamp;
		}

	private:
		friend struct FContextCache;

		EType Type;
		FGuid NodeGuid;
		const FProperty* Property = nullptr;
		uint32 NodeHash = 0;
		FTimestamp Timestamp = FTimestamp::Invalid;
	};

	//--------------------------------------------------------------------
	// Value storing context cache entry - strongly typed
	//--------------------------------------------------------------------
	template<class T>
	struct TContextCacheElement : public FContextCacheElementBase 
	{
		TContextCacheElement(FGuid InNodeGuid, const FProperty* InProperty, T&& InData, uint32 InNodeHash, FTimestamp Timestamp)
			: FContextCacheElementBase(EType::CacheElementTyped, InNodeGuid, InProperty, InNodeHash, Timestamp)
			, Data(Forward<T>(InData))
		{}

		TContextCacheElement(const TContextCacheElement<T>& Other)
			: FContextCacheElementBase(EType::CacheElementTyped, Other.GetNodeGuid(), Other.GetProperty(), Other.GetNodeHash(), Other.GetTimestamp())
			, Data(Other.Data)
		{}
		
		inline const T& GetData(const IContextCacheStore& Context, const FProperty* PropertyIn, const T& Default) const;

		virtual const void* GetUntypedData(const IContextCacheStore& Context, const FProperty* PropertyIn) const
		{
			return &Data;
		}

		virtual int32 GetNumArrayElements(const IContextCacheStore& Context) const override
		{
			if constexpr (TIsTArray<FDataType>::Value)
			{
				return Data.Num();
			}
			else
			{
				return 0;
			}
		}

		virtual TUniquePtr<FContextCacheElementBase> CreateFromArrayElement(const IContextCacheStore& Context, int32 Index, const FProperty* InProperty, const FGuid& InNodeGuid, uint32 InNodeHash, const FTimestamp& InTimestamp) const override
		{
			if constexpr (TIsTArray<FDataType>::Value)
			{
				if (Data.IsValidIndex(Index))
				{
					return MakeUnique<TContextCacheElement<decltype(Data[Index])>>(InNodeGuid, InProperty, Data[Index], InNodeHash, InTimestamp);
				}
				return {};
			}
			else
			{
				return {};
			}
		}

		const T& GetDataDirect() const { return Data; }

		inline virtual TUniquePtr<FContextCacheElementBase> Clone(const IContextCacheStore& Context) const override;

	private:
		typedef typename TDecay<T>::Type FDataType;  // Using universal references here means T could be either const& or an rvalue reference
		const FDataType Data;                        // Decaying T removes any reference and gets the correct underlying storage data type
	};

	//--------------------------------------------------------------------
	// Reference to another context cache entry 
	//--------------------------------------------------------------------
	struct FContextCacheElementReference : public FContextCacheElementBase
	{
		FContextCacheElementReference(FGuid InNodeGuid, const FProperty* InProperty, FContextCacheKey InDataKey, uint32 InNodeHash, FTimestamp Timestamp)
			: FContextCacheElementBase(EType::CacheElementReference, InNodeGuid, InProperty, InNodeHash, Timestamp)
			, DataKey(InDataKey)
		{}

		template<class T>
		inline const T& GetData(const IContextCacheStore& Context, const FProperty* PropertyIn, const T& Default) const;

		inline virtual const void* GetUntypedData(const IContextCacheStore& Context, const FProperty* PropertyIn) const override;

		inline virtual int32 GetNumArrayElements(const IContextCacheStore& Context) const override;

		inline virtual TUniquePtr<FContextCacheElementBase> CreateFromArrayElement(const IContextCacheStore& Context, int32 Index, const FProperty* InProperty, const FGuid& InNodeGuid, uint32 InNodeHash, const FTimestamp& InTimestamp) const override;

		inline virtual TUniquePtr<FContextCacheElementBase> Clone(const IContextCacheStore& Context) const override;

	private:
		const FContextCacheKey DataKey; // this is a key to another cache element
	};

	//--------------------------------------------------------------------
	// Null entry, this will always return a default value 
	//--------------------------------------------------------------------
	struct FContextCacheElementNull : public FContextCacheElementBase
	{
		//
		// IMPORTANT: 
		// Timestamp must be set to (Timestamp.Value - 1) to make sure that this type of entry is always invalid
		//
		UE_DEPRECATED(5.6, "Use the other constructor that does not pass a DataKey (the key is not needed)")
		FContextCacheElementNull(FGuid InNodeGuid, const FProperty* InProperty, FContextCacheKey InDataKey, uint32 InNodeHash, FTimestamp Timestamp)
			: FContextCacheElementBase(EType::CacheElementNull, InNodeGuid, InProperty, InNodeHash, FTimestamp((Timestamp.Value == 0)? 0: (Timestamp.Value - 1)))
		{}

		FContextCacheElementNull(FGuid InNodeGuid, const FProperty* InProperty, uint32 InNodeHash, FTimestamp Timestamp)
			: FContextCacheElementBase(EType::CacheElementNull, InNodeGuid, InProperty, InNodeHash, FTimestamp((Timestamp.Value == 0) ? 0 : (Timestamp.Value - 1)))
		{}

		virtual const void* GetUntypedData(const IContextCacheStore& Context, const FProperty* PropertyIn) const override
		{
			return nullptr;
		}

		virtual int32 GetNumArrayElements(const IContextCacheStore& Context) const override
		{
			return 0;
		}

		virtual TUniquePtr<FContextCacheElementBase> CreateFromArrayElement(const IContextCacheStore& Context, int32 Index, const FProperty* InProperty, const FGuid& InNodeGuid, uint32 InNodeHash, const FTimestamp& InTimestamp) const override
		{
			return {};
		}

		inline virtual TUniquePtr<FContextCacheElementBase> Clone(const IContextCacheStore& Context) const override;
	};

	//--------------------------------------------------------------------
	// UObject cache element, prevents the object from being garbage collected while in the cache
	//--------------------------------------------------------------------
	template<class T>
	struct TContextCacheElementUObject : public FContextCacheElementBase, public FGCObject
	{
		TContextCacheElementUObject(FGuid InNodeGuid, const FProperty* InProperty, T&& InObjectPtr, uint32 InNodeHash, FTimestamp Timestamp)
			: FContextCacheElementBase(EType::CacheElementUObject, InNodeGuid, InProperty, InNodeHash, Timestamp)
			, ObjectPtr(InObjectPtr)
		{}

		TContextCacheElementUObject(const TContextCacheElementUObject<T>& Other)
			: FContextCacheElementBase(EType::CacheElementUObject, Other.GetNodeGuid(), Other.GetProperty(), Other.GetNodeHash(), Other.GetTimestamp())
			, ObjectPtr(Other.ObjectPtr)
		{}

		const T& GetData(const IContextCacheStore& /*Context*/, const FProperty* /*PropertyIn*/, const T& /*Default*/) const
		{
			return ObjectPtr;
		}

		virtual const void* GetUntypedData(const IContextCacheStore& /*Context*/, const FProperty* /*PropertyIn*/) const override
		{
			return (void*)ObjectPtr;
		}

		virtual int32 GetNumArrayElements(const IContextCacheStore& Context) const override
		{
			return 0;
		}

		virtual TUniquePtr<FContextCacheElementBase> CreateFromArrayElement(const IContextCacheStore& Context, int32 Index, const FProperty* InProperty, const FGuid& InNodeGuid, uint32 InNodeHash, const FTimestamp& InTimestamp) const override
		{
			return {};
		}

		inline virtual TUniquePtr<FContextCacheElementBase> Clone(const IContextCacheStore& Context) const override;

		//~ Begin FGCObject interface
		virtual void AddReferencedObjects(FReferenceCollector& Collector) override { Collector.AddReferencedObject(ObjectPtr); }
		virtual FString GetReferencerName() const override { return TEXT("TContextCacheElementUObject"); }
		//~ End FGCObject interface

	private:
		typedef typename TDecay<T>::Type FDataType;  // Using universal references here means T could be either const& or an rvalue reference
		FDataType ObjectPtr;                         // Decaying T removes any reference and gets the correct underlying storage data type
	};

	//--------------------------------------------------------------------
	// TArray<UObjectPtr> cache element, prevents the object from being garbage collected while in the cache
	//--------------------------------------------------------------------
	template<class T>
	struct TContextCacheElementUObjectArray : public FContextCacheElementBase, public FGCObject
	{
		template<typename ArrayT>
		TContextCacheElementUObjectArray(FGuid InNodeGuid, const FProperty* InProperty, ArrayT&& InObjectPtrArray, uint32 InNodeHash, FTimestamp Timestamp)
			: FContextCacheElementBase(EType::CacheElementUObjectArray, InNodeGuid, InProperty, InNodeHash, Timestamp)
			, ObjectPtrArray(Forward<ArrayT>(InObjectPtrArray))
		{}


		TContextCacheElementUObjectArray(const TContextCacheElementUObjectArray<T>& Other)
			: FContextCacheElementBase(EType::CacheElementUObjectArray, Other.GetNodeGuid(), Other.GetProperty(), Other.GetNodeHash(), Other.GetTimestamp())
			, ObjectPtrArray(Other.ObjectPtrArray)
		{}

		const TArray<T>& GetData(const IContextCacheStore& /*Context*/, const FProperty* /*PropertyIn*/, const TArray<T>& /*Default*/) const
		{
			return ObjectPtrArray;
		}

		virtual const void* GetUntypedData(const IContextCacheStore& /*Context*/, const FProperty* /*PropertyIn*/) const override
		{
			return &ObjectPtrArray;
		}

		virtual int32 GetNumArrayElements(const IContextCacheStore& Context) const override
		{
			return ObjectPtrArray.Num();
		}

		virtual TUniquePtr<FContextCacheElementBase> CreateFromArrayElement(const IContextCacheStore& Context, int32 Index, const FProperty* InProperty, const FGuid& InNodeGuid, uint32 InNodeHash, const FTimestamp& InTimestamp) const override
		{
			if (ObjectPtrArray.IsValidIndex(Index))
			{
				FDataType Element = ObjectPtrArray[Index];
				return MakeUnique<TContextCacheElementUObject<FDataType>>(InNodeGuid, InProperty, MoveTemp(Element), InNodeHash, InTimestamp);
			}
			return {};
		}

		inline virtual TUniquePtr<FContextCacheElementBase> Clone(const IContextCacheStore& Context) const override;

		//~ Begin FGCObject interface
		virtual void AddReferencedObjects(FReferenceCollector& Collector) override { Collector.AddReferencedObjects(ObjectPtrArray); }
		virtual FString GetReferencerName() const override { return TEXT("TContextCacheElementUObjectArray"); }
		//~ End FGCObject interface

	private:
		typedef typename TDecay<T>::Type FDataType;     // Using universal references here means T could be either const& or an rvalue reference
		TArray<FDataType> ObjectPtrArray;               // Decaying T removes any reference and gets the correct underlying storage data type
	};

	//--------------------------------------------------------------------
	// UStruct cache element
	//--------------------------------------------------------------------
	struct FContextCacheElementUStruct : public FContextCacheElementBase
	{
		FContextCacheElementUStruct(FGuid InNodeGuid, const FProperty* InProperty, const FConstStructView& StructView, uint32 InNodeHash, FTimestamp Timestamp)
			: FContextCacheElementBase(EType::CacheElementUStruct, InNodeGuid, InProperty, InNodeHash, Timestamp)
			, InstancedStruct(StructView)
		{}

		template<typename T>
		FContextCacheElementUStruct(FGuid InNodeGuid, const FProperty* InProperty, T&& InStruct, uint32 InNodeHash, FTimestamp Timestamp)
			: FContextCacheElementBase(EType::CacheElementUStruct, InNodeGuid, InProperty, InNodeHash, Timestamp)
		{
			InstancedStruct.InitializeAs<typename TDecay<T>::Type>(Forward<T>(InStruct));
		}

		FContextCacheElementUStruct(const FContextCacheElementUStruct& Other)
			: FContextCacheElementBase(EType::CacheElementUStruct, Other.GetNodeGuid(), Other.GetProperty(), Other.GetNodeHash(), Other.GetTimestamp())
			, InstancedStruct(Other.InstancedStruct)
		{}

		template<typename T>
		const T& GetData(const IContextCacheStore& /*Context*/, const FProperty* /*PropertyIn*/, const T& /*Default*/) const
		{
			return InstancedStruct.Get<T>();
		}

		virtual const void* GetUntypedData(const IContextCacheStore& /*Context*/, const FProperty* /*PropertyIn*/) const override
		{
			return InstancedStruct.GetMemory();
		}

		virtual int32 GetNumArrayElements(const IContextCacheStore& Context) const override
		{
			return 0;
		}

		virtual TUniquePtr<FContextCacheElementBase> CreateFromArrayElement(const IContextCacheStore& Context, int32 Index, const FProperty* InProperty, const FGuid& InNodeGuid, uint32 InNodeHash, const FTimestamp& InTimestamp) const override
		{
			return {};
		}

		inline virtual TUniquePtr<FContextCacheElementBase> Clone(const IContextCacheStore& Context) const override
		{
			return MakeUnique<FContextCacheElementUStruct>(*this);
		}

	private:
		FInstancedStruct InstancedStruct;
	};

	//--------------------------------------------------------------------
	// UStruct array cache element
	//--------------------------------------------------------------------
	struct FContextCacheElementUStructArray : public FContextCacheElementBase
	{
		FContextCacheElementUStructArray(FGuid InNodeGuid, const FProperty* InProperty, const FConstStructArrayView& InStructArrayView, uint32 InNodeHash, FTimestamp Timestamp)
			: FContextCacheElementBase(EType::CacheElementUStructArray, InNodeGuid, InProperty, InNodeHash, Timestamp)
			, InstancedStructArray(InStructArrayView)
		{}

		template<typename T>
		FContextCacheElementUStructArray(FGuid InNodeGuid, const FProperty* InProperty, TArray<T>&& InStructArray, uint32 InNodeHash, FTimestamp Timestamp)
			: FContextCacheElementBase(EType::CacheElementUStructArray, InNodeGuid, InProperty, InNodeHash, Timestamp)
			, InstancedStructArray(TBaseStructure<T>::Get())
		{
			InstancedStructArray.Get<T>() = Forward<TArray<T>>(InStructArray);
		}

		FContextCacheElementUStructArray(const FContextCacheElementUStructArray& Other)
			: FContextCacheElementUStructArray(Other.GetNodeGuid(), Other.GetProperty(), Other.GetStructArrayView(), Other.GetNodeHash(), Other.GetTimestamp())
		{}
	
		virtual ~FContextCacheElementUStructArray() override = default;

		template<typename T>
		const TArray<T>& GetData(const IContextCacheStore& /*Context*/, const FProperty* /*PropertyIn*/, const TArray<T>& /*Default*/) const
		{
			return InstancedStructArray.Get<T>();
		}

		virtual const void* GetUntypedData(const IContextCacheStore& /*Context*/, const FProperty* /*PropertyIn*/) const override
		{
			return InstancedStructArray.GetMemory();
		}

		virtual int32 GetNumArrayElements(const IContextCacheStore& Context) const override
		{
			return InstancedStructArray.Num();
		}

		virtual TUniquePtr<FContextCacheElementBase> CreateFromArrayElement(const IContextCacheStore& Context, int32 Index, const FProperty* InProperty, const FGuid& InNodeGuid, uint32 InNodeHash, const FTimestamp& InTimestamp) const override
		{
			if (InstancedStructArray.IsValidIndex(Index))
			{
				//return MakeUnique<FContextCacheElementUStruct>(GetNodeGuid(), GetProperty(), Forward<T>(ObjectPtrArray[Index]), GetNodeHash(), GetTimestamp());
				// todo : support extracting structure from array of structures
				return {};
			}
			return {};
		}

		inline virtual TUniquePtr<FContextCacheElementBase> Clone(const IContextCacheStore& Context) const override
		{
			return MakeUnique<FContextCacheElementUStructArray>(*this);
		}

	private:
		FConstStructArrayView GetStructArrayView() const
		{
			return InstancedStructArray.GetScriptStruct() ?
				FConstStructArrayView(*InstancedStructArray.GetScriptStruct(), InstancedStructArray.GetData(), InstancedStructArray.Num()) :
				FConstStructArrayView();
		}

		// Implements a FInstancedStruct for arrays (FInstancedStructContainer cannot be cast to a TArray)
		struct FInstancedStructArray final : public TArray<uint8>
		{
			explicit FInstancedStructArray(const UScriptStruct* const InScriptStruct)
				: ScriptStruct(InScriptStruct)
			{}
			explicit FInstancedStructArray(const FConstStructArrayView& StructArrayView)
				: ScriptStruct(StructArrayView.GetScriptStruct())
			{
				if (ScriptStruct)  // Null if the array has 0 element
				{
					SetNumUnsafeInternal(StructArrayView.Num());
					ArrayMax = Num();
					GetAllocatorInstance().ResizeAllocation(0, Num(), FMath::Max(1, ScriptStruct->GetStructureSize()), ScriptStruct->GetMinAlignment());
					ScriptStruct->InitializeStruct(GetData(), Num());
					ScriptStruct->CopyScriptStruct(GetData(), StructArrayView.GetData(), Num());
				}
			}
			~FInstancedStructArray()
			{
				if (ScriptStruct)
				{
					ScriptStruct->DestroyStruct(GetData(), Num());
					GetAllocatorInstance().ResizeAllocation(ArrayMax, 0, FMath::Max(1, ScriptStruct->GetStructureSize()), ScriptStruct->GetMinAlignment());
				}
			}
			const UScriptStruct* GetScriptStruct() const
			{
				return ScriptStruct;
			}
			const void* GetMemory() const
			{
				return this;
			}
			template<typename T>
			const TArray<T>& Get() const
			{
				check(!ScriptStruct || TBaseStructure<T>::Get() == ScriptStruct);
				return reinterpret_cast<const TArray<T>&>(*this);
			}
			template<typename T>
			TArray<T>& Get()
			{
				check(!ScriptStruct || TBaseStructure<T>::Get() == ScriptStruct);
				return reinterpret_cast<TArray<T>&>(*this);
			}
		private:
			const UScriptStruct* const ScriptStruct;
		};

		FInstancedStructArray InstancedStructArray;
	};

	// cache element method implementation 
	template<class T>
	const T& FContextCacheElementBase::GetTypedData(const IContextCacheStore& Context, const FProperty* PropertyIn, const T& Default) const
	{
		// check(PropertyIn->IsA<T>()); // @todo(dataflow) compile error for non-class T; find alternatives
		if (Type == EType::CacheElementTyped)
		{
			if constexpr (!TIsUObjectPtrElement<T>::Value)
			{
				return static_cast<const TContextCacheElement<T>&>(*this).GetData(Context, PropertyIn, Default);
			}
		}
		if (Type == EType::CacheElementReference)
		{
			return static_cast<const FContextCacheElementReference&>(*this).GetData(Context, PropertyIn, Default);
		}
		if (Type == EType::CacheElementNull)
		{
			return Default; 
		}
		if (Type == EType::CacheElementUObjectArray)
		{
			if constexpr (TIsTArray<T>::Value)
			{
				if constexpr (TIsUObjectPtrElement<typename T::ElementType>::Value)
				{
					return static_cast<const TContextCacheElementUObjectArray<typename T::ElementType>&>(*this).GetData(Context, PropertyIn, Default);
				}
			}
		}
		if (Type == EType::CacheElementUObject)
		{
			if constexpr (TIsUObjectPtrElement<T>::Value)
			{
				return static_cast<const TContextCacheElementUObject<T>&>(*this).GetData(Context, PropertyIn, Default);
			}
		}
		if (Type == EType::CacheElementUStructArray)
		{
			if constexpr (TIsTArray<T>::Value)
			{
				if constexpr (TIsReflectedStruct<typename T::ElementType>::Value)
				{
					return static_cast<const FContextCacheElementUStructArray&>(*this).GetData<typename T::ElementType>(Context, PropertyIn, Default);
				}
			}
		}
		if (Type == EType::CacheElementUStruct)
		{
			if constexpr (TIsReflectedStruct<T>::Value)
			{
				return static_cast<const FContextCacheElementUStruct&>(*this).GetData<T>(Context, PropertyIn, Default);
			}
		}
		check(false); // should never happen
		return Default;
	}

	struct FContextCache : public TMap<FContextCacheKey, TUniquePtr<FContextCacheElementBase>>
	{
		DATAFLOWCORE_API void Serialize(FArchive& Ar);
	};

	// cache classes implemetation 
	// this needs to be after the FContext definition because they access its methods

	TUniquePtr<FContextCacheElementBase> FContextCacheElementBase::CreateReference(FContextCacheKey InReferenceDataKey) const
	{
		return MakeUnique<FContextCacheElementReference>(GetNodeGuid(), GetProperty(), InReferenceDataKey, GetNodeHash(), GetTimestamp());
	}

	template<class T>
	const T& TContextCacheElement<T>::GetData(const IContextCacheStore& Context, const FProperty* PropertyIn, const T& Default) const
	{
		return Data;
	}

	template<class T>
	TUniquePtr<FContextCacheElementBase> TContextCacheElement<T>::Clone(const IContextCacheStore& Context) const
	{
		return MakeUnique<TContextCacheElement<T>>(*this);
	}

	template<class T>
	const T& FContextCacheElementReference::GetData(const IContextCacheStore& Context, const FProperty* InProperty, const T& Default) const
	{
		if (const TUniquePtr<FContextCacheElementBase>* Cache = Context.FindCacheElement(DataKey))
		{
			return (*Cache)->GetTypedData<T>(Context, InProperty, Default);
		}
		return Default;
	}

	const void* FContextCacheElementReference::GetUntypedData(const IContextCacheStore& Context, const FProperty* InProperty) const
	{
		if (const TUniquePtr<FContextCacheElementBase>* Cache = Context.FindCacheElement(DataKey))
		{
			return (*Cache)->GetUntypedData(Context, InProperty);
		}
		return nullptr;
	}

	int32 FContextCacheElementReference::GetNumArrayElements(const IContextCacheStore& Context) const
	{
		if (const TUniquePtr<FContextCacheElementBase>* Cache = Context.FindCacheElement(DataKey))
		{
			return (*Cache)->GetNumArrayElements(Context);
		}
		return 0;
	}

	TUniquePtr<FContextCacheElementBase> FContextCacheElementReference::CreateFromArrayElement(const IContextCacheStore& Context, int32 Index, const FProperty* InProperty, const FGuid& InNodeGuid, uint32 InNodeHash, const FTimestamp& InTimestamp) const
	{
		if (const TUniquePtr<FContextCacheElementBase>* Cache = Context.FindCacheElement(DataKey))
		{
			return (*Cache)->CreateFromArrayElement(Context, Index, InProperty, InNodeGuid, InNodeHash, InTimestamp);
		}
		return 0;
	}

	TUniquePtr<FContextCacheElementBase> FContextCacheElementReference::Clone(const IContextCacheStore& Context) const
	{
		if (const TUniquePtr<FContextCacheElementBase>* ReferencedCacheEntry = Context.FindCacheElement(DataKey))
		{
			if (ReferencedCacheEntry && ReferencedCacheEntry->IsValid())
			{
				return (*ReferencedCacheEntry)->Clone(Context);
			}
		}
		return MakeUnique<FContextCacheElementNull>(GetNodeGuid(), GetProperty(), GetNodeHash(), GetTimestamp());
	}

	TUniquePtr<FContextCacheElementBase> FContextCacheElementNull::Clone(const IContextCacheStore& Context) const
	{
		return MakeUnique<FContextCacheElementNull>(GetNodeGuid(), GetProperty(), GetNodeHash(), GetTimestamp());
	}

	template<class T>
	TUniquePtr<FContextCacheElementBase> TContextCacheElementUObject<T>::Clone(const IContextCacheStore& Context) const
	{
		return MakeUnique<TContextCacheElementUObject<T>>(*this);
	}

	template<class T>
	TUniquePtr<FContextCacheElementBase> TContextCacheElementUObjectArray<T>::Clone(const IContextCacheStore& Context) const
	{
		return MakeUnique<TContextCacheElementUObjectArray<T>>(*this);
	}
};

inline FArchive& operator<<(FArchive& Ar, UE::Dataflow::FTimestamp& ValueIn)
{
	Ar << ValueIn.Value;
	Ar << ValueIn.Invalid;
	return Ar;
}

inline FArchive& operator<<(FArchive& Ar, UE::Dataflow::FContextCache& ValueIn)
{
	ValueIn.Serialize(Ar);
	return Ar;
}


