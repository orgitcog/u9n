// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "Delegates/DelegateCombinations.h"
#include "MetasoundBuilderBase.h"
#include "MetasoundDocumentInterface.h"
#include "MetasoundFrontendDocumentModifyDelegates.h"

#include "MetasoundEditorBuilderListener.generated.h"

// Forward Declarations
struct FMetaSoundNodeHandle;
struct FMetasoundFrontendLiteral;

// BP Delegates for builder changes 
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMetaSoundBuilderGraphInterfaceMutate, FName, VertexName, FName, DataType);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnMetaSoundBuilderGraphLiteralMutate, FName, VertexName, FMetasoundFrontendLiteral, LiteralValue, FName, PageName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMetaSoundBuilderGraphVertexRename, FName, OldName, FName, NewName);

UCLASS(MinimalAPI, BlueprintType)
class UMetaSoundEditorBuilderListener : public UObject
{
public: 
	GENERATED_BODY()

	virtual ~UMetaSoundEditorBuilderListener() {}
	
	void Init(const TWeakObjectPtr<UMetaSoundBuilderBase> InBuilder);

	void OnGraphInputAdded(int32 Index);
	void OnGraphInputDefaultChanged(int32 Index);
	void OnRemovingGraphInput(int32 Index);

	void OnGraphOutputAdded(int32 Index);
	void OnRemovingGraphOutput(int32 Index);

	void OnGraphInputDataTypeChanged(int32 Index);
	void OnGraphOutputDataTypeChanged(int32 Index);
	void OnGraphInputNameChanged(FName OldName, FName NewName);
	void OnGraphOutputNameChanged(FName OldName, FName NewName);


	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder|Editor|Delegates")
	METASOUNDEDITOR_API void RemoveAllDelegates();

	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "Audio|MetaSound|Builder|Editor|Delegates")
	FOnMetaSoundBuilderGraphInterfaceMutate OnGraphInputAddedDelegate;
	
	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "Audio|MetaSound|Builder|Editor|Delegates")
	FOnMetaSoundBuilderGraphLiteralMutate OnGraphInputDefaultChangedDelegate;

	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "Audio|MetaSound|Builder|Editor|Delegates")
	FOnMetaSoundBuilderGraphInterfaceMutate OnRemovingGraphInputDelegate;
	
	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category= "Audio|MetaSound|Builder|Editor|Delegates")
	FOnMetaSoundBuilderGraphInterfaceMutate OnGraphOutputAddedDelegate;

	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "Audio|MetaSound|Builder|Editor|Delegates")
	FOnMetaSoundBuilderGraphInterfaceMutate OnRemovingGraphOutputDelegate;

	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "Audio|MetaSound|Builder|Editor|Delegates")
	FOnMetaSoundBuilderGraphInterfaceMutate OnGraphInputDataTypeChangedDelegate;

	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "Audio|MetaSound|Builder|Editor|Delegates")
	FOnMetaSoundBuilderGraphInterfaceMutate OnGraphOutputDataTypeChangedDelegate;

	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "Audio|MetaSound|Builder|Editor|Delegates")
	FOnMetaSoundBuilderGraphVertexRename OnGraphInputNameChangedDelegate;

	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "Audio|MetaSound|Builder|Editor|Delegates")
	FOnMetaSoundBuilderGraphVertexRename OnGraphOutputNameChangedDelegate;

private:
	// Handles for document delegates
	FDelegateHandle OnInputAddedHandle;
	FDelegateHandle OnInputDefaultChangedHandle;
	FDelegateHandle OnRemovingInputHandle;

	FDelegateHandle OnOutputAddedHandle;
	FDelegateHandle OnRemovingOutputHandle;

	FDelegateHandle OnInputDataTypeChangedHandle;
	FDelegateHandle OnOutputDataTypeChangedHandle;
	FDelegateHandle OnInputNameChangedHandle;
	FDelegateHandle OnOutputNameChangedHandle;

	TWeakObjectPtr<UMetaSoundBuilderBase> Builder;

	class FEditorBuilderListener : public Metasound::Frontend::IDocumentBuilderTransactionListener
	{
	public: 

		FEditorBuilderListener() = default;
		FEditorBuilderListener(TObjectPtr<UMetaSoundEditorBuilderListener> InParent)
			: Parent(InParent)
		{
		}

		virtual ~FEditorBuilderListener() = default;

		// IDocumentBuilderTransactionListener
		virtual void OnBuilderReloaded(Metasound::Frontend::FDocumentModifyDelegates& OutDelegates) override;

	private: 
		TObjectPtr<UMetaSoundEditorBuilderListener> Parent;
	};

	TSharedPtr<FEditorBuilderListener> BuilderListener;
};