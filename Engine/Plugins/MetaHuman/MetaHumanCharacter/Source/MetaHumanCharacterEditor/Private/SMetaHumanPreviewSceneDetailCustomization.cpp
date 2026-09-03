// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMetaHumanPreviewSceneDetailCustomization.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "SMetaHumanCharacterEditorPreviewSettingsView.h"
#include "IPropertyUtilities.h"
#include "IDetailGroup.h"

TSharedRef<IDetailCustomization> FMetaHumanPreviewSceneCustomization::MakeInstance()
{
	return MakeShareable(new FMetaHumanPreviewSceneCustomization());
}

void FMetaHumanPreviewSceneCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);

	if (Objects.Num() > 0)
	{
		TWeakObjectPtr<UObject> PreviewDescriptionObject = Objects[0];
		TAttribute<bool> IsEnabled = TAttribute<bool>::Create([this, PreviewDescriptionObject]()
			{
				if (UMetaHumanCharacterEditorPreviewSceneDescription* PreviewDescription = Cast<UMetaHumanCharacterEditorPreviewSceneDescription>(PreviewDescriptionObject.Get()))
				{
					return PreviewDescription->bAnimationControllerEnabled;
				}
				return false;
			});

		IDetailCategoryBuilder& Category = DetailBuilder.EditCategory("Animation", FText::GetEmpty(), ECategoryPriority::Default);
		TSharedPtr<IPropertyHandle> AnimationControllerHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaHumanCharacterEditorPreviewSceneDescription, AnimationController));
		Category.AddProperty(AnimationControllerHandle).IsEnabled(IsEnabled);
	}
}