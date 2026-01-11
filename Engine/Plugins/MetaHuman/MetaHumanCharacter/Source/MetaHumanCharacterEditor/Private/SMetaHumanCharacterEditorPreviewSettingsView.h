// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Widgets/SCompoundWidget.h"
#include "CoreMinimal.h"
#include "LiveLinkTypes.h"
#include "Animation/AnimSequence.h"
#include "Engine/DataTable.h"
#include "UObject/SoftObjectPtr.h"
#include "MetaHumanCharacter.h"
#include "MetaHumanCharacterEditorSubsystem.h"
#include "SMetaHumanCharacterEditorPreviewSettingsView.generated.h"

class UAnimSequence;

DECLARE_DELEGATE_TwoParams(FOnMetaHumanCharacterAnimationChanged, UAnimSequence*, UAnimSequence*)
DECLARE_DELEGATE_OneParam(FOnMetaHumanCharacterPlayRateChanged, float)
DECLARE_DELEGATE_OneParam(FOnMetaHumanCharacterLiveLinkSubjectChanged, FLiveLinkSubjectName)
DECLARE_DELEGATE_ThreeParams(FOnMetaHumanCharacterAnimationControllerChanged, EMetaHumanCharacterAnimationController, UAnimSequence*, UAnimSequence*)
DECLARE_DELEGATE_OneParam(FOnMetaHumanPreviewModeChanged, EMetaHumanCharacterSkinPreviewMaterial)
DECLARE_DELEGATE_OneParam(FOnMetaHumanCharacterGroomHiddenChanged, EMetaHumanPreviewAssemblyVisibility)
DECLARE_DELEGATE_OneParam(FOnMetaHumanCharacterClothingHiddenChanged, EMetaHumanPreviewAssemblyVisibility)

UENUM()
enum class EMetaHumanPreviewAssemblyVisibility : uint8
{
	Visible,
	Hidden
};

UENUM()
enum class EMetaHumanCharacterAnimationController : uint8
{
	None UMETA(DisplayName = "None"),
	AnimSequence UMETA(DisplayName = "AnimSequence"),
	LiveLink UMETA(DisplayName = "LiveLink")
};

UENUM()
enum class EMetaHumanAnimationType : uint8
{
	SpecificAnimation,
	TemplateAnimation
};

USTRUCT()
struct FMetaHumanTemplateAnimationRow : public FTableRowBase
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TemplateAnimation")
	TSoftObjectPtr<UAnimSequence> FaceAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TemplateAnimation")
	TSoftObjectPtr<UAnimSequence> BodyAnimation;
};


UCLASS()
class UMetaHumanCharacterEditorPreviewSceneDescription : public UObject
{
public:
	GENERATED_BODY()

	UMetaHumanCharacterEditorPreviewSceneDescription();

	// Have just one delegate and figure our what changed separately?
	FOnMetaHumanCharacterAnimationChanged OnAnimationChanged;
	FOnMetaHumanCharacterPlayRateChanged OnPlayRateChanged;
	FOnMetaHumanCharacterLiveLinkSubjectChanged OnLiveLinkSubjectChanged;
	FOnMetaHumanCharacterAnimationControllerChanged OnAnimationControllerChanged;
	FOnMetaHumanPreviewModeChanged OnPreviewModeChanged;
	FOnMetaHumanCharacterGroomHiddenChanged OnGroomHiddenChanged;
	FOnMetaHumanCharacterClothingHiddenChanged OnClothingHiddenChanged;

	UPROPERTY(Transient)
	bool bAnimationControllerEnabled = true;

	UPROPERTY(EditAnywhere, Category = "Animation")
	EMetaHumanCharacterAnimationController AnimationController = EMetaHumanCharacterAnimationController::AnimSequence;

	// Anim sequence Face Animation options 
	UPROPERTY(EditAnywhere, Transient, Category = "Animation|FaceAnimation", meta = (EditCondition = "AnimationController == EMetaHumanCharacterAnimationController::AnimSequence", EditConditionHides))
	EMetaHumanAnimationType FaceAnimationType = EMetaHumanAnimationType::SpecificAnimation;

	UPROPERTY(EditAnywhere, Transient, Category = "Animation|FaceAnimation",meta = (EditCondition = "AnimationController == EMetaHumanCharacterAnimationController::AnimSequence && FaceAnimationType == EMetaHumanAnimationType::SpecificAnimation", EditConditionHides))
	TObjectPtr<UAnimSequence> FaceSpecificAnimation;

