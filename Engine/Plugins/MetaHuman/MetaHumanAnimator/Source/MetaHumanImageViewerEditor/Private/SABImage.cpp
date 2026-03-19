// Copyright Epic Games, Inc. All Rights Reserved.

#include "SABImage.h"
#include "Math/UnrealMathUtility.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Factories/MaterialFactoryNew.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "ComponentReregisterContext.h"
#include "Fonts/FontMeasure.h"
#include "Materials/MaterialExpressionCustom.h"
#include "MetaHumanEditorSettings.h"
#include "Framework/Application/SlateApplication.h"

#include "Utils/CustomMaterialUtils.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MaterialDomain.h"
#include "UObject/Package.h"

#include "Misc/EngineVersionComparison.h"

#define LOCTEXT_NAMESPACE "MetaHuman"

#define QUARTER_PI (PI / 4)

void SABImage::Setup(bool bInManageTextures)
{
	SetImage(&Brush);
	SetNonConstBrush(&Brush);

	UMetaHumanEditorSettings* Settings = GetMutableDefault<UMetaHumanEditorSettings>();
	Settings->OnSettingsChanged.AddSP(this, &SABImage::GeometryChanged);

	OnGeometryChanged.AddRaw(this, &SABImage::GeometryChanged);

	TObjectPtr<UMaterialFactoryNew> MaterialFactory = NewObject<UMaterialFactoryNew>();

	// Material for AB view
	TObjectPtr<UMaterial> Material = (UMaterial*)MaterialFactory->FactoryCreateNew(UMaterial::StaticClass(), GetTransientPackage(), TEXT("ABView_Material"), RF_Standalone | RF_Public, NULL, GWarn);
	FAssetRegistryModule::AssetCreated(Material);

	TObjectPtr<UMaterialExpressionCustom> CustomNode = NewObject<UMaterialExpressionCustom>(Material);

	CustomMaterialUtils::AddInput<UMaterialExpressionScalarParameter>("ViewMode", Material, CustomNode);
	CustomMaterialUtils::AddInput<UMaterialExpressionScalarParameter>("NavigationMode", Material, CustomNode);
	CustomMaterialUtils::AddInput<UMaterialExpressionTextureObjectParameter>("MovieA", Material, CustomNode, false);
	CustomMaterialUtils::AddInput<UMaterialExpressionTextureObjectParameter>("MovieB", Material, CustomNode, false);
	CustomMaterialUtils::AddInput<UMaterialExpressionTextureCoordinate>("TexCoord", Material, CustomNode);
	CustomMaterialUtils::AddInput<UMaterialExpressionScalarParameter>("X", Material, CustomNode);
	CustomMaterialUtils::AddInput<UMaterialExpressionScalarParameter>("Y", Material, CustomNode);
	CustomMaterialUtils::AddInput<UMaterialExpressionScalarParameter>("AngleCos", Material, CustomNode);
	CustomMaterialUtils::AddInput<UMaterialExpressionScalarParameter>("AngleSin", Material, CustomNode);
	CustomMaterialUtils::AddInput<UMaterialExpressionScalarParameter>("Alpha", Material, CustomNode);
	CustomMaterialUtils::AddInput<UMaterialExpressionScalarParameter>("XMin", Material, CustomNode);
	CustomMaterialUtils::AddInput<UMaterialExpressionScalarParameter>("XMax", Material, CustomNode);
	CustomMaterialUtils::AddInput<UMaterialExpressionScalarParameter>("YMin", Material, CustomNode);
	CustomMaterialUtils::AddInput<UMaterialExpressionScalarParameter>("YMax", Material, CustomNode);

	const FString Code = R"|(
float2 UV = TexCoord;

float4 Result;

if (ViewMode > -0.1 && ViewMode < 0.1) // A
{
	Result = MovieA.Sample(MovieASampler, UV);
}
else if (ViewMode > 0.9 && ViewMode < 1.1) // B
{
	Result = MovieB.Sample(MovieBSampler, UV);
}
else if (ViewMode > 1.9 && ViewMode < 2.1) // AB split
{
	// The X and Y position of the split in clip, not widget, UV space ie account for pan and zoom
	float ClipX = XMin + X * (XMax - XMin);
	float ClipY = YMin + Y * (YMax - YMin);

	// Create 2 vectors both centered on the split position origin. The first vector is the split line
	// orientation, the second vector is the UV sample postion. The sign of the z component of the cross product
	// of these defines which side of the AB line the UV sample is on.
	bool bOnASide = cross(float3(AngleCos, AngleSin, 0), float3(UV.x - ClipX, UV.y - ClipY, 0)).z < 0;

	float4 MovieASample = MovieA.Sample(MovieASampler, UV);

	if (bOnASide)
	{
		Result = MovieASample;
	}
	else
	{
		float4 MovieBSample = MovieB.Sample(MovieBSampler, UV);

		Result[0] = (MovieASample[0] * Alpha) + (MovieBSample[0] * (1 - Alpha));
		Result[1] = (MovieASample[1] * Alpha) + (MovieBSample[1] * (1 - Alpha));
		Result[2] = (MovieASample[2] * Alpha) + (MovieBSample[2] * (1 - Alpha));
	}
}
else if (ViewMode > 2.9 && ViewMode < 3.1) // AB side-by-side
{
	float2 WidgetUV;
	WidgetUV.x = (UV.x - XMin) / (XMax - XMin);
	WidgetUV.y = (UV.y - YMin) / (YMax - YMin);

	bool bOnASide = WidgetUV.x < 0.5;

	float2 ClipUV;
	if (bOnASide)
	{
		ClipUV.x = XMin + (WidgetUV.x * 2) * (XMax - XMin);
	}
	else
	{
		ClipUV.x = XMin + ((WidgetUV.x - 0.5) * 2) * (XMax - XMin);
	}
	ClipUV.y = YMin + ((WidgetUV.y - 0.25) * 2) * (YMax - YMin);

	if (ClipUV.x > 0 && ClipUV.x < 1 && ClipUV.y > 0 && ClipUV.y < 1)
	{
		if (bOnASide)
		{
			Result = MovieA.Sample(MovieASampler, ClipUV);
		}
		else
		{
			Result = MovieB.Sample(MovieBSampler, ClipUV);
		}
	}
	else
	{
		Result = float4(0, 0, 0, 0);
	}
}

return Result;
	)|";

	CustomNode->Code = Code;
	Material->SetShadingModel(EMaterialShadingModel::MSM_Unlit);

