// Copyright Epic Games, Inc. All Rights Reserved.

#include "EditorDragTools/EditorFrustumSelect.h"

#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Components/PrimitiveComponent.h"
#include "Editor/UnrealEdEngine.h"
#include "EditorDragTools/EditorViewportClientProxy.h"
#include "EditorModeManager.h"
#include "EditorModes.h"
#include "Elements/Framework/EngineElementsLibrary.h"
#include "Elements/Framework/TypedElementRegistry.h"
#include "Elements/Framework/TypedElementSelectionSet.h"
#include "Elements/Interfaces/TypedElementWorldInterface.h"
#include "EngineUtils.h"
#include "GameFramework/Volume.h"
#include "LevelEditorSubsystem.h"
#include "LevelEditorViewport.h"
#include "Model.h"
#include "SceneView.h"
#include "ScopedTransaction.h"
#include "Settings/LevelEditorViewportSettings.h"
#include "SnappingUtils.h"
#include "Subsystems/BrushEditingSubsystem.h"
#include "UnrealEdGlobals.h"

namespace UE::EditorDragTools::Private
{
TArray<FTypedElementHandle> GetElementsIntersectingFrustum(
	const AActor* Actor,
	const FConvexVolume& InFrustum,
	const IEditorViewportClientProxy* InEditorViewportClientProxy,
	const FWorldSelectionElementArgs& SelectionArgs
)
{
	if (InEditorViewportClientProxy && InEditorViewportClientProxy->IsActorVisible(Actor))
	{
		if (FTypedElementHandle ActorHandle = UEngineElementsLibrary::AcquireEditorActorElementHandle(Actor))
		{
			if (TTypedElement<ITypedElementWorldInterface> WorldElement =
					UTypedElementRegistry::GetInstance()->GetElement<ITypedElementWorldInterface>(ActorHandle))
			{
				return WorldElement.GetSelectionElementsInConvexVolume(InFrustum, SelectionArgs);
			}
		}
	}

	return {};
}

} // namespace UE::EditorDragTools::Private

void FEditorFrustumSelect::Render(const FSceneView* View, FCanvas* Canvas)
{
	if (!Canvas || !View)
	{
		return;
	}

	FCanvasBoxItem BoxItem(
		FVector2D(Start.X, Start.Y) / Canvas->GetDPIScale(),
		FVector2D(End.X - Start.X, End.Y - Start.Y) / Canvas->GetDPIScale()
	);

	BoxItem.SetColor(FLinearColor::White);
	Canvas->DrawItem(BoxItem);
}

FInputRayHit FEditorFrustumSelect::CanBeginClickDragSequence(const FInputDeviceRay& InPressPos)
{
	// Todo: this could be retrieved from a command for customization
	FInputChord ActivationChord(EModifierKey::Control | EModifierKey::Alt, EKeys::LeftMouseButton);

	return IsActivationChordPressed(ActivationChord)
				&& IsCurrentModeSupported()
			 ? FInputRayHit(TNumericLimits<float>::Max()) // bHit is true. Depth is max to lose the standard tiebreaker.
			 : FInputRayHit();
}

void FEditorFrustumSelect::OnClickPress(const FInputDeviceRay& InPressPos)
{
	// Signal that this tool is now active
	OnActivateTool().Broadcast();

	Start = InPressPos.WorldRay.Origin;
	bIsDragging = true;

	// Snap to constraints.
	if (bUseSnapping)
	{
		const float GridSize = GEditor->GetGridSize();
		const FVector GridBase(GridSize, GridSize, GridSize);
		FSnappingUtils::SnapPointToGrid(Start, GridBase);
	}
	End = Start;

	// Remove any active hover objects
	FLevelEditorViewportClient::ClearHoverFromObjects();

	Start = FVector(InPressPos.ScreenPosition.X, InPressPos.ScreenPosition.Y, 0);
	End = Start;
}

void FEditorFrustumSelect::OnClickDrag(const FInputDeviceRay& InDragPos)
{
	End = FVector(InDragPos.ScreenPosition.X, InDragPos.ScreenPosition.Y, 0);
}

