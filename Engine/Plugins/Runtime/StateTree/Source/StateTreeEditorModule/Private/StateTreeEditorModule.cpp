// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateTreeEditorModule.h"
#include "Blueprint/StateTreeConditionBlueprintBase.h"
#include "Blueprint/StateTreeConsiderationBlueprintBase.h"
#include "Blueprint/StateTreeEvaluatorBlueprintBase.h"
#include "Blueprint/StateTreeTaskBlueprintBase.h"
#include "StateTreePropertyFunctionBase.h"
#include "Customizations/StateTreeAnyEnumDetails.h"
#include "Customizations/StateTreeEnumValueScorePairsDetails.h"
#include "Customizations/StateTreeEditorColorDetails.h"
#include "Customizations/StateTreeEditorDataDetails.h"
#include "Customizations/StateTreeEditorNodeDetails.h"
#include "Customizations/StateTreeReferenceDetails.h"
#include "Customizations/StateTreeReferenceOverridesDetails.h"
#include "Customizations/StateTreeStateDetails.h"
#include "Customizations/StateTreeStateLinkDetails.h"
#include "Customizations/StateTreeStateParametersDetails.h"
#include "Customizations/StateTreeTransitionDetails.h"
#include "Customizations/StateTreeEventDescDetails.h"
#include "Customizations/StateTreeBindingExtension.h"
#include "Customizations/StateTreeBlueprintPropertyRefDetails.h"
#include "IRewindDebuggerExtension.h"
#include "PropertyEditorModule.h"
#include "StateTree.h"
#include "StateTreeCompiler.h"
#include "StateTreeCompilerLog.h"
#include "StateTreeEditorData.h"
#include "Debugger/StateTreeDebuggerCommands.h"
#include "Debugger/StateTreeRewindDebuggerExtensions.h"
#include "StateTreeDelegates.h"
#include "StateTreeEditingSubsystem.h"
#include "StateTreeEditor.h"
#include "StateTreeEditorCommands.h"
#include "StateTreeEditorStyle.h"
#include "StateTreeNodeClassCache.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "StateTreeEditor"

DEFINE_LOG_CATEGORY(LogStateTreeEditor);

IMPLEMENT_MODULE(FStateTreeEditorModule, StateTreeEditorModule)

namespace UE::StateTree::Editor
{
	// @todo Could we make this a IModularFeature?
	static bool CompileStateTree(UStateTree& StateTree)
	{
		FStateTreeCompilerLog Log;
		return UStateTreeEditingSubsystem::CompileStateTree(&StateTree, Log);
	}

}; // UE::StateTree::Editor

FStateTreeEditorModule& FStateTreeEditorModule::GetModule()
{
	return FModuleManager::LoadModuleChecked<FStateTreeEditorModule>("StateTreeEditorModule");
}

FStateTreeEditorModule* FStateTreeEditorModule::GetModulePtr()
{
	return FModuleManager::GetModulePtr<FStateTreeEditorModule>("StateTreeEditorModule");
}