#if UE_VERSION_NEWER_THAN(5, 0, ENGINE_PATCH_VERSION)
	Material->GetExpressionCollection().AddExpression(CustomNode);
	Material->GetEditorOnlyData()->EmissiveColor.Expression = CustomNode;
#else
	Material->Expressions.Add(CustomNode);
	Material->EmissiveColor.Expression = CustomNode;
#endif

	Material->MaterialDomain = EMaterialDomain::MD_UI;

	Material->PreEditChange(nullptr);
	Material->PostEditChange();

	MaterialInstance = UMaterialInstanceDynamic::Create(Material, nullptr);

	if (bInManageTextures)
	{
		for (EABImageViewMode Mode : SingleViewModes())
		{
			RenderTarget.Add(Mode, NewObject<UTextureRenderTarget2D>());
			RenderTarget[Mode]->InitAutoFormat(256, 256);
			RenderTarget[Mode]->UpdateResourceImmediate();
		}
	}

	MaterialInstance->SetScalarParameterValue(FName("ViewMode"), (int32) ViewMode);
	MaterialInstance->SetScalarParameterValue(FName("NavigationMode"), (int32) NavigationMode);

	if (!RenderTarget.IsEmpty())
	{
		MaterialInstance->SetTextureParameterValue(FName("MovieA"), RenderTarget[EABImageViewMode::A]);
		MaterialInstance->SetTextureParameterValue(FName("MovieB"), RenderTarget[EABImageViewMode::B]);
	}

	MaterialInstance->SetScalarParameterValue(FName("X"), Origin.X);
	MaterialInstance->SetScalarParameterValue(FName("Y"), Origin.Y);
	MaterialInstance->SetScalarParameterValue(FName("AngleCos"), FMath::Cos(Angle));
	MaterialInstance->SetScalarParameterValue(FName("AngleSin"), FMath::Sin(Angle));
	MaterialInstance->SetScalarParameterValue(FName("Alpha"), Alpha);

	MaterialInstance->SetScalarParameterValue(FName("XMin"), 0);
	MaterialInstance->SetScalarParameterValue(FName("XMax"), 1);
	MaterialInstance->SetScalarParameterValue(FName("YMin"), 0);
	MaterialInstance->SetScalarParameterValue(FName("YMax"), 1);

	// Lambda that reacts to inputs in the image viewer, used for zooming and panning
	OnViewChanged.AddLambda([this](FBox2f InUV)
		{
			Brush.SetUVRegion(InUV);

			MaterialInstance->SetScalarParameterValue(FName("XMin"), InUV.Min.X);
			MaterialInstance->SetScalarParameterValue(FName("XMax"), InUV.Max.X);
			MaterialInstance->SetScalarParameterValue(FName("YMin"), InUV.Min.Y);
			MaterialInstance->SetScalarParameterValue(FName("YMax"), InUV.Max.Y);
		});

	ResetABWipePostion();
	SMetaHumanImageViewer::ResetView();
}

