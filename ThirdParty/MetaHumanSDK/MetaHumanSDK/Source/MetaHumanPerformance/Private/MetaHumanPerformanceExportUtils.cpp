// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanPerformanceExportUtils.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/Material.h"
#include "MetaHumanPerformance.h"
#include "MetaHumanPerformanceLog.h"
#include "MetaHumanIdentity.h"
#include "MetaHumanIdentityParts.h"
#include "CaptureData.h"
#include "CameraCalibration.h"
#include "MetaHumanFootageComponent.h"
#include "MetaHumanViewportModes.h"
#include "UI/MetaHumanPerformanceControlRigComponent.h"
#include "MetaHumanTrace.h"
#include "MetaHumanDepthMeshComponent.h"
#include "ImageSequenceUtils.h"

#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Factories/AnimSequenceFactory.h"
#include "UObject/SavePackage.h"
#include "ControlRig.h"
#include "ControlRigBlueprint.h"
#include "ControlRigObjectBinding.h"
#include "ControlRigSequencerEditorLibrary.h"
#include "ControlRigBlueprintGeneratedClass.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "ImgMediaSource.h"
#include "MediaTexture.h"
#include "LevelSequence.h"
#include "Animation/BuiltInAttributeTypes.h"
#include "Animation/SkeletalMeshActor.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/SkeletalMesh.h"
#include "Camera/CameraComponent.h"
#include "CineCameraActor.h"
#include "CineCameraComponent.h"
#include "LensComponent.h"
#include "LensDistortionModelHandlerBase.h"
#include "Models/SphericalLensModel.h"
#include "Exporters/AnimSeqExportOption.h"
#include "MovieScene.h"
#include "MovieSceneMediaTrack.h"
#include "MovieSceneMediaSection.h"
#include "Sequencer/MovieSceneControlRigParameterTrack.h"
#include "Tracks/MovieSceneAudioTrack.h"
#include "Tracks/MovieSceneSpawnTrack.h"
#include "Tracks/MovieScene3DTransformTrack.h"
#include "Tracks/MovieSceneSkeletalAnimationTrack.h"
#include "Tracks/MovieSceneFloatTrack.h"
#include "Tracks/MovieSceneCameraCutTrack.h"
#include "Sections/MovieSceneBoolSection.h"
#include "Sections/MovieSceneAudioSection.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "Sections/MovieSceneSkeletalAnimationSection.h"
#include "Sections/MovieSceneCameraCutSection.h"
#include "Sound/SoundWave.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"
#include "PropertyEditorModule.h"
#include "DetailsViewArgs.h"
#include "Widgets/Input/SButton.h"
#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "PackageTools.h"
#include "ObjectTools.h"
#include "Dialogs/Dialogs.h"
#include "GenericPlatform/GenericPlatformMath.h"
#include "Misc/ScopedSlowTask.h"
#include "OpenCVHelperLocal.h"
#include "SequencerUtilities.h"
#include "ImageSequenceTimecodeUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaHumanPerformanceExportUtils)

#define LOCTEXT_NAMESPACE "MetaHumanPerformanceExportUtils"

namespace
{
	static const FName HeadYawCurveName = TEXT("HeadYaw");
	static const FName HeadPitchCurveName = TEXT("HeadPitch");
	static const FName HeadRollCurveName = TEXT("HeadRoll");
	static const FName HeadTranslationXCurveName = TEXT("HeadTranslationX");
	static const FName HeadTranslationYCurveName = TEXT("HeadTranslationY");
	static const FName HeadTranslationZCurveName = TEXT("HeadTranslationZ");
	static const FName RootBoneName = TEXT("root");
	static const FName HeadBoneName = TEXT("head");
	static const FName BackwardsSolveEventName = TEXT("Backwards Solve");
	static const FName HeadIKCurveSwitchName = TEXT("HeadControlSwitch");
	static const FName MetaHumanFaceComponentName = TEXT("Face");
	static const FName MetaHumanBodyComponentName = TEXT("Body");
}

/** Utility function to delete an asset */
static void DeleteAsset(UObject* InObject)
{
	if (InObject != nullptr)
	{
		FAssetRegistryModule::AssetDeleted(InObject);
		InObject->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
	}
}

/** Utility function to add a transform track to a movie scene binding */
static void Add3DTransformTrackToBinding(UMovieScene* InMovieScene, FGuid InBinding, FTransform InTransform)
{
	UMovieScene3DTransformTrack* TransformTrack = InMovieScene->AddTrack<UMovieScene3DTransformTrack>(InBinding);
	UMovieSceneSection* TransformSection = TransformTrack->CreateNewSection();
	TransformSection->SectionRange.Value.SetUpperBound(TRangeBound<FFrameNumber>());	

	TArrayView<FMovieSceneDoubleChannel*> DoubleChannels = TransformSection->GetChannelProxy().GetChannels<FMovieSceneDoubleChannel>();
	check(DoubleChannels.Num() == 9);
	DoubleChannels[0]->SetDefault(InTransform.GetLocation().X);
	DoubleChannels[1]->SetDefault(InTransform.GetLocation().Y);
	DoubleChannels[2]->SetDefault(InTransform.GetLocation().Z);
	DoubleChannels[3]->SetDefault(InTransform.GetRotation().Euler().X);
	DoubleChannels[4]->SetDefault(InTransform.GetRotation().Euler().Y);
	DoubleChannels[5]->SetDefault(InTransform.GetRotation().Euler().Z);
	DoubleChannels[6]->SetDefault(InTransform.GetScale3D().X);
	DoubleChannels[7]->SetDefault(InTransform.GetScale3D().Y);
	DoubleChannels[8]->SetDefault(InTransform.GetScale3D().Z);

	TransformTrack->AddSection(*TransformSection);
}

static void AddFloatTrackToBinding(UMovieScene* InMovieScene, FGuid InBinding, FName InPropertyName, const FString& InPropertyPath)
{
	UMovieSceneFloatTrack* FloatTrack = InMovieScene->AddTrack<UMovieSceneFloatTrack>(InBinding);
	FloatTrack->SetPropertyNameAndPath(InPropertyName, InPropertyPath);
	UMovieSceneSection* Section = FloatTrack->CreateNewSection();
	Section->SectionRange.Value.SetUpperBound(TRangeBound<FFrameNumber>());
	FloatTrack->AddSection(*Section);
}

template<typename T>
static FGuid BindActorComponentToParentActorInLevelSequence(ULevelSequence* InLevelSequence, T& InActorComponent, const FGuid& InParentActorBinding, UObject* ParentContext)
{
	UMovieScene* MovieScene = InLevelSequence->GetMovieScene();
	const FGuid ComponentGuid = MovieScene->AddPossessable(InActorComponent.GetName(), T::StaticClass());
	FMovieScenePossessable* ComponentPossessable = MovieScene->FindPossessable(ComponentGuid);

	ComponentPossessable->SetParent(InParentActorBinding, MovieScene);

	InLevelSequence->BindPossessableObject(ComponentGuid, InActorComponent, ParentContext);

	return ComponentGuid;
}

static UClass* GetDefaultControlRigClass(USkeletalMeshComponent* InSkelMeshComponent)
{
	UClass* ControlRigClass = nullptr;
	if (UObject* DefaultAnimatingRig = InSkelMeshComponent->GetDefaultAnimatingRig().LoadSynchronous())
	{
		if (UControlRigBlueprint* ControlRigBlueprint = Cast<UControlRigBlueprint>(DefaultAnimatingRig))
		{
			ControlRigClass = ControlRigBlueprint->GetRigVMBlueprintGeneratedClass();
		}
	}
	return ControlRigClass;
}

static TObjectPtr<class UImgMediaSource> GetCaptureFootage(const UMetaHumanPerformance* InPerformance, bool InDepth)
{
	TObjectPtr<class UImgMediaSource> Footage = nullptr;
	
	const TArray<TObjectPtr<UImgMediaSource>>* CaptureFootage;
	if (InDepth)
	{
		CaptureFootage = &InPerformance->FootageCaptureData->DepthSequences;
	}
	else
	{
		CaptureFootage = &InPerformance->FootageCaptureData->ImageSequences;
	}

	if (InPerformance->FootageCaptureData)
	{
		int32 CameraViewIndex = InPerformance->FootageCaptureData->GetViewIndexByCameraName(InPerformance->Camera);
		if (CameraViewIndex >= 0 && CameraViewIndex < CaptureFootage->Num())
		{
			Footage = (*CaptureFootage)[CameraViewIndex];
		}
	}

	return Footage;
}

/**
 * @brief Replacement for IAssetTools::CreateAssetWithDialog that filters assets based on the type of the one being created and allows client code to
 *		  specify whether or not to replace an existing asset.
 *
 * The existing IAssetTools::CreateAssetWithDialog function has an issue where the dialog it displays won't filter assets and will show everything in the project.
 * This has been fixed in UE-178311 but this didn't make into UE5.2 so we have this replacement here that has the correct behaviour. Moreover IAssetTools::CreateAssetWithDialog
 * will always replace the existing asset which is not the behaviour we want when exporting Animation Sequences so this function.
 *
 * @param InAssetName name of the asset to create or load
 * @param InPackagePath where to look for the asset
 * @param InFactory factory used to create a new asset if one does not exist
 * @param bInOutUseExisting whether or not to return an existing asset if one is found. If false, replaces an existing asset with a brand new one.
							This value will be changed to true if a new asset has been created
 */
template<typename T>
static T* CreateAssetWithDialog(const FString& InAssetName, const FString& InPackagePath, UFactory* InFactory, const FText& InDialogTitle, bool& bInOutUseExisting)
{
	FSaveAssetDialogConfig SaveAssetDialogConfig;
	SaveAssetDialogConfig.DialogTitleOverride = InDialogTitle;
	SaveAssetDialogConfig.DefaultPath = InPackagePath;
	SaveAssetDialogConfig.AssetClassNames.Add(T::StaticClass()->GetClassPathName());
	SaveAssetDialogConfig.DefaultAssetName = InAssetName;
	SaveAssetDialogConfig.ExistingAssetPolicy = ESaveAssetDialogExistingAssetPolicy::AllowButWarn;

	IContentBrowserSingleton& ContentBrowserSingleton = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser")).Get();
	const FString SaveObjectPath = ContentBrowserSingleton.CreateModalSaveAssetDialog(SaveAssetDialogConfig);

	if (!SaveObjectPath.IsEmpty())
	{
		T* TargetObject = LoadObject<T>(nullptr, *SaveObjectPath, nullptr, LOAD_NoWarn);

		if (TargetObject != nullptr && bInOutUseExisting)
		{
			bInOutUseExisting = false;
			return TargetObject;
		}
		else
		{
			if (TargetObject != nullptr)
			{
				// Delete the existing asset so a new one can be created without a second warning to the user
				DeleteAsset(TargetObject);
			}

			bInOutUseExisting = true;
			const FString TargetPackagePath = FPackageName::GetLongPackagePath(SaveObjectPath);
			const FString TargetAssetName = FPaths::GetBaseFilename(SaveObjectPath);

			IAssetTools& AssetTools = FAssetToolsModule::GetModule().Get();
			return Cast<T>(AssetTools.CreateAsset(TargetAssetName, TargetPackagePath, T::StaticClass(), InFactory));
		}
	}

	return nullptr;
}

/**
 * Widget used to display the Level Sequence Settings to be customized by the user
 */
