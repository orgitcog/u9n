// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanDefaultEditorPipelineActor.h"

#include "MetaHumanCharacterInstance.h"
#include "MetaHumanDefaultPipeline.h"
#include "MetaHumanDefaultEditorPipelineLog.h"
#include "Item/MetaHumanSkeletalMeshPipeline.h"
#include "Item/MetaHumanOutfitPipeline.h"

#include "Algo/Find.h"
#include "ChaosClothAsset/ClothComponent.h"
#include "GroomComponent.h"
#include "Components/SkeletalMeshComponent.h"

namespace UE::MetaHuman::Private
{
	template<typename TComponent, typename TAssemblyData>
	void AssignComponents(
		AMetaHumanDefaultEditorPipelineActor* InActor,
		TArray<TObjectPtr<TComponent>>& InOutComponents,
		const TArray<TAssemblyData>& InAssemblyData,
		USceneComponent* InParentComponent,
		TFunction<void(const TAssemblyData&, TObjectPtr<TComponent>)> InUpdateFun)
	{
		// Remove unneeded components
		for (int32 Index = InAssemblyData.Num(); Index < InOutComponents.Num(); Index++)
		{
			InOutComponents[Index]->DestroyComponent();
		}

		// Remove unneeded entries and create new ones if needed
		InOutComponents.SetNum(InAssemblyData.Num());

		for (int32 Index = 0; Index < InOutComponents.Num(); Index++)
		{
			if (InOutComponents[Index] == nullptr)
			{
				TComponent* NewComponent = NewObject<TComponent>(InActor);
				InActor->AddInstanceComponent(NewComponent);
				NewComponent->AttachToComponent(InParentComponent, FAttachmentTransformRules::KeepRelativeTransform);
				NewComponent->RegisterComponent();

				InOutComponents[Index] = NewComponent;
			}

			InUpdateFun(InAssemblyData[Index], InOutComponents[Index]);
		}
	}
}

AMetaHumanDefaultEditorPipelineActor::AMetaHumanDefaultEditorPipelineActor()
{
	HairComponent = CreateDefaultSubobject<UGroomComponent>(TEXT("Hair"));
	EyebrowsComponent = CreateDefaultSubobject<UGroomComponent>(TEXT("Eyebrows"));
	BeardComponent = CreateDefaultSubobject<UGroomComponent>(TEXT("Beard"));
	MustacheComponent = CreateDefaultSubobject<UGroomComponent>(TEXT("Mustache"));
	EyelashesComponent = CreateDefaultSubobject<UGroomComponent>(TEXT("Eyelashes"));
	PeachfuzzComponent = CreateDefaultSubobject<UGroomComponent>(TEXT("Peachfuzz"));
	
	HairComponent->SetupAttachment(FaceComponent);
	EyebrowsComponent->SetupAttachment(FaceComponent);
	BeardComponent->SetupAttachment(FaceComponent);
	MustacheComponent->SetupAttachment(FaceComponent);
	EyelashesComponent->SetupAttachment(FaceComponent);
	PeachfuzzComponent->SetupAttachment(FaceComponent);
}

void AMetaHumanDefaultEditorPipelineActor::InitializeMetaHumanCharacterEditorActor(
	TNotNull<const UMetaHumanCharacterInstance*> InCharacterInstance,
	TNotNull<UMetaHumanCharacter*> InCharacter,
	TNotNull<USkeletalMesh*> InFaceMesh,
	TNotNull<USkeletalMesh*> InBodyMesh,
	int32 InNumLODs,
	const TArray<int32>& InFaceLODMapping,
	const TArray<int32>& InBodyLODMapping)
{
	Super::InitializeMetaHumanCharacterEditorActor(InCharacterInstance, InCharacter, InFaceMesh, InBodyMesh, InNumLODs, InFaceLODMapping, InBodyLODMapping);

	SetUseCardsOnGroomComponents(InCharacter->ViewportSettings.bAlwaysUseHairCards);
	OnInstanceUpdated();

	CharacterInstance->OnInstanceUpdatedNative.AddUObject(this, &AMetaHumanDefaultEditorPipelineActor::OnInstanceUpdated);
}

void AMetaHumanDefaultEditorPipelineActor::SetHairVisibilityState(EMetaHumanHairVisibilityState State)
{
	Super::SetHairVisibilityState(State);

	if (CurrentHairState == State)
	{
		return;
	}

	const bool bVisible = State == EMetaHumanHairVisibilityState::Shown;

	HairComponent->SetVisibility(bVisible);
	EyebrowsComponent->SetVisibility(bVisible);
	BeardComponent->SetVisibility(bVisible);
	MustacheComponent->SetVisibility(bVisible);
	EyelashesComponent->SetVisibility(bVisible);
	PeachfuzzComponent->SetVisibility(bVisible);

	CurrentHairState = State;
}