void SABImage::AddReferencedObjects(FReferenceCollector& InCollector)
{
	InCollector.AddReferencedObject(MaterialInstance);
}

FString SABImage::GetReferencerName() const
{
	return TEXT("SABImage");
}

FReply SABImage::OnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	FReply Reply = FReply::Unhandled();

	if (bOriginMove || bAngleMove || bAlphaMove)
	{
		// Ignore down event if already manipulating  
	}
	else if (ViewMode == EABImageViewMode::ABSplit && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		TArray<FVector2f> OriginLines, AngleLines, AlphaLines;
		GetLines(InGeometry, OriginLines, AngleLines, AlphaLines);

		const FVector2f LocalMouse = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());

		FVector2f UVMouse;
		UVMouse.X = LocalMouse.X / InGeometry.GetLocalSize().X;
		UVMouse.Y = LocalMouse.Y / InGeometry.GetLocalSize().Y;

		bOriginMove = false;
		bAngleMove = false;
		bAlphaMove = false;

		if (HitLines(LocalMouse, OriginLines) ||
			(LocalMouse.ComponentwiseAllGreaterThan(OriginLines[0]) &&
			 LocalMouse.ComponentwiseAllLessThan(OriginLines[2])))
		{
			bOriginMove = true;

			OriginOffset = Origin - UVMouse;
		}
		else if (HitLines(LocalMouse, AngleLines))
		{
			bAngleMove = true;

			const FVector2f Vector = UVMouse - Origin;

			AngleOffset = Angle - FMath::Atan2(Vector.Y, Vector.X);
		}
		else if (HitLines(LocalMouse, AlphaLines))
		{
			bAlphaMove = true;
		}

		if (bOriginMove || bAngleMove || bAlphaMove)
		{
			Reply = FReply::Handled();
		}
	}
	else if (NavigationMode == EABImageNavigationMode::TwoD)
	{
		Reply = HandleMouseButtonDown(InGeometry, Get2DLocalMouse(InGeometry, InMouseEvent, MouseSideOrig), InMouseEvent.GetEffectingButton());
	}

	if (Reply.IsEventHandled())
	{
		Reply.CaptureMouse(SharedThis(this));
	}

	return Reply;
}

FReply SABImage::OnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	FReply Reply = FReply::Unhandled();

	if (bOriginMove || bAngleMove || bAlphaMove)
	{
		if (ViewMode == EABImageViewMode::ABSplit && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
		{
			bOriginMove = false;
			bAngleMove = false;
			bAlphaMove = false;

			Reply = FReply::Handled();
		}
	}
	else if (NavigationMode == EABImageNavigationMode::TwoD)
	{
		Reply = HandleMouseButtonUp(InGeometry, Get2DLocalMouse(InGeometry, InMouseEvent, MouseSideOrig), InMouseEvent.GetEffectingButton());
	}

	if (Reply.IsEventHandled())
	{
		Reply.ReleaseMouseCapture();
	}

	return Reply;
}

