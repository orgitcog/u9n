// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPropertyTypeCustomization.h"

#include "IDetailChildrenBuilder.h"
#include "IDetailPropertyRow.h"
#include "LiveLinkHubMessages.h"
#include "CoreMinimal.h"
#include "PropertyHandle.h"

/**
 * Customization for the CustomTimeStep settings.
 */
class FLiveLinkHubCustomTimeStepSettingsCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance()
	{
		return MakeShared<FLiveLinkHubCustomTimeStepSettingsCustomization>();
	}

	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
	{

	}

	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override
	{
		uint32 NumberOfChild;
		if (PropertyHandle->GetNumChildren(NumberOfChild) == FPropertyAccess::Success)
		{
			for (uint32 Index = 0; Index < NumberOfChild; ++Index)
			{
				TSharedRef<IPropertyHandle> ChildPropertyHandle = PropertyHandle->GetChildHandle(Index).ToSharedRef();

				if (ChildPropertyHandle->GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(FLiveLinkHubCustomTimeStepSettings, SubjectName))
				{
					FIsResetToDefaultVisible IsResetToDefaultVisible = FIsResetToDefaultVisible::CreateLambda(
						[](TSharedPtr<IPropertyHandle> PropertyHandle)
						{ 
							FName Value;
							PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FLiveLinkSubjectName, Name))->GetValue(Value);
							return Value != NAME_None;
						});

					FResetToDefaultHandler ResetToDefaultHandler = FResetToDefaultHandler::CreateLambda(
						[](TSharedPtr<IPropertyHandle> PropertyHandle)
						{
							PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FLiveLinkSubjectName, Name))->SetValue(NAME_None);
						});

					const FResetToDefaultOverride Override = FResetToDefaultOverride::Create(IsResetToDefaultVisible, ResetToDefaultHandler);

					ChildBuilder.AddProperty(ChildPropertyHandle)
						.OverrideResetToDefault(Override);
				}
				else
				{
					ChildBuilder.AddProperty(ChildPropertyHandle);
				}
			}
		}
	}
};
