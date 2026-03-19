// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

class FJsonObject;
class IDataLinkEditorMenuContext;
class UToolMenu;
struct FJsonObjectWrapper;
struct FToolMenuContext;
template<typename BaseStructT> struct TConstStructView;

namespace UE::DataLinkJsonEditor
{
	void RegisterMenus();

	void PopulateToolbar(UToolMenu* InToolMenu);

	TConstStructView<FJsonObjectWrapper> GetPreviewOutputDataView(const IDataLinkEditorMenuContext& InMenuContext);

	bool CanMakeStructsFromJson(const FToolMenuContext& InMenuContext);

	void MakeStructsFromJson(const FToolMenuContext& InMenuContext);
}