class SMetaHumanPerformanceExportSettings
	: public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMetaHumanPerformanceExportSettings) {}
		SLATE_ARGUMENT(TSet<FString>, AnimationCurves)
		SLATE_ARGUMENT(UObject*, Settings)
		SLATE_ARGUMENT(UMetaHumanPerformance*, Performance)
		SLATE_ATTRIBUTE(TObjectPtr<UObject>, ConditionalCreate)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		check(InArgs._Settings);

		AnimationCurves = InArgs._AnimationCurves;
		SettingsObject = InArgs._Settings;
		Performance = InArgs._Performance;
		ConditionalCreate = InArgs._ConditionalCreate;

		FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
		FDetailsViewArgs DetailsViewArgs;
		DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
		DetailsViewArgs.bHideSelectionTip = true;
		TSharedRef<IDetailsView> DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);

		DetailsView->SetObject(InArgs._Settings);

		DetailsView->SetIsPropertyReadOnlyDelegate(FIsPropertyReadOnly::CreateSP(this, &SMetaHumanPerformanceExportSettings::PropertyIsReadOnly));

		ChildSlot
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("Menu.Background"))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				[
					DetailsView
				]
				// Export/Cancel buttons
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Bottom)
				.Padding(8)
				[
					SNew(SUniformGridPanel)
					.SlotPadding(FAppStyle::GetMargin("StandardDialog.SlotPadding"))
					.MinDesiredSlotWidth(FAppStyle::GetFloat("StandardDialog.MinDesiredSlotWidth"))
					.MinDesiredSlotHeight(FAppStyle::GetFloat("StandardDialog.MinDesiredSlotHeight"))
					+SUniformGridPanel::Slot(0,0)
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.ContentPadding( FAppStyle::GetMargin("StandardDialog.ContentPadding") )
						.OnClicked(this, &SMetaHumanPerformanceExportSettings::ExportClicked)
						.IsEnabled(this, &SMetaHumanPerformanceExportSettings::CanExport)
						.Text(LOCTEXT("ExportButton", "Create"))
					]
					+SUniformGridPanel::Slot(1,0)
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.ContentPadding( FAppStyle::GetMargin("StandardDialog.ContentPadding") )
						.OnClicked(this, &SMetaHumanPerformanceExportSettings::CancelClicked)
						.Text(LOCTEXT("CancelButton", "Cancel"))
					]
				]
			]
		];
	}

	bool CanExport() const
	{
		return !ConditionalCreate.IsBound() || ConditionalCreate.Get();
	}

	FReply ExportClicked()
	{
		if (UMetaHumanPerformanceExportAnimationSettings* ExportAnimationSettings = Cast<UMetaHumanPerformanceExportAnimationSettings>(SettingsObject))
		{
			TArray<FString> MissingCurves;
			if (!ExportAnimationSettings->IsTargetSkeletonCompatible(AnimationCurves, MissingCurves))
			{
				FString MissingCurvesString;
				for (const FString& Curve : MissingCurves)
				{
					MissingCurvesString += Curve + TEXT("\n");
				}

				FSuppressableWarningDialog::FSetupInfo Info(
					FText::Format(LOCTEXT("IncompatibleSkeletonsMessage", "The Animation Sequence that will be exported may not work with the selected target Skeleton due to the following missing curves:\n\n"
																		  "{0}"
																		  "\nDo you want to continue ?"), { FText::FromString(MissingCurvesString) }),
					LOCTEXT("IncompatibleSkeletonsTitle", "Target Skeleton may be incompatible"),
					TEXT("SupressIncompatibleSkeletons"));
				Info.ConfirmText = LOCTEXT("ShouldContinue_ConfirmText", "Yes");
				Info.CancelText = LOCTEXT("ShouldContinue_CancelText", "No");

				FSuppressableWarningDialog ShouldContinueWithExport{ Info };
				FSuppressableWarningDialog::EResult UserInput = ShouldContinueWithExport.ShowModal();

				if (UserInput == FSuppressableWarningDialog::Confirm || UserInput == FSuppressableWarningDialog::Suppressed)
				{
					bExportClicked = true;
				}
			}
			else
			{
				bExportClicked = true;
			}
		}
		else
		{
			bExportClicked = true;
		}

		if (bExportClicked)
		{
			CloseDialog();
		}

		return FReply::Handled();
	}

	FReply CancelClicked()
	{
		CloseDialog();
		return FReply::Handled();
	}

	void CloseDialog()
	{
		if (ParentWindow.IsValid())
		{
			ParentWindow.Pin()->RequestDestroyWindow();
		}
	}

	bool PropertyIsReadOnly(const FPropertyAndParent& InPropertyAndParent) const
	{
		const FProperty& Property = InPropertyAndParent.Property;
		const FName& PropertyName = Property.GetFName();

		if (PropertyName == GET_MEMBER_NAME_CHECKED(UMetaHumanPerformanceExportAnimationSettings, bEnableHeadMovement))
		{
			return !UMetaHumanPerformanceExportUtils::CanExportHeadMovement(Performance);
		}
		else if (PropertyName == GET_MEMBER_NAME_CHECKED(UMetaHumanPerformanceExportLevelSequenceSettings, bEnableControlRigHeadMovement))
		{
			return !UMetaHumanPerformanceExportUtils::CanExportHeadMovement(Performance);
		}
		else if (PropertyName == GET_MEMBER_NAME_CHECKED(UMetaHumanPerformanceExportLevelSequenceSettings, bEnableMetaHumanHeadMovement))
		{
			return !UMetaHumanPerformanceExportUtils::CanExportHeadMovement(Performance);
		}
		else if (PropertyName == GET_MEMBER_NAME_CHECKED(UMetaHumanPerformanceExportLevelSequenceSettings, bExportVideoTrack))
		{
			return !UMetaHumanPerformanceExportUtils::CanExportVideoTrack(Performance);
		}
		else if (PropertyName == GET_MEMBER_NAME_CHECKED(UMetaHumanPerformanceExportLevelSequenceSettings, bExportDepthTrack))
		{
			return !UMetaHumanPerformanceExportUtils::CanExportDepthTrack(Performance);
		}
		else if (PropertyName == GET_MEMBER_NAME_CHECKED(UMetaHumanPerformanceExportLevelSequenceSettings, bExportAudioTrack))
		{
			return !UMetaHumanPerformanceExportUtils::CanExportAudioTrack(Performance);
		}
		else if (PropertyName == GET_MEMBER_NAME_CHECKED(UMetaHumanPerformanceExportLevelSequenceSettings, bExportIdentity))
		{
			return !UMetaHumanPerformanceExportUtils::CanExportIdentity(Performance);
		}
		else if (PropertyName == GET_MEMBER_NAME_CHECKED(UMetaHumanPerformanceExportLevelSequenceSettings, bApplyLensDistortion))
		{
			return !UMetaHumanPerformanceExportUtils::CanExportLensDistortion(Performance);
		}

		return false;
	}

	/** Returns true if the Export button has been clicked */
	static bool ShowSettingsDialog(UObject* InSettingsObject, UMetaHumanPerformance* InPerformance = nullptr, const TSet<FString>& InAnimationCurves = {}, TAttribute<TObjectPtr<UObject>> InConditionalCreate = TAttribute<TObjectPtr<UObject>>())
	{
		TSharedPtr<SMetaHumanPerformanceExportSettings> SettingsWidget;

		TSharedRef<SWindow> SettingsWindow = SNew(SWindow)
			.Title(FText::Format(LOCTEXT("ExportSettingsWindowTitle", "Export {0} Sequence Settings"), InSettingsObject->IsA<UMetaHumanPerformanceExportAnimationSettings>() ? LOCTEXT("AnimationLabel", "Animation") : LOCTEXT("LevelLabel", "Level")))
			.ClientSize(FVector2D{ 500.0, 700.0 })
			.SupportsMinimize(false)
			.SupportsMaximize(false)
			[
				SAssignNew(SettingsWidget, SMetaHumanPerformanceExportSettings)
				.Settings(InSettingsObject)
				.AnimationCurves(InAnimationCurves)
				.ConditionalCreate(InConditionalCreate)
				.Performance(InPerformance)
			];

		SettingsWidget->ParentWindow = SettingsWindow;

		GEditor->EditorAddModalWindow(SettingsWindow);

		return SettingsWidget->bExportClicked;
	}

public:
	TWeakPtr<SWindow> ParentWindow;
	TSet<FString> AnimationCurves;
	TObjectPtr<UObject> SettingsObject;
	TObjectPtr<UMetaHumanPerformance> Performance;
	TAttribute<TObjectPtr<UObject>> ConditionalCreate;

	bool bExportClicked = false;
};

/////////////////////////////////////////////////////
// UMetaHumanPerformanceExportAnimationSettings

USkeleton* UMetaHumanPerformanceExportAnimationSettings::GetTargetSkeleton() const
{
	if (USkeleton* Skeleton = Cast<USkeleton>(TargetSkeletonOrSkeletalMesh))
	{
		return Skeleton;
	}

	if (USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(TargetSkeletonOrSkeletalMesh))
	{
		return SkeletalMesh->GetSkeleton();
	}

	return nullptr;
}

bool UMetaHumanPerformanceExportAnimationSettings::IsTargetSkeletonCompatible(const TSet<FString>& InCurves, TArray<FString>& OutMissingCurvesInSkeleton) const
{
	if (USkeleton* TargetSkeleton = GetTargetSkeleton())
	{
		OutMissingCurvesInSkeleton.Empty();

		// Retrieve the names of all the curves in the skeleton
		TArray<FName> SkeletonCurveNames;
		TargetSkeleton->GetCurveMetaDataNames(SkeletonCurveNames);

		// Check whether the input curve names exist in the skeleton
		for (const FString& Curve : InCurves)
		{
			if (!SkeletonCurveNames.Contains(FName{ Curve }))
			{
				OutMissingCurvesInSkeleton.Add(Curve);
			}
		}

		if (!OutMissingCurvesInSkeleton.IsEmpty())
		{
			return false;
		}
	}

	return true;
}

/////////////////////////////////////////////////////
// UMetaHumanPerformanceExportUtils

UMetaHumanPerformanceExportAnimationSettings* UMetaHumanPerformanceExportUtils::GetExportAnimationSequenceSettings(const UMetaHumanPerformance* InPerformance)
{
	check(InPerformance);

	UMetaHumanPerformanceExportAnimationSettings* ExportAnimationSettings = GetMutableDefault<UMetaHumanPerformanceExportAnimationSettings>();

	ExportAnimationSettings->PackagePath = FPackageName::GetLongPackagePath(InPerformance->GetPathName());
	ExportAnimationSettings->AssetName = TEXT("AS_") + FPackageName::GetShortName(InPerformance->GetName());
	ExportAnimationSettings->bAutoSaveAnimSequence = true;
	ExportAnimationSettings->bFortniteCompatibility = true;
	ExportAnimationSettings->ExportRange = EPerformanceExportRange::ProcessingRange;
	ExportAnimationSettings->bEnableHeadMovement = CanExportHeadMovement(InPerformance) && InPerformance->HeadMovementMode != EPerformanceHeadMovementMode::Disabled;

	// Try to use a MetaHuman as sensible default target skeleton to use when exporting the animation sequence.
	// If not present, force the user to manually select.

	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	FString NewClassPath;
	NewClassPath = ContentBrowserModule.Get().GetInitialPathToSaveAsset(FContentBrowserItemPath(NewClassPath, EContentBrowserPathType::Internal)).GetInternalPathString();
	const FString MetaHumanPath = NewClassPath + TEXT("/MetaHumans/Common/Face/Face_Archetype_Skeleton.Face_Archetype_Skeleton");

	if (USkeleton* ArchetypeSkeleton = LoadObject<USkeleton>(ExportAnimationSettings, *MetaHumanPath, nullptr, LOAD_NoWarn))
	{
		ExportAnimationSettings->TargetSkeletonOrSkeletalMesh = ArchetypeSkeleton;
	}
	else
	{
		ExportAnimationSettings->TargetSkeletonOrSkeletalMesh = nullptr;
	}

	return ExportAnimationSettings;
}

UMetaHumanPerformanceExportLevelSequenceSettings* UMetaHumanPerformanceExportUtils::GetExportLevelSequenceSettings(const UMetaHumanPerformance* InPerformance)
{
	check(InPerformance);

	UMetaHumanPerformanceExportLevelSequenceSettings* ExportLevelSequenceSettings = GetMutableDefault<UMetaHumanPerformanceExportLevelSequenceSettings>();

	ExportLevelSequenceSettings->PackagePath = FPackageName::GetLongPackagePath(InPerformance->GetPathName());
	ExportLevelSequenceSettings->AssetName = TEXT("LS_") + FPackageName::GetShortName(InPerformance->GetName());
	ExportLevelSequenceSettings->bExportCamera = true;
	ExportLevelSequenceSettings->bApplyLensDistortion = false;
	ExportLevelSequenceSettings->bExportDepthMesh = false;
	ExportLevelSequenceSettings->ExportRange = EPerformanceExportRange::WholeSequence;
	ExportLevelSequenceSettings->bKeepFrameRange = true;

	ExportLevelSequenceSettings->bExportVideoTrack = CanExportVideoTrack(InPerformance);
	ExportLevelSequenceSettings->bExportDepthTrack = CanExportDepthTrack(InPerformance);
	ExportLevelSequenceSettings->bExportAudioTrack = CanExportAudioTrack(InPerformance);
	ExportLevelSequenceSettings->bExportIdentity = CanExportIdentity(InPerformance);

	ExportLevelSequenceSettings->bExportImagePlane = ExportLevelSequenceSettings->bExportVideoTrack;
	
	if (ExportLevelSequenceSettings->bExportIdentity)
	{
		ExportLevelSequenceSettings->bExportControlRigTrack = InPerformance->ControlRigClass != nullptr;
		ExportLevelSequenceSettings->bEnableControlRigHeadMovement = CanExportHeadMovement(InPerformance) && InPerformance->HeadMovementMode == EPerformanceHeadMovementMode::ControlRig;
		ExportLevelSequenceSettings->bExportTransformTrack = InPerformance->HeadMovementMode == EPerformanceHeadMovementMode::TransformTrack;
	}
	else
	{
		ExportLevelSequenceSettings->bExportControlRigTrack = false;
		ExportLevelSequenceSettings->bEnableControlRigHeadMovement = false;
	}

	ExportLevelSequenceSettings->bEnableMetaHumanHeadMovement = CanExportHeadMovement(InPerformance) && InPerformance->HeadMovementMode != EPerformanceHeadMovementMode::Disabled;

	return ExportLevelSequenceSettings;
}

UAnimSequence* UMetaHumanPerformanceExportUtils::ExportAnimationSequence(UMetaHumanPerformance* InPerformance, UMetaHumanPerformanceExportAnimationSettings* InExportSettings)
{
	MHA_CPUPROFILER_EVENT_SCOPE(UMetaHumanPerformanceExportUtils::ExportAnimationSequence)

	if (InPerformance == nullptr)
	{
		UE_LOG(LogMetaHumanPerformance, Error, TEXT("Failed to export Animation Sequence. Performance is not valid"));
		return nullptr;
	}

	// Used to determine the name of the exported asset
	const bool bHasUserExportSettings = InExportSettings != nullptr;

	if (InExportSettings == nullptr)
	{
		InExportSettings = GetExportAnimationSequenceSettings(InPerformance);
	}

	if (InExportSettings->PackagePath.IsEmpty())
	{
		InExportSettings->PackagePath = FPackageName::GetLongPackagePath(InPerformance->GetPathName());
	}

	if (InExportSettings->AssetName.IsEmpty() || !bHasUserExportSettings)
	{
		// if the user didn't pass in a settings object, derive the name of the animation sequence based on the Performance's name
		InExportSettings->AssetName = FString::Format(TEXT("AS_{0}"), { InPerformance->GetName() });
	}

	const FString PackagePath = InExportSettings->PackagePath;
	const FString AssetName = InExportSettings->AssetName;

	UAnimSequence* NewAnimSequence = nullptr;

	if (InExportSettings->bShowExportDialog)
	{
		bool bUseExisting = true;
		NewAnimSequence = CreateAssetWithDialog<UAnimSequence>(AssetName, PackagePath, nullptr, LOCTEXT("SaveAssetDialogTitle", "Save Animation Sequence As"), bUseExisting);

		const TSet<FString> AnimationCurves = InPerformance->GetAnimationCurveNames();
		if (NewAnimSequence != nullptr && !SMetaHumanPerformanceExportSettings::ShowSettingsDialog(InExportSettings, InPerformance, AnimationCurves, TAttribute<TObjectPtr<UObject>>::CreateLambda([InExportSettings] { return InExportSettings->TargetSkeletonOrSkeletalMesh; })))
		{
			UE_LOG(LogMetaHumanPerformance, Error, TEXT("Export of Animation Sequence '%s' has been cancelled"), *NewAnimSequence->GetName());
			if (bUseExisting)
			{
				DeleteAsset(NewAnimSequence);
			}

			NewAnimSequence = nullptr;
		}
	}
	else
	{
		IAssetTools& AssetTools = FAssetToolsModule::GetModule().Get();
		NewAnimSequence = Cast<UAnimSequence>(AssetTools.CreateAsset(AssetName, PackagePath, UAnimSequence::StaticClass(), nullptr));
	}

	if (NewAnimSequence != nullptr)
	{
		USkeletalMesh* TargetSkeletalMesh = Cast<USkeletalMesh>(InExportSettings->TargetSkeletonOrSkeletalMesh);
		USkeleton* TargetSkeleton = InExportSettings->GetTargetSkeleton();

		// If we have a skeletal mesh, use the skeleton from it
		if (TargetSkeletalMesh != nullptr)
		{
			TargetSkeleton = TargetSkeletalMesh->GetSkeleton();
		}

		if (TargetSkeleton != nullptr)
		{
			NewAnimSequence->SetSkeleton(TargetSkeleton);
			if (TargetSkeletalMesh != nullptr)
			{
				NewAnimSequence->SetPreviewMesh(TargetSkeletalMesh);
			}
			NewAnimSequence->GetController().InitializeModel();
			NewAnimSequence->MarkPackageDirty();

			RecordAnimationSequence({ NewAnimSequence }, InPerformance, InExportSettings);
		}
		else
		{
			UE_LOG(LogMetaHumanPerformance, Error, TEXT("Failed to create Animation Sequence '%s/%s due to invalid target Skeleton'"), *PackagePath, *AssetName);
		}
	}
	else
	{
		UE_LOG(LogMetaHumanPerformance, Error, TEXT("Failed to create Animation Sequence '%s/%s'"), *PackagePath, *AssetName);
	}

	return NewAnimSequence;
}

