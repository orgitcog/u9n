// Copyright Epic Games, Inc. All Rights Reserved.

#include "ViewModels/HierarchyEditor/NiagaraScriptParametersHierarchyViewModel.h"

#include "NiagaraConstants.h"
#include "NiagaraEditorUtilities.h"
#include "NiagaraGraph.h"
#include "NiagaraScriptSource.h"
#include "ScopedTransaction.h"
#include "Logging/StructuredLog.h"
#include "ViewModels/NiagaraScriptViewModel.h"

#define LOCTEXT_NAMESPACE "NiagaraScriptParameterHierarchyEditor"

void UNiagaraHierarchyScriptParameter::Initialize(const UNiagaraScriptVariable& InParameterScriptVariable)
{
	if(InParameterScriptVariable.Metadata.GetVariableGuid().IsValid() == false)
	{
		UE_LOGFMT(LogNiagaraEditor, Warning, "Invalid hierarchy script parameter initialization. No valid guid found. This hierarchy element will be deleted on next refresh.");
	}
	
	SetIdentity(FHierarchyElementIdentity({InParameterScriptVariable.Metadata.GetVariableGuid()}, {}));
}

bool UNiagaraHierarchyScriptParameter::IsValid() const
{
	return GetScriptVariable() != nullptr;
}

FString UNiagaraHierarchyScriptParameter::ToString() const
{
	TOptional<FNiagaraVariable> Variable = GetVariable();

	if(Variable.IsSet())
	{
		return Variable.GetValue().GetName().ToString();
	}
	
	return TEXT("Invalid"); 
}

FText UNiagaraHierarchyScriptParameter::GetTooltip() const
{
	if(UNiagaraScriptVariable* ScriptVariable = GetScriptVariable())
	{
		return ScriptVariable->Metadata.Description; 
	}

	return FText::GetEmpty();
}

UNiagaraScriptVariable* UNiagaraHierarchyScriptParameter::GetScriptVariable() const
{
	UNiagaraGraph* OwningGraph = GetTypedOuter<UNiagaraGraph>();
	if(ensureMsgf(OwningGraph != nullptr, TEXT("When retrieving the script variable matching this hierarchy parameter, there should always be an owning graph")))
	{
		// If the script variable does not exist, this is an indicator it has been deleted and this hierarchy parameter should be deleted
		return OwningGraph->GetScriptVariable(GetPersistentIdentity().Guids[0]);
	}

	return nullptr;
}

TOptional<FNiagaraVariable> UNiagaraHierarchyScriptParameter::GetVariable() const
{
	if(UNiagaraScriptVariable* ScriptVariable = GetScriptVariable())
	{
		return ScriptVariable->Variable;
	}
	
	return TOptional<FNiagaraVariable>(); 
}

void UNiagaraScriptParametersHierarchyViewModel::Initialize(TSharedRef<FNiagaraScriptViewModel> InScriptViewModel)
{
	ScriptViewModelWeak = InScriptViewModel;

	Cast<UNiagaraScriptSource>(ScriptViewModelWeak.Pin()->GetStandaloneScript().GetScriptData()->GetSource())->NodeGraph->OnParametersChanged().AddUObject(this, &UNiagaraScriptParametersHierarchyViewModel::OnParametersChanged);
	UDataHierarchyViewModelBase::Initialize();

	UNiagaraHierarchyScriptParameterRefreshContext* ScriptParameterRefreshContext = NewObject<UNiagaraHierarchyScriptParameterRefreshContext>(this);
	ScriptParameterRefreshContext->SetNiagaraGraph(Cast<UNiagaraScriptSource>(InScriptViewModel->GetStandaloneScript().GetScriptData()->GetSource())->NodeGraph);
	SetRefreshContext(ScriptParameterRefreshContext);
}

