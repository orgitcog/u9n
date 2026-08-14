// Copyright Epic Games, Inc. All Rights Reserved.

#include "ProductionFunctionLibrary.h"

#include "Algo/Find.h"
#include "CineAssembly.h"
#include "CineAssemblyFactory.h"
#include "CineAssemblySchema.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"

const TArray<FCinematicProduction> UProductionFunctionLibrary::GetAllProductions()
{
	const UProductionSettings* const ProductionSettings = GetDefault<UProductionSettings>();
	return ProductionSettings->GetProductions();
}

bool UProductionFunctionLibrary::GetProduction(FGuid ProductionID, FCinematicProduction& Production)
{
	const UProductionSettings* const ProductionSettings = GetDefault<UProductionSettings>();
	TOptional<const FCinematicProduction> FoundProduction = ProductionSettings->GetProduction(ProductionID);
	if (FoundProduction.IsSet())
	{
		Production = FoundProduction.GetValue();
		return true;
	}

	Production.ProductionID.Invalidate();
	return false;
}

bool UProductionFunctionLibrary::GetActiveProduction(FCinematicProduction& Production)
{
	const UProductionSettings* const ProductionSettings = GetDefault<UProductionSettings>();
	TOptional<const FCinematicProduction> FoundProduction = ProductionSettings->GetActiveProduction();
	if (FoundProduction.IsSet())
	{
		Production = FoundProduction.GetValue();
		return true;
	}

	Production.ProductionID.Invalidate();
	return false;
}

void UProductionFunctionLibrary::SetActiveProduction(FCinematicProduction Production)
{
	UProductionSettings* ProductionSettings = GetMutableDefault<UProductionSettings>();
	ProductionSettings->SetActiveProduction(Production.ProductionID);
}

void UProductionFunctionLibrary::SetActiveProductionByID(FGuid ProductionID)
{
	UProductionSettings* ProductionSettings = GetMutableDefault<UProductionSettings>();
	ProductionSettings->SetActiveProduction(ProductionID);
}

void UProductionFunctionLibrary::ClearActiveProduction()
{
	UProductionSettings* ProductionSettings = GetMutableDefault<UProductionSettings>();
	ProductionSettings->SetActiveProduction(FGuid());
}

bool UProductionFunctionLibrary::IsActiveProduction(FGuid ProductionID)
{
	const UProductionSettings* const ProductionSettings = GetDefault<UProductionSettings>();
	return ProductionSettings->IsActiveProduction(ProductionID);
}

void UProductionFunctionLibrary::AddProduction(FCinematicProduction Production)
{
	UProductionSettings* ProductionSettings = GetMutableDefault<UProductionSettings>();
	ProductionSettings->AddProduction(Production);
}

void UProductionFunctionLibrary::DeleteProduction(FGuid ProductionID)
{
	UProductionSettings* ProductionSettings = GetMutableDefault<UProductionSettings>();
	ProductionSettings->DeleteProduction(ProductionID);
}

void UProductionFunctionLibrary::RenameProduction(FGuid ProductionID, FString NewName)
{
	UProductionSettings* ProductionSettings = GetMutableDefault<UProductionSettings>();
	ProductionSettings->RenameProduction(ProductionID, NewName);
}

UCineAssembly* UProductionFunctionLibrary::CreateAssembly(UCineAssemblySchema* Schema, TSoftObjectPtr<UWorld> Level, TSoftObjectPtr<UCineAssembly> ParentAssembly, TMap<FString, FString> Metadata, const FString& Path, const FString& Name, bool bUseDefaultNameFromSchema)
{
	// If the supplied path is a virtual path, strip off the "/All" from the beginning before proceeding
	FString CreatePath = Path;
	if (CreatePath.StartsWith(TEXT("/All/")))
	{
		CreatePath.RemoveFromStart(TEXT("/All"));
	}

	// Now, make sure that the supplied path is actually valid.
	if (!FPackageName::IsValidPath(CreatePath))
	{
		UE_LOG(LogTemp, Error, TEXT("Could not create Cine Assembly asset because the path '%s' is not valid"), *Path);
		return nullptr;
	}

	UCineAssembly* NewAssembly = NewObject<UCineAssembly>(GetTransientPackage(), NAME_None, RF_Transient);
	NewAssembly->SetSchema(Schema);
	NewAssembly->SetLevel(Level);
	NewAssembly->SetParentAssembly(ParentAssembly);

	// Associate the current active production with this assembly
	const UProductionSettings* const ProductionSettings = GetDefault<UProductionSettings>();
	TOptional<const FCinematicProduction> ActiveProduction = ProductionSettings->GetActiveProduction();
	if (ActiveProduction.IsSet())
	{
		NewAssembly->Production = ActiveProduction->ProductionID;
		NewAssembly->ProductionName = ActiveProduction->ProductionName;
	}

	AddMetadataToAssembly(NewAssembly, Metadata);

	if (!Schema || !bUseDefaultNameFromSchema)
	{
		NewAssembly->AssemblyName.Template = Name;
		NewAssembly->AssemblyName.Resolved = FText::FromString(Name);
	}

	UCineAssemblyFactory::CreateConfiguredAssembly(NewAssembly, CreatePath);

	return NewAssembly;
}

void UProductionFunctionLibrary::AddMetadataToAssembly(UCineAssembly* Assembly, TMap<FString, FString> Metadata)
{
	const UCineAssemblySchema* Schema = Assembly->GetSchema();

	for (const TPair<FString, FString>& Pair : Metadata)
	{
		const FAssemblyMetadataDesc* MetadataDesc = Schema ? Algo::FindBy(Schema->AssemblyMetadata, Pair.Key, &FAssemblyMetadataDesc::Key) : nullptr;

		if (MetadataDesc)
		{
			if (MetadataDesc->Type == ECineAssemblyMetadataType::String)
			{
				Assembly->SetMetadataAsString(Pair.Key, Pair.Value);
			}
			else if (MetadataDesc->Type == ECineAssemblyMetadataType::Bool)
			{
				Assembly->SetMetadataAsBool(Pair.Key, Pair.Value.ToBool());
			}
			else if (MetadataDesc->Type == ECineAssemblyMetadataType::Integer)
			{
				Assembly->SetMetadataAsInteger(Pair.Key, FCString::Atoi(*Pair.Value));
			}
			else if (MetadataDesc->Type == ECineAssemblyMetadataType::Float)
			{
				Assembly->SetMetadataAsFloat(Pair.Key, FCString::Atof(*Pair.Value));
			}
			else if (MetadataDesc->Type == ECineAssemblyMetadataType::AssetPath)
			{
				Assembly->SetMetadataAsString(Pair.Key, Pair.Value);
			}
			else if (MetadataDesc->Type == ECineAssemblyMetadataType::CineAssembly)
			{
				Assembly->SetMetadataAsString(Pair.Key, Pair.Value);
			}
		}
		else
		{
			// If the metadata string does not match anything specified by the schema, simply add the metadata as a string.
			Assembly->InstanceMetadata.Add(*Pair.Key, Pair.Value);
			Assembly->SetMetadataAsString(Pair.Key, Pair.Value);
		}
	}
}
