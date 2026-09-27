// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Internationalization/Text.h"
#include "Templates/Function.h"
#include "Templates/SharedPointer.h"
#include "ILauncherProfile.h"


class SWidget;
struct EVisibility;
namespace ProjectLauncher
{
	class ILaunchProfileTreeBuilder;
	class FLaunchProfileTreeData;
	class FLaunchProfileTreeNode;
	class FLaunchExtensionInstance;
	class FModel;

	typedef TSharedPtr<FLaunchProfileTreeData> FLaunchProfileTreeDataPtr;
	typedef TSharedRef<FLaunchProfileTreeData> FLaunchProfileTreeDataRef;

	typedef TSharedPtr<FLaunchProfileTreeNode> FLaunchProfileTreeNodePtr;
	typedef TSharedRef<FLaunchProfileTreeNode> FLaunchProfileTreeNodeRef;


	class PROJECTLAUNCHER_API FLaunchProfileTreeNode : public TSharedFromThis<FLaunchProfileTreeNode>
	{
	public:
		// construction
		FLaunchProfileTreeNode( FLaunchProfileTreeData* InTreeData );

		typedef TFunction<void()> FFunction;
		typedef TFunction<bool()> FGetBool;
		typedef TFunction<void(bool)> FSetBool;
		typedef TFunction<FString(void)> FGetString;
		typedef TFunction<void(FString)> FSetString;

		// node creation
		struct FCallbacks
		{
			FGetBool IsDefault = nullptr;
			FFunction SetToDefault = nullptr;
			FGetBool IsVisible = nullptr;
			FGetBool IsEnabled = nullptr;
		};
		FLaunchProfileTreeNode& AddWidget( FText InName, FCallbacks&& InWidgetCallbacks, TSharedRef<SWidget> InValueWidget );
		FLaunchProfileTreeNode& AddWidget( FText InName, TSharedRef<SWidget> InValueWidget );


		struct FBooleanCallbacks
		{
			FGetBool GetValue = nullptr;
			FSetBool SetValue = nullptr;
			FGetBool GetDefaultValue = nullptr;
			FGetBool IsVisible = nullptr;
			FGetBool IsEnabled = nullptr;
		};
		FLaunchProfileTreeNode& AddBoolean( FText InName, FBooleanCallbacks&& BooleanCallbacks );

		struct FStringCallbacks
		{
			FGetString GetValue = nullptr;
			FSetString SetValue = nullptr;
			FGetString GetDefaultValue = nullptr;
			FGetBool IsVisible = nullptr;
			FGetBool IsEnabled = nullptr;

		};
		FLaunchProfileTreeNode& AddString( FText InName, FStringCallbacks&& StringCallbacks );
		FLaunchProfileTreeNode& AddDirectoryString( FText InName, FStringCallbacks&& StringCallbacks );
		FLaunchProfileTreeNode& AddCommandLineString( FText InName, FStringCallbacks&& StringCallbacks );



		// member variables
		FText Name;
		TSharedPtr<SWidget> Widget;

		FCallbacks Callbacks;


		TArray<FLaunchProfileTreeNodePtr> Children;
		const FLaunchProfileTreeData* GetTreeData() const { return TreeData; }

	protected:
		FLaunchProfileTreeData* TreeData;
	};



	class PROJECTLAUNCHER_API FLaunchProfileTreeData : public TSharedFromThis<FLaunchProfileTreeData>
	{
	public:
		// construction
		FLaunchProfileTreeData(ILauncherProfilePtr InProfile, TSharedRef<FModel> InModel, ILaunchProfileTreeBuilder* InTreeBuilder);

		// node creation
		FLaunchProfileTreeNode& AddHeading( FText InName );
		void CreateExtensionsUI();

		void RequestTreeRefresh();

		// member variables
		ILauncherProfilePtr Profile;
		TSharedRef<FModel> Model;
		TArray<FLaunchProfileTreeNodePtr> Nodes;
		ILaunchProfileTreeBuilder* TreeBuilder;
		TArray<TSharedPtr<FLaunchExtensionInstance>> ExtensionInstances;
		bool bHasAnyMenuExtensions = false;
		bool bRequestTreeRefresh = false;
	};


}