void FStateTreeEditorModule::StartupModule()
{
	UE::StateTree::Delegates::OnRequestCompile.BindStatic(&UE::StateTree::Editor::CompileStateTree);
	UE::StateTree::Delegates::OnRequestEditorHash.BindLambda([](const UStateTree& InStateTree) -> uint32 { return UStateTreeEditingSubsystem::CalculateStateTreeHash(&InStateTree); });

#if WITH_STATETREE_TRACE_DEBUGGER
	FStateTreeDebuggerCommands::Register();
	RewindDebuggerExtension = MakePimpl<UE::StateTreeDebugger::FRewindDebuggerExtension>();
	IModularFeatures::Get().RegisterModularFeature(IRewindDebuggerExtension::ModularFeatureName, RewindDebuggerExtension.Get());
	RewindDebuggerRuntimeExtension = MakePimpl<UE::StateTreeDebugger::FRewindDebuggerRuntimeExtension>();
	IModularFeatures::Get().RegisterModularFeature(IRewindDebuggerRuntimeExtension::ModularFeatureName, RewindDebuggerRuntimeExtension.Get());
#endif // WITH_STATETREE_TRACE_DEBUGGER

	MenuExtensibilityManager = MakeShareable(new FExtensibilityManager);
	ToolBarExtensibilityManager = MakeShareable(new FExtensibilityManager);

	FStateTreeEditorStyle::Register();
	FStateTreeEditorCommands::Register();

	// Register the details customizer
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomPropertyTypeLayout("StateTreeTransition", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FStateTreeTransitionDetails::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout("StateTreeEventDesc", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FStateTreeEventDescDetails::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout("StateTreeStateLink", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FStateTreeStateLinkDetails::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout("StateTreeEditorNode", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FStateTreeEditorNodeDetails::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout("StateTreeStateParameters", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FStateTreeStateParametersDetails::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout("StateTreeAnyEnum", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FStateTreeAnyEnumDetails::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout("StateTreeReference", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FStateTreeReferenceDetails::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout("StateTreeReferenceOverrides", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FStateTreeReferenceOverridesDetails::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout("StateTreeEditorColorRef", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FStateTreeEditorColorRefDetails::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout("StateTreeEditorColor", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FStateTreeEditorColorDetails::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout("StateTreeBlueprintPropertyRef", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FStateTreeBlueprintPropertyRefDetails::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout("StateTreeEnumValueScorePairs", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FStateTreeEnumValueScorePairsDetails::MakeInstance));
	PropertyModule.RegisterCustomClassLayout("StateTreeState", FOnGetDetailCustomizationInstance::CreateStatic(&FStateTreeStateDetails::MakeInstance));
	PropertyModule.RegisterCustomClassLayout("StateTreeEditorData", FOnGetDetailCustomizationInstance::CreateStatic(&FStateTreeEditorDataDetails::MakeInstance));

	PropertyModule.NotifyCustomizationModuleChanged();
}

void FStateTreeEditorModule::ShutdownModule()
{
	UE::StateTree::Delegates::OnRequestCompile.Unbind();

#if WITH_STATETREE_TRACE_DEBUGGER
	IModularFeatures::Get().UnregisterModularFeature(IRewindDebuggerRuntimeExtension::ModularFeatureName, RewindDebuggerRuntimeExtension.Get());
	IModularFeatures::Get().UnregisterModularFeature(IRewindDebuggerExtension::ModularFeatureName, RewindDebuggerExtension.Get());
	FStateTreeDebuggerCommands::Unregister();
#endif // WITH_STATETREE_TRACE_DEBUGGER
	
	MenuExtensibilityManager.Reset();
	ToolBarExtensibilityManager.Reset();

	FStateTreeEditorStyle::Unregister();
	FStateTreeEditorCommands::Unregister();

	// Unregister the details customization
	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.UnregisterCustomPropertyTypeLayout("StateTreeTransition");
		PropertyModule.UnregisterCustomPropertyTypeLayout("StateTreeEventDesc");
		PropertyModule.UnregisterCustomPropertyTypeLayout("StateTreeStateLink");
		PropertyModule.UnregisterCustomPropertyTypeLayout("StateTreeEditorNode");
		PropertyModule.UnregisterCustomPropertyTypeLayout("StateTreeStateParameters");
		PropertyModule.UnregisterCustomPropertyTypeLayout("StateTreeAnyEnum");
		PropertyModule.UnregisterCustomPropertyTypeLayout("StateTreeReference");
		PropertyModule.UnregisterCustomPropertyTypeLayout("StateTreeReferenceOverrides");
		PropertyModule.UnregisterCustomPropertyTypeLayout("StateTreeEditorColorRef");
		PropertyModule.UnregisterCustomPropertyTypeLayout("StateTreeEditorColor");
		PropertyModule.UnregisterCustomPropertyTypeLayout("StateTreeBlueprintPropertyRef");
		PropertyModule.UnregisterCustomClassLayout("StateTreeState");
		PropertyModule.UnregisterCustomClassLayout("StateTreeEditorData");
		PropertyModule.NotifyCustomizationModuleChanged();
	}
}

TSharedRef<IStateTreeEditor> FStateTreeEditorModule::CreateStateTreeEditor(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UStateTree* StateTree)
{
	TSharedRef<FStateTreeEditor> NewEditor(new FStateTreeEditor());
	NewEditor->InitEditor(Mode, InitToolkitHost, StateTree);
	return NewEditor;
}

void FStateTreeEditorModule::SetDetailPropertyHandlers(IDetailsView& DetailsView)
{
	DetailsView.SetExtensionHandler(MakeShared<FStateTreeBindingExtension>());
	DetailsView.SetChildrenCustomizationHandler(MakeShared<FStateTreeBindingsChildrenCustomization>());
}

TSharedPtr<FStateTreeNodeClassCache> FStateTreeEditorModule::GetNodeClassCache()
{
	if (!NodeClassCache.IsValid())
	{
		NodeClassCache = MakeShareable(new FStateTreeNodeClassCache());
		NodeClassCache->AddRootScriptStruct(FStateTreeEvaluatorBase::StaticStruct());
		NodeClassCache->AddRootScriptStruct(FStateTreeTaskBase::StaticStruct());
		NodeClassCache->AddRootScriptStruct(FStateTreeConditionBase::StaticStruct());
		NodeClassCache->AddRootScriptStruct(FStateTreeConsiderationBase::StaticStruct());
		NodeClassCache->AddRootScriptStruct(FStateTreePropertyFunctionBase::StaticStruct());
		NodeClassCache->AddRootClass(UStateTreeEvaluatorBlueprintBase::StaticClass());
		NodeClassCache->AddRootClass(UStateTreeTaskBlueprintBase::StaticClass());
		NodeClassCache->AddRootClass(UStateTreeConditionBlueprintBase::StaticClass());
		NodeClassCache->AddRootClass(UStateTreeConsiderationBlueprintBase::StaticClass());
		NodeClassCache->AddRootClass(UStateTreeSchema::StaticClass());
	}

	return NodeClassCache;
}

void FStateTreeEditorModule::RegisterEditorDataClass(TNonNullSubclassOf<const UStateTreeSchema> Schema, TNonNullSubclassOf<const UStateTreeEditorData> EditorData)
{
	const bool bContains = EditorDataTypes.ContainsByPredicate(
		[Schema](const FEditorDataType& Other)
		{
			return Other.Schema == Schema.Get();
		});
	if (ensureMsgf(!bContains, TEXT("The type %s is already registered."), *Schema.Get()->GetName()))
	{
		EditorDataTypes.Emplace(TWeakObjectPtr<const UClass>(Schema.Get()), TWeakObjectPtr<const UClass>(EditorData.Get()));
	}
}

void FStateTreeEditorModule::UnregisterEditorDataClass(TNonNullSubclassOf<const UStateTreeSchema> Schema)
{
	EditorDataTypes.RemoveAllSwap(
		[Schema](const FEditorDataType& Other)
		{
			return Other.Schema == Schema.Get();
		});
}

TNonNullSubclassOf<UStateTreeEditorData> FStateTreeEditorModule::GetEditorDataClass(TNonNullSubclassOf<const UStateTreeSchema> Schema) const
{
	const FEditorDataType* FoundEditorData = EditorDataTypes.FindByPredicate(
		[Schema](const FEditorDataType& Other)
		{
			const UClass* OtherSchema = Other.Schema.Get();
			return OtherSchema && Schema.Get()->IsChildOf(OtherSchema);
		});
	const UClass* Result = FoundEditorData && FoundEditorData->EditorData.Get() ? FoundEditorData->EditorData.Get() : UStateTreeEditorData::StaticClass();
	return const_cast<UClass*>(Result); // NewObject wants none const UClass
}


#undef LOCTEXT_NAMESPACE
