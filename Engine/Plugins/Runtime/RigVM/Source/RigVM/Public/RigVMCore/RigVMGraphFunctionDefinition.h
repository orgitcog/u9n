// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMObjectVersion.h"
#include "RigVMCore/RigVMExternalVariable.h"
#include "RigVMCore/RigVMByteCode.h"
#include "RigVMCore/RigVMObjectArchive.h"
#include "UObject/UE5MainStreamObjectVersion.h"
#include "UObject/FortniteMainBranchObjectVersion.h"
#include "UObject/UE5ReleaseStreamObjectVersion.h"
#include "RigVMVariant.h"
#include "RigVMNodeLayout.h"
#include "RigVMStringUtils.h"
#include "RigVMGraphFunctionDefinition.generated.h"

#define UE_API RIGVM_API

class IRigVMGraphFunctionHost;
struct FRigVMGraphFunctionData;

USTRUCT()
struct FRigVMFunctionCompilationPropertyDescription
{
	GENERATED_BODY()

	// The name of the property to create
	UPROPERTY()
	FName Name;

	// The complete CPP type to base a new property off of (for ex: 'TArray<TArray<FVector>>')
	UPROPERTY()
	FString CPPType;

	// The tail CPP Type object, for example the UScriptStruct for a struct
	UPROPERTY()
	TSoftObjectPtr<UObject> CPPTypeObject;

	// The default value to use for this property (for example: '(((X=1.000000, Y=2.000000, Z=3.000000)))')
	UPROPERTY()
	FString DefaultValue;

	friend uint32 GetTypeHash(const FRigVMFunctionCompilationPropertyDescription& Description) 
	{
		uint32 Hash = GetTypeHash(Description.Name.ToString());
		Hash = HashCombine(Hash, GetTypeHash(Description.CPPType));
		// we can't hash based on the pointer since that's not deterministic across sessions
		// Hash = HashCombine(Hash, GetTypeHash(Description.CPPTypeObject));
		Hash = HashCombine(Hash, GetTypeHash(Description.DefaultValue));
		return Hash;
	}

	friend FArchive& operator<<(FArchive& Ar, FRigVMFunctionCompilationPropertyDescription& Data)
	{
		Ar << Data.Name;
		Ar << Data.CPPType;
		Ar << Data.CPPTypeObject;
		Ar << Data.DefaultValue;
		return Ar;
	}

	UE_API FRigVMPropertyDescription ToPropertyDescription() const;
	static UE_API TArray<FRigVMPropertyDescription> ToPropertyDescription(const TArray<FRigVMFunctionCompilationPropertyDescription>& InDescriptions);
};

USTRUCT()
struct FRigVMFunctionCompilationPropertyPath
{
	GENERATED_BODY()

	UPROPERTY()
	int32 PropertyIndex = INDEX_NONE;

	UPROPERTY()
	FString HeadCPPType;

	UPROPERTY()
	FString SegmentPath;

	friend uint32 GetTypeHash(const FRigVMFunctionCompilationPropertyPath& Path)
	{
		uint32 Hash = GetTypeHash(Path.PropertyIndex);
		Hash = HashCombine(Hash, GetTypeHash(Path.HeadCPPType));
		Hash = HashCombine(Hash, GetTypeHash(Path.SegmentPath));
		return Hash;
	}

	friend FArchive& operator<<(FArchive& Ar, FRigVMFunctionCompilationPropertyPath& Data)
	{
		Ar << Data.PropertyIndex;
		Ar << Data.HeadCPPType;
		Ar << Data.SegmentPath;
		return Ar;
	}
};

USTRUCT(BlueprintType)
struct FRigVMFunctionCompilationData
{
	GENERATED_BODY()

	FRigVMFunctionCompilationData()
	: Hash(0)
	, bEncounteredSurpressedErrors(false)
	{}

	UPROPERTY()
	FRigVMByteCode ByteCode;

	UPROPERTY()
	TArray<FName> FunctionNames;

	UPROPERTY()
	TArray<FRigVMFunctionCompilationPropertyDescription> WorkPropertyDescriptions;

