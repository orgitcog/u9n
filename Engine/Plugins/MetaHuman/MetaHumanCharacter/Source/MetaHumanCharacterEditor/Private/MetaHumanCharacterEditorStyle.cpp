// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanCharacterEditorStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateStyleMacros.h"
#include "Styling/StyleColors.h"
#include "Styling/SlateTypes.h"
#include "Interfaces/IPluginManager.h"

#define EDITOR_IMAGE_BRUSH( RelativePath, ... ) FSlateImageBrush(FPaths::EngineContentDir() / "Editor/Slate" / RelativePath + TEXT(".png"), __VA_ARGS__ )

FMetaHumanCharacterEditorStyle::FMetaHumanCharacterEditorStyle()
	: FSlateStyleSet{ TEXT("MetaHumanCharacterEditorStyle") }
{
	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(UE_PLUGIN_NAME);
	check(Plugin.IsValid());

	SetContentRoot(Plugin->GetContentDir());

	const FVector2D Icon6{ 6.0, 6.0 };
	const FVector2D Icon12{ 12.0, 12.0 };
	const FVector2D Icon16{ 16.0, 16.0 };
	const FVector2D Icon24{ 24.0, 24.0 };
	const FVector2D Icon6x20{ 6.0, 20.0 };
	const FVector2D Icon32{ 32.0, 32.0 };
	const FVector2D Icon64{ 64.0, 64.0 };
	const FVector2D IconThumb{ 2.0, 20.0 };
	const FVector2D Icon20{ 20.0 , 20.0 };
	const FVector2D Icon256{ 256.0, 256.0 };
	const FVector2D Icon512{ 512.0, 512.0 };

	// Colors
	const FLinearColor SelectionColor = FLinearColor(FColor(2, 117, 247));

	// Brushes
	Set(TEXT("MetaHumanCharacterEditorTools.WhiteBrush"), new FSlateColorBrush(FLinearColor::White));
	Set(TEXT("MetaHumanCharacterEditorTools.MainToolbar"), new FSlateColorBrush(FLinearColor(.01f, .01f, .01f, 1.f)));
	Set(TEXT("MetaHumanCharacterEditorTools.ActiveToolLabel"), new FSlateColorBrush(FLinearColor(.03f, .03f, .03f, 1.f)));
	Set(TEXT("MetaHumanCharacterEditorTools.Rounded.BlackBrush"), new FSlateRoundedBoxBrush(FLinearColor::Black, 4.f));
	Set(TEXT("MetaHumanCharacterEditorTools.Rounded.DefaultBrush"), new FSlateRoundedBoxBrush(FLinearColor(.04f, .04f, .04f, 1.f), 4.f));
	Set(TEXT("MetaHumanCharacterEditorTools.Rounded.SelectedBrush"), new FSlateRoundedBoxBrush(SelectionColor, 4.f));
	Set(TEXT("MetaHumanCharacterEditorTools.Rounded.WhiteBrush"), new FSlateRoundedBoxBrush(FLinearColor::White, 4.f));
	Set(TEXT("MetaHumanCharacterEditorTools.DropShadow"), new EDITOR_IMAGE_BRUSH("Starship/ContentBrowser/drop-shadow", Icon16));

	//Icons
	Set(TEXT("MetaHumanCharacterEditorTools.ContentDirty"), new IMAGE_BRUSH_SVG("UI/Icons/ContentDirty_12", Icon12));
	Set(TEXT("MetaHumanCharacterEditorTools.ContentChecked"), new IMAGE_BRUSH_SVG("UI/Icons/ContentChecked_16", Icon16));
	Set(TEXT("MetaHumanCharacterEditorTools.ContentSupported"), new IMAGE_BRUSH_SVG("UI/Icons/ContentAvailable_16", Icon16));
	Set(TEXT("MetaHumanCharacterEditorTools.ContentActive"), new IMAGE_BRUSH_SVG("UI/Icons/ContentActive_16", Icon16));
	Set(TEXT("MetaHumanCharacterEditorTools.ThumbnailAreaBackground"), new FSlateRoundedBoxBrush(FStyleColors::Recessed, 4.0f));
	Set(TEXT("MetaHumanCharacterEditorTools.LoadedLayer"), new IMAGE_BRUSH("UI/Icons/LoadedLayer", Icon16));
	Set(TEXT("MetaHumanCharacterEditorTools.AssetViewSlot"), new IMAGE_BRUSH_SVG("UI/Icons/AssetView_Slot", Icon16));

	// Tile View
	Set("MetaHumanCharacterEditorTools.TableViewRow", FTableRowStyle()
		.SetEvenRowBackgroundBrush(FSlateNoResource())
		.SetOddRowBackgroundBrush(FSlateNoResource())
		.SetActiveBrush(FSlateNoResource())
		.SetActiveHighlightedBrush(FSlateNoResource())
		.SetEvenRowBackgroundHoveredBrush(IMAGE_BRUSH(TEXT("UI/Common/TileView_Selector"), Icon512, FLinearColor(.6f, .6f, .6f, .4f)))
		.SetOddRowBackgroundHoveredBrush(IMAGE_BRUSH(TEXT("UI/Common/TileView_Selector"), Icon512, FLinearColor(.6f, .6f, .6f, .4f)))
		.SetParentRowBackgroundBrush(FSlateNoResource())
		.SetParentRowBackgroundHoveredBrush(FSlateNoResource())
		.SetSelectorFocusedBrush(IMAGE_BRUSH(TEXT("UI/Common/TileView_Selector"), Icon512, SelectionColor))
		.SetInactiveBrush(IMAGE_BRUSH(TEXT("UI/Common/TileView_Selector"), Icon512, SelectionColor))
		.SetInactiveHoveredBrush(IMAGE_BRUSH(TEXT("UI/Common/TileView_Selector"), Icon512, SelectionColor))
		.SetActiveHoveredBrush(FSlateNoResource()));

	// Asset View
	Set("MetaHumanCharacterEditorTools.AssetView", FTableRowStyle()
		.SetEvenRowBackgroundBrush(FSlateNoResource())
		.SetEvenRowBackgroundHoveredBrush(FSlateNoResource())
		.SetOddRowBackgroundBrush(FSlateNoResource())
		.SetOddRowBackgroundHoveredBrush(FSlateNoResource())
		.SetSelectorFocusedBrush(FSlateNoResource())
		.SetActiveBrush(FSlateNoResource())
		.SetActiveHoveredBrush(FSlateNoResource())
		.SetInactiveBrush(FSlateNoResource())
		.SetInactiveHoveredBrush(FSlateNoResource())
		.SetTextColor(FStyleColors::AccentWhite)
		.SetSelectedTextColor(FStyleColors::AccentWhite)
	);

	// Tools
	Set(TEXT("MetaHumanCharacterEditorTools.LoadClickTools"), new IMAGE_BRUSH_SVG("UI/Icons/ClickToolsCategory_16", Icon16));
	Set(TEXT("MetaHumanCharacterEditorTools.BeginSingleClickTool"), new IMAGE_BRUSH_SVG("UI/Icons/ClickToolsCategory_16", Icon16));
	Set(TEXT("MetaHumanCharacterEditorTools.BeginMultiClickTool"), new IMAGE_BRUSH_SVG("UI/Icons/ClickToolsCategory_16", Icon16));
	Set(TEXT("MetaHumanCharacterEditorTools.BeginClickAndDragTool"), new IMAGE_BRUSH_SVG("UI/Icons/ClickToolsCategory_16", Icon16));
	Set(TEXT("MetaHumanCharacterEditorTools.BeginMeshClickTool"), new IMAGE_BRUSH_SVG("UI/Icons/ClickToolsCategory_16", Icon16));

	// TODO: Replace these with SVGs
	Set(TEXT("MetaHumanCharacterEditorTools.LoadPresetsTools"), new IMAGE_BRUSH_SVG("UI/Icons/Ribbon_Library", Icon16));

	Set(TEXT("MetaHumanCharacterEditorTools.BeginAddRemoveLandmarkTool"), new IMAGE_BRUSH_SVG("UI/Icons/Tool_16", Icon16));

	Set(TEXT("MetaHumanCharacterEditorTools.LoadHeadTools"), new IMAGE_BRUSH_SVG("UI/Icons/Ribbon_Face", Icon16));
	Set(TEXT("MetaHumanCharacterEditorTools.BeginConformTools"), new IMAGE_BRUSH_SVG("UI/Icons/Tools_Head_Conform", Icon16));
	Set(TEXT("MetaHumanCharacterEditorTools.BeginConformImportDNATool"), new IMAGE_BRUSH_SVG("UI/Icons/SubTools_Dot", Icon6));
	Set(TEXT("MetaHumanCharacterEditorTools.BeginConformImportIdentityTool"), new IMAGE_BRUSH_SVG("UI/Icons/SubTools_Dot", Icon6));
	Set(TEXT("MetaHumanCharacterEditorTools.BeginConformImportTemplateTool"), new IMAGE_BRUSH_SVG("UI/Icons/SubTools_Dot", Icon6));
	Set(TEXT("MetaHumanCharacterEditorTools.BeginHeadModelTools"), new IMAGE_BRUSH_SVG("UI/Icons/Tools_Head_Model", Icon16));
	Set(TEXT("MetaHumanCharacterEditorTools.BeginHeadModelEyelashesTool"), new IMAGE_BRUSH_SVG("UI/Icons/SubTools_Dot", Icon6));
	Set(TEXT("MetaHumanCharacterEditorTools.BeginHeadModelTeethTool"), new IMAGE_BRUSH_SVG("UI/Icons/SubTools_Dot", Icon6));
	Set(TEXT("MetaHumanCharacterEditorTools.BeginFaceBlendTool"), new IMAGE_BRUSH_SVG("UI/Icons/Tools_Head_Blend", Icon16));

	Set(TEXT("MetaHumanCharacterEditorTools.BeginFaceMoveTool"), new IMAGE_BRUSH_SVG("UI/Icons/ScreenSpaceMove", Icon16));
	Set(TEXT("MetaHumanCharacterEditorTools.BeginFaceSculptTool"), new IMAGE_BRUSH_SVG("UI/Icons/Manipulator_SculptHead", Icon16));
	Set(TEXT("MetaHumanCharacterEditorTools.ResetFaceTool"), new IMAGE_BRUSH_SVG("UI/Icons/Manipulator_ResetHead", Icon16));

	Set(TEXT("MetaHumanCharacterEditorTools.LoadBodyTools"), new IMAGE_BRUSH_SVG("UI/Icons/Ribbon_Body", Icon16));
	Set(TEXT("MetaHumanCharacterEditorTools.BeginBodyConformTools"), new IMAGE_BRUSH_SVG("UI/Icons/Tools_Body_Conform", Icon16));
	Set(TEXT("MetaHumanCharacterEditorTools.BeginBodyConformImportBodyDNATool"), new IMAGE_BRUSH_SVG("UI/Icons/SubTools_Dot", Icon6));
	Set(TEXT("MetaHumanCharacterEditorTools.BeginBodyConformImportBodyTemplateTool"), new IMAGE_BRUSH_SVG("UI/Icons/SubTools_Dot", Icon6));
	Set(TEXT("MetaHumanCharacterEditorTools.BeginBodyModelTool"), new IMAGE_BRUSH_SVG("UI/Icons/Tools_Body_Model", Icon16));
	Set(TEXT("MetaHumanCharacterEditorTools.BeginBodyModelParametricTool"), new IMAGE_BRUSH_SVG("UI/Icons/SubTools_Dot", Icon6));
	Set(TEXT("MetaHumanCharacterEditorTools.BeginBodyFixedCompatibilityTool"), new IMAGE_BRUSH_SVG("UI/Icons/SubTools_Dot", Icon6));
	Set(TEXT("MetaHumanCharacterEditorTools.BeginBodyBlendTool"), new IMAGE_BRUSH_SVG("UI/Icons/Tools_Body_Blend", Icon16));

	Set(TEXT("MetaHumanCharacterEditorTools.LoadMaterialsTools"), new IMAGE_BRUSH_SVG("UI/Icons/Ribbon_Materials", Icon16));
	Set(TEXT("MetaHumanCharacterEditorTools.BeginSkinTool"), new IMAGE_BRUSH_SVG("UI/Icons/Tools_Materials_Skin", Icon16));
	Set(TEXT("MetaHumanCharacterEditorTools.BeginEyesTool"), new IMAGE_BRUSH_SVG("UI/Icons/Tools_Materials_Eyes", Icon16));
	Set(TEXT("MetaHumanCharacterEditorTools.BeginMakeupTool"), new IMAGE_BRUSH_SVG("UI/Icons/Tools_Head_Model", Icon16));
	Set(TEXT("MetaHumanCharacterEditorTools.BeginHeadMaterialsTools"), new IMAGE_BRUSH_SVG("UI/Icons/Tools_Head_Model", Icon16));
	Set(TEXT("MetaHumanCharacterEditorTools.BeginHeadMaterialsTeethTool"), new IMAGE_BRUSH_SVG("UI/Icons/SubTools_Dot", Icon6));
	Set(TEXT("MetaHumanCharacterEditorTools.BeginHeadMaterialsEyelashesTool"), new IMAGE_BRUSH_SVG("UI/Icons/SubTools_Dot", Icon6));

	Set(TEXT("MetaHumanCharacterEditorTools.LoadHairAndClothingTools"), new IMAGE_BRUSH_SVG("UI/Icons/Ribbon_HairAndClothing", Icon16));
	Set(TEXT("MetaHumanCharacterEditorTools.BeginWardrobeSelectionTool"), new IMAGE_BRUSH_SVG("UI/Icons/Tools_HnC_Selection", Icon16));
	Set(TEXT("MetaHumanCharacterEditorTools.BeginCostumeDetailsTool"), new IMAGE_BRUSH_SVG("UI/Icons/Tools_HnC_Details", Icon16));

	Set(TEXT("MetaHumanCharacterEditorTools.LoadPipelineTools"), new IMAGE_BRUSH_SVG("UI/Icons/Ribbon_Assembly", Icon16));

	//Toolbar
	Set(TEXT("MetaHumanCharacterEditor.Toolbar.DownloadHighResTextures"), new IMAGE_BRUSH_SVG("UI/Icons/Toolbar_DownloadTextureSources", Icon16));
	Set(TEXT("MetaHumanCharacterEditor.Toolbar.AddRigFull"), new IMAGE_BRUSH_SVG("UI/Icons/Toolbar_AddRigFull", Icon16));
	Set(TEXT("MetaHumanCharacterEditor.Toolbar.AddRigSkeletal"), new IMAGE_BRUSH_SVG("UI/Icons/Toolbar_AddRigSkeletal", Icon16));
	Set(TEXT("MetaHumanCharacterEditor.Toolbar.RemoveRig"), new IMAGE_BRUSH_SVG("UI/Icons/Toolbar_RemoveRig", Icon16));
	Set(TEXT("MetaHumanCharacterEditor.Toolbar.SaveThumbnail"), new IMAGE_BRUSH_SVG("UI/Icons/Toolbar_Thumbnail", Icon16));

	// Move
	Set(TEXT("MetaHumanCharacterEditorTools.Face.ScreenSpaceMoveTool"), new IMAGE_BRUSH_SVG("UI/Icons/ScreenSpaceMove", Icon32));
	Set(TEXT("MetaHumanCharacterEditorTools.Face.TranslateMoveTool"), new IMAGE_BRUSH_SVG("UI/Icons/Manipulator_Translate", Icon32));
	Set(TEXT("MetaHumanCharacterEditorTools.Face.RotateMoveTool"), new IMAGE_BRUSH_SVG("UI/Icons/Manipulator_Rotate", Icon32));
	Set(TEXT("MetaHumanCharacterEditorTools.Face.ScaleMoveTool"), new IMAGE_BRUSH_SVG("UI/Icons/Manipulator_Scale", Icon32));
	Set(TEXT("MetaHumanCharacterEditorTools.Face.SculptTool"), new IMAGE_BRUSH_SVG("UI/Icons/Manipulator_SculptHead", Icon32));

	// Parametric Body Checkbox
	Set("MetaHumanCharacterEditorTools.ParametricBody.CheckBox", FCheckBoxStyle()
		.SetUncheckedImage(IMAGE_BRUSH(TEXT("UI/ParametricBody/unpinned_20"), Icon20))
		.SetUncheckedHoveredImage(IMAGE_BRUSH(TEXT("UI/ParametricBody/unpinned_20"), Icon20))
		.SetUncheckedPressedImage(IMAGE_BRUSH(TEXT("UI/ParametricBody/unpinned_20"), Icon20))
		.SetCheckedImage(IMAGE_BRUSH(TEXT("UI/ParametricBody/pinned_20"), Icon20))
		.SetCheckedHoveredImage(IMAGE_BRUSH(TEXT("UI/ParametricBody/pinned_20"), Icon20))
		.SetCheckedPressedImage(IMAGE_BRUSH(TEXT("UI/ParametricBody/pinned_20"), Icon20))
		.SetUndeterminedImage(IMAGE_BRUSH(TEXT("UI/ParametricBody/mixed_pinned_20"), Icon20))
		.SetUndeterminedHoveredImage(IMAGE_BRUSH(TEXT("UI/ParametricBody/mixed_pinned_20"), Icon20))
		.SetUndeterminedPressedImage(IMAGE_BRUSH(TEXT("UI/ParametricBody/mixed_pinned_20"), Icon20)));

	Set("MetaHumanCharacterEditorTools.ParametricBody.TableRow", FTableRowStyle()
		.SetSelectorFocusedBrush(FSlateNoResource())
		.SetEvenRowBackgroundBrush(FSlateNoResource())
		.SetOddRowBackgroundBrush(FSlateNoResource())
		.SetEvenRowBackgroundHoveredBrush(FSlateNoResource())
		.SetOddRowBackgroundHoveredBrush(FSlateNoResource())
		.SetActiveBrush(FSlateNoResource())
		.SetActiveHoveredBrush(FSlateNoResource())
		.SetInactiveBrush(FSlateNoResource())
		.SetInactiveHoveredBrush(FSlateNoResource()));

	Set("MetaHumanCharacterEditorTools.ParametricBody.TableView", FTableViewStyle()
		.SetBackgroundBrush(FSlateNoResource()));

	// Skin
	Set(TEXT("Skin.Accents.Head"), new IMAGE_BRUSH(TEXT("UI/SkinAccentRegions/SkinAccentsHead"), FVector2D{ 221.0, 285.0 }));

	Set(TEXT("Skin.Freckles.None"), new IMAGE_BRUSH(TEXT("UI/Freckles/Freckles_000"), Icon512));
	Set(TEXT("Skin.Freckles.Type1"), new IMAGE_BRUSH(TEXT("UI/Freckles/Freckles_001"), Icon512));
	Set(TEXT("Skin.Freckles.Type2"), new IMAGE_BRUSH(TEXT("UI/Freckles/Freckles_002"), Icon512));
	Set(TEXT("Skin.Freckles.Type3"), new IMAGE_BRUSH(TEXT("UI/Freckles/Freckles_003"), Icon512));

	// TODO: Replace this with SVG
	Set(TEXT("Skin.SkinTone.Crosshair"), new IMAGE_BRUSH(TEXT("UI/SkinToneCrosshair"), Icon16));

	// Eyes
	Set(TEXT("Eyes.Iris.Iris001"), new IMAGE_BRUSH(TEXT("UI/EyesIris/Iris_thumbnail_001"), Icon512));
	Set(TEXT("Eyes.Iris.Iris002"), new IMAGE_BRUSH(TEXT("UI/EyesIris/Iris_thumbnail_002"), Icon512));
	Set(TEXT("Eyes.Iris.Iris003"), new IMAGE_BRUSH(TEXT("UI/EyesIris/Iris_thumbnail_003"), Icon512));
	Set(TEXT("Eyes.Iris.Iris004"), new IMAGE_BRUSH(TEXT("UI/EyesIris/Iris_thumbnail_004"), Icon512));
	Set(TEXT("Eyes.Iris.Iris005"), new IMAGE_BRUSH(TEXT("UI/EyesIris/Iris_thumbnail_005"), Icon512));
	Set(TEXT("Eyes.Iris.Iris006"), new IMAGE_BRUSH(TEXT("UI/EyesIris/Iris_thumbnail_006"), Icon512));
	Set(TEXT("Eyes.Iris.Iris007"), new IMAGE_BRUSH(TEXT("UI/EyesIris/Iris_thumbnail_007"), Icon512));
	Set(TEXT("Eyes.Iris.Iris008"), new IMAGE_BRUSH(TEXT("UI/EyesIris/Iris_thumbnail_008"), Icon512));
	Set(TEXT("Eyes.Iris.Iris009"), new IMAGE_BRUSH(TEXT("UI/EyesIris/Iris_thumbnail_009"), Icon512));

	// Eyelashes
	Set(TEXT("Eyelashes.None"), new IMAGE_BRUSH(TEXT("UI/Eyelashes/None"), Icon512));
	Set(TEXT("Eyelashes.Sparse"), new IMAGE_BRUSH(TEXT("UI/Eyelashes/Sparse"), Icon512));
	Set(TEXT("Eyelashes.ShortFine"), new IMAGE_BRUSH(TEXT("UI/Eyelashes/ShortFine"), Icon512));
	Set(TEXT("Eyelashes.Thin"), new IMAGE_BRUSH(TEXT("UI/Eyelashes/Thin"), Icon512));
	Set(TEXT("Eyelashes.SlightCurl"), new IMAGE_BRUSH(TEXT("UI/Eyelashes/SlightCurl"), Icon512));
	Set(TEXT("Eyelashes.LongCurl"), new IMAGE_BRUSH(TEXT("UI/Eyelashes/LongCurl"), Icon512));
	Set(TEXT("Eyelashes.ThickCurl"), new IMAGE_BRUSH(TEXT("UI/Eyelashes/ThickCurl"), Icon512));

	// Makeup
	Set(TEXT("Makeup.Eyes.None"), new IMAGE_BRUSH(TEXT("UI/EyeMakeup/None"), Icon512));
	Set(TEXT("Makeup.Eyes.ThinLiner"), new IMAGE_BRUSH(TEXT("UI/EyeMakeup/ThinLiner"), Icon512));
	Set(TEXT("Makeup.Eyes.SoftSmokey"), new IMAGE_BRUSH(TEXT("UI/EyeMakeup/SoftSmokey"), Icon512));
	Set(TEXT("Makeup.Eyes.FullThinLiner"), new IMAGE_BRUSH(TEXT("UI/EyeMakeup/FullThinLiner"), Icon512));
	Set(TEXT("Makeup.Eyes.CatEye"), new IMAGE_BRUSH(TEXT("UI/EyeMakeup/CatEye"), Icon512));
	Set(TEXT("Makeup.Eyes.PandaSmudge"), new IMAGE_BRUSH(TEXT("UI/EyeMakeup/PandaSmudge"), Icon512));
	Set(TEXT("Makeup.Eyes.DramaticSmudge"), new IMAGE_BRUSH(TEXT("UI/EyeMakeup/DramaticSmudge"), Icon512));
	Set(TEXT("Makeup.Eyes.DoubleMod"), new IMAGE_BRUSH(TEXT("UI/EyeMakeup/DoubleMod"), Icon512));
	Set(TEXT("Makeup.Eyes.ClassicBar"), new IMAGE_BRUSH(TEXT("UI/EyeMakeup/ClassicBar"), Icon512));

	Set(TEXT("Makeup.Blush.None"), new IMAGE_BRUSH(TEXT("UI/BlushMakeup/None"), Icon512));
	Set(TEXT("Makeup.Blush.Angled"), new IMAGE_BRUSH(TEXT("UI/BlushMakeup/Angled"), Icon512));
	Set(TEXT("Makeup.Blush.Apple"), new IMAGE_BRUSH(TEXT("UI/BlushMakeup/Apple"), Icon512));
	Set(TEXT("Makeup.Blush.HighCurve"), new IMAGE_BRUSH(TEXT("UI/BlushMakeup/HighCurve"), Icon512));
	Set(TEXT("Makeup.Blush.LowSweep"), new IMAGE_BRUSH(TEXT("UI/BlushMakeup/LowSweep"), Icon512));

	Set(TEXT("Makeup.Lips.None"), new IMAGE_BRUSH(TEXT("UI/LipsMakeup/None"), Icon512));
	Set(TEXT("Makeup.Lips.Natural"), new IMAGE_BRUSH(TEXT("UI/LipsMakeup/Natural"), Icon512));
	Set(TEXT("Makeup.Lips.Hollywood"), new IMAGE_BRUSH(TEXT("UI/LipsMakeup/Hollywood"), Icon512));
	Set(TEXT("Makeup.Lips.Cupid"), new IMAGE_BRUSH(TEXT("UI/LipsMakeup/Cupid"), Icon512));

	// Teeth
	Set(TEXT("Teeth.Preview"), new IMAGE_BRUSH(TEXT("UI/Teeth/teeth-preview"), Icon512));
	Set(TEXT("Teeth.EmptyElipse"), new IMAGE_BRUSH(TEXT("UI/Teeth/teeth-preview-elipse"), Icon24));
	Set(TEXT("Teeth.FullElipse"), new IMAGE_BRUSH(TEXT("UI/Teeth/teeth-preview-circle-pressed"), Icon24));
	Set(TEXT("Teeth.Arrow"), new IMAGE_BRUSH(TEXT("UI/Teeth/teeth-preview-arrow"), FVector2D(14.f, 6.f)));

	Set("MetaHumanCharacterEditorTools.Teeth.Slider", FSliderStyle()
		.SetNormalBarImage(IMAGE_BRUSH(TEXT("UI/Teeth/teeth-slider-back"), FVector2D(30.f, 60.f), FLinearColor::White))
		.SetHoveredBarImage(IMAGE_BRUSH(TEXT("UI/Teeth/teeth-slider-back"), FVector2D(30.f, 60.f), FLinearColor::White))
		.SetDisabledBarImage(FSlateNoResource())
		.SetNormalThumbImage(IMAGE_BRUSH(TEXT("UI/Teeth/teeth-handler-pressed"), Icon20))
		.SetHoveredThumbImage(IMAGE_BRUSH(TEXT("UI/Teeth/teeth-handler-pressed"), Icon20))
		.SetDisabledThumbImage(FSlateNoResource())
		.SetBarThickness(60.f));

	// Animation
	Set(TEXT("Viewport.AnimationBar.Play"), new IMAGE_BRUSH(TEXT("UI/Viewport/AnimationBar/Play"), Icon20));
	Set(TEXT("Viewport.AnimationBar.Stop"), new IMAGE_BRUSH(TEXT("UI/Viewport/AnimationBar/Stop"), Icon20));
	Set(TEXT("Viewport.AnimationBar.Pause"), new IMAGE_BRUSH(TEXT("UI/Viewport/AnimationBar/Pause"), Icon20));
	Set(TEXT("Viewport.AnimationBar.SliderThumb"), new IMAGE_BRUSH(TEXT("UI/Viewport/AnimationBar/SliderThumb"), IconThumb));
	
	Set(TEXT("Viewport.Icons.Environment"), new IMAGE_BRUSH(TEXT("UI/Viewport/ViewportEnvironment"), Icon16));
	Set(TEXT("Viewport.Icons.Camera"), new IMAGE_BRUSH(TEXT("UI/Viewport/ViewportCamera"), Icon16));
	Set(TEXT("Viewport.Icons.Clay"), new IMAGE_BRUSH(TEXT("UI/Viewport/ViewportClayMaterial"), Icon16));
	Set(TEXT("Viewport.Icons.Hair"), new IMAGE_BRUSH(TEXT("UI/Viewport/ViewportHideHair"), Icon16));
	Set(TEXT("Viewport.Icons.LOD"), new IMAGE_BRUSH(TEXT("UI/Viewport/ViewportLOD"), Icon16));
	Set(TEXT("Viewport.Icons.Quality"), new IMAGE_BRUSH(TEXT("UI/Viewport/ViewportQuality"), Icon16));
	Set(TEXT("Viewport.Icons.Keyboard"), new IMAGE_BRUSH_SVG(TEXT("UI/Icons/Keyboard"), Icon16));

	Set(TEXT("Viewport.LightScenarios.Studio"), new IMAGE_BRUSH(TEXT("UI/Viewport/Studio/Studio"), Icon256));
	Set(TEXT("Viewport.LightScenarios.Split"), new IMAGE_BRUSH(TEXT("UI/Viewport/Studio/Split"), Icon256));
	Set(TEXT("Viewport.LightScenarios.Fireside"), new IMAGE_BRUSH(TEXT("UI/Viewport/Studio/Fireside"), Icon256));
	Set(TEXT("Viewport.LightScenarios.Moonlight"), new IMAGE_BRUSH(TEXT("UI/Viewport/Studio/Moonlight"), Icon256));
	Set(TEXT("Viewport.LightScenarios.Tungsten"), new IMAGE_BRUSH(TEXT("UI/Viewport/Studio/Tungsten"), Icon256));
	Set(TEXT("Viewport.LightScenarios.Portrait"), new IMAGE_BRUSH(TEXT("UI/Viewport/Studio/Portrait"), Icon256));
	Set(TEXT("Viewport.LightScenarios.RedLantern"), new IMAGE_BRUSH(TEXT("UI/Viewport/Studio/RedLantern"), Icon256));
	Set(TEXT("Viewport.LightScenarios.TextureBooth"), new IMAGE_BRUSH(TEXT("UI/Viewport/Studio/Studio"), Icon256));

	// Wardrobe
	Set(TEXT("Wardrobe.AssetView.TileIcon.Tick"), new IMAGE_BRUSH_SVG("UI/Icons/WardrobeTools_AssetView_TileIcon_Tick_16", Icon16));
	Set(TEXT("Wardrobe.AssetView.FolderIcon"), new IMAGE_BRUSH_SVG("UI/Icons/WardrobeTools_AssetView_FolderIcon_24", Icon24));

	// Blend
	Set(TEXT("MetaHumanCharacterEditorTools.BlendTool.Circle"), new IMAGE_BRUSH("UI/Common/BlendTool_Circle", Icon512));

	SetSkinAccentRegionStyle(FSkinAccentRegionStyleParams
	{
		.Property = TEXT("Skin.Accents.Scalp"),
		.BrushSize = FVector2D{ 148.0, 40.0 },
		.Image = TEXT("UI/SkinAccentRegions/Scalp"),
	});

	SetSkinAccentRegionStyle(FSkinAccentRegionStyleParams
	{
		.Property = TEXT("Skin.Accents.Forehead"),
		.BrushSize = FVector2D{ 190.0, 59.0 },
		.Image = TEXT("UI/SkinAccentRegions/Forehead"),
	});

	SetSkinAccentRegionStyle(FSkinAccentRegionStyleParams
	{
		.Property = TEXT("Skin.Accents.Nose"),
		.BrushSize = FVector2D{ 55.0, 78.0 },
		.Image = TEXT("UI/SkinAccentRegions/Nose"),
	});

	SetSkinAccentRegionStyle(FSkinAccentRegionStyleParams
	{
		.Property = TEXT("Skin.Accents.UnderEyeLeft"),
		.BrushSize = FVector2D{ 58.0, 42.0 },
		.Image = TEXT("UI/SkinAccentRegions/UnderEyeLeft"),
	});

	SetSkinAccentRegionStyle(FSkinAccentRegionStyleParams
	{
		.Property = TEXT("Skin.Accents.UnderEyeRight"),
		.BrushSize = FVector2D{ 58.0, 42.0 },
		.Image = TEXT("UI/SkinAccentRegions/UnderEyeRight"),
	});

	SetSkinAccentRegionStyle(FSkinAccentRegionStyleParams
	{
		.Property = TEXT("Skin.Accents.EarLeft"),
		.BrushSize = FVector2D{ 21.0, 70.0 },
		.Image = TEXT("UI/SkinAccentRegions/EarLeft"),
	});

	SetSkinAccentRegionStyle(FSkinAccentRegionStyleParams
	{
		.Property = TEXT("Skin.Accents.EarRight"),
		.BrushSize = FVector2D{ 21.0, 70.0 },
		.Image = TEXT("UI/SkinAccentRegions/EarRight"),
	});

	SetSkinAccentRegionStyle(FSkinAccentRegionStyleParams
	{
		.Property = TEXT("Skin.Accents.CheekLeft"),
		.BrushSize = FVector2D{ 58.0, 90.0 },
		.Image = TEXT("UI/SkinAccentRegions/CheekLeft"),
	});

	SetSkinAccentRegionStyle(FSkinAccentRegionStyleParams
	{
		.Property = TEXT("Skin.Accents.CheekRight"),
		.BrushSize = FVector2D{ 58.0, 90.0 },
		.Image = TEXT("UI/SkinAccentRegions/CheekRight"),
	});

	SetSkinAccentRegionStyle(FSkinAccentRegionStyleParams
	{
		.Property = TEXT("Skin.Accents.Lips"),
		.BrushSize = FVector2D{ 88.0, 46.0 },
		.Image = TEXT("UI/SkinAccentRegions/Lips"),
	});

	SetSkinAccentRegionStyle(FSkinAccentRegionStyleParams
	{
		.Property = TEXT("Skin.Accents.Chin"),
		.BrushSize = FVector2D{ 85.0, 33.0 },
		.Image = TEXT("UI/SkinAccentRegions/Chin"),
	});

	// Register the thumbnails used in the legacy body selection UI
	// LegacyBody_xyz: x=height, y=BMI, z=gender
	const FVector2D Thumb128x128(128.0f, 128.0f);

	Set("Legacy.Body.f_med_nrw", new IMAGE_BRUSH("UI/Icons/f_med_nrw", Thumb128x128));
	Set("Legacy.Body.f_med_ovw", new IMAGE_BRUSH("UI/Icons/f_med_ovw", Thumb128x128));
	Set("Legacy.Body.f_med_unw", new IMAGE_BRUSH("UI/Icons/f_med_unw", Thumb128x128));
	Set("Legacy.Body.f_srt_nrw", new IMAGE_BRUSH("UI/Icons/f_srt_nrw", Thumb128x128));
	Set("Legacy.Body.f_srt_ovw", new IMAGE_BRUSH("UI/Icons/f_srt_ovw", Thumb128x128));
	Set("Legacy.Body.f_srt_unw", new IMAGE_BRUSH("UI/Icons/f_srt_unw", Thumb128x128));
	Set("Legacy.Body.f_tal_nrw", new IMAGE_BRUSH("UI/Icons/f_tal_nrw", Thumb128x128));
	Set("Legacy.Body.f_tal_ovw", new IMAGE_BRUSH("UI/Icons/f_tal_ovw", Thumb128x128));
	Set("Legacy.Body.f_tal_unw", new IMAGE_BRUSH("UI/Icons/f_tal_unw", Thumb128x128));
	Set("Legacy.Body.m_med_nrw", new IMAGE_BRUSH("UI/Icons/m_med_nrw", Thumb128x128));
	Set("Legacy.Body.m_med_ovw", new IMAGE_BRUSH("UI/Icons/m_med_ovw", Thumb128x128));
	Set("Legacy.Body.m_med_unw", new IMAGE_BRUSH("UI/Icons/m_med_unw", Thumb128x128));
	Set("Legacy.Body.m_srt_nrw", new IMAGE_BRUSH("UI/Icons/m_srt_nrw", Thumb128x128));
	Set("Legacy.Body.m_srt_ovw", new IMAGE_BRUSH("UI/Icons/m_srt_ovw", Thumb128x128));
	Set("Legacy.Body.m_srt_unw", new IMAGE_BRUSH("UI/Icons/m_srt_unw", Thumb128x128));
	Set("Legacy.Body.m_tal_nrw", new IMAGE_BRUSH("UI/Icons/m_tal_nrw", Thumb128x128));
	Set("Legacy.Body.m_tal_ovw", new IMAGE_BRUSH("UI/Icons/m_tal_ovw", Thumb128x128));
	Set("Legacy.Body.m_tal_unw", new IMAGE_BRUSH("UI/Icons/m_tal_unw", Thumb128x128));
	
	// Creating a Thumb for Animation Scrub bar
	const FVector2D ThumbImageSize = FVector2D(22.0f, 22.0f);
	const FSlateColor ThumbColor = FStyleColors::White;
	const FSlateBrush* ThumbImage = GetBrush("Viewport.AnimationBar.SliderThumb");

	Set("Viewport.AnimationBar.TimelineStyle", FSliderStyle()
		.SetNormalBarImage(FSlateColorBrush(FColor::Black))
		.SetHoveredBarImage(FSlateColorBrush(FColor::Black))
		.SetDisabledBarImage(FSlateColorBrush(FColor::Black))
		.SetNormalThumbImage(*ThumbImage)
		.SetHoveredThumbImage(*ThumbImage)
		.SetDisabledThumbImage(*ThumbImage)
		.SetBarThickness(18.0f)
	);
}

