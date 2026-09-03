// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanIdentityStyle.h"
#include "Styling/SlateStyleMacros.h"
#include "Styling/SlateStyleRegistry.h"
#include "Interfaces/IPluginManager.h"

/** List of all curves that we want to load thumbnail images for. */
static const TArray<FString> OutlinerTooltipCurveImages =
{
	TEXT("crv_lip_upper_outer_l"),
	TEXT("crv_lip_upper_outer_r"),
	TEXT("crv_lip_lower_outer_l"),
	TEXT("crv_lip_lower_outer_r"),
	TEXT("crv_lip_lower_inner_l"),
	TEXT("crv_lip_lower_inner_r"),
	TEXT("crv_lip_upper_inner_l"),
	TEXT("crv_lip_upper_inner_r"),
	TEXT("crv_lip_philtrum_l"),
	TEXT("crv_lip_philtrum_r"),
	TEXT("crv_nasolabial_l"),
	TEXT("crv_nasolabial_r"),
	TEXT("crv_nostril_l"),
	TEXT("crv_nostril_r"),
	TEXT("crv_ear_outer_helix_l"),
	TEXT("ear_outer_helix_r"),
	TEXT("crv_ear_inner_helix_l"),
	TEXT("crv_ear_inner_helix_r"),
	TEXT("crv_ear_central_lower_l"),
	TEXT("crv_ear_central_lower_r"),
	TEXT("crv_ear_central_upper_l"),
	TEXT("crv_ear_central_upper_r"),
	TEXT("crv_brow_upper_l"),
	TEXT("brow_middle_line_l"),
	TEXT("crv_brow_lower_l"),
	TEXT("crv_brow_intermediate_l"),
	TEXT("crv_brow_upper_r"),
	TEXT("brow_middle_line_r"),
	TEXT("crv_brow_lower_r"),
	TEXT("crv_brow_intermediate_r"),
	TEXT("crv_mentolabial_fold"),
	TEXT("crv_eyecrease_l"),
	TEXT("crv_eyecrease_r"),
	TEXT("crv_eyelid_lower_l"),
	TEXT("crv_eyelid_lower_r"),
	TEXT("crv_eyelid_upper_l"),
	TEXT("eye_plica_semilunaris_l"),
	TEXT("eye_plica_semilunaris_r"),
	TEXT("crv_eyelid_upper_r"),
	TEXT("crv_outer_eyelid_edge_left_lower"),
	TEXT("crv_outer_eyelid_edge_right_lower"),
	TEXT("crv_outer_eyelid_edge_left_upper"),
	TEXT("crv_outer_eyelid_edge_right_upper"),
	TEXT("crv_iris_l"),
	TEXT("crv_iris_r"),
	TEXT("pt_left_contact"),
	TEXT("pt_right_contact"),
	TEXT("pt_tooth_upper"),
	TEXT("pt_tooth_lower"),
	TEXT("pt_tooth_upper_2"),
	TEXT("pt_tooth_lower_2")
};