	UPROPERTY()
	TArray<FRigVMFunctionCompilationPropertyPath> WorkPropertyPathDescriptions;

	UPROPERTY()
	TArray<FRigVMFunctionCompilationPropertyDescription> LiteralPropertyDescriptions;

	UPROPERTY()
	TArray<FRigVMFunctionCompilationPropertyPath> LiteralPropertyPathDescriptions;

	UPROPERTY()
	TArray<FRigVMFunctionCompilationPropertyDescription> DebugPropertyDescriptions;

	UPROPERTY()
	TArray<FRigVMFunctionCompilationPropertyPath> DebugPropertyPathDescriptions;

	UPROPERTY()
	TArray<FRigVMFunctionCompilationPropertyDescription> ExternalPropertyDescriptions;

	UPROPERTY()
	TArray<FRigVMFunctionCompilationPropertyPath> ExternalPropertyPathDescriptions;

	UPROPERTY()
	TMap<int32, FName> ExternalRegisterIndexToVariable;

	UPROPERTY()
	TMap<FString, FRigVMOperand> Operands;

	UPROPERTY()
	uint32 Hash;

	UPROPERTY(Transient)
	bool bEncounteredSurpressedErrors;

	TMap<FRigVMOperand, TArray<FRigVMOperand>> OperandToDebugRegisters;

	bool IsValid() const
	{
		return Hash != 0;
	}

	bool RequiresRecompilation() const
	{
		return bEncounteredSurpressedErrors;
	}

	friend uint32 GetTypeHash(const FRigVMFunctionCompilationData& Data) 
	{
		uint32 DataHash = Data.ByteCode.GetByteCodeHash();
		for (const FName& Name : Data.FunctionNames)
		{
			DataHash = HashCombine(DataHash, GetTypeHash(Name.ToString()));
		}

		for (const FRigVMFunctionCompilationPropertyDescription& Description : Data.WorkPropertyDescriptions)
		{
			DataHash = HashCombine(DataHash, GetTypeHash(Description));
		}
		for (const FRigVMFunctionCompilationPropertyPath& Path : Data.WorkPropertyPathDescriptions)
		{
			DataHash = HashCombine(DataHash, GetTypeHash(Path));
		}

		for (const FRigVMFunctionCompilationPropertyDescription& Description : Data.LiteralPropertyDescriptions)
		{
			DataHash = HashCombine(DataHash, GetTypeHash(Description));
		}
		for (const FRigVMFunctionCompilationPropertyPath& Path : Data.LiteralPropertyPathDescriptions)
		{
			DataHash = HashCombine(DataHash, GetTypeHash(Path));
		}

		for (const FRigVMFunctionCompilationPropertyDescription& Description : Data.DebugPropertyDescriptions)
		{
			DataHash = HashCombine(DataHash, GetTypeHash(Description));
		}
		for (const FRigVMFunctionCompilationPropertyPath& Path : Data.DebugPropertyPathDescriptions)
		{
			DataHash = HashCombine(DataHash, GetTypeHash(Path));
		}

		for (const FRigVMFunctionCompilationPropertyDescription& Description : Data.ExternalPropertyDescriptions)
		{
			DataHash = HashCombine(DataHash, GetTypeHash(Description));
		}
		for (const FRigVMFunctionCompilationPropertyPath& Path : Data.ExternalPropertyPathDescriptions)
		{
			DataHash = HashCombine(DataHash, GetTypeHash(Path));
		}
		
		for (const TPair<int32,FName>& Pair : Data.ExternalRegisterIndexToVariable)
		{
			DataHash = HashCombine(DataHash, GetTypeHash(Pair.Key));
			DataHash = HashCombine(DataHash, GetTypeHash(Pair.Value.ToString()));
		}

		for (const TPair<FString, FRigVMOperand>& Pair : Data.Operands)
		{
			DataHash = HashCombine(DataHash, GetTypeHash(Pair.Key));
			DataHash = HashCombine(DataHash, GetTypeHash(Pair.Value));
		}

		return DataHash;
	}