FReply SABImage::OnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	FReply Reply = FReply::Unhandled();

	if (ViewMode == EABImageViewMode::ABSplit && !bIsPanning)
	{
		const FVector2f LocalMouse = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());

		FVector2f UVMouse;
		UVMouse.X = LocalMouse.X / InGeometry.GetLocalSize().X;
		UVMouse.Y = LocalMouse.Y / InGeometry.GetLocalSize().Y;

		if (bOriginMove)
		{
			Origin = UVMouse + OriginOffset;

			MaterialInstance->SetScalarParameterValue(FName("X"), Origin.X);
			MaterialInstance->SetScalarParameterValue(FName("Y"), Origin.Y);
		}
		else if (bAngleMove)
		{
			const FVector2f LocalOrigin(Origin.X * InGeometry.GetLocalSize().X, Origin.Y * InGeometry.GetLocalSize().Y);
			if ((LocalOrigin - LocalMouse).SizeSquared() > 400) // Disallow rotation when too close to pivot point
			{
				const FVector2f Vector = UVMouse - Origin;

				Angle = FMath::Atan2(Vector.Y, Vector.X) + AngleOffset;

				MaterialInstance->SetScalarParameterValue(FName("AngleCos"), FMath::Cos(Angle));
				MaterialInstance->SetScalarParameterValue(FName("AngleSin"), FMath::Sin(Angle));
			}
		}
		else if (bAlphaMove)
		{
			const FVector2f LocalOrigin(Origin.X * InGeometry.GetLocalSize().X, Origin.Y * InGeometry.GetLocalSize().Y);
			if ((LocalOrigin - LocalMouse).SizeSquared() > 400) // Disallow rotation when too close to pivot point
			{
				const FVector2f Vector = UVMouse - Origin;

				float AlphaAngle = FMath::Atan2(Vector.Y, Vector.X);

				FVector ZeroAlphaLine = FVector(FMath::Cos(Angle + QUARTER_PI), FMath::Sin(Angle + QUARTER_PI), 0); // alpha=0 line position
				FVector AlphaLine = FVector(FMath::Cos(AlphaAngle), FMath::Sin(AlphaAngle), 0); // Current alpha line position

				if (FVector::CrossProduct(ZeroAlphaLine, AlphaLine).Z > 0) // only allow an alpha line movement in one half space
				{
					AlphaAngle = FMath::Clamp(FMath::Acos(FVector::DotProduct(ZeroAlphaLine, AlphaLine)), 0, HALF_PI);

					Alpha = AlphaAngle / HALF_PI;
					MaterialInstance->SetScalarParameterValue(FName("Alpha"), Alpha);
				}
			}
		}
		else
		{
			TArray<FVector2f> OriginLines, AngleLines, AlphaLines;
			GetLines(InGeometry, OriginLines, AngleLines, AlphaLines);

			bOriginHighlighted = false;
			bAngleHighlighted = false;
			bAlphaHighlighted = false;

			if (HitLines(LocalMouse, OriginLines) || 
				(LocalMouse.ComponentwiseAllGreaterThan(OriginLines[0]) &&
				 LocalMouse.ComponentwiseAllLessThan(OriginLines[2])))
			{
				bOriginHighlighted = true;
			}
			else if (HitLines(LocalMouse, AngleLines))
			{
				bAngleHighlighted = true;
			}
			else if (HitLines(LocalMouse, AlphaLines))
			{
				bAlphaHighlighted = true;
			}
		}

		if (bOriginMove || bAngleMove || bAlphaMove || bOriginHighlighted || bAngleHighlighted || bAlphaHighlighted)
		{
			Reply = FReply::Handled();
		}
	}

	if (!Reply.IsEventHandled() && NavigationMode == EABImageNavigationMode::TwoD)
	{
		EABImageMouseSide CurrentMouseSide; // Mouse movements only valid on same side of AB split line as when the mouse button was pressed
		const FVector2f LocalMouse = Get2DLocalMouse(InGeometry, InMouseEvent, CurrentMouseSide);
		if (CurrentMouseSide == MouseSideOrig)
		{ 
			Reply = HandleMouseMove(InGeometry, LocalMouse);
		}
		else
		{
			Reply = FReply::Handled();
		}
	}

#if WITH_EDITOR
	if (Reply.IsEventHandled())
	{
		OnInvalidateDelegate.Broadcast();
	}
#endif

	return Reply;
}

FReply SABImage::OnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (NavigationMode == EABImageNavigationMode::TwoD)
	{
		return HandleMouseWheel(InGeometry, Get2DLocalMouse(InGeometry, InMouseEvent, MouseSideOrig), InMouseEvent.GetWheelDelta());
	}
	else
	{
		return FReply::Unhandled();
	}
}