void FEditorFrustumSelect::OnClickRelease(const FInputDeviceRay& InReleasePos)
{
	End = FVector(InReleasePos.ScreenPosition.X, InReleasePos.ScreenPosition.Y, 0);

	if (!EditorViewportClientProxy)
	{
		return;
	}

	FEditorViewportClient* const EditorViewportClient = EditorViewportClientProxy->GetEditorViewportClient();
	if (!EditorViewportClient)
	{
		return;
	}

	FViewport* Viewport = EditorViewportClient->Viewport;
	if (!Viewport)
	{
		return;
	}

	UBrushEditingSubsystem* BrushSubsystem = GEditor->GetEditorSubsystem<UBrushEditingSubsystem>();
	const bool bGeometryMode = BrushSubsystem ? BrushSubsystem->IsGeometryEditorModeActive() : false;

	FSceneViewFamilyContext ViewFamily(
		FSceneViewFamily::ConstructionValues(Viewport, EditorViewportClient->GetScene(), EditorViewportClient->EngineShowFlags)
	);
	FSceneView* SceneView = EditorViewportClient->CalcSceneView(&ViewFamily);

	// Generate a frustum out of the dragged box
	FConvexVolume Frustum;
	CalculateFrustum(SceneView, Frustum, true);

	FScopedTransaction Transaction(NSLOCTEXT("ActorFrustumSelect", "MarqueeSelectTransaction", "Marquee Select"));

	if (!FInputDeviceState::IsShiftKeyDown(InputState))
	{
		// If the user is selecting, but isn't hold down SHIFT, remove all current selections.
		ModeTools->SelectNone();
	}

	// Does an actor have to be fully contained in the box to be selected
	const bool bStrictDragSelection = GetDefault<ULevelEditorViewportSettings>()->bStrictBoxSelection;

	constexpr bool bShouldSelect = true; // bLeftMouseButtonDown
	// Let the editor mode try to handle the selection.
	const bool bEditorModeHandledSelection = ModeTools->FrustumSelect(Frustum, EditorViewportClient, bShouldSelect);

	// Let the component visualizers try to handle the selection.
	const bool bComponentVisHandledSelection =
		!bEditorModeHandledSelection
		&& GUnrealEd->ComponentVisManager.HandleFrustumSelect(Frustum, EditorViewportClient, Viewport);

	if (!bEditorModeHandledSelection && !bComponentVisHandledSelection)
	{
		UTypedElementSelectionSet* SelectionSet = GEditor->GetEditorSubsystem<ULevelEditorSubsystem>()->GetSelectionSet();
		SelectionSet->Modify();

		FWorldSelectionElementArgs SeletionArgs{ SelectionSet,
												 ETypedElementSelectionMethod::Primary,
												 FTypedElementSelectionOptions(),
												 &(EditorViewportClient->EngineShowFlags),
												 bStrictDragSelection,
												 bGeometryMode };

		const int32 ViewportSizeX = Viewport->GetSizeXY().X;
		const int32 ViewportSizeY = Viewport->GetSizeXY().Y;

		if (Start.X > End.X)
		{
			Swap(Start.X, End.X);
		}

		if (Start.Y > End.Y)
		{
			Swap(Start.Y, End.Y);
		}

		TArray<FTypedElementHandle> ElementsToSelect;
		const bool bTransparentBoxSelection = GetDefault<ULevelEditorViewportSettings>()->bTransparentBoxSelection;
		if (bTransparentBoxSelection)
		{
			// Get a list of frustum-culled actors
			for (FActorIterator It(EditorViewportClient->GetWorld()); It; ++It)
			{
				AActor* Actor = *It;
				ElementsToSelect.Append(
					UE::EditorDragTools::Private::GetElementsIntersectingFrustum(
						Actor, Frustum, EditorViewportClientProxy, SeletionArgs
					)
				);
			}
		}
		else
		{
			// Extend the endpoint of the rect to get the actual line

			const int32 MinX = UE::LWC::FloatToIntCastChecked<int32>(FMath::Max<double>(0.0, Start.X));
			const int32 MinY = UE::LWC::FloatToIntCastChecked<int32>(FMath::Max<double>(0.0, Start.Y));
			const int32 MaxX = FMath::Min(ViewportSizeX, FMath::TruncToInt32(End.X + 1.0));
			const int32 MaxY = FMath::Min(ViewportSizeY, FMath::TruncToInt32(End.Y + 1.0));

			const FIntPoint Min{ MinX, MinY };
			const FIntPoint Max{ MaxX, MaxY };
			const FIntRect BoxRect{ Min, Max };

			// Typed Element selection
			{
				FTypedElementListRef ElementList = UTypedElementRegistry::GetInstance()->CreateElementList();
				Viewport->GetElementHandlesInRect(BoxRect, ElementList);

				if (bStrictDragSelection)
				{
					ElementList->ForEachElement<ITypedElementWorldInterface>(
						[bStrictDragSelection, &Frustum, &SelectionSet, &ElementsToSelect](
							const TTypedElement<ITypedElementWorldInterface>& InElement
						)
						{
							if (InElement.IsElementInConvexVolume(Frustum, bStrictDragSelection))
							{
								ElementsToSelect.Add(
									SelectionSet->GetSelectionElement(InElement, ETypedElementSelectionMethod::Primary)
								);
							}

							return true;
						}
					);
				}
				else
				{
					// Grab only the selectable handles (this remove the components from the selection and select the actor instead)
					ElementList->ForEachElementHandle(
						[&SelectionSet, &ElementsToSelect](const FTypedElementHandle& InHandle)
						{
							ElementsToSelect.Add(
								SelectionSet->GetSelectionElement(InHandle, ETypedElementSelectionMethod::Primary)
							);
							return true;
						}
					);
				}
			}

			// We need this old code to support the BSP
			TSet<AActor*> BSPActors;
			TSet<UModel*> HitModels;
			Viewport->GetActorsAndModelsInHitProxy(BoxRect, BSPActors, HitModels);
			BSPActors.Empty(HitModels.Num());

			if (HitModels.Num() > 0)
			{
				// Check every model to see if its BSP surfaces should be selected
				for (auto It = HitModels.CreateConstIterator(); It; ++It)
				{
					UModel& Model = **It;
					// Check every node in the model
					for (int32 NodeIndex = 0; NodeIndex < Model.Nodes.Num(); NodeIndex++)
					{
						if (IntersectsFrustum(Model, NodeIndex, Frustum, bStrictDragSelection))
						{
							uint32 SurfaceIndex = Model.Nodes[NodeIndex].iSurf;
							FBspSurf& Surf = Model.Surfs[SurfaceIndex];
							BSPActors.Add(Surf.Actor);
						}
					}
				}
			}

			if (BSPActors.Num() > 0)
			{
				for (auto It = BSPActors.CreateConstIterator(); It; ++It)
				{
					AActor* Actor = *It;
					if (bStrictDragSelection)
					{
						ElementsToSelect.Append(
							UE::EditorDragTools::Private::GetElementsIntersectingFrustum(
								Actor, Frustum, EditorViewportClientProxy, SeletionArgs
							)
						);
					}
					else
					{
						ElementsToSelect.Add(UEngineElementsLibrary::AcquireEditorActorElementHandle(Actor));
					}
				}
			}
		}

		FTypedElementSelectionOptions ElementSelectionOption;

		if (!FInputDeviceState::IsShiftKeyDown(InputState))
		{
			// If the user is selecting, but isn't hold down SHIFT, remove the previous selections.
			SelectionSet->SetSelection(MoveTemp(ElementsToSelect), ElementSelectionOption);
		}
		else
		{
			SelectionSet->SelectElements(MoveTemp(ElementsToSelect), ElementSelectionOption);
		}
	}

	// Clear any hovered objects that might have been created while dragging
	FLevelEditorViewportClient::ClearHoverFromObjects();

	FEditorDragToolBehaviorTarget::OnClickRelease(InReleasePos);
}

