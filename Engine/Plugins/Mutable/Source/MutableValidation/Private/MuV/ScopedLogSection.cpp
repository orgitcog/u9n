// Copyright Epic Games, Inc. All Rights Reserved.

#include "ScopedLogSection.h"
#include "Logging/LogMacros.h"
#include "UObject/NameTypes.h"


DECLARE_LOG_CATEGORY_EXTERN(LogMutableValidation, Log, All);
DEFINE_LOG_CATEGORY(LogMutableValidation);


FScopedLogSection::FScopedLogSection(EMutableLogSection Section, const FName& InSectionTarget)
{
	// Set the current section handled by this object
	CurrentSection = Section;

	// Cache the name of the current target. Will only be displayed for sections of type Object.
	if (!InSectionTarget.IsNone())
	{
		SectionObject = InSectionTarget;
	}

	if (CurrentSection == EMutableLogSection::Object)
	{
		UE_LOG(LogMutableValidation, Log, TEXT(" SECTION START : %s - [%s] "), *GetLogSectionName(CurrentSection), *SectionObject.ToString());
	}
	else
	{
		UE_LOG(LogMutableValidation, Log, TEXT(" SECTION START : %s "), *GetLogSectionName(CurrentSection));
	}
}


FScopedLogSection::~FScopedLogSection()
{
	if (CurrentSection == EMutableLogSection::Object)
	{
		UE_LOG(LogMutableValidation, Log, TEXT(" SECTION END : %s - [%s] "), *GetLogSectionName(CurrentSection), *SectionObject.ToString());
	}
	else
	{
		UE_LOG(LogMutableValidation, Log, TEXT(" SECTION END : %s "), *GetLogSectionName(CurrentSection));
	}
	
	// Set the current section to none (undefined)
	CurrentSection = EMutableLogSection::Undefined;
}


	
FString FScopedLogSection::GetLogSectionName(EMutableLogSection Section) const
{
	FString Output (TEXT("unknown"));
	
	 switch(Section)
	 {
	 case EMutableLogSection::Undefined:
	 	Output = TEXT("undefined");
 		break;
	 case EMutableLogSection::Compilation:
	 	Output = TEXT("compilation");
 		break;
	 case EMutableLogSection::Update:
	 	Output = TEXT("update");
 		break;
	 case EMutableLogSection::Bake:
	 	Output = TEXT("bake");
 		break;
	 case EMutableLogSection::Object:
	 	Output = TEXT("object");
	 	break;
	 default:
	 	checkNoEntry();
	 	break;
	 }

	return Output;
}