template<typename TComp>
static TComp* GetComponentByNameOrClass(AActor* InActor, const FName& InComponentName)
{
	if (InActor != nullptr)
	{
		if (InComponentName != NAME_None)
		{
			TInlineComponentArray<TComp*> Components;
			InActor->GetComponents(Components);

			TComp** FoundComponent = Components.FindByPredicate([&InComponentName](const TComp* Comp)
			{
				return Comp->GetFName() == InComponentName;
			});

			if (FoundComponent != nullptr)
			{
				return *FoundComponent;
			}
		}
		else
		{
			return InActor->FindComponentByClass<TComp>();
		}
	}

	return nullptr;
}

void UMetaHumanPerformanceExportUtils::BakeControlRigTrack(const FBakeControlRigTrackParams& InParams)
{
	MHA_CPUPROFILER_EVENT_SCOPE(UMetaHumanPerformanceExportUtils::BakeControlRigTrack)

	check(InParams.LevelSequence);
	check(InParams.Performance);
	check(InParams.ExportSettings);
	check(InParams.ObjectToBind);
	check(InParams.Binding.IsValid());

	if (InParams.ControlRigClass)
	{
		UMovieScene* MovieScene = InParams.LevelSequence->GetMovieScene();
		const int32 ProcessingLimitStartFrame = InParams.Performance->GetProcessingLimitFrameRange().GetLowerBoundValue().Value;

		if (UMovieSceneControlRigParameterTrack* ControlRigTrack = MovieScene->AddTrack<UMovieSceneControlRigParameterTrack>(InParams.Binding))
		{
			FString ControlRigObjectName = InParams.ControlRigClass->GetName();
			ControlRigObjectName.RemoveFromEnd(TEXT("_C"));

			UControlRig* ControlRig = NewObject<UControlRig>(ControlRigTrack, InParams.ControlRigClass, FName{ *ControlRigObjectName }, RF_Transactional);
			ControlRig->SetObjectBinding(MakeShared<FControlRigObjectBinding>());
			ControlRig->GetObjectBinding()->BindToObject(InParams.ObjectToBind);
			ControlRig->GetDataSourceRegistry()->RegisterDataSource(UControlRig::OwnerComponent, ControlRig->GetObjectBinding()->GetBoundObject());
			ControlRig->Initialize();
			ControlRig->Evaluate_AnyThread();

			ControlRigTrack->Modify();
			ControlRigTrack->SetTrackName(FName{ *ControlRigObjectName });
			ControlRigTrack->SetDisplayName(FText::FromString(ControlRigObjectName));

			constexpr bool bSequencerOwnsControlRig = true;
			UMovieSceneControlRigParameterSection* ControlRigSection = CastChecked<UMovieSceneControlRigParameterSection>(ControlRigTrack->CreateControlRigSection(0, ControlRig, bSequencerOwnsControlRig));
			ControlRigSection->Modify();

			// Repopulate the control rig track with existing animation data, if any
			const TArray64<FFrameAnimationData>& AnimationData = InParams.Performance->AnimationData;
			const FTransform ReferenceTransform = InParams.Performance->CalculateReferenceFramePose();
			for (int32 AnimationFrameIndex = 0; AnimationFrameIndex < AnimationData.Num(); ++AnimationFrameIndex)
			{
				if (AnimationData[AnimationFrameIndex].ContainsData())
				{
					UMetaHumanPerformanceExportUtils::BakeControlRigAnimationData(InParams.Performance,
						InParams.LevelSequence,
						AnimationFrameIndex + ProcessingLimitStartFrame,
						ControlRigSection,
						ReferenceTransform,
						InParams.Performance->GetExcludedFrame(AnimationFrameIndex + ProcessingLimitStartFrame + 1) == EFrameRangeType::None ? InParams.ExportSettings->CurveInterpolation.GetValue() : ERichCurveInterpMode::RCIM_Linear,
						ControlRig,
						FVector::ZeroVector);
				}
			}

			UMetaHumanPerformanceExportUtils::SetHeadControlSwitchEnabled(ControlRigTrack, InParams.bEnableHeadMovementSwitch);

			if (InParams.ExportSettings->ExportRange == EPerformanceExportRange::ProcessingRange && !InParams.ExportSettings->bKeepFrameRange)
			{
				ControlRigSection->MoveSection(-InParams.ProcessingRange.GetLowerBoundValue());
			}
		}
		else
		{
			UE_LOG(LogMetaHumanPerformance, Error, TEXT("Failed to add new Control Rig track in the Level Sequence '%s'"), *(InParams.LevelSequence->GetName()));
		}
	}
	else
	{
		UE_LOG(LogMetaHumanPerformance, Error, TEXT("Failed to obtain Control Rig Blueprint class '%s'"), *(InParams.Performance->ControlRigClass->GetName()));
	}
}

