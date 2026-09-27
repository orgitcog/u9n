// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/NameTypes.h"
#include "Templates/SharedPointer.h"
#include "UObject/ObjectPtr.h"
#include "Textures/SlateIcon.h"

class FUICommandInfo;
class UInteractiveToolBuilder;
class FUICommandList;
class UInteractiveTool;

namespace UE::Dataflow
{
	class DATAFLOWEDITOR_API FDataflowToolRegistry
	{
	public:

		// Interface for binding/unbinding tool actions. When a tool begins or ends we switch out the currently available FUICommandList. This allows multiple tools to have
		// individual hotkey actions with the same key chords, for example.
		class DATAFLOWEDITOR_API IDataflowToolActionCommands
		{
		public:
			virtual ~IDataflowToolActionCommands() = default;
			virtual void UnbindActiveCommands(const TSharedPtr<FUICommandList>& UICommandList) const = 0;
			virtual void BindCommandsForCurrentTool(const TSharedPtr<FUICommandList>& UICommandList, UInteractiveTool* Tool) const = 0;
		};

		static FDataflowToolRegistry& Get();
		static void TearDown();

		void AddNodeToToolMapping(const FName& NodeName, 
			TObjectPtr<UInteractiveToolBuilder> ToolBuilder, 
			const TSharedRef<const IDataflowToolActionCommands>& ToolActionCommands, 
			const FSlateIcon& AddNodeButtonIcon,
			const FText& AddNodeButtonText, 
			const FName& ToolCategory = FName("General"));

		UE_DEPRECATED(5.6, "Please use the version of AddNodeToToolMapping taking AddNodeButtonIcon and AddNodeButtonText")
		void AddNodeToToolMapping(const FName& NodeName,
			TObjectPtr<UInteractiveToolBuilder> ToolBuilder,
			const TSharedRef<const IDataflowToolActionCommands>& ToolActionCommands);

		void RemoveNodeToToolMapping(const FName& NodeName);

		TArray<FName> GetNodeNames() const;

		TSharedPtr<FUICommandInfo>& GetAddNodeCommandForNode(const FName& NodeType);
		const FSlateIcon& GetAddNodeButtonIcon(const FName& NodeType) const;
		const FText& GetAddNodeButtonText(const FName& NodeType) const;

		TSharedPtr<FUICommandInfo>& GetToolCommandForNode(const FName& NodeName);

		/** Return the category the tool has been registered in */
		const FName& GetToolCategoryForNode(const FName& NodeName);

		UInteractiveToolBuilder* GetToolBuilderForNode(const FName& NodeName);
		const UInteractiveToolBuilder* GetToolBuilderForNode(const FName& NodeName) const;

		void UnbindActiveCommands(const TSharedPtr<FUICommandList>& UICommandList) const;
		void BindCommandsForCurrentTool(const TSharedPtr<FUICommandList>& UICommandList, UInteractiveTool* Tool) const;

	private:

		struct FToolInfo
		{
			// Specified when registering the tool
			TObjectPtr<UInteractiveToolBuilder> ToolBuilder;
			TSharedRef<const IDataflowToolActionCommands> ToolActionCommands;
			FSlateIcon AddNodeButtonIcon;
			FText AddNodeButtonText;

			// Constructed automatically in FDataflowEditorCommandsImpl::RegisterCommands
			TSharedPtr<FUICommandInfo> AddNodeCommand;
			TSharedPtr<FUICommandInfo> ToolCommand;

			/** Tool category for filtering */
			FName ToolCategory;
		};

		TMap<FName, FToolInfo> NodeTypeToToolMap;
	
	};

}