FMetaHumanIdentityStyle::FMetaHumanIdentityStyle()
	: FSlateStyleSet{ TEXT("MetaHumanIdentityStyle") }
{
	const FVector2D Icon16x16(16.0f, 16.0f);
	const FVector2D Icon20x20(20.0f, 20.0f);
	const FVector2D Icon32x16(32.0f, 16.0f);
	const FVector2D Thumb64x64(64.0f, 64.0f);
	const FVector2D Thumb128x128(128.0f, 128.0f);
	const FVector2D Thumb256x256(256.0f, 256.0f);

	SetContentRoot(IPluginManager::Get().FindPlugin(UE_PLUGIN_NAME)->GetContentDir());

	// Register all the icons used by the MetaHuman Identity asset and editors
	Set("Identity.Tab.Parts", new IMAGE_BRUSH_SVG("Icons/IdentityPartsTab_16", Icon16x16));
	Set("Identity.Root", new IMAGE_BRUSH_SVG("Icons/IdentityRoot_16", Icon16x16));
	Set("Identity.Face.Part", new IMAGE_BRUSH_SVG("Icons/IdentityFacePart_16", Icon16x16));
	Set("Identity.Face.SkeletalMesh", new IMAGE_BRUSH_SVG("Icons/IdentityFaceSkeletalMesh_16", Icon16x16));
	Set("Identity.Face.ConformalMesh", new IMAGE_BRUSH_SVG("Icons/IdentityFaceConformalMesh_16", Icon16x16));
	Set("Identity.Face.Rig", new IMAGE_BRUSH_SVG("Icons/IdentityFaceSkeletalMesh_16", Icon16x16));
	Set("Identity.Face.Poses", new IMAGE_BRUSH_SVG("Icons/IdentityFacePoses_16", Icon16x16));
	Set("Identity.Body.Part", new IMAGE_BRUSH_SVG("Icons/IdentityBodyPart_16", Icon16x16));
	Set("Identity.Pose.Neutral", new IMAGE_BRUSH_SVG("Icons/IdentityFacePoseNeutral_16", Icon16x16));
	Set("Identity.Pose.Teeth", new IMAGE_BRUSH_SVG("Icons/IdentityFacePoseTeeth_16", Icon16x16));
	Set("Identity.Pose.Custom", new IMAGE_BRUSH_SVG("Icons/IdentityFacePoseCustom_16", Icon16x16));
	Set("Identity.Tools.CreateComponents", new IMAGE_BRUSH_SVG("Icons/IdentityComponentsFromAsset_16", Icon16x16));
	Set("Identity.Tools.ComponentsFromMesh", new IMAGE_BRUSH_SVG("Icons/IdentityComponentsFromMesh_16", Icon16x16));
	Set("Identity.Tools.ComponentsFromFootage", new IMAGE_BRUSH_SVG("Icons/IdentityComponentsFromFootage_16", Icon16x16));
	Set("Identity.Tools.ComponentsFromConformed", new IMAGE_BRUSH_SVG("Icons/IdentityComponentsFromConformed_16", Icon16x16));

	//Promoted Frames Timeline
	Set("Identity.PromotedFrames.PromoteFrameOnTimeline", new IMAGE_BRUSH_SVG("Icons/IdentityPromotedFramesPromoteFrame_20", Icon20x20));
	Set("Identity.PromotedFrames.DemoteFrameOnTimeline", new IMAGE_BRUSH_SVG("Icons/IdentityPromotedFramesDemoteFrame_20", Icon20x20));
	Set("Identity.PromotedFrames.Autotracked", new IMAGE_BRUSH_SVG("Icons/IdentityPromotedFramesAutotracked_16", Icon16x16));
	Set("Identity.PromotedFrames.Tracked", new IMAGE_BRUSH_SVG("Icons/IdentityPromotedFramesTracked_16", Icon16x16));
	Set("Identity.PromotedFrames.Modified", new IMAGE_BRUSH_SVG("Icons/IdentityPromotedFramesModified_16", Icon16x16));
	Set("Identity.PromotedFrames.Front", new IMAGE_BRUSH_SVG("Icons/IdentityPromotedFramesFrontFrame_16", Icon16x16));
	Set("Identity.PromotedFrames.Camera", new IMAGE_BRUSH_SVG("Icons/IdentityPromotedFramesCamera_16", Icon16x16));
	Set("Identity.PromotedFrames.CameraLocked", new IMAGE_BRUSH_SVG("Icons/IdentityPromotedFramesCameraLocked_16", Icon16x16));
	Set("Identity.PromotedFrames.FrameLocked", new IMAGE_BRUSH_SVG("Icons/IdentityPromotedFramesFrameLocked_16", Icon16x16));
	Set("Identity.PromotedFrames.CameraFreeRoam", new IMAGE_BRUSH_SVG("Icons/IdentityPromotedFramesCameraFreeRoam_32x16", Icon32x16));

	//Outliner
	Set("Identity.Outliner.Modified", new IMAGE_BRUSH_SVG("Icons/IdentityOutlinerModified_16", Icon16x16));
	Set("Identity.Outliner.Autotracked", new IMAGE_BRUSH_SVG("Icons/IdentityOutlinerAutotracked_16", Icon16x16));
	Set("Identity.Outliner.UsedToSolveFull", new IMAGE_BRUSH_SVG("Icons/IdentityOutlinerUsedToSolveFull_16", Icon16x16));
	Set("Identity.Outliner.UsedToSolveEmpty", new IMAGE_BRUSH_SVG("Icons/IdentityOutlinerUsedToSolveEmpty_16", Icon16x16));
	Set("Identity.Outliner.Visible", new IMAGE_BRUSH_SVG("Icons/IdentityOutlinerVisible_16", Icon16x16));
	Set("Identity.Outliner.Hidden", new IMAGE_BRUSH_SVG("Icons/IdentityOutlinerHidden_16", Icon16x16));
	Set("Identity.Outliner.Mixed", new IMAGE_BRUSH_SVG("Icons/IdentityOutlinerMixed_16", Icon16x16));
	Set("Identity.Outliner.MarkerCurve", new IMAGE_BRUSH_SVG("Icons/IdentityOutlinerMarkerCurve_16", Icon16x16));
	Set("Identity.Outliner.MarkerGroup", new IMAGE_BRUSH_SVG("Icons/IdentityOutlinerMarkerGroup_16", Icon16x16));
	Set("Identity.Outliner.Frame", new IMAGE_BRUSH_SVG("Icons/IdentityOutlinerFrame_16", Icon16x16));

	// Bigger (20x20) icons for Toolbar versions of commands, following UE5 UI guidelines
	// They are overriding the automatic (smaller) versions below
	Set("MetaHuman Identity.Toolbar.CreateComponents", new IMAGE_BRUSH_SVG("Icons/IdentityToolbarComponentsFromAsset_20", Icon20x20));
	Set("MetaHuman Identity.Toolbar.MeshToMetaHuman", new IMAGE_BRUSH_SVG("Icons/IdentityToolbarMeshToMetaHuman_20", Icon20x20));
	Set("MetaHuman Identity.Toolbar.ImportDNA", new IMAGE_BRUSH_SVG("Icons/IdentityToolbarImportDNA_20", Icon20x20));
	Set("MetaHuman Identity.Toolbar.ExportDNA", new IMAGE_BRUSH_SVG("Icons/IdentityToolbarExportDNA_20", Icon20x20));
	Set("MetaHuman Identity.Toolbar.FitTeeth", new IMAGE_BRUSH_SVG("Icons/IdentityToolbarFitTeeth_20", Icon20x20));
	Set("MetaHuman Identity.Toolbar.IdentitySolve", new IMAGE_BRUSH_SVG("Icons/IdentityToolbarSolve_20", Icon20x20));
	Set("MetaHuman Identity.Toolbar.PromoteFrame", new IMAGE_BRUSH_SVG("Icons/IdentityToolbarAddPromotedFrame_20", Icon20x20));
	Set("MetaHuman Identity.Toolbar.DemoteFrame", new IMAGE_BRUSH_SVG("Icons/IdentityToolbarRemovePromotedFrame_20", Icon20x20));
	Set("MetaHuman Identity.Toolbar.TrackCurrent", new IMAGE_BRUSH_SVG("Icons/IdentityToolbarTrackCurrentFrame_20", Icon20x20));
	Set("MetaHuman Identity.Toolbar.PrepareForPerformance", new IMAGE_BRUSH_SVG("Icons/IdentityToolbarPrepareForPerformance_20", Icon20x20));

	// Smaller icons (16x16) for commands in the menu and in normal (non-toolbar) buttons. They are automatically associated with a command
	Set("MetaHuman Identity.ImportDNA", new IMAGE_BRUSH_SVG("Icons/IdentityImportDNA_16", Icon16x16));
	Set("MetaHuman Identity.ExportDNA", new IMAGE_BRUSH_SVG("Icons/IdentityExportDNA_16", Icon16x16));
	Set("MetaHuman Identity.FitTeeth", new IMAGE_BRUSH_SVG("Icons/IdentityFitTeeth_16", Icon16x16));
	Set("MetaHuman Identity.IdentitySolve", new IMAGE_BRUSH_SVG("Icons/IdentitySolve_16", Icon16x16));
	Set("MetaHuman Identity.MeshToMetaHumanDNAOnly", new IMAGE_BRUSH_SVG("Icons/IdentityMeshToMetaHumanDNAOnly_16", Icon16x16));
	Set("MetaHuman Identity.PromoteFrame", new IMAGE_BRUSH_SVG("Icons/IdentityAddPromotedFrame_16", Icon16x16));
	Set("MetaHuman Identity.DemoteFrame", new IMAGE_BRUSH_SVG("Icons/IdentityRemovePromotedFrame_16", Icon16x16));
	Set("MetaHuman Identity.TrackCurrent", new IMAGE_BRUSH_SVG("Icons/IdentityTrackCurrentFrame_16", Icon16x16));
	Set("MetaHuman Identity.PrepareForPerformance", new IMAGE_BRUSH_SVG("Icons/IdentityPrepareForPerformance_16", Icon16x16));

	// Register the thumbnails used in the Identity asset's body selection UI
	// IdentityBody_xyz: x=height, y=BMI, z=gender

	//Height: 0
	Set("Identity.Body.000", new IMAGE_BRUSH("Icons/IdentityBody_000", Thumb128x128));
	Set("Identity.Body.001", new IMAGE_BRUSH("Icons/IdentityBody_001", Thumb128x128));
	Set("Identity.Body.010", new IMAGE_BRUSH("Icons/IdentityBody_010", Thumb128x128));
	Set("Identity.Body.011", new IMAGE_BRUSH("Icons/IdentityBody_011", Thumb128x128));
	Set("Identity.Body.020", new IMAGE_BRUSH("Icons/IdentityBody_020", Thumb128x128));
	Set("Identity.Body.021", new IMAGE_BRUSH("Icons/IdentityBody_021", Thumb128x128));

	//Height: 1
	Set("Identity.Body.100", new IMAGE_BRUSH("Icons/IdentityBody_100", Thumb128x128));
	Set("Identity.Body.101", new IMAGE_BRUSH("Icons/IdentityBody_101", Thumb128x128));
	Set("Identity.Body.110", new IMAGE_BRUSH("Icons/IdentityBody_110", Thumb128x128));
	Set("Identity.Body.111", new IMAGE_BRUSH("Icons/IdentityBody_111", Thumb128x128));
	Set("Identity.Body.120", new IMAGE_BRUSH("Icons/IdentityBody_120", Thumb128x128));
	Set("Identity.Body.121", new IMAGE_BRUSH("Icons/IdentityBody_121", Thumb128x128));
	
	//Height: 2
	Set("Identity.Body.200", new IMAGE_BRUSH("Icons/IdentityBody_200", Thumb128x128));
	Set("Identity.Body.201", new IMAGE_BRUSH("Icons/IdentityBody_201", Thumb128x128));
	Set("Identity.Body.210", new IMAGE_BRUSH("Icons/IdentityBody_210", Thumb128x128));
	Set("Identity.Body.211", new IMAGE_BRUSH("Icons/IdentityBody_211", Thumb128x128));
	Set("Identity.Body.220", new IMAGE_BRUSH("Icons/IdentityBody_220", Thumb128x128));
	Set("Identity.Body.221", new IMAGE_BRUSH("Icons/IdentityBody_221", Thumb128x128));

	Set("Identity.ABSplit.A.Small", new IMAGE_BRUSH_SVG("Icons/IdentityABSplit_A_Small_16", Icon16x16));
	Set("Identity.ABSplit.A.Large", new IMAGE_BRUSH_SVG("Icons/IdentityABSplit_A_Large_16", Icon16x16));
	Set("Identity.ABSplit.B.Small", new IMAGE_BRUSH_SVG("Icons/IdentityABSplit_B_Small_16", Icon16x16));
	Set("Identity.ABSplit.B.Large", new IMAGE_BRUSH_SVG("Icons/IdentityABSplit_B_Large_16", Icon16x16));

	// Icons for the mask toolbar
	Set("Identity.Mask.0", new IMAGE_BRUSH("Icons/IdentityMask_0", Thumb128x128));
	Set("Identity.Mask.1", new IMAGE_BRUSH("Icons/IdentityMask_1", Thumb128x128));
	Set("Identity.Mask.2", new IMAGE_BRUSH("Icons/IdentityMask_2", Thumb128x128));
	Set("Identity.Mask.3", new IMAGE_BRUSH("Icons/IdentityMask_3", Thumb128x128));

	// Outliner row help icon
	Set("Identity.Outliner.Help", new IMAGE_BRUSH_SVG("Icons/IdentityOutliner_Help_16", Icon16x16));

	// Curve thumbnails for outliner tooltip
	for (const FString& CurveName : OutlinerTooltipCurveImages)
	{
		FSlateImageBrush* SlateBrush = new IMAGE_BRUSH(FString::Printf(TEXT("Icons/IdentityOutliner_%s"), *CurveName), Thumb256x256);
		Set(*FString::Printf(TEXT("Identity.Outliner.%s"), *CurveName), SlateBrush);
	}

	//128x128 asset thumbnails
	Set("ClassThumbnail.MetaHumanIdentity", new IMAGE_BRUSH_SVG("Icons/AssetMetaHumanIdentity_64", Thumb64x64));
	Set("ClassThumbnail.FootageCaptureData", new IMAGE_BRUSH_SVG("Icons/AssetCaptureDataFootage_64", Thumb64x64));
	Set("ClassThumbnail.MeshCaptureData", new IMAGE_BRUSH_SVG("Icons/AssetCaptureDataMesh_64", Thumb64x64));
	Set("ClassThumbnail.FaceContourTracker", new IMAGE_BRUSH_SVG("Icons/AssetFaceContourTracker_64", Thumb64x64));

	//16x16 asset icons
	Set("ClassIcon.MetaHumanIdentity", new IMAGE_BRUSH_SVG("Icons/AssetMetaHumanIdentity_16", Icon16x16));
	Set("ClassIcon.MeshCaptureData", new IMAGE_BRUSH_SVG("Icons/AssetCaptureDataMesh_16", Icon16x16));
	Set("ClassIcon.FootageCaptureData", new IMAGE_BRUSH_SVG("Icons/AssetCaptureDataFootage_16", Icon16x16));
	Set("ClassIcon.FaceContourTracker", new IMAGE_BRUSH_SVG("Icons/AssetFaceContourTracker_16", Icon16x16));
}

void FMetaHumanIdentityStyle::Register()
{
	FSlateStyleRegistry::RegisterSlateStyle(Get());
}

void FMetaHumanIdentityStyle::Unregister()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(Get());
}

FMetaHumanIdentityStyle& FMetaHumanIdentityStyle::Get()
{
	static FMetaHumanIdentityStyle Inst;
	return Inst;
}