FReply SABImage::OnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) 
{ 
	if (NavigationMode == EABImageNavigationMode::TwoD)
	{
		return FReply::Handled();
	}
	else
	{
		return FReply::Unhandled();
	}
}

void SABImage::ResetABWipePostion()
{
	Origin = FVector2f(0.5, 0.5);
	Angle = -HALF_PI;
	Alpha = 0;

	MaterialInstance->SetScalarParameterValue(FName("X"), Origin.X);
	MaterialInstance->SetScalarParameterValue(FName("Y"), Origin.Y);
	MaterialInstance->SetScalarParameterValue(FName("AngleCos"), FMath::Cos(Angle));
	MaterialInstance->SetScalarParameterValue(FName("AngleSin"), FMath::Sin(Angle));
	MaterialInstance->SetScalarParameterValue(FName("Alpha"), Alpha);

	MaterialInstance->SetScalarParameterValue(FName("XMin"), 0);
	MaterialInstance->SetScalarParameterValue(FName("XMax"), 1);
	MaterialInstance->SetScalarParameterValue(FName("YMin"), 0);
	MaterialInstance->SetScalarParameterValue(FName("YMax"), 1);
}

int32 SABImage::OnPaint(const FPaintArgs& InArgs, const FGeometry& InAllottedGeometry,
	const FSlateRect& InWidgetClippingRect, FSlateWindowElementList& OutDrawElements,
	int32 InLayerId, const FWidgetStyle& InWidgetStyle, bool InParentEnabled) const
{
	if (InAllottedGeometry != Geometry)
	{
		Geometry = InAllottedGeometry;
		OnGeometryChanged.Broadcast();
	}

	int32 LayerId = InLayerId;

	if (IsTextureView())
	{
		LayerId++;
		LayerId = SMetaHumanImageViewer::OnPaint(InArgs, InAllottedGeometry, InWidgetClippingRect, OutDrawElements, LayerId, InWidgetStyle, InParentEnabled);
	}

	if (ViewMode == EABImageViewMode::ABSplit)
	{
		TArray<FVector2f> OriginLines, AngleLines, AlphaLines;
		GetLines(InAllottedGeometry, OriginLines, AngleLines, AlphaLines);

		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, InAllottedGeometry.ToPaintGeometry(), OriginLines, ESlateDrawEffect::None, bOriginHighlighted ? HighlightedColour : NormalColour, true, LineThickness);
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, InAllottedGeometry.ToPaintGeometry(), AngleLines, ESlateDrawEffect::None, bAngleHighlighted ? HighlightedColour : NormalColour, true, LineThickness);
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, InAllottedGeometry.ToPaintGeometry(), AlphaLines, ESlateDrawEffect::None, bAlphaHighlighted ? HighlightedColour : NormalColour, true, LineThickness);

		const FSlateFontInfo Font(FCoreStyle::GetDefaultFont(), 10, "Regular");

		const FVector2f LocalOrigin = FVector2f(Origin.X * InAllottedGeometry.GetLocalSize().X, Origin.Y * InAllottedGeometry.GetLocalSize().Y);

		float ALabelAngle = Angle + HALF_PI;
		FVector2f ALabelLine = FVector2f(FMath::Cos(ALabelAngle), FMath::Sin(ALabelAngle));
		ALabelLine *= LabelOffset;

		const FText ALabel(LOCTEXT("ALabel", "A"));
		const FVector2f ALabelSize = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(ALabel, Font, 1.0);
		const FGeometry ALabelGeom = InAllottedGeometry.MakeChild(LocalOrigin - ALabelLine - (ALabelSize / 2), ALabelSize);
		FSlateDrawElement::MakeText(OutDrawElements, LayerId, ALabelGeom.ToPaintGeometry(), ALabel, Font, ESlateDrawEffect::None, NormalColour);

		float BLabelAngle = Angle - HALF_PI;
		FVector2f BLabelLine = FVector2f(FMath::Cos(BLabelAngle), FMath::Sin(BLabelAngle));
		BLabelLine *= LabelOffset;

		const FText BLabel(LOCTEXT("BLabel", "B"));
		const FVector2f BLabelSize = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(BLabel, Font, 1.0);
		const FGeometry BLabelGeom = InAllottedGeometry.MakeChild(LocalOrigin - BLabelLine - (BLabelSize / 2), BLabelSize);
		FSlateDrawElement::MakeText(OutDrawElements, LayerId, BLabelGeom.ToPaintGeometry(), BLabel, Font, ESlateDrawEffect::None, NormalColour);
	}
	else if (ViewMode == EABImageViewMode::ABSide)
	{
		TArray<FVector2f> CenterLines;

		CenterLines.Add(FVector2f(InAllottedGeometry.GetLocalSize().X / 2, -10));
		CenterLines.Add(FVector2f(InAllottedGeometry.GetLocalSize().X / 2, InAllottedGeometry.GetLocalSize().Y + 10));

		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, InAllottedGeometry.ToPaintGeometry(), CenterLines, ESlateDrawEffect::None, NormalColour, true, LineThickness);
	}

	return LayerId;
}

