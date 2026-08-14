// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanParameterMappingTable.h"

#include "HAL/IConsoleManager.h"
#include "HAL/PlatformProperties.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "Misc/ConfigCacheIni.h"
#include "PerQualityLevelProperties.h"
#include "UObject/UnrealType.h"

#if WITH_EDITOR
#include "Interfaces/ITargetPlatform.h"
#endif

bool FMetaHumanParameterValue::operator==(const FMetaHumanParameterValue& Other) const
{
	if (Type != Other.Type)
	{
		return false;
	}

	// Data for inactive types is ignored
	switch (Type)
	{
		case EMetaHumanParameterValueType::Invalid:
			// No data to compare
			return true;

		case EMetaHumanParameterValueType::Texture:
			return TextureValue == Other.TextureValue;

		case EMetaHumanParameterValueType::Name:
			return NameValue == Other.NameValue;

		case EMetaHumanParameterValueType::Color:
			return ColorValue == Other.ColorValue;

		case EMetaHumanParameterValueType::Float:
			return FloatValue == Other.FloatValue;

		case EMetaHumanParameterValueType::Bool:
			return bBoolValue == Other.bBoolValue;

		default:
			// Unhandled type
			checkNoEntry();
			return false;
	}
}

bool FMetaHumanParameterValue::Matches(const FMetaHumanParameterMappingInput& MappingInput) const
{
	if (!ensureMsgf(MappingInput.Type == EMetaHumanParameterMappingInputSourceType::Parameter,
		TEXT("Comparing a parameter value to a mapping input that's not a parameter. Mapping input name is %s"),
		*MappingInput.Name.ToString()))
	{
		return false;
	}

	// Data for inactive types is ignored
	switch (Type)
	{
		case EMetaHumanParameterValueType::Invalid:
			// Can't match with an invalid value
			return false;

		case EMetaHumanParameterValueType::Texture:
			// Textures are not (yet?) supported as an input type
			return false;

		case EMetaHumanParameterValueType::Name:
			return NameValue == MappingInput.NameValue;

		case EMetaHumanParameterValueType::Color:
			// Colors are not (yet?) supported as an input type
			return false;

		case EMetaHumanParameterValueType::Float:
			return FloatValue == MappingInput.FloatValue;

		case EMetaHumanParameterValueType::Bool:
			return bBoolValue == MappingInput.bBoolValue;

		default:
			// Unhandled type
			checkNoEntry();
			return false;
	}
}

FMetaHumanCompiledParameterMappingTable::FMetaHumanCompiledParameterMappingTable(
	TArray<FMetaHumanParameterMapping>&& InMappings,
	TMap<FName, FMetaHumanScalabilityValueSet>&& InReachableScalabilityValues)
: Mappings(InMappings)
, ReachableScalabilityValues(InReachableScalabilityValues)
{
}