	RIGVM_API friend FArchive& operator<<(FArchive& Ar, FRigVMFunctionCompilationData& Data);
};

USTRUCT(BlueprintType)
struct FRigVMGraphFunctionArgument
{
	GENERATED_BODY()
	
	FRigVMGraphFunctionArgument()
	: Name(NAME_None)
	, DisplayName(NAME_None)
	, CPPType(NAME_None)
	, CPPTypeObject(nullptr)
	, bIsArray(false)
	, Direction(ERigVMPinDirection::Input)
	, bIsConst(false)
	{}

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category=FunctionArgument)
	FName Name;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category=FunctionArgument)
	FName DisplayName;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category=FunctionArgument)
	FName CPPType;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category=FunctionArgument)
	TSoftObjectPtr<UObject> CPPTypeObject;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category=FunctionArgument)
	bool bIsArray;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category=FunctionArgument)
	ERigVMPinDirection Direction;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category=FunctionArgument)
	FString DefaultValue;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category=FunctionArgument)
	bool bIsConst;
	
	UPROPERTY()
	TMap<FString, FText> PathToTooltip;

	UE_API FRigVMExternalVariable GetExternalVariable() const;

	// validates and potentially loads the CPP Type Object
	UE_API bool IsCPPTypeObjectValid() const;

	// returns true if this argument is an execute context
	UE_API bool IsExecuteContext() const;

	friend uint32 GetTypeHash(const FRigVMGraphFunctionArgument& Argument)
	{
		uint32 Hash = HashCombine(GetTypeHash(Argument.Name.ToString()), GetTypeHash(Argument.DisplayName.ToString()));
		Hash = HashCombine(Hash, GetTypeHash(Argument.CPPType.ToString()));
		Hash = HashCombine(Hash, GetTypeHash(Argument.CPPTypeObject));
		Hash = HashCombine(Hash, GetTypeHash(Argument.bIsArray));
		Hash = HashCombine(Hash, GetTypeHash(Argument.Direction));
		Hash = HashCombine(Hash, GetTypeHash(Argument.DefaultValue));
		Hash = HashCombine(Hash, GetTypeHash(Argument.bIsConst));
		for (const TPair<FString, FText>& Pair : Argument.PathToTooltip)
		{
			Hash = HashCombine(Hash, GetTypeHash(Pair.Key));
			Hash = HashCombine(Hash, GetTypeHash(Pair.Value.ToString()));
		}
		return Hash;
	}

	bool operator==(const FRigVMGraphFunctionArgument& Other) const
	{
		return true;
	}

	friend FArchive& operator<<(FArchive& Ar, FRigVMGraphFunctionArgument& Data)
	{
		Ar << Data.Name;
		Ar << Data.DisplayName;
		Ar << Data.CPPType;
		Ar << Data.CPPTypeObject;
		Ar << Data.bIsArray;
		Ar << Data.Direction;
		Ar << Data.DefaultValue;
		Ar << Data.bIsConst;
		Ar << Data.PathToTooltip;
		return Ar;
	}
};

USTRUCT(BlueprintType)
struct FRigVMGraphFunctionIdentifier
{
	GENERATED_BODY()

	UPROPERTY(meta=(DeprecatedProperty))
	FSoftObjectPath LibraryNode_DEPRECATED;

private:
	UPROPERTY(VisibleAnywhere, Category=FunctionIdentifier)
	mutable FString LibraryNodePath;

public:

	// A path to the IRigVMGraphFunctionHost that stores the function information, and compilation data (e.g. RigVMBlueprintGeneratedClass)
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category=FunctionIdentifier)
	FSoftObjectPath HostObject;

	FRigVMGraphFunctionIdentifier()
		: LibraryNodePath(FString()), HostObject(nullptr) {}
	
	FRigVMGraphFunctionIdentifier(FSoftObjectPath InHostObject, FString InLibraryNodePath)
		: LibraryNodePath(InLibraryNodePath), HostObject(InHostObject) {}

	friend uint32 GetTypeHash(const FRigVMGraphFunctionIdentifier& Pointer)
	{
		return HashCombine(GetTypeHash(Pointer.GetLibraryNodePath()), GetTypeHash(Pointer.HostObject.ToString()));
	}

	bool operator==(const FRigVMGraphFunctionIdentifier& Other) const
	{
		return HostObject == Other.HostObject && GetNodeSoftPath().GetSubPathString() == Other.GetNodeSoftPath().GetSubPathString();
	}

	bool IsValid() const
	{
		return !HostObject.IsNull() && (!GetLibraryNodePath().IsEmpty());
	}

	FString GetFunctionName() const
	{
		if(IsValid())
		{
			const FString Path = GetLibraryNodePath();
			FString NodeName;
			if(Path.Split(TEXT("."), nullptr, &NodeName, ESearchCase::CaseSensitive, ESearchDir::FromEnd))
			{
				return NodeName;
			}
		}
		return FString();
	}
	
	FName GetFunctionFName() const
	{
		if(!IsValid())
		{
			return NAME_None;
		}
		return *GetFunctionName();
	}

	FString& GetLibraryNodePath() const
	{
		if (LibraryNodePath.IsEmpty() && LibraryNode_DEPRECATED.IsValid())
		{
			LibraryNodePath = LibraryNode_DEPRECATED.ToString();
		}
		return LibraryNodePath;
	}

	void SetLibraryNodePath(const FString& InPath)
	{
		LibraryNodePath = InPath;
	}

	FSoftObjectPath GetNodeSoftPath() const
	{
		return GetLibraryNodePath();
	}

	RIGVM_API friend FArchive& operator<<(FArchive& Ar, FRigVMGraphFunctionIdentifier& Data);

	UE_API bool IsVariant() const;
	UE_API TArray<FRigVMVariantRef> GetVariants(bool bIncludeSelf = false) const;
	UE_API TArray<FRigVMGraphFunctionIdentifier> GetVariantIdentifiers(bool bIncludeSelf = false) const;
	UE_API bool IsVariantOf(const FRigVMGraphFunctionIdentifier& InOther) const;

protected:
	
	static UE_API TFunction<TArray<FRigVMVariantRef>(const FGuid& InGuid)> GetVariantRefsByGuidFunc;

	friend class URigVMBuildData;
};

USTRUCT(BlueprintType)
struct FRigVMGraphFunctionHeader
{
	GENERATED_BODY()

	FRigVMGraphFunctionHeader()
		: LibraryPointer(nullptr, FString())
		, Name(NAME_None)
	{}

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category=FunctionHeader)
	FRigVMGraphFunctionIdentifier LibraryPointer;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category=FunctionIdentifier)
	FRigVMVariant Variant;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category=FunctionHeader)
	FName Name;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category=FunctionHeader)
	FString NodeTitle;
	
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category=FunctionHeader)
	FLinearColor NodeColor = FLinearColor::White;

	UPROPERTY(meta=(DeprecatedProperty))
	FText Tooltip_DEPRECATED;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category=FunctionHeader)
	FString Description;
	
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category=FunctionHeader)
	FString Category;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category=FunctionHeader)
	FString Keywords;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category=FunctionHeader)
	TArray<FRigVMGraphFunctionArgument> Arguments;

	UPROPERTY()
	TMap<FRigVMGraphFunctionIdentifier, uint32> Dependencies;

	UPROPERTY()
	TArray<FRigVMExternalVariable> ExternalVariables;

	UPROPERTY()
	FRigVMNodeLayout Layout;

	UE_API bool IsMutable() const;

	bool IsValid() const { return LibraryPointer.IsValid(); }

	FString GetHash() const
	{
		return RigVMStringUtils::JoinStrings(LibraryPointer.HostObject.ToString(), Name.ToString(), TEXT(":"));
	}

	friend uint32 GetTypeHash(const FRigVMGraphFunctionHeader& Header)
	{
		return GetTypeHash(Header.LibraryPointer);
	}

	bool operator==(const FRigVMGraphFunctionHeader& Other) const
	{
		return LibraryPointer == Other.LibraryPointer;
	}

	UE_API IRigVMGraphFunctionHost* GetFunctionHost(bool bLoadIfNecessary = true) const;

	UE_API FRigVMGraphFunctionData* GetFunctionData(bool bLoadIfNecessary = true) const;

	FText GetTooltip() const
	{
		FString TooltipStr = FString::Printf(TEXT("%s (%s)\n%s"),
		*Name.ToString(),
		*LibraryPointer.GetNodeSoftPath().GetAssetPathString(),
		*Description);
		return FText::FromString(TooltipStr);
	}

	RIGVM_API friend FArchive& operator<<(FArchive& Ar, FRigVMGraphFunctionHeader& Data);

	static UE_API FRigVMGraphFunctionHeader FindGraphFunctionHeader(const FSoftObjectPath& InFunctionObjectPath, bool* bOutIsPublic = nullptr, FString* OutErrorMessage = nullptr);

	static UE_API FRigVMGraphFunctionHeader FindGraphFunctionHeader(const FSoftObjectPath& InHostObjectPath, const FName& InFunctionName, bool* bOutIsPublic = nullptr, FString* OutErrorMessage = nullptr);

	static UE_API FRigVMGraphFunctionHeader FindGraphFunctionHeader(const FRigVMGraphFunctionIdentifier& InIdentifier, bool* bOutIsPublic = nullptr, FString* OutErrorMessage = nullptr);