void AMetaHumanDefaultEditorPipelineActor::SetClothingVisibilityState(EMetaHumanClothingVisibilityState State, UMaterialInterface* OverrideMaterial)
{
	if (State == EMetaHumanClothingVisibilityState::UseOverrideMaterial
		&& OverrideMaterial == nullptr)
	{
		// No override material provided, so just ignore this call
		return;
	}

	Super::SetClothingVisibilityState(State, OverrideMaterial);

	if (CurrentClothingState == State
		&& CurrentOverrideMaterial == OverrideMaterial)
	{
		// Already in the requested state
		return;
	}

	bool bShouldSaveOriginalMaterials = false;
	if (State == EMetaHumanClothingVisibilityState::UseOverrideMaterial)
	{
		if (CurrentOverrideMaterial == nullptr)
		{
			// The components are currently set to their original materials
			
			// SavedMaterials should already be empty, but empty it if not
			if (!ensure(SavedMaterials.Num() == 0))
			{
				SavedMaterials.Reset();
			}

			bShouldSaveOriginalMaterials = true;
		}
		else
		{
			// The components are set to a different override material, so we need to preserve the
			// saved materials as they are now.
			bShouldSaveOriginalMaterials = false;
		}

		CurrentOverrideMaterial = OverrideMaterial;
	}
	else
	{
		CurrentOverrideMaterial = nullptr;
	}

	auto ApplyStateToMeshComponent = 
		[State, OverrideMaterial, bShouldSaveOriginalMaterials, &SavedMaterials = SavedMaterials, CurrentClothingState = CurrentClothingState](UMeshComponent* Component)
		{
			if (!Component)
			{
				return;
			}

			const bool bVisible = 
				State == EMetaHumanClothingVisibilityState::Shown
				|| State == EMetaHumanClothingVisibilityState::UseOverrideMaterial;

			Component->SetVisibility(bVisible);

			if (State == EMetaHumanClothingVisibilityState::UseOverrideMaterial)
			{
				if (bShouldSaveOriginalMaterials)
				{
					FMetaHumanComponentMaterials& Materials = SavedMaterials.AddDefaulted_GetRef();
					Materials.MeshComponent = Component;
					Materials.Materials = Component->GetMaterials();
				}

				const int32 NumMaterials = Component->GetNumMaterials();
				for (int32 MaterialIndex = 0; MaterialIndex < NumMaterials; MaterialIndex++)
				{
					Component->SetMaterial(MaterialIndex, OverrideMaterial);
				}
			}
			else if (CurrentClothingState == EMetaHumanClothingVisibilityState::UseOverrideMaterial)
			{
				// State is no longer UseOverrideMaterial, so restore original materials

				const TWeakObjectPtr<UMeshComponent> WeakComponent(Component);
				const FMetaHumanComponentMaterials* Materials = Algo::FindBy(SavedMaterials, WeakComponent, &FMetaHumanComponentMaterials::MeshComponent);
				if (Materials)
				{
					if (ensure(Materials->Materials.Num() == Component->GetNumMaterials()))
					{
						for (int32 MaterialIndex = 0; MaterialIndex < Materials->Materials.Num(); MaterialIndex++)
						{
							Component->SetMaterial(MaterialIndex, Materials->Materials[MaterialIndex]);
						}
					}
				}
				else
				{
					UE_LOGFMT(LogMetaHumanDefaultEditorPipeline, Error, "Failed to restore materials for mesh component {Component}", GetFullNameSafe(Component));
				}
			}
		};

	for (UChaosClothComponent* Component : ClothComponents)
	{
		ApplyStateToMeshComponent(Component);
	}

	for (USkeletalMeshComponent* Component : OutfitMeshComponents)
	{
		ApplyStateToMeshComponent(Component);
	}

	for (USkeletalMeshComponent* Component : SkeletalMeshComponents)
	{
		ApplyStateToMeshComponent(Component);
	}

	if (State != EMetaHumanClothingVisibilityState::UseOverrideMaterial)
	{
		// Materials should have been restored, so SavedMaterials is no longer needed
		SavedMaterials.Reset();
	}

	CurrentClothingState = State;
}