ULevelSequence* UMetaHumanPerformanceExportUtils::ExportLevelSequence(UMetaHumanPerformance* InPerformance, UMetaHumanPerformanceExportLevelSequenceSettings* InExportSettings)
{
	MHA_CPUPROFILER_EVENT_SCOPE(UMetaHumanPerformanceExportUtils::ExportLevelSequence)

	if (InPerformance == nullptr)
	{
		UE_LOG(LogMetaHumanPerformance, Error, TEXT("Invalid Performance passed to ExportLevelSequence"));
		return nullptr;
	}

	// Used to determine the name of the exported asset
	const bool bHasUserExportSettings = InExportSettings != nullptr;

	if (InExportSettings == nullptr)
	{
		InExportSettings = GetExportLevelSequenceSettings(InPerformance);
	}

	if (InExportSettings->PackagePath.IsEmpty())
	{
		InExportSettings->PackagePath = FPackageName::GetLongPackagePath(InPerformance->GetPathName());
	}

	if (InExportSettings->AssetName.IsEmpty() || !bHasUserExportSettings)
	{
		// if the user didn't pass in a settings object, derive the name of the level sequence based on the Performance's name
		InExportSettings->AssetName = TEXT("LS_") + FPackageName::GetShortName(InPerformance->GetName());
	}

	IAssetTools& AssetTools = FAssetToolsModule::GetModule().Get();

	// Find the level sequence factory so we can call CreateAssetWithDialog
	TArray<UFactory*> Factories = AssetTools.GetNewAssetFactories();
	UFactory** LevelSequenceFactory = Factories.FindByPredicate([](const UFactory* InFactory)
	{
		return InFactory->GetSupportedClass() == ULevelSequence::StaticClass();
	});
	check(LevelSequenceFactory && *LevelSequenceFactory);

	ULevelSequence* NewLevelSequence = nullptr;
	if (InExportSettings->bShowExportDialog)
	{
		bool bUseExisting = false;
		NewLevelSequence = CreateAssetWithDialog<ULevelSequence>(InExportSettings->AssetName, InExportSettings->PackagePath, *LevelSequenceFactory, LOCTEXT("SaveLevelSequenceAssetDialogTitle", "Save Level Sequence As"), bUseExisting);
		if (NewLevelSequence == nullptr)
		{
			UE_LOG(LogMetaHumanPerformance, Error, TEXT("Failed to create new Level Sequence named '%s'"), *(InExportSettings->PackagePath / InExportSettings->AssetName))
			return nullptr;
		}

		if (!SMetaHumanPerformanceExportSettings::ShowSettingsDialog(InExportSettings, InPerformance))
		{
			UE_LOG(LogMetaHumanPerformance, Warning, TEXT("Export of Level Sequence '%s' has been cancelled"), *NewLevelSequence->GetName());
			if (bUseExisting)
			{
				DeleteAsset(NewLevelSequence);
			}
			return nullptr;
		}
	}
	else
	{
		NewLevelSequence = Cast<ULevelSequence>(AssetTools.CreateAsset(InExportSettings->AssetName, InExportSettings->PackagePath, ULevelSequence::StaticClass(), *LevelSequenceFactory));

		if (NewLevelSequence == nullptr)
		{
			UE_LOG(LogMetaHumanPerformance, Error, TEXT("Failed to create new Level Sequence asset named '%s'"), *(InExportSettings->PackagePath / InExportSettings->AssetName))
			return nullptr;
		}
	}

	UMovieScene* NewMovieScene = NewLevelSequence->GetMovieScene();
	check(NewMovieScene);
	UMovieSceneSequence* MovieSceneSequence = Cast<UMovieSceneSequence>(NewLevelSequence);

	// Dividing this function into the following blocks for progress reporting purposes
	// - Setup
	// - Identity
	// - Video Track
	// - Depth Track
	// - Audio Track
	// - Target MetaHuman
	//	- Bake Face Control Rig
	//  - Bake Body Control Rig
	// - Camera

	float AmountOfWork = 7.0f;
	if (InExportSettings->bExportControlRigTrack)
	{
		AmountOfWork += 1.0f;
	}
	if (InExportSettings->TargetMetaHumanClass != nullptr)
	{
		AmountOfWork += 2.0f;
	}

	FScopedSlowTask ExportLevelSequenceTask{ AmountOfWork, LOCTEXT("ExportLevelSequence", "Exporting Level Sequence...") };
	ExportLevelSequenceTask.MakeDialog();

	ExportLevelSequenceTask.EnterProgressFrame(1.0f);

	USkeletalMeshComponent* IdentityFaceComponent = nullptr;

	// Transform used to position the camera's height to focus on the Identity Actor and the footage plane.
	// This is updated based on the height of the nose bone of the Identity Actor or is updated based on the MetaHuman if chosen for export
	FTransform OffsetTransform;
	FName NoseBoneName = "FACIAL_C_12IPV_NoseTip2";
	if (CanExportIdentity(InPerformance))
	{
		UMetaHumanIdentityFace* Face = InPerformance->Identity->FindPartOfClass<UMetaHumanIdentityFace>();
		IdentityFaceComponent = Face->RigComponent;

		FVector IdentityNosePosition = UMetaHumanPerformance::GetSkelMeshReferenceBoneLocation(IdentityFaceComponent, NoseBoneName);
		OffsetTransform.SetTranslation(FVector{0.0, 0.0, IdentityNosePosition.Z});
	}
	else
	{
		OffsetTransform = FTransform{ FVector{ 0.0, 0.0, 145.98f } }; // default nose height
	}

	// Reference frame transform from performance.
	FTransform ReferenceFramePose = InPerformance->CalculateReferenceFramePose();
	if (ReferenceFramePose.Equals(FTransform::Identity))
	{
		// Set default reference frame pose if there isn't one in the performance to position things in level sensibly
		ReferenceFramePose.SetTranslation(FVector(50.0, 0.0, -OffsetTransform.GetTranslation().Z));
		ReferenceFramePose.SetRotation(FQuat(FRotator(0.0, 90.0, 0.0)));
	}

	TObjectPtr<UFootageCaptureData> FootageCaptureData = InPerformance->FootageCaptureData;
	
	// Get footage view for performance's camera if available
	TObjectPtr<class UImgMediaSource> ImageSequence = GetCaptureFootage(InPerformance, false);
	TObjectPtr<class UImgMediaSource> DepthSequence = GetCaptureFootage(InPerformance, true);

	FTimecode ImageTimecode = FTimecode();
	FTimecode DepthTimecode = FTimecode();

	if (ImageSequence)
	{
		ImageTimecode = UImageSequenceTimecodeUtils::GetTimecode(ImageSequence);
	}

	if (DepthSequence)
	{
		DepthTimecode = UImageSequenceTimecodeUtils::GetTimecode(DepthSequence);
	}
	
	// Transform the frame ranges to use the new sequence tick resolution so we can set the values in the sections
	const FFrameRate SourceRate = (ImageSequence && ImageSequence->FrameRateOverride.IsValid()) ? ImageSequence->FrameRateOverride : NewMovieScene->GetDisplayRate();
	const FFrameRate TargetRate = NewMovieScene->GetTickResolution();
	TMap<TWeakObjectPtr<UObject>, TRange<FFrameNumber>> MediaFrameRanges = InPerformance->GetMediaFrameRanges();
	for (TPair<TWeakObjectPtr<UObject>, TRange<FFrameNumber>>& MediaFrameRangePair : MediaFrameRanges)
	{
		TRange<FFrameNumber>& FrameRange = MediaFrameRangePair.Value;
		FrameRange.SetLowerBoundValue(FFrameRate::TransformTime(FrameRange.GetLowerBoundValue(), SourceRate, TargetRate).FrameNumber);
		FrameRange.SetUpperBoundValue(FFrameRate::TransformTime(FrameRange.GetUpperBoundValue(), SourceRate, TargetRate).FrameNumber);
	}

	const TRange<FFrameNumber> ProcessingRange(FFrameRate::TransformTime(FFrameTime((int32)InPerformance->StartFrameToProcess), SourceRate, TargetRate).FrameNumber,
											   FFrameRate::TransformTime(FFrameTime((int32)InPerformance->EndFrameToProcess), SourceRate, TargetRate).FrameNumber);

	const FFrameRate TickRate = NewMovieScene->GetTickResolution();
	TRange<FFrameNumber> PlaybackRange = ProcessingRange;
	TRange<FFrameNumber> ViewRange = ProcessingRange;
	if (InExportSettings->ExportRange == EPerformanceExportRange::ProcessingRange)
	{
		if (!InExportSettings->bKeepFrameRange)
		{
			PlaybackRange.SetLowerBoundValue(0);
			PlaybackRange.SetUpperBoundValue(ProcessingRange.GetUpperBoundValue() - ProcessingRange.GetLowerBoundValue());
		}

		ViewRange = PlaybackRange;
	}
	else if (InExportSettings->ExportRange == EPerformanceExportRange::WholeSequence)
	{
		const TRange<FFrameNumber>& ProcessingLimitRange = InPerformance->GetProcessingLimitFrameRange();
		ViewRange.SetLowerBoundValue(FFrameRate::TransformTime(ProcessingLimitRange.GetLowerBoundValue(), SourceRate, TickRate).FrameNumber);
		ViewRange.SetUpperBoundValue(FFrameRate::TransformTime(ProcessingLimitRange.GetUpperBoundValue(), SourceRate, TickRate).FrameNumber);
	}

	NewMovieScene->SetPlaybackRange(PlaybackRange);
	NewMovieScene->SetDisplayRate(SourceRate);

	constexpr float ViewTimeOffset = 0.1f;
	FMovieSceneEditorData& EditorData = NewMovieScene->GetEditorData();
	EditorData.ViewStart = TickRate.AsSeconds(ViewRange.GetLowerBoundValue()) - ViewTimeOffset;
	EditorData.ViewEnd = TickRate.AsSeconds(ViewRange.GetUpperBoundValue()) + ViewTimeOffset;
	EditorData.WorkStart = EditorData.ViewStart;
	EditorData.WorkEnd = EditorData.ViewEnd;

	// Path where to store extra assets needed by the level sequence
	// This includes a media texture and a material instance that is applied to the footage plane actor
	const FString NewLevelSequencerAssetsPackagePath = NewLevelSequence->GetOuter()->GetName() + TEXT("_Assets");

	// Temporary object used to calculate the transform to be applied to the FootagePlaneActor and the FieldOfView to set in the Camera Actor
	UMetaHumanFootageComponent* TempFootageComponent = nullptr;

	// We can only create the temp footage component if we have valid camera information - so we avoid constructing it if that is not the case
	if (InExportSettings->bExportVideoTrack || InExportSettings->bExportImagePlane)
	{
		TempFootageComponent = NewObject<UMetaHumanFootageComponent>(GetTransientPackage());
		check(TempFootageComponent);

		if (FootageCaptureData)
		{
			if (FootageCaptureData->CameraCalibrations.IsEmpty())
			{
				int32 NumImageFrames = 0;
				FIntVector2 ImDims;
				if (FImageSequenceUtils::GetImageSequenceInfoFromAsset(ImageSequence, ImDims, NumImageFrames))
				{
					TempFootageComponent->SetFootageResolution(FVector2D(ImDims[0], ImDims[1]));
				}
			}
			else
			{
				TempFootageComponent->SetCameraCalibration(FootageCaptureData->CameraCalibrations[0]);
			}
		}
		TempFootageComponent->SetCamera(InPerformance->Camera);
		TempFootageComponent->ShowColorChannel(EABImageViewMode::A);
	}

	// Start frame for processing that takes into account timecode alignment between tracks
	const int32 ProcessingLimitStartFrame = InPerformance->GetProcessingLimitFrameRange().GetLowerBoundValue().Value;
	FFrameNumber ImageStartTime = FFrameRate::TransformTime(FFrameTime(ProcessingLimitStartFrame), SourceRate, TargetRate).FrameNumber;
	FFrameNumber ImageEndTime = FFrameRate::TransformTime(FFrameTime((int32)InPerformance->GetProcessingLimitFrameRange().GetUpperBoundValue().Value), SourceRate, TargetRate).FrameNumber;

	// Export the target MetaHuman first
	// If exporting the target MetaHuman, update the offset transform to position everything using the MetaHuman height
	ExportLevelSequenceTask.EnterProgressFrame(1.0f, LOCTEXT("ExportTargetMetaHuman", "Exporting Target MetaHuman"));
	TSharedRef<UE::MovieScene::FSharedPlaybackState> TransientPlaybackState = MovieSceneHelpers::CreateTransientSharedPlaybackState(GEditor->GetEditorWorldContext().World(), MovieSceneSequence);

	UE::Sequencer::FCreateBindingParams CreateBindingParams;
	CreateBindingParams.bSpawnable = true;
	CreateBindingParams.bAllowCustomBinding = true;

	if (InExportSettings->TargetMetaHumanClass != nullptr)
	{
		const FGuid MetaHumanBinding = FSequencerUtilities::CreateOrReplaceBinding(nullptr, MovieSceneSequence, InExportSettings->TargetMetaHumanClass, CreateBindingParams);
		check(MetaHumanBinding.IsValid());

		check(MovieSceneHelpers::SupportsObjectTemplate(MovieSceneSequence, MetaHumanBinding, TransientPlaybackState));
		AActor* MetaHumanActor =  CastChecked<AActor>(MovieSceneHelpers::GetObjectTemplate(MovieSceneSequence, MetaHumanBinding, TransientPlaybackState, 0));

		if(MetaHumanActor)
		{
			// MetaHuman needs to be spawned for components to be created to set the object template and bind to them. Spawn into temporary world.
			bool bInformEngineOfWorld = false;
			bool bAddToRoot = false;
			UWorld* TempWorld = UWorld::CreateWorld(EWorldType::Editor, bInformEngineOfWorld, MakeUniqueObjectName(GetTransientPackage(), UWorld::StaticClass(), TEXT("MetahumanExportUtilsSpawner")), GetTransientPackage(), bAddToRoot);
			check(TempWorld);

			FActorSpawnParameters SpawnInfo;
			SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			SpawnInfo.bNoFail = true;
			SpawnInfo.bTemporaryEditorActor = true;
			SpawnInfo.ObjectFlags = RF_Transactional | RF_Standalone;

			if (AActor* SpawnedMetaHuman = TempWorld->SpawnActor<AActor>(MetaHumanActor->GetClass(), SpawnInfo))
			{
				// First set the MetaHuman offset transform
				Add3DTransformTrackToBinding(NewMovieScene, MetaHumanBinding, ReferenceFramePose * OffsetTransform);

				// Update offset transform
				if (USkeletalMeshComponent* FaceComponent = GetComponentByNameOrClass<USkeletalMeshComponent>(SpawnedMetaHuman, MetaHumanFaceComponentName))
				{
					// Get the difference between the MetaHuman nose bone position and the current offset transform
					FVector MHNosePosition = UMetaHumanPerformance::GetSkelMeshReferenceBoneLocation(FaceComponent, NoseBoneName);
					FTransform MHNoseOffset = FTransform{FVector{ 0, 0, MHNosePosition.Z - OffsetTransform.GetTranslation().Z}};

					// Update the offset transform with the MetaHuman offset
					OffsetTransform = OffsetTransform * MHNoseOffset;
				}
				else
				{
					UE_LOG(LogMetaHumanPerformance, Warning, TEXT("Unable to update offset transform. Failed to find '%s' component in spawned class '%s'"), *MetaHumanFaceComponentName.ToString(), *InExportSettings->TargetMetaHumanClass->GetName());
				}

				// Updated spawnable to be owned by the movie scene and set as the spawnable's template
				SpawnedMetaHuman->Rename(nullptr, NewMovieScene);
				MovieSceneHelpers::SetObjectTemplate(MovieSceneSequence, MetaHumanBinding, SpawnedMetaHuman, TransientPlaybackState);

				// Bind components and bake control rig tracks
				if (USkeletalMeshComponent* FaceComponent = GetComponentByNameOrClass<USkeletalMeshComponent>(SpawnedMetaHuman, MetaHumanFaceComponentName))
				{
					// Bind face component
					FGuid FaceComponentGuid = BindActorComponentToParentActorInLevelSequence<USkeletalMeshComponent>(NewLevelSequence, *FaceComponent, MetaHumanBinding, SpawnedMetaHuman);
					check(FaceComponentGuid.IsValid())

					// Bake control rig to face
					ExportLevelSequenceTask.EnterProgressFrame(1.0f, LOCTEXT("BakeFaceControlRig", "Baking MetaHuman Face Control Rig"));

					UClass* FaceControlRigClass = GetDefaultControlRigClass(FaceComponent);
					BakeControlRigTrack({
						.Performance = InPerformance,
						.ExportSettings = InExportSettings,
						.ProcessingRange = ProcessingRange,
						.LevelSequence = NewLevelSequence,
						.ControlRigClass = FaceControlRigClass,
						.Binding = FaceComponentGuid,
						.ObjectToBind = FaceComponent,
						.bEnableHeadMovementSwitch = false
					});
				}
				else
				{
					UE_LOG(LogMetaHumanPerformance, Warning, TEXT("Failed to find '%s' component in spawned class '%s'"), *MetaHumanFaceComponentName.ToString(), *InExportSettings->TargetMetaHumanClass->GetName());
				}

				if (USkeletalMeshComponent* BodyComponent = GetComponentByNameOrClass<USkeletalMeshComponent>(SpawnedMetaHuman, MetaHumanBodyComponentName))
				{
					FGuid BodyComponentGuid = BindActorComponentToParentActorInLevelSequence<USkeletalMeshComponent>(NewLevelSequence, *BodyComponent, MetaHumanBinding, SpawnedMetaHuman);
					check(BodyComponentGuid.IsValid())

					ExportLevelSequenceTask.EnterProgressFrame(1.0f, LOCTEXT("BakeBodyControlRig", "Baking MetaHuman Body Control Rig"));

					UClass* BodyControlRigClass = GetDefaultControlRigClass(BodyComponent);
					bool bEnableHeadMovement = InExportSettings->bEnableMetaHumanHeadMovement && CanExportHeadMovement(InPerformance);

					BakeControlRigTrack({
						.Performance = InPerformance,
						.ExportSettings = InExportSettings,
						.ProcessingRange = ProcessingRange,
						.LevelSequence = NewLevelSequence,
						.ControlRigClass = BodyControlRigClass,
						.Binding = BodyComponentGuid,
						.ObjectToBind = BodyComponent,
						.bEnableHeadMovementSwitch = !!bEnableHeadMovement
					});
				}
				else
				{
					UE_LOG(LogMetaHumanPerformance, Warning, TEXT("Failed to find '%s' component in spawned class '%s'"), *MetaHumanBodyComponentName.ToString(), *InExportSettings->TargetMetaHumanClass->GetName());
				}
			}
			else
			{
				UE_LOG(LogMetaHumanPerformance, Warning, TEXT("Failed to spawn Actor of class '%s'"), *InExportSettings->TargetMetaHumanClass->GetName());
			}

			// Clean up temporary world
			TempWorld->ClearWorldComponents();
			TempWorld->CleanupWorld();
		}
		else
		{
			UE_LOG(LogMetaHumanPerformance, Warning, TEXT("Failed to spawn Actor of class '%s' in the Level"), *InExportSettings->TargetMetaHumanClass->GetName());
		}
	}

	ExportLevelSequenceTask.EnterProgressFrame(1.0f, LOCTEXT("ExportIdentity", "Exporting MetaHuman Identity"));

	if (InExportSettings->bExportIdentity)
	{
		if (CanExportIdentity(InPerformance))
		{
		    const FGuid IdentityActorBinding = FSequencerUtilities::CreateOrReplaceBinding(nullptr, MovieSceneSequence, ASkeletalMeshActor::StaticClass(), CreateBindingParams);
		    check(IdentityActorBinding.IsValid());

		    FMovieScenePossessable* IdentityPossessable = NewMovieScene->FindPossessable(IdentityActorBinding);
		    check(IdentityPossessable);
		    IdentityPossessable->SetName(InPerformance->Identity->GetName());

		    check(MovieSceneHelpers::SupportsObjectTemplate(MovieSceneSequence, IdentityActorBinding, TransientPlaybackState));

		    ASkeletalMeshActor* IdentityActor = CastChecked<ASkeletalMeshActor>(MovieSceneHelpers::GetObjectTemplate(MovieSceneSequence, IdentityActorBinding, TransientPlaybackState, 0));
		    check(IdentityActor);

			IdentityActor->GetSkeletalMeshComponent()->SetSkeletalMesh(IdentityFaceComponent->GetSkeletalMeshAsset());
			IdentityActor->GetSkeletalMeshComponent()->UpdateBounds();

			Add3DTransformTrackToBinding(NewMovieScene, IdentityActorBinding, ReferenceFramePose * OffsetTransform);
			NewMovieScene->AddTrack<UMovieSceneSkeletalAnimationTrack>(IdentityActorBinding);

			if (InExportSettings->bExportControlRigTrack)
			{
				ExportLevelSequenceTask.EnterProgressFrame(1.0f, LOCTEXT("BakeIdentityControlRig", "Baking Identity Control Rig"));

				bool bEnableHeadMovement = InExportSettings->bEnableControlRigHeadMovement && CanExportHeadMovement(InPerformance);

				BakeControlRigTrack({
					.Performance = InPerformance,
					.ExportSettings = InExportSettings,
					.ProcessingRange = ProcessingRange,
					.LevelSequence = NewLevelSequence,
					.ControlRigClass = InPerformance->ControlRigClass,
					.Binding = IdentityActorBinding,
					.ObjectToBind = IdentityActor,
					.bEnableHeadMovementSwitch = !!bEnableHeadMovement
				});
			}

			if (InExportSettings->bExportTransformTrack)
			{
				if (UMovieScene3DTransformTrack* TransformTrack = NewMovieScene->FindTrack<UMovieScene3DTransformTrack>(IdentityActorBinding))
				{
					check(!TransformTrack->GetAllSections().IsEmpty());

					UMovieScene3DTransformSection* TransformSection = Cast<UMovieScene3DTransformSection>(TransformTrack->GetAllSections()[0]);
					check(TransformSection);

					// Bake the animation data into the transform section of the new level sequence
					const TArray64<FFrameAnimationData>& AnimationData = InPerformance->AnimationData;
					for (int32 AnimationFrameIndex = 0; AnimationFrameIndex < AnimationData.Num(); ++AnimationFrameIndex)
					{
						if (AnimationData[AnimationFrameIndex].ContainsData())
						{
							BakeTransformAnimationData(InPerformance, NewLevelSequence, AnimationFrameIndex + ProcessingLimitStartFrame, TransformSection, InPerformance->GetExcludedFrame(AnimationFrameIndex + ProcessingLimitStartFrame + 1) == EFrameRangeType::None ? InExportSettings->CurveInterpolation.GetValue() : ERichCurveInterpMode::RCIM_Linear, OffsetTransform);
						}
					}

					if (InExportSettings->ExportRange == EPerformanceExportRange::ProcessingRange && !InExportSettings->bKeepFrameRange)
					{
						TransformSection->MoveSection(-ProcessingRange.GetLowerBoundValue());
					}
				}
				else
				{
					UE_LOG(LogMetaHumanPerformance, Warning, TEXT("Failed to add new Transform track in the Level Sequence '%s'"), *InExportSettings->AssetName);
				}
			}
		}
		else
		{
			UE_LOG(LogMetaHumanPerformance, Warning, TEXT("Export identity was set no valid identity was found in '%s'. Skipping identity export"), *InExportSettings->AssetName);
		}
	}

	ExportLevelSequenceTask.EnterProgressFrame(1.0f, LOCTEXT("ExportVideoTrack", "Exporting Video Track"));

	if (InExportSettings->bExportVideoTrack)
	{
		if (CanExportVideoTrack(InPerformance))
		{
			if (UMovieSceneMediaTrack* NewVideoTrack = NewMovieScene->AddTrack<UMovieSceneMediaTrack>())
			{
				NewVideoTrack->SetDisplayName(LOCTEXT("VideoSequenceTrack", "Video"));

				// Can't use UMovieSceneMediaTrack::AddNewMediaSource because that will trigger an update on the range when opening the sequence and
				// we want control over that. Creating a new section using NewObject seems to prevent this from happening
				UMovieSceneMediaSection* NewVideoSection = NewObject<UMovieSceneMediaSection>(NewVideoTrack, NAME_None, RF_Transactional);
				NewVideoSection->TimecodeSource = ImageTimecode;
				NewVideoSection->MediaSource = ImageSequence;

				if (InExportSettings->ExportRange == EPerformanceExportRange::ProcessingRange)
				{
					NewVideoSection->SetRange(ProcessingRange);
					NewVideoSection->StartFrameOffset = ProcessingRange.GetLowerBoundValue() - ImageStartTime;

					if (!InExportSettings->bKeepFrameRange)
					{
						NewVideoSection->MoveSection(-ProcessingRange.GetLowerBoundValue());
					}
				}
				else
				{
					if (TRange<FFrameNumber>* FoundVideoFrameRange = MediaFrameRanges.Find(ImageSequence))
					{
						NewVideoSection->SetRange(*FoundVideoFrameRange);
					}
					else
					{
						UE_LOG(LogMetaHumanPerformance, Warning, TEXT("Failed to determine frame range for the video media track"));
					}
				}

				NewVideoTrack->AddSection(*NewVideoSection);

				if (InExportSettings->bExportImagePlane)
				{
					const FGuid FootageActorBinding = FSequencerUtilities::CreateOrReplaceBinding(nullptr, MovieSceneSequence, AStaticMeshActor::StaticClass(), CreateBindingParams);
					check(FootageActorBinding.IsValid());

					FMovieScenePossessable* FootagePossessable = NewMovieScene->FindPossessable(FootageActorBinding);
					check(FootagePossessable);
					FootagePossessable->SetName(FString::Format(TEXT("{0} Video Plane"), { InPerformance->GetName() }));
					
					check(MovieSceneHelpers::SupportsObjectTemplate(MovieSceneSequence, FootageActorBinding, TransientPlaybackState));

					AStaticMeshActor* FootagePlaneActor = CastChecked<AStaticMeshActor>(MovieSceneHelpers::GetObjectTemplate(MovieSceneSequence, FootageActorBinding, TransientPlaybackState, 0));
					check(FootagePlaneActor);

					UStaticMesh* FootagePlaneMesh = LoadObject<UStaticMesh>(FootagePlaneActor, TEXT("/Engine/BasicShapes/Plane"));
					FootagePlaneActor->GetStaticMeshComponent()->SetStaticMesh(FootagePlaneMesh);
					FootagePlaneActor->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					FootagePlaneActor->GetStaticMeshComponent()->UpdateBounds();

					check(TempFootageComponent);
					const FTransform FootagePlaneTransform = TempFootageComponent->GetFootagePlaneComponent(EABImageViewMode::A)->GetComponentTransform();
					Add3DTransformTrackToBinding(NewMovieScene, FootageActorBinding, FootagePlaneTransform * OffsetTransform);

					FString NewAssetName;
					FString NewPackageName;
					AssetTools.CreateUniqueAssetName(NewLevelSequencerAssetsPackagePath + TEXT("/T_") + NewLevelSequence->GetName(), TEXT(""), NewPackageName, NewAssetName);
					NewVideoSection->MediaTexture = Cast<UMediaTexture>(AssetTools.CreateAsset(NewAssetName, NewLevelSequencerAssetsPackagePath, UMediaTexture::StaticClass(), nullptr));
					NewVideoSection->MediaTexture->UpdateResource();

					UMaterial* DefaultMediaMaterial = LoadObject<UMaterial>(NewMovieScene, TEXT("/Script/Engine.Material'/" UE_PLUGIN_NAME "/Exporter/M_ImagePlaneMaterial.M_ImagePlaneMaterial'"));
					AssetTools.CreateUniqueAssetName(NewLevelSequencerAssetsPackagePath + TEXT("/MI_") + NewLevelSequence->GetName(), TEXT(""), NewPackageName, NewAssetName);

					UMaterialInstanceConstantFactoryNew* MaterialFactory = NewObject<UMaterialInstanceConstantFactoryNew>();
					MaterialFactory->InitialParent = DefaultMediaMaterial;

					UMaterialInstanceConstant* MediaMaterialInstance = Cast<UMaterialInstanceConstant>(AssetTools.CreateAsset(NewAssetName, NewLevelSequencerAssetsPackagePath, UMaterialInstanceConstant::StaticClass(), MaterialFactory));
					MediaMaterialInstance->SetTextureParameterValueEditorOnly(FMaterialParameterInfo{ TEXT("MediaTexture") }, NewVideoSection->MediaTexture);
					MediaMaterialInstance->PostEditChange();

					FootagePlaneActor->GetStaticMeshComponent()->SetMaterial(0, MediaMaterialInstance);
					FootagePlaneActor->PostEditChange();
				}
			}
		}
		else
		{
			UE_LOG(LogMetaHumanPerformance, Warning, TEXT("Export video track was set no video sequence was found in '%s'. Skipping video track export"), (FootageCaptureData != nullptr) ? *FootageCaptureData->GetName() : TEXT("No Footage Capture Data"));
		}
	}

	ExportLevelSequenceTask.EnterProgressFrame(1.0f, LOCTEXT("ExportDepthTrack", "Exporting Depth Track"));

	if (InExportSettings->bExportDepthTrack)
	{
		if (CanExportDepthTrack(InPerformance))
		{
			if (UMovieSceneMediaTrack* NewDepthTrack = NewMovieScene->AddTrack<UMovieSceneMediaTrack>())
			{
				NewDepthTrack->SetDisplayName(LOCTEXT("DepthSequenceTrack", "Depth"));

				// Can't use UMovieSceneMediaTrack::AddNewMediaSource because that will trigger an update on the range when opening the sequence and
				// we want control over that. Creating a new section using NewObject seems to prevent this from happening
				UMovieSceneMediaSection* NewDepthSection = NewObject<UMovieSceneMediaSection>(NewDepthTrack, NAME_None, RF_Transactional);
				NewDepthSection->TimecodeSource = DepthTimecode;
				NewDepthSection->MediaSource = DepthSequence;

				if (InExportSettings->ExportRange == EPerformanceExportRange::ProcessingRange)
				{
					NewDepthSection->SetRange(ProcessingRange);
					NewDepthSection->StartFrameOffset = ProcessingRange.GetLowerBoundValue() - ImageStartTime;

					if (!InExportSettings->bKeepFrameRange)
					{
						NewDepthSection->MoveSection(-ProcessingRange.GetLowerBoundValue());
					}
				}
				else
				{
					if (TRange<FFrameNumber>* FoundDepthFrameRange = MediaFrameRanges.Find(DepthSequence))
					{
						NewDepthSection->SetRange(*FoundDepthFrameRange);
					}
					else
					{
						UE_LOG(LogMetaHumanPerformance, Warning, TEXT("Failed to determine frame range for depth media track"));
					}
				}

				NewDepthTrack->AddSection(*NewDepthSection);

				if (InExportSettings->bExportDepthMesh)
				{
					const FGuid DepthMeshBinding = FSequencerUtilities::CreateOrReplaceBinding(nullptr, MovieSceneSequence, AActor::StaticClass(), CreateBindingParams);
					check(DepthMeshBinding.IsValid());

					FMovieScenePossessable* DepthMeshPossessable = NewMovieScene->FindPossessable(DepthMeshBinding);
					check(DepthMeshPossessable);
					DepthMeshPossessable->SetName(FString::Format(TEXT("{0} Depth Mesh"), { InPerformance->GetName() }));

					AActor* DepthMeshActor = CastChecked<AActor>(MovieSceneHelpers::GetObjectTemplate(MovieSceneSequence, DepthMeshBinding, TransientPlaybackState, 0));
					check(DepthMeshActor);

					USceneComponent* RootComponent = NewObject<USceneComponent>(DepthMeshActor, USceneComponent::GetDefaultSceneRootVariableName(), RF_Transactional);
					RootComponent->Mobility = EComponentMobility::Movable;	
					DepthMeshActor->SetRootComponent(RootComponent);
					DepthMeshActor->AddInstanceComponent(RootComponent);

					UMetaHumanDepthMeshComponent* DepthMeshComponent = NewObject<UMetaHumanDepthMeshComponent>(DepthMeshActor);
					DepthMeshActor->AddInstanceComponent(DepthMeshComponent);
					DepthMeshComponent->AttachToComponent(DepthMeshActor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);

					FString NewAssetName;
					FString NewPackageName;
					AssetTools.CreateUniqueAssetName(NewLevelSequencerAssetsPackagePath + TEXT("/T_Depth_") + NewLevelSequence->GetName(), TEXT(""), NewPackageName, NewAssetName);
					NewDepthSection->MediaTexture = Cast<UMediaTexture>(AssetTools.CreateAsset(NewAssetName, NewLevelSequencerAssetsPackagePath, UMediaTexture::StaticClass(), nullptr));
					NewDepthSection->MediaTexture->UpdateResource();

					DepthMeshComponent->SetDepthTexture(NewDepthSection->MediaTexture);
					DepthMeshComponent->SetDepthRange(10.0, 55.0);
					DepthMeshComponent->SetCameraCalibration(FootageCaptureData->CameraCalibrations[0]);
					DepthMeshComponent->UpdateBounds();

					Add3DTransformTrackToBinding(NewMovieScene, DepthMeshBinding, OffsetTransform);
				}
			}
			else
			{
				UE_LOG(LogMetaHumanPerformance, Warning, TEXT("Failed to add depth track in the exported Level Sequence '%s'"), *InExportSettings->AssetName);
			}
		}
		else
		{
			UE_LOG(LogMetaHumanPerformance, Warning, TEXT("Export depth track was set but no depth sequence was found in '%s' Capture Data. Skipping depth track export"), (FootageCaptureData != nullptr) ? *FootageCaptureData->GetName() : TEXT("No Footage Capture Data"));
		}
	}

	ExportLevelSequenceTask.EnterProgressFrame(1.0f, LOCTEXT("ExportAudioTrack", "Exporting Audio Track"));

	if (InExportSettings->bExportAudioTrack)
	{
		if (CanExportAudioTrack(InPerformance))
		{
			if (UMovieSceneAudioTrack* NewAudioTrack = NewMovieScene->AddTrack<UMovieSceneAudioTrack>())
			{
				NewAudioTrack->SetDisplayName(LOCTEXT("AudioSequenceTrack", "Audio"));

				TObjectPtr<class USoundWave> AudioForProcessing = InPerformance->GetAudioForProcessing();
				UMovieSceneAudioSection* NewAudioSection = CastChecked<UMovieSceneAudioSection>(NewAudioTrack->AddNewSound(AudioForProcessing, 0));
				NewAudioSection->TimecodeSource = InPerformance->GetAudioMediaTimecode();

				if (InExportSettings->ExportRange == EPerformanceExportRange::ProcessingRange)
				{
					NewAudioSection->SetRange(ProcessingRange);
					NewAudioSection->SetStartOffset(ProcessingRange.GetLowerBoundValue());

					if (!InExportSettings->bKeepFrameRange)
					{
						NewAudioSection->MoveSection(-ProcessingRange.GetLowerBoundValue());
					}
				}
				else
				{
					if (TRange<FFrameNumber>* FoundAudioFrameRange = MediaFrameRanges.Find(AudioForProcessing))
					{
						NewAudioSection->SetRange(*FoundAudioFrameRange);
					}
					else
					{
						UE_LOG(LogMetaHumanPerformance, Warning, TEXT("Failed to determine frame range for audio track"));
					}
				}
			}
			else
			{
				UE_LOG(LogMetaHumanPerformance, Warning, TEXT("Failed to add audio track in the exported Level Sequence '%s'"), *InExportSettings->AssetName);
			}
		}
		else
		{
			UE_LOG(LogMetaHumanPerformance, Warning, TEXT("Failed to add audio track in the exported Level Sequence '%s'"), *InExportSettings->AssetName);
		}
	}

	ExportLevelSequenceTask.EnterProgressFrame(1.0f, LOCTEXT("ExportCamera", "Exporting Camera"));

	if (InExportSettings->bExportCamera)
	{
		const FGuid CameraBinding = FSequencerUtilities::CreateOrReplaceBinding(nullptr, MovieSceneSequence, ACineCameraActor::StaticClass(), CreateBindingParams);
		check(CameraBinding.IsValid());

		FMovieScenePossessable* CameraPossessable = NewMovieScene->FindPossessable(CameraBinding);
		check(CameraPossessable);

		// Set the name of the Camera Track
		CameraPossessable->SetName(FString::Format(TEXT("{0} Camera"), { InPerformance->GetName() }));

		check(MovieSceneHelpers::SupportsObjectTemplate(MovieSceneSequence, CameraBinding, TransientPlaybackState));

		ACineCameraActor* CameraActor = CastChecked<ACineCameraActor>(MovieSceneHelpers::GetObjectTemplate(MovieSceneSequence, CameraBinding, TransientPlaybackState, 0));
		check(CameraActor);

		FTransform InverseCameraExtrinsics = FTransform::Identity;
		TArray<FCameraCalibration> Calibrations;

		if (FootageCaptureData && !FootageCaptureData->CameraCalibrations.IsEmpty())
		{
			TArray<TPair<FString, FString>> StereoReconstructionPairs;
			FootageCaptureData->CameraCalibrations[0]->ConvertToTrackerNodeCameraModels(Calibrations, StereoReconstructionPairs);

			int32 CameraViewIndex = FootageCaptureData->CameraCalibrations[0]->GetCalibrationIndexByName(InPerformance->Camera);
			InverseCameraExtrinsics = FTransform(Calibrations[CameraViewIndex].Transform.Inverse());
			FOpenCVHelperLocal::ConvertOpenCVToUnreal(InverseCameraExtrinsics);
		}

		UCineCameraComponent* CameraComponent = CameraActor->GetCineCameraComponent();
		check(CameraComponent);

		const FGuid CameraComponentGuid = NewMovieScene->AddPossessable(CameraComponent->GetName(), UCineCameraComponent::StaticClass());
		check(CameraComponentGuid.IsValid());

		FMovieScenePossessable* CameraComponentPossessable = NewMovieScene->FindPossessable(CameraComponentGuid);
		check(CameraComponentPossessable);

		CameraComponentPossessable->SetParent(CameraBinding, NewMovieScene);
		NewLevelSequence->BindPossessableObject(CameraComponentGuid, *CameraComponent, CameraActor);

		Add3DTransformTrackToBinding(NewMovieScene, CameraComponentGuid, InverseCameraExtrinsics * OffsetTransform);

		if (FootageCaptureData)
		{
			// Calculate the field of view to set in the camera component based on the camera sensor size so we can fit the footage in the viewport when viewed through the camera
			const FVector2D ViewportSize{ CameraComponent->Filmback.SensorWidth, CameraComponent->Filmback.SensorHeight };
			float FieldOfView = 0.0f;
			FBox2D ScreenRect;
			FTransform Transform;
			check(TempFootageComponent);
			TempFootageComponent->GetFootageScreenRect(ViewportSize, FieldOfView, ScreenRect, Transform);
			CameraComponent->SetFieldOfView(FieldOfView);
		}
		else
		{
			CameraComponent->SetCurrentFocalLength(10.0f);
		}

		// Set the focus distance to be the distance from the camera to the centre of the Identity or MetaHuman along
		// the optical (X) axis.
		CameraComponent->FocusSettings.ManualFocusDistance = (InverseCameraExtrinsics * OffsetTransform * ReferenceFramePose).GetLocation().X;

		// Set the maximum aperture possible so everything is in focus
		CameraComponent->SetCurrentAperture(CameraActor->GetCineCameraComponent()->LensSettings.MaxFStop);


		AddFloatTrackToBinding(NewMovieScene, CameraComponentGuid, "CurrentAperture", "CurrentAperture");
		AddFloatTrackToBinding(NewMovieScene, CameraComponentGuid, "CurrentFocalLength", "CurrentFocalLength");
		AddFloatTrackToBinding(NewMovieScene, CameraComponentGuid, "ManualFocusDistance", "FocusSettings.ManualFocusDistance");

		UMovieSceneTrack* CameraCutTrack = NewMovieScene->AddCameraCutTrack(UMovieSceneCameraCutTrack::StaticClass());
		UMovieSceneCameraCutSection* CameraCutSection = Cast<UMovieSceneCameraCutSection>(CameraCutTrack->CreateNewSection());
		CameraCutSection->SetCameraGuid(CameraBinding);
		CameraCutSection->SetRange(NewMovieScene->GetPlaybackRange());
		CameraCutTrack->AddSection(*CameraCutSection);

		if (InExportSettings->bApplyLensDistortion && CanExportLensDistortion(InPerformance))
		{
 			ULensComponent* LensComponent = NewObject<ULensComponent>(CameraActor, MakeUniqueObjectName(CameraActor, ULensComponent::StaticClass(), TEXT("Lens")));
 			CameraActor->AddInstanceComponent(LensComponent);
 
 			LensComponent->SetApplyDistortion(true);
 			LensComponent->SetDistortionSource(EDistortionSource::Manual);
 			LensComponent->SetLensModel(USphericalLensModel::StaticClass());
 
			int32 CameraViewIndex = FootageCaptureData->CameraCalibrations[0]->GetCalibrationIndexByName(InPerformance->Camera);
 			const FCameraCalibration& DistortionVals = Calibrations[CameraViewIndex];
			FLensDistortionState ManualDistortion;
 			ManualDistortion.DistortionInfo.Parameters.Add(DistortionVals.K1);
 			ManualDistortion.DistortionInfo.Parameters.Add(DistortionVals.K2);
 			ManualDistortion.DistortionInfo.Parameters.Add(DistortionVals.K3);
 			ManualDistortion.DistortionInfo.Parameters.Add(DistortionVals.P1);
 			ManualDistortion.DistortionInfo.Parameters.Add(DistortionVals.P2);
 
 			ManualDistortion.ImageCenter.PrincipalPoint = DistortionVals.PrincipalPoint / DistortionVals.ImageSize;
 			ManualDistortion.FocalLengthInfo.FxFy = DistortionVals.FocalLength / DistortionVals.ImageSize;
 
 			LensComponent->SetDistortionState(ManualDistortion);
		}
	}

	// If we are showing the export dialog, also notifies the user that the level sequence export is complete
	if (InExportSettings->bShowExportDialog)
	{
		const FText NotificationText = FText::Format(LOCTEXT("LevelSequenceExported", "'{0}' has been successfully exported"), FText::FromString(NewLevelSequence->GetName()));
		FNotificationInfo Info{ NotificationText };
		Info.ExpireDuration = 8.0f;
		Info.bUseLargeFont = false;
		Info.Hyperlink = FSimpleDelegate::CreateWeakLambda(NewLevelSequence, [NewLevelSequence]()
		{
			GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(NewLevelSequence);
		});
		Info.HyperlinkText = FText::Format(LOCTEXT("OpenNewLevelSequence", "Open {0}"), FText::FromString(NewLevelSequence->GetName()));
		if (TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info))
		{
			Notification->SetCompletionState(SNotificationItem::CS_Success);
		}
	}

	return NewLevelSequence;
}