void FMetaHumanCharacterEditorStyle::SetSkinAccentRegionStyle(const FSkinAccentRegionStyleParams& InParams)
{
	const FLinearColor SelectedTint{ 0.0f, 0.162029f, 0.745404f, 1.0f }; // #0070E0FF

	const FString LineImageName = InParams.Image.ToString() + TEXT("Line");
	const FString HoverImageName = InParams.Image.ToString() + TEXT("Hover");

	const FSlateImageBrush LineBrush = IMAGE_BRUSH(LineImageName, InParams.BrushSize);
	const FSlateImageBrush HoverBrush = IMAGE_BRUSH(HoverImageName, InParams.BrushSize);
	const FSlateImageBrush SelectedBrush = IMAGE_BRUSH(HoverImageName, InParams.BrushSize, SelectedTint);

	Set(InParams.Property, FCheckBoxStyle()
		.SetCheckBoxType(ESlateCheckBoxType::ToggleButton)
		.SetUncheckedImage(LineBrush)
		.SetUncheckedHoveredImage(HoverBrush)
		.SetUncheckedPressedImage(HoverBrush)
		.SetCheckedImage(SelectedBrush)
		.SetCheckedHoveredImage(SelectedBrush)
		.SetCheckedPressedImage(SelectedBrush)
	);
}

void FMetaHumanCharacterEditorStyle::Register()
{
	FSlateStyleRegistry::RegisterSlateStyle(Get());
}

void FMetaHumanCharacterEditorStyle::Unregister()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(Get());
}

const FMetaHumanCharacterEditorStyle& FMetaHumanCharacterEditorStyle::Get()
{
	static FMetaHumanCharacterEditorStyle Inst;
	return Inst;
}