	UPROPERTY(EditAnywhere, Transient, Category = "Animation|FaceAnimation", meta = (EditCondition = "AnimationController == EMetaHumanCharacterAnimationController::AnimSequence && FaceAnimationType == EMetaHumanAnimationType::TemplateAnimation", EditConditionHides, GetOptions="GetTemplateAnimationOptions"))
	FName FaceTemplateAnimation;
	
	
	// Anim sequence Body Animation options
	UPROPERTY(EditAnywhere, Transient, Category = "Animation|BodyAnimation", meta = (EditCondition = "AnimationController == EMetaHumanCharacterAnimationController::AnimSequence", EditConditionHides))
	EMetaHumanAnimationType BodyAnimationType = EMetaHumanAnimationType::SpecificAnimation;

	UPROPERTY(EditAnywhere, Transient, Category = "Animation|BodyAnimation",meta = (EditCondition = "AnimationController == EMetaHumanCharacterAnimationController::AnimSequence && BodyAnimationType == EMetaHumanAnimationType::SpecificAnimation", EditConditionHides))
	TObjectPtr<UAnimSequence> BodySpecificAnimation;

	UPROPERTY(EditAnywhere, Transient, Category = "Animation|BodyAnimation",meta = (EditCondition = "AnimationController == EMetaHumanCharacterAnimationController::AnimSequence && BodyAnimationType == EMetaHumanAnimationType::TemplateAnimation", EditConditionHides, GetOptions = "GetTemplateAnimationOptions"))
	FName BodyTemplateAnimation;

	UPROPERTY(EditAnywhere, Transient, Category = "Animation", meta = (EditCondition = "AnimationController == EMetaHumanCharacterAnimationController::AnimSequence", EditConditionHides, ClampMin = "0.05", ClampMax = "10.0"))
	float PlayRate = 1.0f;

	UPROPERTY(EditAnywhere, Transient, Category = "Animation", meta = (EditCondition = "AnimationController == EMetaHumanCharacterAnimationController::LiveLink", EditConditionHides))
	FLiveLinkSubjectName LiveLinkSubjectName;

	UPROPERTY(EditAnywhere, Transient, Category = "Grooms", DisplayName = "Preview")
	EMetaHumanPreviewAssemblyVisibility PreviewAssemblyGrooms = EMetaHumanPreviewAssemblyVisibility::Visible;

	UPROPERTY(EditAnywhere, Transient, Category = "Outfit Clothing", DisplayName = "Preview")
	EMetaHumanPreviewAssemblyVisibility PreviewAssemblyClothing = EMetaHumanPreviewAssemblyVisibility::Visible;

	UPROPERTY(Transient)
	TObjectPtr<UDataTable> TemplateAnimationDataTable;

	FName DefaultBodyTemplateAnimationName;
	FName DefaultFaceTemplateAnimationName;

	/**
	 * Add template animations from the given data table asset to the list of available template animations for previewing.
	 */
	void AddTemplateAnimationsFromDataTable(const FSoftObjectPath& DataTableObjectPath);

	void OnRiggingStateChanged(TNotNull<const UMetaHumanCharacter*> Character, EMetaHumanCharacterRigState State);

	void SetAnimationController(EMetaHumanCharacterAnimationController InAnimationController);

	void SceneDescriptionPropertyChanged(const FName& PropertyName);

private:
	virtual void PostTransacted(const FTransactionObjectEvent& TransactionEvent) override;
	FName GetFirstLiveLinkSubject() const;

	UFUNCTION()
	TArray<FName> GetTemplateAnimationOptions() const;

public:
	UAnimSequence* GetTemplateAnimation(const bool bIsFaceAnimation, const FName& AnimationName);

protected:

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
};

class SMetaHumanCharacterEditorPreviewSettingsView :
    public SCompoundWidget , public FGCObject
{
public:
	SLATE_BEGIN_ARGS(SMetaHumanCharacterEditorPreviewSettingsView) {}
		SLATE_ARGUMENT(UMetaHumanCharacterEditorPreviewSceneDescription*, SettingsObject)
	SLATE_END_ARGS()

	/** Constructs the widget */
	void Construct(const FArguments& InArgs);

	/** FGCObject interface Begin*/
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override;
	virtual FString GetReferencerName() const override;
	/** FGCObject interface End*/

private:
	TObjectPtr<UMetaHumanCharacterEditorPreviewSceneDescription> PreviewSceneDescription;
	TSharedPtr<class IDetailsView> SettingsDetailsView;
};