bool UMetaHumanPerformanceExportUtils::GetBoneGlobalTransform(const USkeleton* InSkeleton, const FName& InBoneName, FTransform& OutTransform)
{
	check(InSkeleton);

	const FReferenceSkeleton& RefSkeleton = InSkeleton->GetReferenceSkeleton();
	const TArray<FTransform>& RefPoses = InSkeleton->GetRefLocalPoses();

	int32 CurrentBoneIndex = RefSkeleton.FindBoneIndex(InBoneName);

	if (CurrentBoneIndex != INDEX_NONE)
	{
		OutTransform = RefPoses[CurrentBoneIndex];

		// Go up in the hierarchy of bones accumulating the transforms to get the bone global reference global transform
		while (CurrentBoneIndex != INDEX_NONE)
		{
			CurrentBoneIndex = RefSkeleton.GetParentIndex(CurrentBoneIndex);

			if (CurrentBoneIndex != INDEX_NONE)
			{
				OutTransform *= RefPoses[CurrentBoneIndex];
			}
		}

		return true;
	}

	return false;
}

void UMetaHumanPerformanceExportUtils::RecordControlRigKeys(UMovieSceneControlRigParameterSection* InSection, FFrameNumber InFrameNumber, UControlRig* InControlRig, ERichCurveInterpMode InCurveInterpolation)
{
	MHA_CPUPROFILER_EVENT_SCOPE(UMetaHumanPerformanceExportUtils::RecordControlRigKeys)

	TArray<FRigControlElement*> Controls;
	InControlRig->GetControlsInOrder(Controls);

	URigHierarchy* RigHierarchy = InControlRig->GetHierarchy();

	TArrayView<FMovieSceneFloatChannel*> FloatChannels = InSection->GetChannelProxy().GetChannels<FMovieSceneFloatChannel>();

	auto AddVectorKeyToFloatChannels = [FloatChannels, InCurveInterpolation](int32& ChannelIndex, FFrameNumber FrameNumber, const FVector3f& Value)
	{
		FMovieSceneFloatValue FloatValue;
		FloatValue.InterpMode = InCurveInterpolation;

		FloatValue.Value = Value.X;
		FloatChannels[ChannelIndex++]->GetData().UpdateOrAddKey(FrameNumber, FloatValue);

		FloatValue.Value = Value.Y;
		FloatChannels[ChannelIndex++]->GetData().UpdateOrAddKey(FrameNumber, FloatValue);

		FloatValue.Value = Value.Z;
		FloatChannels[ChannelIndex++]->GetData().UpdateOrAddKey(FrameNumber, FloatValue);
	};

	for (FRigControlElement* ControlElement : Controls)
	{
		if (RigHierarchy->IsAnimatable(ControlElement))
		{
			if (FChannelMapInfo* ChannelIndexInfo = InSection->ControlChannelMap.Find(ControlElement->GetFName()))
			{
				int32 ChannelIndex = ChannelIndexInfo->ChannelIndex;

				switch (ControlElement->Settings.ControlType)
				{
					case ERigControlType::Float:
					{
						const float Val = RigHierarchy->GetControlValue(ControlElement, ERigControlValueType::Current).Get<float>();

						FMovieSceneFloatValue FloatValue;
						FloatValue.InterpMode = InCurveInterpolation;
						FloatValue.Value = Val;
						FloatChannels[ChannelIndex++]->GetData().UpdateOrAddKey(InFrameNumber, FloatValue);
						break;
					}

					case ERigControlType::Vector2D:
					{
						const FVector3f Val = RigHierarchy->GetControlValue(ControlElement, ERigControlValueType::Current).Get<FVector3f>();

						FMovieSceneFloatValue FloatValue;
						FloatValue.InterpMode = InCurveInterpolation;

						FloatValue.Value = Val.X;
						FloatChannels[ChannelIndex++]->GetData().UpdateOrAddKey(InFrameNumber, FloatValue);

						FloatValue.Value = Val.Y;
						FloatChannels[ChannelIndex++]->GetData().UpdateOrAddKey(InFrameNumber, FloatValue);
						break;
					}

					case ERigControlType::Position:
					case ERigControlType::Scale:
					case ERigControlType::Rotator:
					{
						const FVector3f Val = RigHierarchy->GetControlValue(ControlElement, ERigControlValueType::Current).Get<FVector3f>();
						AddVectorKeyToFloatChannels(ChannelIndex, InFrameNumber, Val);
						break;
					}

					case ERigControlType::Transform:
					{
						const FTransform Val = RigHierarchy->GetControlValue(ControlElement, ERigControlValueType::Current).Get<FRigControlValue::FTransform_Float>().ToTransform();
						AddVectorKeyToFloatChannels(ChannelIndex, InFrameNumber, FVector3f(Val.GetTranslation()));
						AddVectorKeyToFloatChannels(ChannelIndex, InFrameNumber, FVector3f(Val.GetRotation().Euler()));
						AddVectorKeyToFloatChannels(ChannelIndex, InFrameNumber, FVector3f(Val.GetScale3D()));
						break;
					}

					case ERigControlType::TransformNoScale:
					{
						const FTransform Val = RigHierarchy->GetControlValue(ControlElement, ERigControlValueType::Current).Get<FRigControlValue::FTransformNoScale_Float>().ToTransform();
						AddVectorKeyToFloatChannels(ChannelIndex, InFrameNumber, FVector3f(Val.GetTranslation()));
						AddVectorKeyToFloatChannels(ChannelIndex, InFrameNumber, FVector3f(Val.GetRotation().Euler()));
						break;
					}

					case ERigControlType::EulerTransform:
					{
						const FTransform Val = RigHierarchy->GetControlValue(ControlElement, ERigControlValueType::Current).Get<FRigControlValue::FEulerTransform_Float>().ToTransform().ToFTransform();
						AddVectorKeyToFloatChannels(ChannelIndex, InFrameNumber, FVector3f(Val.GetTranslation()));
						AddVectorKeyToFloatChannels(ChannelIndex, InFrameNumber, FVector3f(Val.GetRotation().Euler()));
						AddVectorKeyToFloatChannels(ChannelIndex, InFrameNumber, FVector3f(Val.GetScale3D()));
						break;
					}
				}
			}
		}
	}
}

