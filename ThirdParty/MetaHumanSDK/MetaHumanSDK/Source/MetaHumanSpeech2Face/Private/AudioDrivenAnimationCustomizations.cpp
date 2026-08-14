// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR

#include "AudioDrivenAnimationCustomizations.h"

#include "AudioDrivenAnimationConfig.h"

#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"

namespace UE::MetaHuman::Private
{
TSharedRef<IPropertyTypeCustomization> FAudioSolveOverridesPropertyTypeCustomization::MakeInstance()
{
	return MakeShared<FAudioSolveOverridesPropertyTypeCustomization>();
}

void FAudioSolveOverridesPropertyTypeCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	HeaderRow
		.NameContent()[StructPropertyHandle->CreatePropertyNameWidget()]
		.ValueContent()[StructPropertyHandle->CreatePropertyValueWidget()];
}

void FAudioSolveOverridesPropertyTypeCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	TSharedRef<IPropertyHandle> MoodPropertyHandle = StructPropertyHandle->GetChildHandle(
		GET_MEMBER_NAME_CHECKED(FAudioDrivenAnimationSolveOverrides, Mood)
	).ToSharedRef();

	StructBuilder.AddProperty(MoodPropertyHandle);

	TSharedRef<IPropertyHandle> MoodIntensityPropertyHandle = StructPropertyHandle->GetChildHandle(
		GET_MEMBER_NAME_CHECKED(FAudioDrivenAnimationSolveOverrides, MoodIntensity)
	).ToSharedRef();

	StructBuilder
		.AddProperty(MoodIntensityPropertyHandle)
		.IsEnabled(
			MakeAttributeLambda(
				[MoodPropertyHandle]()
				{
					uint8 MoodPropertyValue;
					const FPropertyAccess::Result Result = MoodPropertyHandle->GetValue(MoodPropertyValue);

					if (Result == FPropertyAccess::Success)
					{
						EAudioDrivenAnimationMood Mood = static_cast<EAudioDrivenAnimationMood>(MoodPropertyValue);
						// We disable the mood intensity property for the neutral mood, it has no meaning there.
						return Mood != EAudioDrivenAnimationMood::Neutral;
					}

					return false;
				}
			)
		);
}
} // namespace UE::MetaHuman::Private

#endif // WITH_EDITOR
