// Copyright Epic Games, Inc. All Rights Reserved.

#include "ViewModels/HierarchyEditor/NiagaraUserParametersHierarchyViewModel.h"
#include "NiagaraEditorUtilities.h"
#include "NiagaraEmitterEditorData.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraScriptVariable.h"
#include "NiagaraSystem.h"
#include "NiagaraSystemEditorData.h"
#include "SDropTarget.h"
#include "Customizations/NiagaraScriptVariableCustomization.h"
#include "ViewModels/NiagaraSystemViewModel.h"

#define LOCTEXT_NAMESPACE "NiagaraUserParametersHierarchyEditor"

void UNiagaraHierarchyUserParameter::Initialize(UNiagaraScriptVariable& InUserParameterScriptVariable)
{
	UserParameterScriptVariable = &InUserParameterScriptVariable;
	SetIdentity(FHierarchyElementIdentity({InUserParameterScriptVariable.Metadata.GetVariableGuid()}, {}));
}

UObject* FNiagaraHierarchyUserParameterViewModel::GetDataForEditing()
{
	UNiagaraHierarchyUserParameter* HierarchyUserParameter = Cast<UNiagaraHierarchyUserParameter>(GetDataMutable<UNiagaraHierarchyUserParameter>());
	FNiagaraVariable ContainedVariable = HierarchyUserParameter->GetUserParameter();
	UNiagaraUserParametersHierarchyViewModel* UserParametersHierarchyViewModel = Cast<UNiagaraUserParametersHierarchyViewModel>(GetHierarchyViewModel());
	UserParametersHierarchyViewModel->GetSystemViewModel()->GetSystem().GetExposedParameters().RedirectUserVariable(ContainedVariable);
	TObjectPtr<UNiagaraScriptVariable> ScriptVariable = FNiagaraEditorUtilities::UserParameters::GetScriptVariableForUserParameter(ContainedVariable, UserParametersHierarchyViewModel->GetSystemViewModel());
	return ScriptVariable.Get();
}

bool FNiagaraHierarchyUserParameterViewModel::DoesExternalDataStillExist(const UHierarchyDataRefreshContext* Context) const
{
	const UNiagaraHierarchyUserParameterRefreshContext* UserParameterRefreshContext = CastChecked<UNiagaraHierarchyUserParameterRefreshContext>(Context);
	const UNiagaraSystem* System = UserParameterRefreshContext->GetSystem();
	return FNiagaraEditorUtilities::UserParameters::FindScriptVariableForUserParameter(GetData()->GetPersistentIdentity().Guids[0], *System) != nullptr;
}

TSharedRef<FNiagaraSystemViewModel> UNiagaraUserParametersHierarchyViewModel::GetSystemViewModel() const
{
	TSharedPtr<FNiagaraSystemViewModel> SystemViewModelPinned = SystemViewModelWeak.Pin();
	checkf(SystemViewModelPinned.IsValid(), TEXT("System view model destroyed before user parameters hierarchy view model."));
	return SystemViewModelPinned.ToSharedRef();
}

void UNiagaraUserParametersHierarchyViewModel::Initialize(TSharedRef<FNiagaraSystemViewModel> InSystemViewModel)
{
	SystemViewModelWeak = InSystemViewModel;
	UDataHierarchyViewModelBase::Initialize();

	UNiagaraHierarchyUserParameterRefreshContext* UserParameterRefreshContext = NewObject<UNiagaraHierarchyUserParameterRefreshContext>(this);
	UserParameterRefreshContext->SetSystem(&InSystemViewModel->GetSystem());

	SetRefreshContext(UserParameterRefreshContext);
	
	UNiagaraSystemEditorData& SystemEditorData = InSystemViewModel->GetEditorData();
	SystemEditorData.OnUserParameterScriptVariablesSynced().AddUObject(this, &UNiagaraUserParametersHierarchyViewModel::ForceFullRefresh);
}

void UNiagaraUserParametersHierarchyViewModel::FinalizeInternal()
{
	TSharedPtr<FNiagaraSystemViewModel> SystemViewModel = SystemViewModelWeak.Pin();
	if(SystemViewModel.IsValid())
	{
		GetSystemViewModel()->GetSystem().GetExposedParameters().RemoveAllOnChangedHandlers(this);
		UNiagaraSystemEditorData* SystemEditorData = Cast<UNiagaraSystemEditorData>(GetSystemViewModel()->GetSystem().GetEditorData());
		SystemEditorData->OnUserParameterScriptVariablesSynced().RemoveAll(this);
	}
}

