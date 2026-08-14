// Copyright Epic Games, Inc. All Rights Reserved.

#include "Processors/AssetProcessors.h"

#include "AssetDefinition.h"
#include "AssetDefinitionRegistry.h"
#include "CollectionManagerTypes.h"
#include "ContentBrowserDataSubsystem.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserDataModule.h"
#include "Elements/Columns/TypedElementFolderColumns.h"
#include "Elements/Columns/TypedElementSlateWidgetColumns.h"
#include "Elements/Framework/TypedElementQueryBuilder.h"
#include "Elements/Interfaces/TypedElementQueryStorageInterfaces.h"
#include "Experimental/ContentBrowserExtensionUtils.h"
#include "TedsAssetDataColumns.h"
#include "Elements/Columns/TypedElementMiscColumns.h"
#include "String/LexFromString.h"
#include "TedsAlerts.h"

#define LOCTEXT_NAMESPACE "UTedsAssetDataFactory"

namespace UE::TedsAssetDataFactory::Private
{
	TAutoConsoleVariable<bool> CVarTedsAssetDataFactory(TEXT("TEDS.TedsAssetDataFactory"),
		false,
		TEXT("When true this will enable some experimental features that are not optimized to work at scale yet. Note: The value need to be set a boot time to see the effect of this cvar."));

	// @return true if any of the texture dimensions are not a power of 2
	bool IsTextureNonSquare(FStringView Dimensions)
	{
		// Texture dimensions are of the form XxY (2D) or XxYxZ (3D)

		// Extract the X Dimension first (everything to the left of the first x)
		int32 FirstXPos;
		if (!Dimensions.FindChar('x', FirstXPos))
		{
			return false; // Failsafe, in case the dimension isn't in the correct format somehow.
		}
		
		FStringView XDimension(Dimensions.Left(FirstXPos));
		int32 X;

		// Convert the X Dimension to an int
		LexFromString(X, XDimension);

		// If it isn't a power of 2, early out since we already know this is a non square without needing to check the rest
		if (!FMath::IsPowerOfTwo(X))
		{
			return true;
		}

		// Extract out the YDimension, or YxZ in case of a 3D texture
		FStringView YDimension(Dimensions.RightChop(FirstXPos + 1));
		int32 SecondXPos;

		// If we find another x, this is a 3D texture
		if (YDimension.FindChar('x', SecondXPos))
		{
			// Extract out the Z Dimension first, which is everything to the right of the second X. And then convert it to an int
			FStringView ZDimension = YDimension.RightChop(SecondXPos + 1);
			int32 Z;
			LexFromString(Z, ZDimension);

			// Early out if the Z Dimension is not a power of 2 so we don't need to check Y Dimension
			if (!FMath::IsPowerOfTwo(Z))
			{
				return true;
			}

			// The Y Dimension is everything between the two x's for a 3D texture
			YDimension = YDimension.Left(SecondXPos);
		}

		// Regardless of 2D/3D, once we are here YDimension should be the correct value so check it finally
		int32 Y;
		LexFromString(Y, YDimension);

		if (!FMath::IsPowerOfTwo(Y))
		{
			return true;
		}

		// If we are here, all dimensions are a power of 2 and the texture is square.
		return false;

	}
}