void UMetaHumanPerformanceExportUtils::BakeControlRigAnimationData(UMetaHumanPerformance* InPerformance, UMovieSceneSequence* InSequence, int32 InFrameNumber, 
		UMovieSceneControlRigParameterSection* InControlRigSection, const FTransform & InReferenceFrameRootPose, ERichCurveInterpMode InCurveInterpolation, 
		UControlRig* InRecordControlRig, const FVector& InVisualizeMeshHeightOffset)
{
	MHA_CPUPROFILER_EVENT_SCOPE(UMetaHumanPerformanceExportUtils::BakeControlRigAnimationData);

	check(InPerformance);
	check(InSequence);
	check(InControlRigSection);

	const FFrameRate FrameRate = InPerformance->GetFrameRate();
	const FFrameRate TickRate = InSequence->GetMovieScene()->GetTickResolution();
	const FFrameTime TransformedFrameTime = FFrameRate::TransformTime(FFrameNumber{ InFrameNumber }, FrameRate, TickRate);
	const FFrameNumber TransformedFrameNumber = TransformedFrameTime.GetFrame();

	FFrameAnimationData AnimationFrame = InPerformance->AnimationData[InFrameNumber - InPerformance->GetProcessingLimitFrameRange().GetLowerBoundValue().Value];

	if (!ApplyNeutralPoseCalibration(InPerformance, InFrameNumber, AnimationFrame))
	{
		return;
	}

	// Select which ControlRig to use for the Backwards Solve when recording keys in sequencer
	// If InRecordControlRig is provided use that, otherwise use the one from InControlRigSection
	UControlRig* RecordControlRig = InRecordControlRig != nullptr ? InRecordControlRig : InControlRigSection->GetControlRig();
	check(RecordControlRig);

	if (!RecordControlRig->SupportsEvent(BackwardsSolveEventName))
	{
		UE_LOG(LogMetaHumanPerformance, Warning, TEXT("ControlRig '%s' doesn't support the Backwards Solve event. No keys will be recorded."), *RecordControlRig->GetName());
		return;
	}

	URigHierarchy* RigHierarchy = RecordControlRig->GetHierarchy();
	check(RigHierarchy);

	FTransform HeadPose;
	if (AnimationFrame.Pose.IsValid())
	{
		if (InPerformance->InputType == EDataInputType::Audio && !InPerformance->bRealtimeAudio)
		{
			FTransform HeadPoseAtHeadBone = InPerformance->AudioDrivenHeadPoseTransformInverse(AnimationFrame.Pose);
			HeadPose = HeadPoseAtHeadBone * FTransform(InVisualizeMeshHeightOffset);
		}
		else
		{
			USkeletalMesh* PreviewSkelMesh = InPerformance->GetVisualizationMesh();
			FTransform HeadBoneInitialTransform;

			if (!PreviewSkelMesh)
			{
				UE_LOG(LogMetaHumanPerformance, Error, TEXT("Could not find Skeleton. Head Movement export will be disabled."));
			}
			else if (GetBoneGlobalTransform(PreviewSkelMesh->GetSkeleton(), HeadBoneName, HeadBoneInitialTransform))
			{
				const FTransform HeadBoneInitialTransformInverse = HeadBoneInitialTransform.Inverse();

				// Apply the visualization height offset to the Pose of the current animation frame
				FTransform RootTransform = AnimationFrame.Pose * FTransform(InVisualizeMeshHeightOffset);

				// Make the head pose relative to the reference transform so we get a delta transform
				// that can be used for the head control curves in Control Rig
				RootTransform.SetToRelativeTransform(InReferenceFrameRootPose);

				// Finally make animation pose relative to the head bone
				HeadPose = HeadBoneInitialTransform * RootTransform * HeadBoneInitialTransformInverse;
			}
			else
			{
				UE_LOG(LogMetaHumanPerformance, Error, TEXT("Could not find head bone in Skeleton '%s'. Head Movement export will be disabled."), *PreviewSkelMesh->GetSkeleton()->GetName());
			}
		}
	}

	const FVector Location = HeadPose.GetLocation();
	const FRotator Rotation = HeadPose.Rotator();

	// Get a list of all curve keys and update them with the values from the animation data
	// Also set the head movement curve values if the current control rig supports it
	const TArray<FRigElementKey> CurveKeys = RigHierarchy->GetCurveKeys();
	for (const FRigElementKey& Curve : CurveKeys)
	{
		if (const float* Value = AnimationFrame.AnimationData.Find(Curve.Name.ToString()))
		{
			RigHierarchy->SetCurveValue(Curve, *Value);
		}
		else if (Curve.Name == HeadRollCurveName)
		{
			RigHierarchy->SetCurveValue(Curve, Rotation.Roll);
		}
		else if (Curve.Name == HeadPitchCurveName)
		{
			RigHierarchy->SetCurveValue(Curve, Rotation.Pitch);
		}
		else if (Curve.Name == HeadYawCurveName)
		{
			RigHierarchy->SetCurveValue(Curve, Rotation.Yaw);
		}
		else if (Curve.Name == HeadTranslationXCurveName)
		{
			RigHierarchy->SetCurveValue(Curve, Location.X);
		}
		else if (Curve.Name == HeadTranslationYCurveName)
		{
			RigHierarchy->SetCurveValue(Curve, Location.Y);
		}
		else if (Curve.Name == HeadTranslationZCurveName)
		{
			RigHierarchy->SetCurveValue(Curve, Location.Z);
		}
	}

	// Use ControlRig evaluation mechanism to compute the backwards solve and obtain the values
	// for the face board control curves

	// TODO: The name "Backwards Solve" is part of the ControlRig private class FRigUnit_InverseExecution
	// There is no way to use the class here so using the name as temporary hack for now
	RecordControlRig->Execute(BackwardsSolveEventName);

	// Finally, record the control rig control values in the control rig section
	RecordControlRigKeys(InControlRigSection, TransformedFrameNumber, RecordControlRig, InCurveInterpolation);
}

