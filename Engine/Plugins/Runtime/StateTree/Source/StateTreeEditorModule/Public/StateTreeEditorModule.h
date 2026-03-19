// Copyright Epic Games, Inc. All Rights Reserved.

#pragma  once

#include "Modules/ModuleInterface.h"
#include "Logging/LogMacros.h"
#include "Templates/NonNullSubclassOf.h"
#include "Templates/PimplPtr.h"
#include "Toolkits/AssetEditorToolkit.h"

class UStateTree;
class UStateTreeEditorData;
class UStateTreeSchema;
class UUserDefinedStruct;
class IStateTreeEditor;
struct FStateTreeNodeClassCache;

namespace UE::StateTreeDebugger
{
	class FRewindDebuggerExtension;
	class FRewindDebuggerRuntimeExtension;
}

STATETREEEDITORMODULE_API DECLARE_LOG_CATEGORY_EXTERN(LogStateTreeEditor, Log, All);

/**
* The public interface to this module
*/
class STATETREEEDITORMODULE_API FStateTreeEditorModule : public IModuleInterface, public IHasMenuExtensibility, public IHasToolBarExtensibility
{
public:
	//~Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~End IModuleInterface

	/** Gets this module, will attempt to load and should always exist. */
	static FStateTreeEditorModule& GetModule();

	/** Gets this module, will not attempt to load and may not exist. */
	static FStateTreeEditorModule* GetModulePtr();

	/** Creates an instance of StateTree editor. Only virtual so that it can be called across the DLL boundary. */
	virtual TSharedRef<IStateTreeEditor> CreateStateTreeEditor(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UStateTree* StateTree);

	/** Sets the Details View with required State Tree Detail Property Handlers */
	static void SetDetailPropertyHandlers(IDetailsView& DetailsView);

	virtual TSharedPtr<FExtensibilityManager> GetMenuExtensibilityManager() override { return MenuExtensibilityManager; }
	virtual TSharedPtr<FExtensibilityManager> GetToolBarExtensibilityManager() override { return ToolBarExtensibilityManager; }

	TSharedPtr<FStateTreeNodeClassCache> GetNodeClassCache();
	
	DECLARE_EVENT_OneParam(FStateTreeEditorModule, FOnRegisterLayoutExtensions, FLayoutExtender&);
	FOnRegisterLayoutExtensions& OnRegisterLayoutExtensions() { return RegisterLayoutExtensions; }

	/** Register the editor data type for a specific schema. */
	void RegisterEditorDataClass(TNonNullSubclassOf<const UStateTreeSchema> Schema, TNonNullSubclassOf<const UStateTreeEditorData> EditorData);
	/** Unregister the editor data type for a specific schema. */
	void UnregisterEditorDataClass(TNonNullSubclassOf<const UStateTreeSchema> Schema);
	/** Get the editor data type for a specific schema. */
	TNonNullSubclassOf<UStateTreeEditorData> GetEditorDataClass(TNonNullSubclassOf<const UStateTreeSchema> Schema) const;

protected:
	TSharedPtr<FExtensibilityManager> MenuExtensibilityManager;
	TSharedPtr<FExtensibilityManager> ToolBarExtensibilityManager;
	TSharedPtr<FStateTreeNodeClassCache> NodeClassCache;

	struct FEditorDataType
	{
		TWeakObjectPtr<const UClass> Schema;
		TWeakObjectPtr<const UClass> EditorData;
	};
	TArray<FEditorDataType> EditorDataTypes;

#if WITH_STATETREE_TRACE_DEBUGGER
	TPimplPtr<UE::StateTreeDebugger::FRewindDebuggerExtension> RewindDebuggerExtension;
	TPimplPtr<UE::StateTreeDebugger::FRewindDebuggerRuntimeExtension> RewindDebuggerRuntimeExtension;
#endif  // WITH_STATETREE_TRACE_DEBUGGER

	FDelegateHandle OnUserDefinedStructReinstancedHandle;
	FOnRegisterLayoutExtensions RegisterLayoutExtensions;
};