void FMetaHumanCompiledParameterMappingTable::Evaluate(
	const TMap<FName, FMetaHumanParameterValue>& TableInputParameters,
	const TArray<FMetaHumanParameterMappingInput>& ConsoleVariableOverrides,
	const FOutputParameterDelegate& OutputParameterDelegate) const
{
#if WITH_EDITOR
	// Use the running platform (i.e. the editor platform) if no target platform was set when this
	// table was compiled.
	const FName ParameterMappingPlatformName = TargetPlatformName != NAME_None
		? TargetPlatformName
		: FPlatformProperties::PlatformName();
#endif

	for (const FMetaHumanParameterMapping& Mapping : Mappings)
	{
		int32 MatchingRowIndex = INDEX_NONE;
		TSet<FName> OutOfRangeScalabilityVariables;

		for (int32 RowIndex = 0; RowIndex < Mapping.Rows.Num(); RowIndex++)
		{
			const FMetaHumanParameterMappingRow& Row = Mapping.Rows[RowIndex];

			bool bRowMatches = true;

			for (const FMetaHumanParameterMappingInput& RowInput : Row.InputParameters)
			{
				bool bInputMatches = true;

				switch (RowInput.Type)
				{
					case EMetaHumanParameterMappingInputSourceType::Parameter:
					{
						const FMetaHumanParameterValue* TableInputValue = TableInputParameters.Find(RowInput.Name);
						if (!TableInputValue)
						{
							// TODO: Throw error: A required parameter has not been passed in
							bInputMatches = false;
							break;
						}

						if (!TableInputValue->Matches(RowInput))
						{
							bInputMatches = false;
						}
					}
					break;

					case EMetaHumanParameterMappingInputSourceType::Scalability:
					{
						const IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(*RowInput.Name.ToString());
						if (!CVar)
						{
							// TODO: Throw error: Couldn't find cvar

							bInputMatches = false;
							break;
						}

						if (!CVar->TestFlags(static_cast<EConsoleVariableFlags>(ECVF_Scalability | ECVF_ScalabilityGroup)))
						{
							// TODO: Throw error: Not a scalability cvar. Use ConsoleVariable type instead.

							bInputMatches = false;
							break;
						}

						const FMetaHumanParameterMappingInput* Override =
							Algo::FindBy(ConsoleVariableOverrides, RowInput.Name, &FMetaHumanParameterMappingInput::Name);

						int32 ValueToUse;

						if (CVar->IsVariableInt())
						{
							if (Override)
							{
								ValueToUse = FMath::RoundToInt32(Override->FloatValue);
							}
							else
							{
								ValueToUse = CVar->GetInt();
							}
						}
						else if (CVar->IsVariableFloat())
						{
							if (Override)
							{
								ValueToUse = FMath::RoundToInt32(Override->FloatValue);
							}
							else
							{
								ValueToUse = FMath::RoundToInt32(CVar->GetFloat());
							}
						}
						else
						{
							// TODO: Throw error: Unsupported cvar type
							bInputMatches = false;
							break;
						}

						if (!OutOfRangeScalabilityVariables.Contains(RowInput.Name))
						{
							if (const FMetaHumanScalabilityValueSet* ReachableValues = ReachableScalabilityValues.Find(RowInput.Name))
							{
								if (!ReachableValues->Values.Contains(ValueToUse))
								{
									// Don't warn about this variable again
									OutOfRangeScalabilityVariables.Add(RowInput.Name);

									// TODO: Improve this message. Note that this error may show during PIE if the CO was compiled for a specific target platform. This is not necessarily a problem.
									UE_LOG(LogTemp, Error, TEXT("Scalability variable %s is set to %i, which is outside the range of expected values"),
										*RowInput.Name.ToString(), ValueToUse);
								}
							}
						}

						if (!FMath::IsNearlyEqual(static_cast<float>(ValueToUse), RowInput.FloatValue))
						{
							bInputMatches = false;
						}
					}
					break;

					case EMetaHumanParameterMappingInputSourceType::Platform:
					{
						// On cooked platforms, platform inputs should be stripped from the mapping at cook time
						check(!FPlatformProperties::RequiresCookedData());

#if WITH_EDITOR
						const FMetaHumanParameterMappingInput* Override =
							Algo::FindBy(ConsoleVariableOverrides, TEXT("Platform"), &FMetaHumanParameterMappingInput::Name);

						if (Override)
						{
							if (Override->NameValue != RowInput.Name)
							{
								bInputMatches = false;
							}
						}
						else
						{
							if (ParameterMappingPlatformName != RowInput.Name)
							{
								bInputMatches = false;
							}
						}
#endif // WITH_EDITOR

					}
					break;

					default:
						// TODO: Throw error: Unsupported source type
						bInputMatches = false;
				}

				if (!bInputMatches)
				{
					bRowMatches = false;
					break;
				}
			}

			if (bRowMatches)
			{
				MatchingRowIndex = RowIndex;
				break;
			}
		}

		// The compiler should generate a set of rows where at least one is guaranteed to match,
		// but if it doesn't we handle it as gracefully as possible here.
		if (MatchingRowIndex == INDEX_NONE)
		{
			// The compiler shouldn't allow Rows to be empty.
			//
			// It may be impossible to recover gracefully at this point if Rows is empty, so we
			// can't really continue.
			//
			// For example, if we just leave the output parameter set to its default value, that
			// may not be a valid value for the Pipeline, so it will just cause problems downstream
			// that will be harder to debug.
			check(Mapping.Rows.Num() > 0);

			MatchingRowIndex = 0;

			// TODO: Log an error: Failed to find a matching row
			UE_LOG(LogTemp, Error, TEXT("Failed to find matching row for parameter %s"), *Mapping.ParameterName.ToString());
		}

		// Output parameter from matching row
		const FMetaHumanParameterMappingRow& MatchingRow = Mapping.Rows[MatchingRowIndex];
		OutputParameterDelegate.ExecuteIfBound(Mapping.ParameterName, MatchingRow.Value);
	}
}