void UMetaHumanPerformanceExportUtils::BakeTransformAnimationData(UMetaHumanPerformance* InPerformance, UMovieSceneSequence* InSequence, int32 InFrameNumber, UMovieScene3DTransformSection* InTransformSection, ERichCurveInterpMode InCurveInterpolation, const FTransform& InOffsetTransform, const FVector& InVisualizeMeshHeightOffset)
{
	MHA_CPUPROFILER_EVENT_SCOPE(UMetaHumanPerformanceExportUtils::BakeTransformAnimationData);

	check(InPerformance);
	check(InSequence);
	check(InTransformSection);

	const FFrameRate FrameRate = InPerformance->GetFrameRate();
	const FFrameRate TickRate = InSequence->GetMovieScene()->GetTickResolution();
	const FFrameTime TransformedFrameTime = FFrameRate::TransformTime(FFrameNumber{ InFrameNumber }, FrameRate, TickRate);
	const FFrameNumber TransformedFrameNumber = TransformedFrameTime.GetFrame();

	const FFrameAnimationData& AnimationFrame = InPerformance->AnimationData[InFrameNumber - InPerformance->GetProcessingLimitFrameRange().GetLowerBoundValue().Value];

	if (AnimationFrame.Pose.IsValid())
	{
		const FTransform Pose = FTransform(InVisualizeMeshHeightOffset) * AnimationFrame.Pose * InOffsetTransform;

		const FVector Location = Pose.GetLocation();
		const FVector Rotation = Pose.Rotator().Euler();
		const FVector Scale = Pose.GetScale3D();

		TArrayView<FMovieSceneDoubleChannel*> DoubleChannels = InTransformSection->GetChannelProxy().GetChannels<FMovieSceneDoubleChannel>();
		auto AddVectorToDoubleChannels = [DoubleChannels, InCurveInterpolation](int32& ChannelIndex, FFrameNumber FrameNumber, const FVector& Value)
		{
			FMovieSceneDoubleValue DoubleValue;
			DoubleValue.InterpMode = InCurveInterpolation;

			DoubleValue.Value = Value.X;
			DoubleChannels[ChannelIndex++]->GetData().UpdateOrAddKey(FrameNumber, DoubleValue);

			DoubleValue.Value = Value.Y;
			DoubleChannels[ChannelIndex++]->GetData().UpdateOrAddKey(FrameNumber, DoubleValue);

			DoubleValue.Value = Value.Z;
			DoubleChannels[ChannelIndex++]->GetData().UpdateOrAddKey(FrameNumber, DoubleValue);
		};

		int32 ChannelIndex = 0;
		AddVectorToDoubleChannels(ChannelIndex, TransformedFrameNumber, Location);
		AddVectorToDoubleChannels(ChannelIndex, TransformedFrameNumber, Rotation);
		AddVectorToDoubleChannels(ChannelIndex, TransformedFrameNumber, Scale);
	}
}

void UMetaHumanPerformanceExportUtils::SetHeadControlSwitchEnabled(class UMovieSceneControlRigParameterTrack* InControlRigTrack, bool bInEnableHeadControl)
{
	if (InControlRigTrack != nullptr)
	{
		check(!InControlRigTrack->GetAllSections().IsEmpty());
		if (UMovieSceneControlRigParameterSection* ControlRigSection = Cast<UMovieSceneControlRigParameterSection>(InControlRigTrack->GetAllSections()[0]))
		{
			ControlRigSection->Modify();

			// Enable or disable control rig head movement
			TMovieSceneChannelHandle<FMovieSceneIntegerChannel> HeadControlSwitchChannelHandle = ControlRigSection->GetChannelProxy().GetChannelByName<FMovieSceneIntegerChannel>(UMetaHumanPerformanceControlRigComponent::HeadIKSwitchControlName);
			if (FMovieSceneIntegerChannel* HeadControlSwitchChannel = HeadControlSwitchChannelHandle.Get())
			{
				// Remove any existing keys in this channel and set the default value of the track
				HeadControlSwitchChannel->Reset();
				HeadControlSwitchChannel->SetDefault(bInEnableHeadControl ? 1 : 0);
			}
			else
			{
				UE_LOG(LogMetaHumanPerformance, Error, TEXT("Could not find switch for head control '%s' for Control Rig '%s'"), *UMetaHumanPerformanceControlRigComponent::HeadIKSwitchControlName.ToString(), *InControlRigTrack->GetControlRig()->GetName());
			}
		}
	}
}

bool UMetaHumanPerformanceExportUtils::RecordAnimationSequence(const TArray<UObject*>& InNewAssets, UMetaHumanPerformance* InPerformance, UMetaHumanPerformanceExportAnimationSettings* InExportSettings)
{
	MHA_CPUPROFILER_EVENT_SCOPE(UMetaHumanPerformanceExportUtils::RecordAnimationSequence);

	if (InNewAssets.IsEmpty())
	{
		return false;
	}

	if (UAnimSequence* AnimSequence = Cast<UAnimSequence>(InNewAssets[0]))
	{
		IAnimationDataController& AnimationController = AnimSequence->GetController();

		const TRange<FFrameNumber> ExportFrameRange = InPerformance->GetExportFrameRange(InExportSettings->ExportRange);

		const FFrameRate FrameRate = InPerformance->GetFrameRate();
		// The AnimationLength will be one frame less than the export range to prevent invalid data after the last frame
		const FFrameNumber AnimationLength = FMath::Max(0, ExportFrameRange.GetUpperBoundValue().Value - ExportFrameRange.GetLowerBoundValue().Value - 1);

		constexpr bool bShouldTransact = false;
		// Any modifications to the animation sequence MUST be inside this bracket to minimize the likelihood of a race condition
		// between this thread (game thread) and the anim sequence background tasks which update the animation data cache
		// (the animation cache is not updated while within brackets)
		AnimationController.OpenBracket(LOCTEXT("PerformerAnimation_Bracket", "Exporting MetaHuman Performance Animation"), bShouldTransact);

		// Always reset animation in case we are overriding an existing one
		AnimationController.RemoveAllBoneTracks(bShouldTransact);
		AnimationController.RemoveAllCurvesOfType(ERawCurveTrackTypes::RCT_Float, bShouldTransact);
		AnimationController.RemoveAllCurvesOfType(ERawCurveTrackTypes::RCT_Transform, bShouldTransact);
		AnimationController.RemoveAllAttributes(bShouldTransact);

		// Set the frame rate and number of frames as the first thing to avoid issues of resizing
		AnimationController.SetFrameRate(FrameRate, bShouldTransact);
		AnimationController.SetNumberOfFrames(AnimationLength, bShouldTransact);

		// Add timecode
		AnimationController.AddBoneCurve(RootBoneName, bShouldTransact);
		AnimationController.SetBoneTrackKeys(RootBoneName, { FVector3f::ZeroVector }, { FQuat4f::Identity }, { FVector3f::OneVector }, bShouldTransact);

		UScriptStruct* IntScriptStruct = FIntegerAnimationAttribute::StaticStruct();
		UScriptStruct* FloatScriptStruct = FFloatAnimationAttribute::StaticStruct();

		TArray<FAnimationAttributeIdentifier> TimecodeAttributeIdentifiers;
		TimecodeAttributeIdentifiers.Reserve(5);

		for (const FName AttributeName : { TEXT("TCHour") , TEXT("TCMinute"), TEXT("TCSecond"), TEXT("TCFrame") })
		{
			FAnimationAttributeIdentifier AttributeIdentifier = UAnimationAttributeIdentifierExtensions::CreateAttributeIdentifier(AnimSequence, AttributeName, RootBoneName, IntScriptStruct);
			AnimationController.AddAttribute(AttributeIdentifier, bShouldTransact);

			TimecodeAttributeIdentifiers.Add(AttributeIdentifier);
		}

		for (const FName AttributeName : { TEXT("TCRate") })
		{
			FAnimationAttributeIdentifier AttributeIdentifier = UAnimationAttributeIdentifierExtensions::CreateAttributeIdentifier(AnimSequence, AttributeName, RootBoneName, FloatScriptStruct);
			AnimationController.AddAttribute(AttributeIdentifier, bShouldTransact);

			TimecodeAttributeIdentifiers.Add(AttributeIdentifier);
		}

		FFrameRate TimecodeRate;
		FFrameNumber TimecodeFrame;

		if (InPerformance->InputType == EDataInputType::Audio)
		{
			if (TObjectPtr<USoundWave> Audio = InPerformance->GetAudioForProcessing())
			{
				TimecodeRate = InPerformance->GetAudioMediaTimecodeRate();
				const FTimecode Timecode = InPerformance->GetAudioMediaTimecode();
				TimecodeFrame = Timecode.ToFrameNumber(TimecodeRate);

				TMap<TWeakObjectPtr<UObject>, TRange<FFrameNumber>> MediaFrameRanges = InPerformance->GetMediaFrameRanges();
				if (TRange<FFrameNumber>* AudioFrameRange = MediaFrameRanges.Find(Audio))
				{
					FFrameNumber TimecodeOrigin = ExportFrameRange.GetLowerBoundValue();
					TimecodeFrame += TimecodeOrigin - AudioFrameRange->GetLowerBoundValue().Value;
				}
				else
				{
					UE_LOG(LogMetaHumanPerformance, Warning, TEXT("Failed to determine frame range for soundwave asset. Timecode information will be incorrect."));
				}
			}
		}
		else if (InPerformance->FootageCaptureData && !InPerformance->FootageCaptureData->ImageSequences.IsEmpty())
		{
			const FTimecode Timecode = InPerformance->FootageCaptureData->GetEffectiveImageTimecode(0);
			TimecodeRate = InPerformance->FootageCaptureData->GetEffectiveImageTimecodeRate(0);
			TimecodeFrame = Timecode.ToFrameNumber(TimecodeRate);

			TMap<TWeakObjectPtr<UObject>, TRange<FFrameNumber>> MediaFrameRanges = InPerformance->GetMediaFrameRanges();
			if (TRange<FFrameNumber>* ImageSequenceFrameRange = MediaFrameRanges.Find(InPerformance->FootageCaptureData->ImageSequences[0]))
			{
				FFrameNumber TimecodeOrigin = ExportFrameRange.GetLowerBoundValue();
				TimecodeFrame += TimecodeOrigin - ImageSequenceFrameRange->GetLowerBoundValue().Value;
			}
			else
			{
				UE_LOG(LogMetaHumanPerformance, Warning, TEXT("Failed to determine frame range for the image sequence. Timecode information will be incorrect."));
			}
		}

		const float TimecodeRateDecimal = TimecodeRate.AsDecimal();

		// Find the first frame with a valid pose, this will be the reference point for head movement
		FTransform HeadBoneInitialTransform;
		if (!GetBoneGlobalTransform(AnimSequence->GetSkeleton(), HeadBoneName, HeadBoneInitialTransform))
		{
			UE_LOG(LogMetaHumanPerformance, Error, TEXT("Could not find head bone in Skeleton '%s'. Head Movement export will be disabled."), *AnimSequence->GetSkeleton()->GetName());
			InExportSettings->bEnableHeadMovement = false;
		}

		const FTransform HeadBoneInitialTransformInverse = HeadBoneInitialTransform.Inverse();

		FTransform ReferenceFrameRootPose;
		if (InPerformance->HeadMovementReferenceFrameCalculated == -1)
		{
			ReferenceFrameRootPose = InPerformance->GetFirstValidAnimationPose();
		}
		else
		{
			ReferenceFrameRootPose = InPerformance->AnimationData[InPerformance->HeadMovementReferenceFrameCalculated].Pose;
		}

		const float NumFrames = ExportFrameRange.GetUpperBoundValue().Value - ExportFrameRange.GetLowerBoundValue().Value;
		FScopedSlowTask RecordAnimationTask{ NumFrames, LOCTEXT("RecordingAnimSequence", "Recording Animation Sequence...") };
		RecordAnimationTask.MakeDialog();

		// Store all the animation curves to be written in the animation sequence in bulk
		TMap<FAnimationCurveIdentifier, TArray<FRichCurveKey>> AnimationCurveKeys;

		// Add animation curves
		TSet<FString> AddedCurves;
		for (int32 FrameIndex = ExportFrameRange.GetLowerBoundValue().Value; FrameIndex < ExportFrameRange.GetUpperBoundValue().Value; ++FrameIndex, ++TimecodeFrame)
		{
			const ERichCurveInterpMode CurveInterpolation = InPerformance->GetExcludedFrame(FrameIndex + 1) == EFrameRangeType::None ? InExportSettings->CurveInterpolation.GetValue() : ERichCurveInterpMode::RCIM_Linear;

			RecordAnimationTask.EnterProgressFrame();

			const float FrameTime = (FrameIndex - ExportFrameRange.GetLowerBoundValue().Value) / FrameRate.AsDecimal();

			FTimecode Timecode = FTimecode::FromFrameNumber(TimecodeFrame, TimecodeRate);

			AnimationController.SetAttributeKey(TimecodeAttributeIdentifiers[0], FrameTime, &Timecode.Hours, IntScriptStruct, bShouldTransact);
			AnimationController.SetAttributeKey(TimecodeAttributeIdentifiers[1], FrameTime, &Timecode.Minutes, IntScriptStruct, bShouldTransact);
			AnimationController.SetAttributeKey(TimecodeAttributeIdentifiers[2], FrameTime, &Timecode.Seconds, IntScriptStruct, bShouldTransact);
			AnimationController.SetAttributeKey(TimecodeAttributeIdentifiers[3], FrameTime, &Timecode.Frames, IntScriptStruct, bShouldTransact);
			AnimationController.SetAttributeKey(TimecodeAttributeIdentifiers[4], FrameTime, &TimecodeRateDecimal, FloatScriptStruct, bShouldTransact);

			FFrameAnimationData FrameAnimData = InPerformance->AnimationData[FrameIndex - InPerformance->GetProcessingLimitFrameRange().GetLowerBoundValue().Value];
			if (!FrameAnimData.ContainsData())
			{
				continue;
			}

			if (!ApplyNeutralPoseCalibration(InPerformance, FrameIndex, FrameAnimData))
			{
				return false;
			}

			for (const TPair<FString, float>& Sample : FrameAnimData.AnimationData)
			{
				FName SampleCurveName{ Sample.Key };

				const FAnimationCurveIdentifier CurveId{ SampleCurveName, ERawCurveTrackTypes::RCT_Float };

				if (!AddedCurves.Contains(Sample.Key))
				{
					if (!AnimationController.AddCurve(CurveId, AACF_Editable, bShouldTransact))
					{
						UE_LOG(LogMetaHumanPerformance, Warning, TEXT("Failed to add animation curve '%s' into '%s'"), *Sample.Key, *AnimSequence->GetName());
						continue;
					}

					AddedCurves.Add(Sample.Key);
				}

				AnimationCurveKeys.FindOrAdd(CurveId).Add(FRichCurveKey{ FrameTime, Sample.Value, 0, 0, CurveInterpolation });
			}

			// Always export the head movement curves but disable the switch that triggers the movement if the user request to do so

			FTransform HeadPose;

			if (InPerformance->InputType == EDataInputType::Audio && !InPerformance->bRealtimeAudio)
			{
				HeadPose = InPerformance->AudioDrivenHeadPoseTransformInverse(FrameAnimData.Pose);
			}
			else
			{
				// Apply the pose offset to the Pose of the current animation frame
				FTransform RootTransform = FrameAnimData.Pose;

				// Make the head pose relative to the neutral frame head pose so we get a delta transform
				// that can be used for the head control curves in Control Rig
				RootTransform.SetToRelativeTransform(ReferenceFrameRootPose);

				// Finally make animation pose relative to the head bone
				HeadPose = HeadBoneInitialTransform * RootTransform * HeadBoneInitialTransformInverse;
			}

			// Retrieve the names of all the curves in the skeleton
			TArray<FName> SkeletonCurveNames;
			AnimSequence->GetSkeleton()->GetCurveMetaDataNames(SkeletonCurveNames);
			const bool bHasHeadYawCurve = SkeletonCurveNames.Contains(HeadYawCurveName);

			// Store which curves exist in the skeleton in temp booleans
			const bool bHasHeadPitchCurve = SkeletonCurveNames.Contains(HeadPitchCurveName);
			const bool bHasHeadRollCurve = SkeletonCurveNames.Contains(HeadRollCurveName);
			const bool bHasHeadLocationXCurve = SkeletonCurveNames.Contains(HeadTranslationXCurveName);
			const bool bHasHeadLocationYCurve = SkeletonCurveNames.Contains(HeadTranslationYCurveName);
			const bool bHasHeadLocationZCurve = SkeletonCurveNames.Contains(HeadTranslationZCurveName);
			const bool bHasHeadIKControlSwitchCurve = SkeletonCurveNames.Contains(HeadIKCurveSwitchName);

			const FAnimationCurveIdentifier HeadYawCurveId{ HeadYawCurveName, ERawCurveTrackTypes::RCT_Float };
			const FAnimationCurveIdentifier HeadPitchCurveId{ HeadPitchCurveName, ERawCurveTrackTypes::RCT_Float };
			const FAnimationCurveIdentifier HeadRollCurveId{ HeadRollCurveName, ERawCurveTrackTypes::RCT_Float };
			const FAnimationCurveIdentifier HeadLocationXCurveId{ HeadTranslationXCurveName, ERawCurveTrackTypes::RCT_Float };
			const FAnimationCurveIdentifier HeadLocationYCurveId{ HeadTranslationYCurveName, ERawCurveTrackTypes::RCT_Float };
			const FAnimationCurveIdentifier HeadLocationZCurveId{ HeadTranslationZCurveName, ERawCurveTrackTypes::RCT_Float };
			const FAnimationCurveIdentifier HeadIKControlCurveId{ HeadIKCurveSwitchName, ERawCurveTrackTypes::RCT_Float };

			if (!AddedCurves.Contains(HeadYawCurveName.ToString()) && bHasHeadYawCurve)
			{
				AddedCurves.Add(HeadYawCurveName.ToString());
				AnimationController.AddCurve(HeadYawCurveId, AACF_Editable, bShouldTransact);
			}
			if (!AddedCurves.Contains(HeadPitchCurveName.ToString()) && bHasHeadPitchCurve)
			{
				AddedCurves.Add(HeadPitchCurveName.ToString());
				AnimationController.AddCurve(HeadPitchCurveId, AACF_Editable, bShouldTransact);
			}
			if (!AddedCurves.Contains(HeadRollCurveName.ToString()) && bHasHeadRollCurve)
			{
				AddedCurves.Add(HeadRollCurveName.ToString());
				AnimationController.AddCurve(HeadRollCurveId, AACF_Editable, bShouldTransact);
			}
			if (!AddedCurves.Contains(HeadTranslationXCurveName.ToString()) && bHasHeadLocationXCurve)
			{
				AddedCurves.Add(HeadTranslationXCurveName.ToString());
				AnimationController.AddCurve(HeadLocationXCurveId, AACF_Editable, bShouldTransact);
			}
			if (!AddedCurves.Contains(HeadTranslationYCurveName.ToString()) && bHasHeadLocationYCurve)
			{
				AddedCurves.Add(HeadTranslationYCurveName.ToString());
				AnimationController.AddCurve(HeadLocationYCurveId, AACF_Editable, bShouldTransact);
			}
			if (!AddedCurves.Contains(HeadTranslationZCurveName.ToString()) && bHasHeadLocationZCurve)
			{
				AddedCurves.Add(HeadTranslationZCurveName.ToString());
				AnimationController.AddCurve(HeadLocationZCurveId, AACF_Editable, bShouldTransact);
			}
			if (!AddedCurves.Contains(HeadIKCurveSwitchName.ToString()) && bHasHeadIKControlSwitchCurve)
			{
				AddedCurves.Add(HeadIKCurveSwitchName.ToString());
				AnimationController.AddCurve(HeadIKControlCurveId, AACF_Editable, bShouldTransact);
			}

			if (bHasHeadYawCurve && bHasHeadPitchCurve && bHasHeadRollCurve)
			{
				AnimationCurveKeys.FindOrAdd(HeadYawCurveId).Add(FRichCurveKey(FrameTime, HeadPose.Rotator().Yaw, 0, 0, CurveInterpolation));
				AnimationCurveKeys.FindOrAdd(HeadPitchCurveId).Add(FRichCurveKey(FrameTime, HeadPose.Rotator().Pitch, 0, 0, CurveInterpolation));
				AnimationCurveKeys.FindOrAdd(HeadRollCurveId).Add(FRichCurveKey(FrameTime, HeadPose.Rotator().Roll, 0, 0, CurveInterpolation));
			}

			if (bHasHeadLocationXCurve && bHasHeadLocationYCurve && bHasHeadLocationZCurve)
			{
				AnimationCurveKeys.FindOrAdd(HeadLocationXCurveId).Add(FRichCurveKey(FrameTime, HeadPose.GetLocation().X, 0, 0, CurveInterpolation));
				AnimationCurveKeys.FindOrAdd(HeadLocationYCurveId).Add(FRichCurveKey(FrameTime, HeadPose.GetLocation().Y, 0, 0, CurveInterpolation));
				AnimationCurveKeys.FindOrAdd(HeadLocationZCurveId).Add(FRichCurveKey(FrameTime, HeadPose.GetLocation().Z, 0, 0, CurveInterpolation));
			}

			if (bHasHeadIKControlSwitchCurve)
			{
				bool bExportHeadMovement = InExportSettings->bEnableHeadMovement && CanExportHeadMovement(InPerformance);
				AnimationCurveKeys.FindOrAdd(HeadIKControlCurveId).Add(FRichCurveKey(FrameTime, bExportHeadMovement ? 1.0f : 0.0f, 0, 0, CurveInterpolation));
			}
		}

		for (TPair<FAnimationCurveIdentifier, TArray<FRichCurveKey>>& CurveKeysPair : AnimationCurveKeys)
		{
			AnimationController.SetCurveKeys(CurveKeysPair.Key, CurveKeysPair.Value, bShouldTransact);
		}

		// Flush the bone tracks that were unnecessarily added to avoid animation mismatch for meshes with different ref poses
		AnimationController.RemoveAllBoneTracks(bShouldTransact);
		// Add the root bone track back to avoid timecode attributes to be ignored
		AnimationController.AddBoneCurve(RootBoneName, bShouldTransact);

		// Add metadata tags to enable animation to be played on Fortnite characters
		if (InExportSettings->bFortniteCompatibility)
		{
			for (const FName MetaDataName : { TEXT("MHFDSVersion"), TEXT("DisableFaceOverride") })
			{
				const FAnimationCurveIdentifier MetaDataCurveId(MetaDataName, ERawCurveTrackTypes::RCT_Float);
				AnimationController.AddCurve(MetaDataCurveId, AACF_Metadata, bShouldTransact);
				AnimationController.SetCurveKeys(MetaDataCurveId, { FRichCurveKey(0.f, 1.f) }, bShouldTransact);
			}
		}

		// Updates the AnimationSequence asset with new information from the controller
		AnimationController.NotifyPopulated();
		AnimationController.CloseBracket(bShouldTransact);

		AnimSequence->MarkPackageDirty();

		// Auto save the package to disk
		if (InExportSettings->bAutoSaveAnimSequence)
		{
			UPackage* Package = AnimSequence->GetOutermost();
			const FString PackageName = Package->GetName();
			const FString PackageFileName = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());

			FSavePackageArgs SaveArgs;
			SaveArgs.TopLevelFlags = RF_Standalone;
			SaveArgs.SaveFlags = SAVE_NoError;
			UPackage::SavePackage(Package, nullptr, *PackageFileName, SaveArgs);
		}

		// Notify the user
		if (InExportSettings->bShowExportDialog && GEditor)
		{
			const FText NotificationText = FText::Format(LOCTEXT("AnimationExported", "'{0}' has been successfully exported [{1} keys : {2} sec(s) @ {3} Hz]"),
														 FText::FromString(AnimSequence->GetName()),
														 FText::AsNumber(AnimSequence->GetDataModel()->GetNumberOfKeys()),
														 FText::AsNumber(AnimSequence->GetPlayLength()),
														 FText::AsNumber(1.0 / FrameRate.AsInterval()));


			FNotificationInfo Info{ NotificationText };
			Info.ExpireDuration = 8.0f;
			Info.bUseLargeFont = false;
			Info.Hyperlink = FSimpleDelegate::CreateWeakLambda(AnimSequence, [AnimSequence]
			{
				GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(AnimSequence);
			});
			Info.HyperlinkText = FText::Format(LOCTEXT("OpenNewAnimationHyperlink", "Open {0}"), FText::FromString(AnimSequence->GetName()));
			TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
			if (Notification.IsValid())
			{
				Notification->SetCompletionState(SNotificationItem::CS_Success);
			}

		}

		return true;
	}

	return false;
}