TSharedRef<SWidget> FNiagaraUserParameterHierarchyDragDropOp::CreateCustomDecorator() const
{
	return FNiagaraParameterUtilities::GetParameterWidget(GetUserParameter(), true, false);
}

UHierarchyRoot* UNiagaraUserParametersHierarchyViewModel::GetHierarchyRoot() const
{
	UHierarchyRoot* RootItem = GetSystemViewModel()->GetEditorData().UserParameterHierarchy;

	ensure(RootItem != nullptr);
	return RootItem;
}

TSharedPtr<FHierarchyElementViewModel> UNiagaraUserParametersHierarchyViewModel::CreateCustomViewModelForElement(UHierarchyElement* ItemBase, TSharedPtr<FHierarchyElementViewModel> Parent)
{
	if(UNiagaraHierarchyUserParameter* UserParameter = Cast<UNiagaraHierarchyUserParameter>(ItemBase))
	{
		return MakeShared<FNiagaraHierarchyUserParameterViewModel>(UserParameter, Parent.ToSharedRef(), this);
	}
	
	return nullptr;
}

void UNiagaraUserParametersHierarchyViewModel::PrepareSourceItems(UHierarchyRoot* SourceRoot, TSharedPtr<FHierarchyRootViewModel>)
{	
	TArray<FNiagaraVariable> UserParameters;
	GetSystemViewModel()->GetSystem().GetExposedParameters().GetUserParameters(UserParameters);

	// we ensure we have one data child per user parameter, and we get rid of deleted ones
	for(FNiagaraVariable& UserParameter : UserParameters)
	{
		FNiagaraVariable RedirectedUserParameter = UserParameter;
		GetSystemViewModel()->GetSystem().GetExposedParameters().RedirectUserVariable(RedirectedUserParameter);
		TObjectPtr<UNiagaraScriptVariable> ScriptVariable = FNiagaraEditorUtilities::UserParameters::GetScriptVariableForUserParameter(RedirectedUserParameter, GetSystemViewModel());
		
		// since the source items are transient we need to create them here and keep them around until the end of the tool's lifetime
		UNiagaraHierarchyUserParameter* UserParameterHierarchyObject = NewObject<UNiagaraHierarchyUserParameter>(SourceRoot);
		UserParameterHierarchyObject->Initialize(*ScriptVariable.Get());
		SourceRoot->GetChildrenMutable().Add(UserParameterHierarchyObject);
	}
}

void UNiagaraUserParametersHierarchyViewModel::SetupCommands()
{
	// no custom commands yet
}

TSharedRef<FHierarchyDragDropOp> UNiagaraUserParametersHierarchyViewModel::CreateDragDropOp(TSharedRef<FHierarchyElementViewModel> Item)
{
	if(const UNiagaraHierarchyUserParameter* UserParameter = Cast<UNiagaraHierarchyUserParameter>(Item->GetData()))
	{
		TSharedRef<FNiagaraUserParameterHierarchyDragDropOp> ParameterDragDropOp = MakeShared<FNiagaraUserParameterHierarchyDragDropOp>(Item);
		ParameterDragDropOp->Construct();
		return ParameterDragDropOp;
	}

	if(UHierarchyCategory* HierarchyCategory = Cast<UHierarchyCategory>(Item->GetDataMutable()))
	{
		TSharedRef<FHierarchyDragDropOp> CategoryDragDropOp = MakeShared<FHierarchyDragDropOp>(Item);
		CategoryDragDropOp->Construct();
		return CategoryDragDropOp;
	}

	return MakeShared<FHierarchyDragDropOp>(nullptr);
}

TArray<TTuple<UClass*, FOnGetDetailCustomizationInstance>> UNiagaraUserParametersHierarchyViewModel::GetInstanceCustomizations()
{
	return {{UNiagaraScriptVariable::StaticClass(), FOnGetDetailCustomizationInstance::CreateStatic(&FNiagaraScriptVariableHierarchyDetails::MakeInstance)}};
}

#undef LOCTEXT_NAMESPACE
