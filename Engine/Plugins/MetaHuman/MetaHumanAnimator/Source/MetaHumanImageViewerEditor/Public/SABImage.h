// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Math/Color.h"
#include "SMetaHumanImageViewer.h"
#include "MetaHumanViewportModes.h"

class UTexture;
class UTextureRenderTarget2D;
class UMaterialInstanceDynamic;

class METAHUMANIMAGEVIEWEREDITOR_API SABImage : public SMetaHumanImageViewer, public FGCObject
{
public:

#if WITH_EDITOR
	DECLARE_MULTICAST_DELEGATE(FOnInvalidate);

	FOnInvalidate& OnInvalidate() { return OnInvalidateDelegate; }
#endif

	void Setup(bool bInManageTextures);

	virtual void AddReferencedObjects(FReferenceCollector&) override;
	virtual FString GetReferencerName() const override;

	virtual FReply OnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply OnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	virtual int32 OnPaint(const FPaintArgs& InArgs, const FGeometry& InAllottedGeometry,
		const FSlateRect& InWidgetClippingRect, FSlateWindowElementList& OutDrawElements,
		int32 InLayerId, const FWidgetStyle& InWidgetStyle, bool InParentEnabled) const override;

	void SetTextures(UTexture* InTextureA, UTexture* InTextureB);
	UTextureRenderTarget2D* GetRenderTarget(EABImageViewMode InMode) const;

	void SetViewMode(EABImageViewMode InViewMode);
	EABImageViewMode GetViewMode() const;

	TArray<EABImageViewMode> SingleViewModes() const { return { EABImageViewMode::A, EABImageViewMode::B }; }
	TArray<EABImageViewMode> AllViewModes() const
	{
		return { EABImageViewMode::A, EABImageViewMode::B, EABImageViewMode::ABSide, EABImageViewMode::ABSplit };
	}
	bool IsSingleView() const { return SingleViewModes().Contains(ViewMode); }
	bool IsMultiView() const { return !IsSingleView(); }
	bool IsTextureView() const { return (IsMultiView() || NavigationMode == EABImageNavigationMode::TwoD); }

	void SetNavigationMode(EABImageNavigationMode InNavigationMode);
	EABImageNavigationMode GetNavigationMode() const;

	void AdjustZoomForFootageInDualView(double InFootageAspect) const;

	/** Reset the position of wipe mode to default */
	void ResetABWipePostion();

private:

#if WITH_EDITOR
	FOnInvalidate OnInvalidateDelegate;
#endif

	TMap<EABImageViewMode, TObjectPtr<UTextureRenderTarget2D>> RenderTarget;

	EABImageViewMode ViewMode = EABImageViewMode::A;
	EABImageNavigationMode NavigationMode = EABImageNavigationMode::TwoD;

	FVector2f Origin = FVector2f::ZeroVector;
	float Angle = 0;
	float Alpha = 0;

	const float OriginSize = 20;
	const float LineThickness = 1;
	const float PickSensitivity = 5;
	const float LabelOffset = 30;
	const float AlphaLineLength = 200;

	const FLinearColor NormalColour = FLinearColor(1, 0, 0);
	const FLinearColor HighlightedColour = FLinearColor(1, 1, 0);

	bool bOriginHighlighted = false;
	bool bOriginMove = false;
	bool bAngleHighlighted = false;
	bool bAngleMove = false;
	bool bAlphaHighlighted = false;
	bool bAlphaMove = false;

	FVector2f OriginOffset = FVector2f::ZeroVector;
	float AngleOffset = 0;

	void GetLines(const FGeometry& InGeometry, TArray<FVector2f>& OutOrigin, TArray<FVector2f>& OutAngle, TArray<FVector2f>& OutAlpha) const;
	bool HitLines(const FVector2f& InPoint, const TArray<FVector2f>& InLines) const;


protected:
	virtual void GeometryChanged() override;

private:

	TObjectPtr<UMaterialInstanceDynamic> MaterialInstance = nullptr;

	FSlateBrush Brush;

	FVector2f Get2DLocalMouse(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, EABImageMouseSide &OutMouseSide);
	EABImageMouseSide MouseSideOrig = EABImageMouseSide::NotApplicable;
};