void FEditorFrustumSelect::OnTerminateDragSequence()
{
	FEditorDragToolBehaviorTarget::OnTerminateDragSequence();

	Start = End = FVector::ZeroVector;
}

TArray<FEditorModeID> FEditorFrustumSelect::GetUnsupportedModes()
{
	const TArray<FEditorModeID> UnsupportedModes = { FBuiltinEditorModes::EM_Landscape, FBuiltinEditorModes::EM_Foliage };
	return UnsupportedModes;
}

bool FEditorFrustumSelect::IntersectsFrustum(
	const UModel& InModel, int32 NodeIndex, const FConvexVolume& InFrustum, bool bUseStrictSelection
)
{
	FBox NodeBB;
	// Get a bounding box of the node being checked
	InModel.GetNodeBoundingBox(InModel.Nodes[NodeIndex], NodeBB);

	bool bFullyContained = false;

	// Does the box intersect the frustum
	bool bIntersects = InFrustum.IntersectBox(NodeBB.GetCenter(), NodeBB.GetExtent(), bFullyContained);

	return bIntersects && (!bUseStrictSelection || (bUseStrictSelection && bFullyContained));
}

void FEditorFrustumSelect::CalculateFrustum(const FSceneView* InView, FConvexVolume& OutFrustum, bool bUseBoxFrustum) const
{
	if (!InView || !EditorViewportClientProxy)
	{
		return;
	}

	FEditorViewportClient* const EditorViewportClient = EditorViewportClientProxy->GetEditorViewportClient();
	if (!EditorViewportClient)
	{
		return;
	}

	if (bUseBoxFrustum)
	{
		FVector CamPoint = EditorViewportClient->GetViewLocation();
		FVector BoxPoint1, BoxPoint2, BoxPoint3, BoxPoint4;
		FVector WorldDir1, WorldDir2, WorldDir3, WorldDir4;
		// extend the 2D box of 1 pixel if needed to avoid degenerated volume
		const double DX = Start.X == End.X ? 0.5 : 0.0;
		const double Left = FMath::Min(Start.X, End.X) - DX;
		const double Right = FMath::Max(Start.X, End.X) + DX;
		const double DY = Start.Y == End.Y ? 0.5 : 0.0;
		const double Bottom = FMath::Min(Start.Y, End.Y) - DY;
		const double Top = FMath::Max(Start.Y, End.Y) + DY;
		// Deproject the four corners of the selection box
		FVector2D Point1(Left, Bottom); // Upper Left Corner
		FVector2D Point2(Right, Bottom); // Upper Right Corner
		FVector2D Point3(Right, Top); // Lower Right Corner
		FVector2D Point4(Left, Top); // Lower Left Corner
		InView->DeprojectFVector2D(Point1, BoxPoint1, WorldDir1);
		InView->DeprojectFVector2D(Point2, BoxPoint2, WorldDir2);
		InView->DeprojectFVector2D(Point3, BoxPoint3, WorldDir3);
		InView->DeprojectFVector2D(Point4, BoxPoint4, WorldDir4);
		// Use the camera position and the selection box to create the bounding planes
		FPlane TopPlane(BoxPoint1, BoxPoint2, CamPoint); // Top Plane
		FPlane RightPlane(BoxPoint2, BoxPoint3, CamPoint); // Right Plane
		FPlane BottomPlane(BoxPoint3, BoxPoint4, CamPoint); // Bottom Plane
		FPlane LeftPlane(BoxPoint4, BoxPoint1, CamPoint); // Left Plane

		// Try to get all six planes to create a frustum.
		// The frustum is built with the first four planes corresponding to the sides of the frustum.
		FPlane NearPlane;
		FPlane FarPlane;
		OutFrustum.Planes.Empty();
		OutFrustum.Planes.Add(TopPlane);
		OutFrustum.Planes.Add(RightPlane);
		OutFrustum.Planes.Add(BottomPlane);
		OutFrustum.Planes.Add(LeftPlane);
		if (InView->ViewMatrices.GetViewProjectionMatrix().GetFrustumNearPlane(NearPlane))
		{
			OutFrustum.Planes.Add(NearPlane);
		}
		if (InView->ViewMatrices.GetViewProjectionMatrix().GetFrustumFarPlane(FarPlane))
		{
			OutFrustum.Planes.Add(FarPlane);
		}
		OutFrustum.Init();
	}
	else
	{
		OutFrustum = InView->ViewFrustum;
		OutFrustum.Init();
	}
}