TSharedRef<FNiagaraScriptViewModel> UNiagaraScriptParametersHierarchyViewModel::GetScriptViewModel() const
{
	TSharedPtr<FNiagaraScriptViewModel> ScriptViewModelPinned = ScriptViewModelWeak.Pin();
	checkf(ScriptViewModelPinned.IsValid(), TEXT("Script view model destroyed before parameters hierarchy view model."));
	return ScriptViewModelPinned.ToSharedRef();
}


UHierarchyRoot* UNiagaraScriptParametersHierarchyViewModel::GetHierarchyRoot() const
{
	const TArray<FVersionedNiagaraScriptWeakPtr>& Scripts = GetScriptViewModel()->GetScripts();
	if(ensure(Scripts.Num() >= 1 && Scripts[0].Pin().Script != nullptr) == false)
	{
		return nullptr;
	}

	FVersionedNiagaraScriptData* ScriptData = Scripts[0].Pin().GetScriptData();
	if(ensure(ScriptData != nullptr) == false)
	{
		return nullptr;
	}

	return Cast<UNiagaraScriptSource>(ScriptData->GetSource())->NodeGraph->GetScriptParameterHierarchyRoot();
}

UObject* UNiagaraScriptParametersHierarchyViewModel::GetOuterForSourceRoot() const
{
	// The graph always needs to be valid as the outer
	if(UNiagaraGraph* Graph = Cast<UNiagaraScriptSource>(ScriptViewModelWeak.Pin()->GetStandaloneScript().GetScriptData()->GetSource())->NodeGraph)
	{
		return Graph;
	}

	checkNoEntry();
	return nullptr;
}

TSubclassOf<UHierarchyCategory> UNiagaraScriptParametersHierarchyViewModel::GetCategoryDataClass() const
{
	return UNiagaraHierarchyScriptCategory::StaticClass();
}

TSharedPtr<FHierarchyElementViewModel> UNiagaraScriptParametersHierarchyViewModel::CreateCustomViewModelForElement(UHierarchyElement* Element, TSharedPtr<FHierarchyElementViewModel> Parent)
{
	if(UNiagaraHierarchyScriptParameter* Item = Cast<UNiagaraHierarchyScriptParameter>(Element))
	{
		return MakeShared<FNiagaraHierarchyScriptParameterViewModel>(Item, Parent.ToSharedRef(), this);
	}
	if(UHierarchyCategory* Category = Cast<UHierarchyCategory>(Element))
	{
		return MakeShared<FNiagaraHierarchyScriptCategoryViewModel>(Category, Parent.ToSharedRef(), this);
	}
	else if(UHierarchyRoot* Root = Cast<UHierarchyRoot>(Element))
	{
		// If the root is the hierarchy root, we know it's for the hierarchy. If not, it's the transient source root
		bool bIsForHierarchy = GetHierarchyRoot() == Element;
		return MakeShared<FNiagaraHierarchyScriptRootViewModel>(Root, this, bIsForHierarchy);
	}

	return nullptr;
}

void UNiagaraScriptParametersHierarchyViewModel::PrepareSourceItems(UHierarchyRoot* SourceRoot, TSharedPtr<FHierarchyRootViewModel> SourceRootViewModel)
{
	UNiagaraScriptSourceBase* SourceBase = GetScriptViewModel()->GetStandaloneScript().GetScriptData()->GetSource();
	UNiagaraScriptSource* ScriptSource = Cast<UNiagaraScriptSource>(SourceBase);
	const TMap<FNiagaraVariable, TObjectPtr<UNiagaraScriptVariable>>& ScriptVariableMap = ScriptSource->NodeGraph->GetAllMetaData();

	for(const auto& ScriptVariablePair : ScriptVariableMap)
	{
		// We only want to be able to organize module inputs & static switches 
		if(ScriptVariablePair.Key.IsInNameSpace(FNiagaraConstants::ModuleNamespace) == false && ScriptVariablePair.Value->GetIsStaticSwitch() == false)
		{
			continue;
		}
		
		UNiagaraScriptVariable* ScriptVariable = ScriptVariablePair.Value;
		
		UNiagaraHierarchyScriptParameter* ScriptParameterHierarchyObject = NewObject<UNiagaraHierarchyScriptParameter>(SourceRoot, NAME_None, RF_Transient);
		ScriptParameterHierarchyObject->Initialize(*ScriptVariable);
		SourceRoot->GetChildrenMutable().Add(ScriptParameterHierarchyObject);
	}
}