protected:

	static UE_API FName GetFunctionNameFromObjectPath(const FString& InObjectPath, const FName& InOptionalFunctionName = NAME_None);
	
	static UE_API TFunction<FRigVMGraphFunctionHeader(const FSoftObjectPath&, const FName&, bool*)> FindFunctionHeaderFromPathFunc;

	friend class URigVMBuildData;
	friend struct FRigVMGraphFunctionData;
};

USTRUCT(BlueprintType)
struct FRigVMGraphFunctionData
{
	GENERATED_BODY()

	FRigVMGraphFunctionData(){}

	FRigVMGraphFunctionData(const FRigVMGraphFunctionHeader& InHeader)
		: Header(InHeader) {	}

	UPROPERTY()
	FRigVMGraphFunctionHeader Header;

	UPROPERTY()
	FRigVMFunctionCompilationData CompilationData;

	UPROPERTY()
	FString SerializedCollapsedNode_DEPRECATED;

	UPROPERTY()
	FRigVMObjectArchive CollapseNodeArchive;

	RIGVM_API bool IsMutable() const;

	bool operator==(const FRigVMGraphFunctionData& Other) const
	{
		return Header == Other.Header;
	}

	void ClearCompilationData() { CompilationData = FRigVMFunctionCompilationData(); }

	RIGVM_API friend FArchive& operator<<(FArchive& Ar, FRigVMGraphFunctionData& Data);

	static RIGVM_API FRigVMGraphFunctionData* FindFunctionData(const FSoftObjectPath& InHostObjectPath, const FName& InFunctionName, bool* bOutIsPublic = nullptr, FString* OutErrorMessage = nullptr);	

	static RIGVM_API FRigVMGraphFunctionData* FindFunctionData(const FRigVMGraphFunctionIdentifier& InIdentifier, bool* bOutIsPublic = nullptr, FString* OutErrorMessage = nullptr);	

	static RIGVM_API FString GetArgumentNameFromPinHash(const FString& InPinHash);
	
	RIGVM_API FRigVMOperand GetOperandForArgument(const FName& InArgumentName) const;

	RIGVM_API bool IsAnyOperandSharedAcrossArguments() const;

	RIGVM_API bool PatchSharedArgumentOperandsIfRequired();

	static const inline TCHAR* EntryString = TEXT("Entry");
	static const inline TCHAR* ReturnString = TEXT("Return");
	static RIGVM_API TFunction<IRigVMGraphFunctionHost*(UObject*)> GetFunctionHostFromObjectFunc;

	friend class URigVMBuildData;
};

#undef UE_API
