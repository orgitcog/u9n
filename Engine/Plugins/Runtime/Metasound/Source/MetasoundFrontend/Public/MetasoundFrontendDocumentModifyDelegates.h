// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Delegates/DelegateCombinations.h"
#include "MetasoundFrontendDocument.h"
#include "Templates/SharedPointer.h"

#define UE_API METASOUNDFRONTEND_API


// TODO: Move these to namespace
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMetaSoundFrontendDocumentMutateArray, int32 /* Index */);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMetaSoundFrontendDocumentMutateInterfaceArray, const FMetasoundFrontendInterface& /* Interface */);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnMetaSoundFrontendDocumentRemoveSwappingArray, int32 /* Index */, int32 /* LastIndex */);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnMetaSoundFrontendDocumentRenameClass, const int32 /* Index */, const FMetasoundFrontendClassName& /* NewName */);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnMetaSoundFrontendDocumentMutateNodeInputLiteralArray, int32 /* NodeIndex */, int32 /* VertexIndex */, int32 /* LiteralIndex */);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnMetaSoundFrontendDocumentRenameVertex, FName /* OldName */, FName /* NewName */);

namespace Metasound::Frontend
{
	struct FDocumentMutatePageArgs
	{
		FGuid PageID;
	};

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnDocumentPageAdded, const FDocumentMutatePageArgs& /* Args */);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnDocumentRemovingPage, const FDocumentMutatePageArgs& /* Args */);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnDocumentPageSet, const FDocumentMutatePageArgs& /* Args */);

	struct FPageModifyDelegates
	{
		FOnDocumentPageAdded OnPageAdded;
		FOnDocumentRemovingPage OnRemovingPage;
		FOnDocumentPageSet OnPageSet;
	};

	struct FInterfaceModifyDelegates
	{
		FOnMetaSoundFrontendDocumentMutateInterfaceArray OnInterfaceAdded;
		FOnMetaSoundFrontendDocumentMutateInterfaceArray OnRemovingInterface;

		FOnMetaSoundFrontendDocumentMutateArray OnInputAdded;

// Currently only used in editor contexts  
// so only enabled there to avoid unnecessary delegate overhead
// but may be changed in the future 
#if WITH_EDITOR
		FOnMetaSoundFrontendDocumentMutateArray OnInputDataTypeChanged;
#endif // WITH_EDITOR
		FOnMetaSoundFrontendDocumentMutateArray OnInputDefaultChanged;

		FOnMetaSoundFrontendDocumentRenameVertex OnInputNameChanged;
		FOnMetaSoundFrontendDocumentMutateArray OnRemovingInput;

		FOnMetaSoundFrontendDocumentMutateArray OnOutputAdded;
#if WITH_EDITOR
		FOnMetaSoundFrontendDocumentMutateArray OnOutputDataTypeChanged;
#endif // WITH_EDITOR
		FOnMetaSoundFrontendDocumentRenameVertex OnOutputNameChanged;
		FOnMetaSoundFrontendDocumentMutateArray OnRemovingOutput;
	};

	struct FNodeModifyDelegates
	{
		FOnMetaSoundFrontendDocumentMutateArray OnNodeAdded;
		FOnMetaSoundFrontendDocumentRemoveSwappingArray OnRemoveSwappingNode;

		FOnMetaSoundFrontendDocumentMutateNodeInputLiteralArray OnNodeInputLiteralSet;
		FOnMetaSoundFrontendDocumentMutateNodeInputLiteralArray OnRemovingNodeInputLiteral;
	};

	struct FEdgeModifyDelegates
	{
		FOnMetaSoundFrontendDocumentMutateArray OnEdgeAdded;
		FOnMetaSoundFrontendDocumentRemoveSwappingArray OnRemoveSwappingEdge;
	};

	struct FDocumentModifyDelegates : TSharedFromThis<FDocumentModifyDelegates>
	{
		UE_API FDocumentModifyDelegates();
		UE_API FDocumentModifyDelegates(const FDocumentModifyDelegates& InModifyDelegates);
		UE_API FDocumentModifyDelegates(FDocumentModifyDelegates&&);
		UE_API FDocumentModifyDelegates(const FMetasoundFrontendDocument& Document);
		UE_API FDocumentModifyDelegates& operator=(const FDocumentModifyDelegates&);
		UE_API FDocumentModifyDelegates& operator=(FDocumentModifyDelegates&&);


		FOnMetaSoundFrontendDocumentMutateArray OnDependencyAdded;
		FOnMetaSoundFrontendDocumentRemoveSwappingArray OnRemoveSwappingDependency;
		FOnMetaSoundFrontendDocumentRenameClass OnRenamingDependencyClass;

		FPageModifyDelegates PageDelegates;
		FInterfaceModifyDelegates InterfaceDelegates;

		UE_DEPRECATED(5.5, "Public exposition of NodeDelegates will be removed in a future build.  Use accessor 'FindNodeDelegates' instead")
		FNodeModifyDelegates NodeDelegates;

		UE_DEPRECATED(5.5, "Public exposition of EdgeDelegates will be removed in a future build.  Use accessor 'FindEdgeDelegates' instead")
		FEdgeModifyDelegates EdgeDelegates;

		UE_API void AddPageDelegates(const FGuid& InPageID);
		UE_API void RemovePageDelegates(const FGuid& InPageID, bool bBroadcastNotify = true);

	private:
		TSortedMap<FGuid, FNodeModifyDelegates> PageNodeDelegates;
		TSortedMap<FGuid, FEdgeModifyDelegates> PageEdgeDelegates;

	public:
		UE_API FNodeModifyDelegates& FindNodeDelegatesChecked(const FGuid& InPageID);
		UE_API FEdgeModifyDelegates& FindEdgeDelegatesChecked(const FGuid& InPageID);

		void IterateGraphEdgeDelegates(TFunctionRef<void(FEdgeModifyDelegates&)> Func)
		{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
			Func(EdgeDelegates);

			for (TPair<FGuid, FEdgeModifyDelegates>& Delegates : PageEdgeDelegates)
			{
				Func(Delegates.Value);
			}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
		}

		void IterateGraphNodeDelegates(TFunctionRef<void(FNodeModifyDelegates&)> Func)
		{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
			Func(NodeDelegates);

			for (TPair<FGuid, FNodeModifyDelegates>& Delegates : PageNodeDelegates)
			{
				Func(Delegates.Value);
			}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
		}
	};

	class IDocumentBuilderTransactionListener : public TSharedFromThis<IDocumentBuilderTransactionListener>
	{
	public:
		virtual ~IDocumentBuilderTransactionListener() = default;

		// Called when the builder is reloaded, at which point the document cache and delegates are refreshed
		virtual void OnBuilderReloaded(FDocumentModifyDelegates& OutDelegates) = 0;
	};
} // namespace Metasound::Frontend

#undef UE_API