void UNiagaraScriptParametersHierarchyViewModel::SetupCommands()
{
	Super::SetupCommands();
}

TSharedRef<FHierarchyDragDropOp> UNiagaraScriptParametersHierarchyViewModel::CreateDragDropOp(TSharedRef<FHierarchyElementViewModel> Item)
{
	if(UHierarchyCategory* HierarchyCategory = Cast<UHierarchyCategory>(Item->GetDataMutable()))
	{
		TSharedRef<FHierarchyDragDropOp> CategoryDragDropOp = MakeShared<FHierarchyDragDropOp>(Item);
		CategoryDragDropOp->Construct();
		return CategoryDragDropOp;
	}
	else if(UNiagaraHierarchyScriptParameter* ScriptParameter = Cast<UNiagaraHierarchyScriptParameter>(Item->GetDataMutable()))
	{
		TSharedPtr<FHierarchyItemViewModel> ScriptParameterViewModel = StaticCastSharedRef<FHierarchyItemViewModel>(Item);
		TSharedRef<FHierarchyDragDropOp> ScriptParameterDragDropOp = MakeShared<FNiagaraHierarchyScriptParameterDragDropOp>(ScriptParameterViewModel);
		ScriptParameterDragDropOp->Construct();
		return ScriptParameterDragDropOp;
	}
	
	check(false);
	return MakeShared<FHierarchyDragDropOp>(nullptr);
}

void UNiagaraScriptParametersHierarchyViewModel::FinalizeInternal()
{
	if(ScriptViewModelWeak.IsValid())
	{
		// If this is called during Undo, it's possible the Graph does no longer exist
		if(UNiagaraGraph* Graph = Cast<UNiagaraScriptSource>(ScriptViewModelWeak.Pin()->GetStandaloneScript().GetScriptData()->GetSource())->NodeGraph)
		{
			Graph->OnParametersChanged().RemoveAll(this);
		}
	}
}

void UNiagaraScriptParametersHierarchyViewModel::OnParametersChanged(TOptional<TInstancedStruct<FNiagaraParametersChangedData>> ParametersChangedData)
{
	if(ParametersChangedData.IsSet())
	{
		TInstancedStruct<FNiagaraParametersChangedData> ChangedData = ParametersChangedData.GetValue();
		if(ChangedData.GetScriptStruct() == FNiagaraParameterRenamedData::StaticStruct())
		{
			const FNiagaraParameterRenamedData& RenamedData = ChangedData.Get<FNiagaraParameterRenamedData>();
			if(RenamedData.OldScriptVariable->GetIsStaticSwitch() && RenamedData.NewScriptVariable->GetIsStaticSwitch())
			{
				TArray<UNiagaraHierarchyScriptParameter*> AllHierarchyScriptParameters;
				HierarchyRoot->GetChildrenOfType<UNiagaraHierarchyScriptParameter>(AllHierarchyScriptParameters, true);

				UNiagaraHierarchyScriptParameter** FoundHierarchyScriptParameter = AllHierarchyScriptParameters.FindByPredicate([Guid = RenamedData.OldScriptVariable->Metadata.GetVariableGuid()](UNiagaraHierarchyScriptParameter* Candidate)
				{
					return Candidate->GetPersistentIdentity().Guids[0] == Guid;
				});

				if(FoundHierarchyScriptParameter)
				{
					HierarchyRoot->Modify();
					(*FoundHierarchyScriptParameter)->Initialize(*RenamedData.NewScriptVariable);
				}
			}
		}
	}
	ForceFullRefresh();
}