#if WITH_EDITOR
bool FMetaHumanParameterMappingTable::TryCompile(
	const TMap<FName, FMetaHumanParameterValue>& ConstantParameters,
	const ITargetPlatform* TargetPlatform,
	FMetaHumanCompiledParameterMappingTable& OutCompiledTable,
	TMap<FName, TArray<FMetaHumanParameterValue>>& OutPossibleParameterValues) const
{
	if (!Table)
	{
		return false;
	}

	struct FMetaHumanParameterMappingGenerationData
	{
		FMetaHumanParameterMapping Mapping;
		TArray<FMetaHumanParameterValue> PossibleValues;
	};

	// Key is parameter name
	TMap<FName, FMetaHumanParameterMappingGenerationData> ParameterMappingData;
	// Key is scalability cvar name
	TMap<FName, FMetaHumanScalabilityValueSet> ReachableScalabilityValues;

	TSet<const void*> NewRowPopulatedValues;
	TSet<FName> ReadParameters;

	const FName ParameterMappingPlatformName = TargetPlatform ? FName(*TargetPlatform->PlatformName()) : NAME_None;

	// Gather all scalability cvars that are read from the table and build a lookup with 
	// their possible values for this platform, so that we can cull any rows that would be
	// unreachable.
	{
		for (const TPair<FName, uint8*>& RowPair : Table->GetRowMap())
		{
			for (const FMetaHumanParameterMappingInputColumnSet& InputColumnSet : InputColumnSets)
			{
				FMetaHumanParameterMappingInput Input;

				// TODO: The type and name reading code is copied from below. It should be factored out into a function.

				// Type
				{
					const FProperty* TypeProperty = Table->FindTableProperty(InputColumnSet.TypeColumn);
					if (!TypeProperty)
					{
						// TODO: Compile error
						continue;
					}

					const FEnumProperty* TypeEnumProperty = CastField<FEnumProperty>(TypeProperty);
					if (!TypeEnumProperty
						|| TypeEnumProperty->GetEnum() != StaticEnum<EMetaHumanParameterMappingInputSourceType>())
					{
						// TODO: Compile error: Column must be of correct enum type
						continue;
					}

					check(TypeEnumProperty->GetElementSize() == sizeof(EMetaHumanParameterMappingInputSourceType));
					TypeEnumProperty->GetSingleValue_InContainer(RowPair.Value, &Input.Type, 0);
				}

				if (Input.Type != EMetaHumanParameterMappingInputSourceType::Scalability)
				{
					break;
				}

				// Name
				{
					const FProperty* NameProperty = Table->FindTableProperty(InputColumnSet.NameColumn);
					if (!NameProperty)
					{
						// TODO: Compile error
						continue;
					}

					if (NameProperty->IsA<FNameProperty>())
					{
						check(NameProperty->GetElementSize() == sizeof(FName));
						NameProperty->GetSingleValue_InContainer(RowPair.Value, &Input.Name, 0);
					}
					else if (NameProperty->IsA<FStrProperty>())
					{
						FString TempString;
						check(NameProperty->GetElementSize() == sizeof(FString));
						NameProperty->GetSingleValue_InContainer(RowPair.Value, &TempString, 0);
						Input.Name = FName(*TempString);
					}
					else
					{
						// TODO: Compile error: Unsupported column type for parameter name
						continue;
					}
				}

				if (Input.Name != NAME_None)
				{
					ReachableScalabilityValues.FindOrAdd(Input.Name);
				}
			}
		}

		for (TPair<FName, FMetaHumanScalabilityValueSet>& PossibleValuesEntry : ReachableScalabilityValues)
		{
			// If this variable could be set from a section in *Scalability.ini, we need to
			// find out which section it could be set from.
			FString ScalabilitySection;
			if (!PossibleValuesEntry.Key.ToString().StartsWith(TEXT("sg.")))
			{
				FConfigCacheIni* ConfigSystemPlatform = FConfigCacheIni::ForPlatform(ParameterMappingPlatformName);

				// Load the scalability platform file
				if (const FConfigFile* PlatformScalability = ConfigSystemPlatform->FindConfigFile(GScalabilityIni))
				{
					for (const TPair<FString, FConfigSection>& Section : *PlatformScalability)
					{
						int32 IndexOfDelimiter = INDEX_NONE;
						if (Section.Value.Contains(PossibleValuesEntry.Key)
							&& Section.Key.FindChar(TEXT('@'), IndexOfDelimiter))
						{
							ScalabilitySection = Section.Key.Left(IndexOfDelimiter);

							// It should be safe to assume that a cvar is only set from one
							// scalability group, otherwise the two groups are going to
							// conflict.
							// 
							// Therefore, we only need to find the first section that sets
							// this cvar in order to determine the scalability group that
							// owns it.
							break;
						}
					}
				}
			}

			FPerQualityLevelInt TempProperty;
			TempProperty.SetQualityLevelCVarForCooking(*PossibleValuesEntry.Key.ToString(), *ScalabilitySection);

			// GetSupportedQualityLevels returns all the possible int values this cvar
			// could be set to by device profile or scalability group on this platform.
			//
			// The intention is that they represent scalability quality values, but
			// technically any int value will work. For example, if this cvar is set to 100
			// by a device profile for the current target platform, 100 will be in the list
			// of values returned even though quality levels are usually in the range 0-4.
			PossibleValuesEntry.Value.Values = TempProperty.GetSupportedQualityLevels(*ParameterMappingPlatformName.ToString()).Array();
				
			// Sort the entries, so that they display nicely in the editor UI
			PossibleValuesEntry.Value.Values.Sort();

			if (PossibleValuesEntry.Value.Values.Num() == 0)
			{
				// TODO: Compile error: No reachable values detected for cvar
			}
		}
	}

	for (const TPair<FName, uint8*>& RowPair : Table->GetRowMap())
	{
		for (const FMetaHumanParameterMappingOutputColumnSet& OutputColumnSet : OutputColumnSets)
		{
			FName WrittenParameterName;
			{
				const FProperty* NameProperty = Table->FindTableProperty(OutputColumnSet.NameColumn);
				if (!NameProperty)
				{
					// TODO: Compile error
					continue;
				}

				if (NameProperty->IsA<FNameProperty>())
				{
					check(NameProperty->GetElementSize() == sizeof(FName));
					NameProperty->GetSingleValue_InContainer(RowPair.Value, &WrittenParameterName, 0);
				}
				else if (NameProperty->IsA<FStrProperty>())
				{
					FString TempString;
					check(NameProperty->GetElementSize() == sizeof(FString));
					NameProperty->GetSingleValue_InContainer(RowPair.Value, &TempString, 0);
					WrittenParameterName = FName(*TempString);
				}
				else
				{
					// TODO: Compile error: Unsupported column type for parameter name
					continue;
				}
			}

			// TODO: Set default value (or remove this and make it a row instead)
			FMetaHumanParameterMappingGenerationData& MappingGenerationData = ParameterMappingData.FindOrAdd(WrittenParameterName);
			if (MappingGenerationData.Mapping.ParameterName == NAME_None)
			{
				MappingGenerationData.Mapping.ParameterName = WrittenParameterName;
			}

			FMetaHumanParameterMappingRow NewRow;
			NewRowPopulatedValues.Reset();

			bool bIsNewRowValid = true;

			for (const FName ValueColumnName : OutputColumnSet.ValueColumns)
			{
				const FProperty* ValueProperty = Table->FindTableProperty(ValueColumnName);
				if (!ValueProperty)
				{
					// TODO: Compile error
					continue;
				}

				const void* PopulatedValuePtr;

				if (ValueProperty->IsA<FNameProperty>())
				{
					check(ValueProperty->GetElementSize() == sizeof(FName));
					ValueProperty->GetSingleValue_InContainer(RowPair.Value, &NewRow.Value.NameValue, 0);
					PopulatedValuePtr = &NewRow.Value.NameValue;
				}
				else if (ValueProperty->IsA<FStrProperty>())
				{
					FString TempString;
					check(ValueProperty->GetElementSize() == sizeof(FString));
					ValueProperty->GetSingleValue_InContainer(RowPair.Value, &TempString, 0);

					NewRow.Value.NameValue = FName(*TempString);
					PopulatedValuePtr = &NewRow.Value.NameValue;
				}
				else if (ValueProperty->IsA<FNumericProperty>())
				{
					// We coerce any numerical property to a float here, as floats are the only 
					// numerical type supported by FMetaHumanParameterValue.
					//
					// This parameter value representation is still WIP, so we could support other
					// numerical types in future depending on the eventual implementation.

					const void* ValuePointer = ValueProperty->ContainerPtrToValuePtr<void>(RowPair.Value);

					const FNumericProperty* NumericValueProperty = CastFieldChecked<FNumericProperty>(ValueProperty);
					if (NumericValueProperty->IsFloatingPoint())
					{
						const double TempValue = NumericValueProperty->GetFloatingPointPropertyValue(ValuePointer);
						NewRow.Value.FloatValue = static_cast<float>(TempValue);
					}
					else 
					{
						const int64 TempValue = NumericValueProperty->GetSignedIntPropertyValue(ValuePointer);
						NewRow.Value.FloatValue = static_cast<float>(TempValue);
					}

					PopulatedValuePtr = &NewRow.Value.FloatValue;
				}
				else if (ValueProperty->IsA<FBoolProperty>())
				{
					check(ValueProperty->GetElementSize() == sizeof(bool));
					ValueProperty->GetSingleValue_InContainer(RowPair.Value, &NewRow.Value.bBoolValue, 0);
					PopulatedValuePtr = &NewRow.Value.bBoolValue;
				}
				else if (ValueProperty->IsA<FSoftObjectProperty>())
				{
					check(ValueProperty->GetElementSize() == sizeof(TSoftObjectPtr<UTexture2D>));
					ValueProperty->GetSingleValue_InContainer(RowPair.Value, &NewRow.Value.TextureValue, 0);
					PopulatedValuePtr = &NewRow.Value.TextureValue;
				}
				else if (ValueProperty->IsA<FObjectPropertyBase>())
				{
					UObject* TempObject = CastFieldChecked<FObjectPropertyBase>(ValueProperty)->LoadObjectPropertyValue_InContainer(RowPair.Value);
					if (!TempObject)
					{
						continue;
					}

					UTexture2D* Texture = Cast<UTexture2D>(TempObject);
					if (!Texture)
					{
						// TODO: Compile error: Object not a texture
						continue;
					}

					NewRow.Value.TextureValue = TSoftObjectPtr<UTexture2D>(Texture);
					PopulatedValuePtr = &NewRow.Value.TextureValue;
				}
				else if (ValueProperty->IsA<FStructProperty>())
				{
					const FStructProperty* StructProperty = CastFieldChecked<FStructProperty>(ValueProperty);
					if (StructProperty->Struct == TBaseStructure<FLinearColor>::Get())
					{
						check(ValueProperty->GetElementSize() == sizeof(FLinearColor));
						ValueProperty->GetSingleValue_InContainer(RowPair.Value, &NewRow.Value.ColorValue, 0);
						PopulatedValuePtr = &NewRow.Value.ColorValue;
					}
					else
					{
						// TODO: Compile error: Unsupported column type for parameter value
						continue;
					}
				}
				else
				{
					// TODO: Compile error: Unsupported column type for parameter value
					continue;
				}

				bool bIsAlreadyInSet = false;
				NewRowPopulatedValues.FindOrAdd(PopulatedValuePtr, &bIsAlreadyInSet);
				if (bIsAlreadyInSet)
				{
					// TODO: Compile error: Two value columns of same type
				}
			}

			NewRowPopulatedValues.Reset();

			for (const FMetaHumanParameterMappingInputColumnSet& InputColumnSet : InputColumnSets)
			{
				FMetaHumanParameterMappingInput Input;
						
				// Type
				{
					const FProperty* TypeProperty = Table->FindTableProperty(InputColumnSet.TypeColumn);
					if (!TypeProperty)
					{
						// TODO: Compile error
						continue;
					}

					const FEnumProperty* TypeEnumProperty = CastField<FEnumProperty>(TypeProperty);
					if (!TypeEnumProperty
						|| TypeEnumProperty->GetEnum() != StaticEnum<EMetaHumanParameterMappingInputSourceType>())
					{
						// TODO: Compile error: Column must be of correct enum type
						continue;
					}

					check(TypeEnumProperty->GetElementSize() == sizeof(EMetaHumanParameterMappingInputSourceType));
					TypeEnumProperty->GetSingleValue_InContainer(RowPair.Value, &Input.Type, 0);
				}

				// Name
				{
					const FProperty* NameProperty = Table->FindTableProperty(InputColumnSet.NameColumn);
					if (!NameProperty)
					{
						// TODO: Compile error
						continue;
					}

					if (NameProperty->IsA<FNameProperty>())
					{
						check(NameProperty->GetElementSize() == sizeof(FName));
						NameProperty->GetSingleValue_InContainer(RowPair.Value, &Input.Name, 0);
					}
					else if (NameProperty->IsA<FStrProperty>())
					{
						FString TempString;
						check(NameProperty->GetElementSize() == sizeof(FString));
						NameProperty->GetSingleValue_InContainer(RowPair.Value, &TempString, 0);
						Input.Name = FName(*TempString);
					}
					else
					{
						// TODO: Compile error: Unsupported column type for parameter name
						continue;
					}
				}

				if (Input.Name == NAME_None)
				{
					// An empty name means this input column set is a wildcard
					continue;
				}

				if (Input.Type == EMetaHumanParameterMappingInputSourceType::Parameter)
				{
					ReadParameters.Add(Input.Name);
				}

				bool bFoundNumericValueColumn = false;
				for (const FName ValueColumnName : InputColumnSet.ValueColumns)
				{
					const FProperty* ValueProperty = Table->FindTableProperty(ValueColumnName);
					if (!ValueProperty)
					{
						// TODO: Compile error
						continue;
					}

					const void* PopulatedValuePtr;

					if (ValueProperty->IsA<FNameProperty>())
					{
						check(ValueProperty->GetElementSize() == sizeof(FName));
						ValueProperty->GetSingleValue_InContainer(RowPair.Value, &Input.NameValue, 0);
						PopulatedValuePtr = &Input.NameValue;
					}
					else if (ValueProperty->IsA<FStrProperty>())
					{
						FString TempString;
						check(ValueProperty->GetElementSize() == sizeof(FString));
						ValueProperty->GetSingleValue_InContainer(RowPair.Value, &TempString, 0);

						Input.NameValue = FName(*TempString);
						PopulatedValuePtr = &Input.NameValue;
					}
					else if (ValueProperty->IsA<FNumericProperty>())
					{
						// We coerce any numerical property to a float here, as floats are the only 
						// numerical type supported by FMetaHumanParameterValue.
						//
						// This parameter value representation is still WIP, so we could support other
						// numerical types in future depending on the eventual implementation.

						const void* ValuePointer = ValueProperty->ContainerPtrToValuePtr<void>(RowPair.Value);

						const FNumericProperty* NumericValueProperty = CastFieldChecked<FNumericProperty>(ValueProperty);
						if (NumericValueProperty->IsFloatingPoint())
						{
							const double TempValue = NumericValueProperty->GetFloatingPointPropertyValue(ValuePointer);
							Input.FloatValue = static_cast<float>(TempValue);
						}
						else
						{
							const int64 TempValue = NumericValueProperty->GetSignedIntPropertyValue(ValuePointer);
							Input.FloatValue = static_cast<float>(TempValue);
						}

						PopulatedValuePtr = &Input.FloatValue;
						bFoundNumericValueColumn = true;
					}
					else if (ValueProperty->IsA<FBoolProperty>())
					{
						check(ValueProperty->GetElementSize() == sizeof(bool));
						ValueProperty->GetSingleValue_InContainer(RowPair.Value, &Input.bBoolValue, 0);
						PopulatedValuePtr = &Input.bBoolValue;
					}
					else
					{
						// TODO: Compile error: Unsupported column type for parameter value
						continue;
					}

					bool bIsAlreadyInSet = false;
					NewRowPopulatedValues.FindOrAdd(PopulatedValuePtr, &bIsAlreadyInSet);
					if (bIsAlreadyInSet)
					{
						// TODO: Compile error: Two value columns of same type
					}
				}

				if (Input.Type == EMetaHumanParameterMappingInputSourceType::Scalability
					&& !bFoundNumericValueColumn)
				{
					// TODO: Compile error: Scalability values are numeric, so there must be a numeric value to compare them against
				}

				// If this row is unreachable for this platform, it can be culled from the 
				// compiled mapping, which reduces the set of possible values that a mapped
				// parameter can be set to.
				//
				// This should allow us to produce more optimal built data in future.
				bool bShouldIncludeThisInput = true;
				{
					// If Type is platform and we're cooking, cull this row if it doesn't match
					// the current target platform.
					if (Input.Type == EMetaHumanParameterMappingInputSourceType::Platform)
					{
						if (ParameterMappingPlatformName == Input.Name)
						{
							// All non-matching rows will be culled, so no need to evaluate
							// platform at runtime.
							bShouldIncludeThisInput = false;
						}
						else
						{
							bIsNewRowValid = false;
							break;
						}
					}
					else if (Input.Type == EMetaHumanParameterMappingInputSourceType::Scalability)
					{
						// The name should always be found, as this map is populated by
						// scanning the table
						const FMetaHumanScalabilityValueSet& PossibleValues = ReachableScalabilityValues[Input.Name];
						if (!PossibleValues.Values.Contains(FMath::RoundToInt32(Input.FloatValue)))
						{
							// This scalability variable can never be set to the value
							// specified by this row on this platform, so the row is
							// redundant.
							bIsNewRowValid = false;
							break;
						}
					}
					else if (Input.Type == EMetaHumanParameterMappingInputSourceType::Parameter)
					{
						const FMetaHumanParameterValue* ConstantParameterValue = ConstantParameters.Find(Input.Name);
						if (ConstantParameterValue)
						{
							// This row refers to a parameter that has been made constant at
							// compile time.

							if (ConstantParameterValue->Matches(Input))
							{
								// Keep this row and skip the evaluation of this parameter at runtime
								bShouldIncludeThisInput = false;
							}
							else
							{
								// The parameter value for this row doesn't match the constant that 
								// the parameter is being locked to, so this row can never be
								// activated at runtime.
								bIsNewRowValid = false;
								break;
							}
						}
					}
				}

				if (bShouldIncludeThisInput)
				{
					NewRow.InputParameters.Add(Input);
				}
			}

			if (bIsNewRowValid)
			{
				MappingGenerationData.Mapping.Rows.Add(NewRow);
			}
		}
	}

	for (const FName ReadParameter : ReadParameters)
	{
		if (ParameterMappingData.Contains(ReadParameter))
		{
			// TODO: Compile error: Can't read and write to the same parameter
		}
	}

	TArray<FMetaHumanParameterMapping> CompiledMapping;
	CompiledMapping.Reset(ParameterMappingData.Num());
	OutPossibleParameterValues.Empty(ParameterMappingData.Num());

	for (TPair<FName, FMetaHumanParameterMappingGenerationData>& Pair : ParameterMappingData)
	{
		check(Pair.Value.PossibleValues.Num() == 0);

		for (const FMetaHumanParameterMappingRow& Row : Pair.Value.Mapping.Rows)
		{
			Pair.Value.PossibleValues.AddUnique(Row.Value);
		}

		if (Pair.Value.PossibleValues.Num() == 0)
		{
			// TODO: Compile error: All rows were culled. This can happen if e.g. a parameter is only set on a certain platform and we're compiling for a different platform
		}

		CompiledMapping.Add(MoveTemp(Pair.Value.Mapping));
		OutPossibleParameterValues.Add(Pair.Key, Pair.Value.PossibleValues);
	}

	OutCompiledTable = FMetaHumanCompiledParameterMappingTable(MoveTemp(CompiledMapping), MoveTemp(ReachableScalabilityValues));
	return true;
}
#endif // WITH_EDITOR

bool FMetaHumanParameterMappingTable::IsValid() const
{
	return Table != nullptr;
}