bool UMetaHumanPerformanceExportUtils::CanExportHeadMovement(const class UMetaHumanPerformance* InPerformance)
{
	return InPerformance->HasValidAnimationPose();
}

bool UMetaHumanPerformanceExportUtils::CanExportVideoTrack(const class UMetaHumanPerformance* InPerformance)
{
	if (InPerformance->InputType == EDataInputType::DepthFootage || InPerformance->InputType == EDataInputType::MonoFootage)
	{
		if (TObjectPtr<class UImgMediaSource> ImageSequence = GetCaptureFootage(InPerformance, false))
		{
			return ImageSequence != nullptr;
		}
	}

	return false;
}

bool UMetaHumanPerformanceExportUtils::CanExportDepthTrack(const class UMetaHumanPerformance* InPerformance)
{
	if (InPerformance->InputType == EDataInputType::DepthFootage)
	{
		if (TObjectPtr<class UImgMediaSource> DepthSequence = GetCaptureFootage(InPerformance, true))
		{
			return DepthSequence != nullptr;
		}
	}

	return false;
}

bool UMetaHumanPerformanceExportUtils::CanExportAudioTrack(const class UMetaHumanPerformance* InPerformance)
{
	return InPerformance->GetAudioForProcessing() != nullptr;
}

bool UMetaHumanPerformanceExportUtils::CanExportIdentity(const class UMetaHumanPerformance* InPerformance)
{
	if (InPerformance->InputType == EDataInputType::DepthFootage)
	{
		if (InPerformance->Identity != nullptr)
		{
			UMetaHumanIdentityFace* Face = InPerformance->Identity->FindPartOfClass<UMetaHumanIdentityFace>();
			return Face->IsConformalRigValid();
		}
	}
	
	return false;
}

bool UMetaHumanPerformanceExportUtils::CanExportLensDistortion(const class UMetaHumanPerformance* InPerformance)
{
	if (InPerformance->InputType == EDataInputType::DepthFootage || InPerformance->InputType == EDataInputType::MonoFootage)
	{
		return InPerformance->FootageCaptureData && !InPerformance->FootageCaptureData->CameraCalibrations.IsEmpty();
	}

	return false;
}

bool UMetaHumanPerformanceExportUtils::ApplyNeutralPoseCalibration(const UMetaHumanPerformance* InPerformance, const int32 InFrameNumber, FFrameAnimationData& InOutAnimationFrame)
{
	// Apply neutral pose calibration, but not when processing is running since we may not have the neutral frame values yet.
	// The FMetaHumanRealtimeCalibration is recreated each frame which is not ideal but in practice is not a heavyweight thing
	// and recreating it every frame will be less error prone than creating it once at a higher level everywhere that
	// this function is called from.

	if (InPerformance->bNeutralPoseCalibrationEnabled && !InPerformance->IsProcessing())
	{
		const FFrameAnimationData& NeutralAnimationFrame = InPerformance->AnimationData[InPerformance->NeutralPoseCalibrationFrame - InPerformance->GetProcessingLimitFrameRange().GetLowerBoundValue().Value];
		if (!NeutralAnimationFrame.Pose.IsValid())
		{
			UE_LOG(LogMetaHumanPerformance, Warning, TEXT("Neutral pose calibration frame %i is not a solved frame"), InPerformance->NeutralPoseCalibrationFrame);
			return false;
		}

		TArray<float> NeutralVaues;
		for (const FName& Curve : InPerformance->NeutralPoseCalibrationCurves)
		{
			if (const float* NeutralValue = NeutralAnimationFrame.AnimationData.Find(Curve.ToString()))
			{
				NeutralVaues.Add(*NeutralValue);
			}
			else
			{
				UE_LOG(LogMetaHumanPerformance, Warning, TEXT("Neutral pose calibration curve '%s' not found in neutral pose calibration frame %i"), *Curve.ToString(), InPerformance->NeutralPoseCalibrationFrame);
				return false;
			}
		}

		FMetaHumanRealtimeCalibration NeutralPoseCalibration(InPerformance->NeutralPoseCalibrationCurves, NeutralVaues, InPerformance->NeutralPoseCalibrationAlpha);

		TArray<float> AnimationValues;
		for (const FName& Curve : InPerformance->NeutralPoseCalibrationCurves)
		{
			if (const float* UnCalibratedValue = InOutAnimationFrame.AnimationData.Find(Curve.ToString()))
			{
				AnimationValues.Add(*UnCalibratedValue);
			}
			else
			{
				UE_LOG(LogMetaHumanPerformance, Warning, TEXT("Neutral pose calibration curve '%s' not found in frame %i"), *Curve.ToString(), InFrameNumber);
				return false;
			}
		}

		if (!NeutralPoseCalibration.ProcessFrame(InPerformance->NeutralPoseCalibrationCurves, AnimationValues))
		{
			UE_LOG(LogMetaHumanPerformance, Warning, TEXT("Neutral pose calibration failed for frame %i"), InFrameNumber);
			return false;
		}

		for (int32 Index = 0; Index < InPerformance->NeutralPoseCalibrationCurves.Num(); ++Index)
		{
			InOutAnimationFrame.AnimationData[InPerformance->NeutralPoseCalibrationCurves[Index].ToString()] = AnimationValues[Index];
		}
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