bool UNiagaraScriptParametersHierarchyViewModel::MatchesContext(const FTransactionContext& InContext, const TArray<TPair<UObject*, FTransactionObjectEvent>>& TransactionObjectContexts) const
{
	return Super::MatchesContext(InContext, TransactionObjectContexts) || TransactionObjectContexts.ContainsByPredicate([](TPair<UObject*, FTransactionObjectEvent> ContextPair)
	{
		return ContextPair.Key->IsA<UNiagaraGraph>() || ContextPair.Key->IsA<UNiagaraScriptVariable>();
	});
}

TSharedRef<SWidget> FNiagaraHierarchyScriptParameterDragDropOp::CreateCustomDecorator() const
{
	if(const UNiagaraHierarchyScriptParameter* ScriptParameter = Cast<UNiagaraHierarchyScriptParameter>(DraggedElement.Pin()->GetData()))
	{
		TOptional<FNiagaraVariable> Variable = ScriptParameter->GetVariable();
		if(Variable.IsSet())
		{
			return FNiagaraParameterUtilities::GetParameterWidget(Variable.GetValue(), false, false);
		}
	}

	return SNullWidget::NullWidget;
}

FHierarchyElementViewModel::FCanPerformActionResults FNiagaraHierarchyScriptCategoryViewModel::CanDropOnInternal(TSharedPtr<FHierarchyElementViewModel> DraggedElement, EItemDropZone ItemDropZone)
{
	if(IsEditableByUser().bCanPerform == false)
	{
		return false;
	}
	
	FCanPerformActionResults CanPerformActionResults = FHierarchyCategoryViewModel::CanDropOnInternal(DraggedElement, ItemDropZone);

	// We allow dropping parameters above/below categories
	if(ItemDropZone != EItemDropZone::OntoItem && DraggedElement->GetData()->IsA<UNiagaraHierarchyScriptParameter>())
	{
		CanPerformActionResults.CanPerformMessage = FText::GetEmpty();
		CanPerformActionResults.bCanPerform = true;
	}

	return CanPerformActionResults;
}

bool FNiagaraHierarchyScriptParameterViewModel::DoesExternalDataStillExist(const UHierarchyDataRefreshContext* Context) const
{
	// GetScriptVariable checks the owning graph if it still exists
	const UNiagaraScriptVariable* ScriptVariable = Cast<UNiagaraHierarchyScriptParameter>(GetDataMutable())->GetScriptVariable();
	
	if(ScriptVariable == nullptr)
	{
		return false;
	}	

	// We make sure that the variable not only still exists but also qualifies for the hierarchy (namespace can change for example)
	if(ScriptVariable->GetIsStaticSwitch() == false && ScriptVariable->Variable.IsInNameSpace(FNiagaraConstants::ModuleNamespace) == false)
	{
		return false;
	}
	
	return true;
}