void SABImage::SetTextures(UTexture* InTextureA, UTexture* InTextureB)
{
	check(RenderTarget.IsEmpty());

	MaterialInstance->SetTextureParameterValue(FName("MovieA"), InTextureA);
	MaterialInstance->SetTextureParameterValue(FName("MovieB"), InTextureB);
}

UTextureRenderTarget2D* SABImage::GetRenderTarget(EABImageViewMode InMode) const 
{ 
	check(RenderTarget.Contains(InMode));

	return RenderTarget[InMode];
}

void SABImage::SetViewMode(EABImageViewMode InViewMode)
{
	if (InViewMode != ViewMode)
	{
		ViewMode = InViewMode;

		SetDrawBlanking(ViewMode != EABImageViewMode::ABSide);

		MaterialInstance->SetScalarParameterValue(FName("ViewMode"), (int32)ViewMode);
	}
}

EABImageViewMode SABImage::GetViewMode() const
{
	return ViewMode;
}

void SABImage::SetNavigationMode(EABImageNavigationMode InNavigationMode)
{
	if (InNavigationMode != NavigationMode)
	{
		NavigationMode = InNavigationMode;

		MaterialInstance->SetScalarParameterValue(FName("NavigationMode"), (int32)NavigationMode);
	}
}

EABImageNavigationMode SABImage::GetNavigationMode() const
{
	return NavigationMode;
}

void SABImage::GetLines(const FGeometry& InGeometry, TArray<FVector2f>& OutOrigin, TArray<FVector2f>& OutAngle, TArray<FVector2f>& OutAlpha) const
{
	const float HalfOriginSize = OriginSize / 2;

	const FVector2f LocalOrigin = FVector2f(Origin.X * InGeometry.GetLocalSize().X, Origin.Y * InGeometry.GetLocalSize().Y);

	OutOrigin.Add(LocalOrigin + FVector2f(-HalfOriginSize, -HalfOriginSize));
	OutOrigin.Add(LocalOrigin + FVector2f(HalfOriginSize, -HalfOriginSize));
	OutOrigin.Add(LocalOrigin + FVector2f(HalfOriginSize, HalfOriginSize));
	OutOrigin.Add(LocalOrigin + FVector2f(-HalfOriginSize, HalfOriginSize));
	OutOrigin.Add(LocalOrigin + FVector2f(-HalfOriginSize, -HalfOriginSize));

	FVector2f AngleLine = FVector2f(FMath::Cos(Angle) * InGeometry.GetLocalSize().X, FMath::Sin(Angle) * InGeometry.GetLocalSize().Y);
	AngleLine *= 10; // To oversize it to an offscreen position

	OutAngle.Add(LocalOrigin - AngleLine);
	OutAngle.Add(LocalOrigin + AngleLine);

	const float AlphaAngle = (Angle + QUARTER_PI) + (Alpha * HALF_PI);

	FVector2f AlphaLine = FVector2f(FMath::Cos(AlphaAngle) * InGeometry.GetLocalSize().X, FMath::Sin(AlphaAngle) * InGeometry.GetLocalSize().Y);
	AlphaLine.Normalize();
	AlphaLine *= AlphaLineLength;

	OutAlpha.Add(LocalOrigin);
	OutAlpha.Add(LocalOrigin + AlphaLine);
}

bool SABImage::HitLines(const FVector2f& InPoint, const TArray<FVector2f>& InLines) const
{
	for (int32 Index = 1; Index < InLines.Num(); ++Index)
	{
		const FVector2D ClosestPoint = FMath::ClosestPointOnSegment2D(
			FVector2D(InPoint), FVector2D(InLines[Index - 1]), FVector2D(InLines[Index]));
		const float Dist = (FVector2D(InPoint) - ClosestPoint).Size();

		if (Dist < PickSensitivity)
		{
			return true;
		}
	}

	return false;
}

