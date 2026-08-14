// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Animation/SkeletalMeshActor.h"
#include "CapturePerformer.h"
#include "Retargeter/IKRetargetProfile.h"

#include "CaptureCharacter.generated.h"

#define UE_API PERFORMANCECAPTURECORE_API

class URetargetComponent;
class UIKRetargeter;

UCLASS(MinimalAPI, Blueprintable, ClassGroup ="Performance Capture", Category = "Performance Capture",  HideCategories = ("Mesh", "Rendering", "Animation", "LOD", "Misc", "Physics", "Streaming"))
class ACaptureCharacter : public ASkeletalMeshActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	UE_API ACaptureCharacter();

	/**
	* CapturePerformer Actor that will be the source for retargeting.
	*/
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Performance Capture|Character")
	TSoftObjectPtr<ACapturePerformer> SourcePerformer;

	/**
	* The IKRetarget Asset to use for retargeting between the SourcePerformer and this Character.
	*/
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Performance Capture|Character")
	TObjectPtr<UIKRetargeter> RetargetAsset;

	/**
	* Force all skeletal meshes to use the root SkeletalMesh as their Leader. Default = True.
	*/
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Performance Capture|Character")
	bool bForceAllSkeletalMeshesToFollowLeader = true;
	
	/**
	* Set a Custom Retarget Profile.
	* @param InProfile New Retarget Profile.
	*/
	UFUNCTION(BlueprintCallable, Category = "Performance Capture|Character")
	UE_API void SetCustomRetargetProfile(FRetargetProfile InProfile);

	/**
	* Get a Custom Retarget Profile.
	* @return Current Custom Retarget Profile.
	*/
	UFUNCTION(BlueprintCallable, Category = "Performance Capture|Character")
	UE_API FRetargetProfile GetCustomRetargetProfile();

	/**
	* Set the Source CapturePerformer Actor.
	* @param InPerformer New Performer.
	*/
	UFUNCTION(BlueprintCallable, Category = "Performance Capture|Character")
	UE_API void SetSourcePerformer(ACapturePerformer* InPerformer);

	/**
	* Set the Retarget Asset.
	* @param InRetargetAsset New IKRetarget Asset.
	*/
	UFUNCTION(BlueprintCallable, Category = "Performance Capture|Character")
	UE_API void SetRetargetAsset(UIKRetargeter* InRetargetAsset);

	/**
	* Force all Skeletal Meshes to follow the Controlled Skeletal mesh. 
	* @param InFollowLeader New Bool.
	*/
	UFUNCTION(BlueprintCallable, Category = "Performance Capture|Character")
	UE_API void SetForceAllSkeletalMeshesToFollowLeader (bool InFollowLeader);

protected:
	UE_API virtual void PostRegisterAllComponents() override;
#if WITH_EDITOR
	UE_API virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	// retarget component is private and hidden from the editor UI
	UPROPERTY()
	TObjectPtr<URetargetComponent> RetargetComponent;
};

#undef UE_API