void UTedsAssetDataFactory::RegisterQueries(UE::Editor::DataStorage::ICoreProvider& DataStorage)
{
	using namespace UE::Editor::DataStorage;
	using namespace UE::Editor::DataStorage::Columns;
	using namespace UE::Editor::DataStorage::Queries;

	if (!UE::TedsAssetDataFactory::Private::CVarTedsAssetDataFactory.GetValueOnGameThread())
	{
		return;
	}

	DataStorage.RegisterQuery(
		Select(
			TEXT("TedsAssetDataFactory: Sync folder color from world"),
			FProcessor(EQueryTickPhase::PostPhysics, DataStorage.GetQueryTickGroupName(EQueryTickGroups::SyncExternalToDataStorage))
			.SetExecutionMode(EExecutionMode::GameThread),
			[](IQueryContext& Context, const RowHandle* Rows, const FAssetPathColumn_Experimental* AssetPathColumn, FSlateColorColumn* ColorColumn)
			{
				const int32 NumOfRowToProcess = Context.GetRowCount();

				for (int32 Index = 0; Index < NumOfRowToProcess; ++Index)
				{
					if (TOptional<FLinearColor> Color = UE::Editor::ContentBrowser::ExtensionUtils::GetFolderColor(AssetPathColumn[Index].Path))
					{
						ColorColumn[Index].Color = Color.GetValue();
					}
				}
			}
		)
		.Where()
			.All<FFolderTag, FUpdatedPathTag, FVirtualPathColumn_Experimental>()
		.Compile()
		);
	
	struct FSetFolderColorCommand
	{
		void operator()()
		{
			
			UE::Editor::ContentBrowser::ExtensionUtils::SetFolderColor(FolderPath, NewFolderColor);
		}
		FName FolderPath;
		FLinearColor NewFolderColor;;
	};

	DataStorage.RegisterQuery(
		Select(
			TEXT("TedsAssetDataFactory: Sync folder color back to world"),
			FProcessor(EQueryTickPhase::PrePhysics, DataStorage.GetQueryTickGroupName(EQueryTickGroups::SyncDataStorageToExternal))
			.SetExecutionMode(EExecutionMode::GameThread),
			[](IQueryContext& Context, const RowHandle* Rows, const FAssetPathColumn_Experimental* PathColumn, const FSlateColorColumn* ColorColumn)
			{
				const int32 NumOfRowToProcess = Context.GetRowCount();

				for (int32 Index = 0; Index < NumOfRowToProcess; ++Index)
				{
					FSlateColor FolderColor = ColorColumn[Index].Color.GetSpecifiedColor();

					if (FolderColor.IsColorSpecified())
					{
						// Defer the add because it will fire a CB delegate that UTedsAssetDataFactory::OnSetFolderColor registers to and ends up
						// accessing the data storage in the middle of a processor callback which is not allowed
						Context.PushCommand(FSetFolderColorCommand
						{
							.FolderPath = PathColumn[Index].Path,
							.NewFolderColor = ColorColumn[Index].Color.GetSpecifiedColor()
						});
					}
				}
			}
		)
		.Where()
			.All<FFolderTag, FTypedElementSyncBackToWorldTag, FVirtualPathColumn_Experimental>()
		.Compile()
		);

	DataStorage.RegisterQuery(
		Select(
			TEXT("TedsAssetDataFactory: Add/Remove non-square texture warning"),
			FProcessor(EQueryTickPhase::PostPhysics, DataStorage.GetQueryTickGroupName(EQueryTickGroups::SyncExternalToDataStorage)),
			[](IQueryContext& Context, const RowHandle* Rows)
			{
				const int32 NumRows = Context.GetRowCount();
				static const FName AlertName = "NonSquareTextureAlert";

				TConstArrayView<FItemStringAttributeColumn_Experimental> ColumnView = MakeConstArrayView(
								Context.GetColumn<FItemStringAttributeColumn_Experimental>("Dimensions"),
								NumRows);
				
				for (int32 Index = 0; Index < NumRows; ++Index)
				{
					if (UE::TedsAssetDataFactory::Private::IsTextureNonSquare(ColumnView[Index].Value))
					{
						Alerts::AddAlert(Context, Rows[Index], AlertName,
							LOCTEXT("NonSquareTextureAlert", "Texture has a non-square aspect ratio."),
							FAlertColumnType::Error);
					}
					else
					{
						Alerts::RemoveAlert(Context, Rows[Index], AlertName);
					}
				}
			}
		)
		.ReadOnly<FItemStringAttributeColumn_Experimental>("Dimensions")
		.Where()
			.All<FAssetTag, FUpdatedAssetDataTag>()
		.Compile()
		);
}

void UTedsAssetDataFactory::PreRegister(UE::Editor::DataStorage::ICoreProvider& DataStorage)
{
	if (!UE::TedsAssetDataFactory::Private::CVarTedsAssetDataFactory.GetValueOnGameThread())
	{
		return;
	}

	if (FContentBrowserModule* ContentBrowserModule = FModuleManager::Get().GetModulePtr<FContentBrowserModule>("ContentBrowser"))
	{
		ContentBrowserModule->GetOnSetFolderColor().AddUObject(this, &UTedsAssetDataFactory::OnSetFolderColor, &DataStorage);
	}
}

void UTedsAssetDataFactory::PreShutdown(UE::Editor::DataStorage::ICoreProvider& DataStorage)
{
	if (FContentBrowserModule* ContentBrowserModule = FModuleManager::Get().GetModulePtr<FContentBrowserModule>("ContentBrowser"))
	{
		ContentBrowserModule->GetOnSetFolderColor().RemoveAll(this);
	}
}

void UTedsAssetDataFactory::OnSetFolderColor(const FString& Path, UE::Editor::DataStorage::ICoreProvider* DataStorage)
{
	const UE::Editor::DataStorage::FMapKey PathKey = UE::Editor::DataStorage::FMapKey(FName(Path));
	const UE::Editor::DataStorage::RowHandle Row = DataStorage->LookupMappedRow(PathKey);

	if (DataStorage->IsRowAvailable(Row))
	{
		DataStorage->AddColumn<FUpdatedPathTag>(Row);
	}
}

#undef LOCTEXT_NAMESPACE