void SABImage::GeometryChanged()
{
	const FVector2f WidgetGeom = GetCachedGeometry().GetLocalSize();

	Brush.SetImageSize(WidgetGeom);

	Brush.SetUVRegion(FBox2f{ FVector2f{ 0.0f, 0.0f }, FVector2f{ 1.0f, 1.0f } });

	MaterialInstance->SetScalarParameterValue(FName("XMin"), 0);
	MaterialInstance->SetScalarParameterValue(FName("XMax"), 1);
	MaterialInstance->SetScalarParameterValue(FName("YMin"), 0);
	MaterialInstance->SetScalarParameterValue(FName("YMax"), 1);

	Brush.SetResourceObject(MaterialInstance);

	// Resize render target. Use a size greater than widget size to allow for some level of zoom without pixelatting.

	UMetaHumanEditorSettings* Settings = GetMutableDefault<UMetaHumanEditorSettings>();

	float OverSample = Settings->SampleCount;

	if (WidgetGeom.X * OverSample > Settings->MaximumResolution)
	{
		OverSample = Settings->MaximumResolution / WidgetGeom.X;
	}

	if (WidgetGeom.Y * OverSample > Settings->MaximumResolution)
	{
		OverSample = Settings->MaximumResolution / WidgetGeom.Y;
	}

	for (auto Target : RenderTarget)
	{
		Target.Value->ResizeTarget(FMath::Max(1, WidgetGeom.X * OverSample), FMath::Max(1, WidgetGeom.Y * OverSample));
	}

	SMetaHumanImageViewer::GeometryChanged();
}

FVector2f SABImage::Get2DLocalMouse(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, EABImageMouseSide& OutMouseSide)
{
	// Account for the fact that in 2D side-by-side mode the effective mouse position is mapped to either half of the widget

	OutMouseSide = EABImageMouseSide::NotApplicable;

	FVector2f LocalMouse = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());

	if (ViewMode == EABImageViewMode::ABSide)
	{
		const FVector2f LocalSize = InGeometry.GetLocalSize();

		if (LocalMouse.X < LocalSize.X / 2)
		{
			OutMouseSide = EABImageMouseSide::A;
		}
		else
		{
			OutMouseSide = EABImageMouseSide::B;
			LocalMouse.X -= LocalSize.X / 2;
		}

		LocalMouse.X *= 2;
		LocalMouse.Y -= LocalSize.Y / 4;
		LocalMouse.Y *= 2;
	}

	return LocalMouse;
}

void SABImage::AdjustZoomForFootageInDualView(double InFootageAspect) const
{
	// The image displayed by this class fills the widget size.
	// However, the image is not the footage - when in a textured based view the image
	// can be the footage plus black blanking on the edge. This blanking does not need
	// to be visible on screen, and in fact zooming the image can produce a better fit 
	// for the footage portion of the image when taking into account the smaller effective
	// size of the A and B sides (the effective width of the A and B sides is half the 
	// widget width.

	check(ViewMode == EABImageViewMode::ABSide);

	const FVector2f WidgetGeom = GetCachedGeometry().GetLocalSize();

	if (WidgetGeom.X > 0)
	{
		double FullWidgetAspect = WidgetGeom.Y / WidgetGeom.X;
		double DualWidgetAspect = WidgetGeom.Y / (WidgetGeom.X / 2);

		if (InFootageAspect > FullWidgetAspect) // Footage was fit to height in full widget
		{
			double ScaleFactor = 1;

			if (InFootageAspect < DualWidgetAspect) // Now it needs to be fit to width in the half size widget
			{
				ScaleFactor = DualWidgetAspect / InFootageAspect;
			}

			ScaleFactor /= 2; // To account for dual view scaling

			OnViewChanged.Broadcast(FBox2f(FVector2f(0.5 - ScaleFactor / 2, 0.5 - ScaleFactor / 2), FVector2f(0.5 + ScaleFactor / 2, 0.5 + ScaleFactor / 2)));
		}
	}
}

#undef LOCTEXT_NAMESPACE
#undef QUARTER_PI