FHierarchyElementViewModel::FCanPerformActionResults FNiagaraHierarchyScriptParameterViewModel::CanDropOnInternal(TSharedPtr<FHierarchyElementViewModel> DraggedItem, EItemDropZone ItemDropZone)
{
	// if the input isn't editable, we don't allow any drops on/above/below the item.
	// Even though it technically works, the merge process will only re-add the item at the end and not preserve order so there is no point in allowing dropping above/below
	if(IsEditableByUser().bCanPerform == false)
	{
		return false;
	}

	FCanPerformActionResults AllowDrop(false);
	
	TSharedPtr<FHierarchyElementViewModel> TargetDropItem = AsShared();

	// we only allow drops if some general conditions are fulfilled
	if(DraggedItem->GetData() != TargetDropItem->GetData() &&
		(!DraggedItem->HasParent(TargetDropItem, false) || ItemDropZone != EItemDropZone::OntoItem)  &&
		!TargetDropItem->HasParent(DraggedItem, true))
	{
		if(ItemDropZone == EItemDropZone::OntoItem)
		{
			// We support nested inputs
			if(DraggedItem->GetData()->IsA<UNiagaraHierarchyScriptParameter>() && TargetDropItem->GetData()->IsA<UNiagaraHierarchyScriptParameter>())
			{
				// But not if the dragged input already has a child
				if(DraggedItem->GetData()->DoesOneChildExist<UNiagaraHierarchyScriptParameter>())
				{
					FText BaseMessage = LOCTEXT("DraggedItemHasChild_NotAllowed", "Can't nest input {0} under input {1}.\nRemove children of {2} first. ");
					AllowDrop.CanPerformMessage = FText::FormatOrdered(BaseMessage, DraggedItem->ToStringAsText(), TargetDropItem->ToStringAsText(), DraggedItem->ToStringAsText());
					AllowDrop.bCanPerform = false;
					return AllowDrop;
				}
				
				// And only up to 1 layer if we are going to create a new child input
				if(TargetDropItem->GetParent().Pin()->GetData<UNiagaraHierarchyScriptParameter>())
				{
					FText BaseMessage = LOCTEXT("DroppingInputOnInputNestedChildTooDeep", "Can't nest input {0} under input {1}.\nChildren inputs can only have one layer of depth!");
					AllowDrop.CanPerformMessage = FText::FormatOrdered(BaseMessage, DraggedItem->ToStringAsText(), TargetDropItem->ToStringAsText());
					AllowDrop.bCanPerform = false;
					return AllowDrop;
				}
				
				FText BaseMessage = LOCTEXT("DroppingInputOnInputNestedChild", "This will nest input {0} under input {1}");
				AllowDrop.CanPerformMessage = FText::FormatOrdered(BaseMessage, DraggedItem->ToStringAsText(), TargetDropItem->ToStringAsText());
				AllowDrop.bCanPerform = true;
				return AllowDrop;
			}
		}
		else
		{
			// if the dragged item is an input, we generally allow above/below, even for nested child inputs
			if(DraggedItem->GetData()->IsA<UNiagaraHierarchyScriptParameter>())
			{
				AllowDrop.bCanPerform = true;
			}
			// if the dragged item is a category, we generally allow putting it above/below other parameters, but not above/below child parameters
			else if(DraggedItem->GetData()->IsA<UHierarchyCategory>())
			{
				AllowDrop.bCanPerform = true;
				if(TargetDropItem->GetData()->IsA(UNiagaraHierarchyScriptParameter::StaticClass()) && TargetDropItem->GetParent().Pin()->GetData()->IsA(UNiagaraHierarchyScriptParameter::StaticClass()))
				{
					AllowDrop.bCanPerform = false;
				}
			}
		}
	}

	return AllowDrop;
}

void FNiagaraHierarchyScriptParameterViewModel::OnDroppedOnInternal(TSharedPtr<FHierarchyElementViewModel> DroppedItem, EItemDropZone ItemDropZone)
{
	if(ItemDropZone != EItemDropZone::OntoItem)
	{
		return FHierarchyItemViewModel::OnDroppedOnInternal(DroppedItem, ItemDropZone);
	}
	else
	{
		FScopedTransaction Transaction(LOCTEXT("Transaction_AddedChildInput", "Added child input"));
		HierarchyViewModel->GetHierarchyRoot()->Modify();
		
		if(DroppedItem->IsForHierarchy() == false)
		{
			TSharedRef<FHierarchyElementViewModel> AddedItemViewModel = DuplicateToThis(DroppedItem);
			AddedItemViewModel->SyncViewModelsToData();
		}
		else
		{
			TSharedRef<FHierarchyElementViewModel> ReparentedViewModel = ReparentToThis(DroppedItem);
			ReparentedViewModel->SyncViewModelsToData();
		}

		HierarchyViewModel->RefreshHierarchyView();
		HierarchyViewModel->RefreshSourceView();
	}
}

#undef LOCTEXT_NAMESPACE