void AMetaHumanDefaultEditorPipelineActor::OnInstanceUpdated()
{
	if (!CharacterInstance->GetAssemblyOutput().GetPtr<FMetaHumanDefaultAssemblyOutput>())
	{
		// No assembly output yet
		return;
	}

	EMetaHumanHairVisibilityState SavedHairState = CurrentHairState;
	EMetaHumanClothingVisibilityState SavedClothingState = CurrentClothingState;
	TObjectPtr<UMaterialInterface> SavedOverrideMaterial = CurrentOverrideMaterial;

	// New components may be created below, and new materials may be assigned to existing 
	// components, so set everything back to the default visibility state (which is Shown) and then
	// restore it to the requested state afterwards.
	SetHairVisibilityState(EMetaHumanHairVisibilityState::Shown);
	SetClothingVisibilityState(EMetaHumanClothingVisibilityState::Shown);	

	const FMetaHumanDefaultAssemblyOutput& AssemblyOutput = CharacterInstance->GetAssemblyOutput().Get<FMetaHumanDefaultAssemblyOutput>();

	// If cards are forced, temporary disable cards to load strands data when adding groom asset.
	UMetaHumanGroomPipeline::ApplyGroomAssemblyOutputToGroomComponent(AssemblyOutput.Hair, HairComponent);
	UMetaHumanGroomPipeline::ApplyGroomAssemblyOutputToGroomComponent(AssemblyOutput.Eyebrows, EyebrowsComponent);
	UMetaHumanGroomPipeline::ApplyGroomAssemblyOutputToGroomComponent(AssemblyOutput.Beard, BeardComponent);
	UMetaHumanGroomPipeline::ApplyGroomAssemblyOutputToGroomComponent(AssemblyOutput.Mustache, MustacheComponent);
	UMetaHumanGroomPipeline::ApplyGroomAssemblyOutputToGroomComponent(AssemblyOutput.Eyelashes, EyelashesComponent);
	UMetaHumanGroomPipeline::ApplyGroomAssemblyOutputToGroomComponent(AssemblyOutput.Peachfuzz, PeachfuzzComponent);

	// Set up cloth and skeletal mesh components for outfits
	{
		TArray<FMetaHumanOutfitPipelineAssemblyOutput> OutfitAssetClothData;
		TArray<FMetaHumanOutfitPipelineAssemblyOutput> SkeletalMeshClothData;
		OutfitAssetClothData.Reserve(AssemblyOutput.ClothData.Num());
		SkeletalMeshClothData.Reserve(AssemblyOutput.ClothData.Num());
	
		for (const FMetaHumanOutfitPipelineAssemblyOutput& ClothData : AssemblyOutput.ClothData)
		{
			if (ClothData.Outfit)
			{
				OutfitAssetClothData.Add(ClothData);
			}
			else if (ClothData.OutfitMesh)
			{
				SkeletalMeshClothData.Add(ClothData);
			}
		}

		UE::MetaHuman::Private::AssignComponents<UChaosClothComponent, FMetaHumanOutfitPipelineAssemblyOutput>(
			this,
			ClothComponents,
			OutfitAssetClothData,
			BodyComponent,
			[](const FMetaHumanOutfitPipelineAssemblyOutput& AssemblyData, TObjectPtr<UChaosClothComponent> Component)
			{
				UMetaHumanOutfitPipeline::ApplyOutfitAssemblyOutputToClothComponent(AssemblyData, Component);
			});

		UE::MetaHuman::Private::AssignComponents<USkeletalMeshComponent, FMetaHumanOutfitPipelineAssemblyOutput>(
			this,
			OutfitMeshComponents,
			SkeletalMeshClothData,
			BodyComponent,
			[](const FMetaHumanOutfitPipelineAssemblyOutput& AssemblyData, TObjectPtr<USkeletalMeshComponent> Component)
			{
				UMetaHumanOutfitPipeline::ApplyOutfitAssemblyOutputToMeshComponent(AssemblyData, Component);
			});
	}

	// Set up skeletal mesh components
	UE::MetaHuman::Private::AssignComponents<USkeletalMeshComponent, FMetaHumanSkeletalMeshPipelineAssemblyOutput>(
		this,
		SkeletalMeshComponents,
		AssemblyOutput.SkeletalMeshData,
		BodyComponent,
		[this](const FMetaHumanSkeletalMeshPipelineAssemblyOutput& AssemblyData, TObjectPtr<USkeletalMeshComponent> Component)
		{
			UMetaHumanSkeletalMeshPipeline::ApplySkeletalMeshAssemblyOutputToSkeletalMeshComponent(AssemblyData, Component, BodyComponent);
		});

	// Restore visibility states
	SetHairVisibilityState(SavedHairState);
	SetClothingVisibilityState(SavedClothingState, SavedOverrideMaterial);
}

void AMetaHumanDefaultEditorPipelineActor::SetUseCardsOnGroomComponents(bool bInUseCards)
{
	HairComponent->SetUseCards(bInUseCards);
	EyebrowsComponent->SetUseCards(bInUseCards);
	BeardComponent->SetUseCards(bInUseCards);
	MustacheComponent->SetUseCards(bInUseCards);
	EyelashesComponent->SetUseCards(bInUseCards);
	PeachfuzzComponent->SetUseCards(bInUseCards);
}
