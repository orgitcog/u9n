// Copyright Epic Games, Inc. All Rights Reserved.


#include "EditorInteractiveGizmoManager.h"

#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "EditorInteractiveGizmoConditionalBuilder.h"
#include "EditorInteractiveGizmoSelectionBuilder.h"
#include "EditorInteractiveGizmoSubsystem.h"
#include "EditorModeManager.h"
#include "Tools/EdModeInteractiveToolsContext.h"
#include "HAL/IConsoleManager.h"
#include "HAL/Platform.h"
#include "HAL/PlatformCrt.h"
#include "InputRouter.h"
#include "InteractiveGizmo.h"
#include "InteractiveGizmoBuilder.h"
#include "Misc/AssertionMacros.h"
#include "SceneView.h"
#include "ShowFlags.h"
#include "Snapping/EditorSnappingManager.h"
#include "Templates/Casts.h"
#include "ToolContextInterfaces.h"
#include "TransformGizmoEditorSettings.h"
#include "EditorGizmos/TransformGizmo.h"

class FCanvas;

#define LOCTEXT_NAMESPACE "UEditorInteractiveGizmoManager"

namespace GizmoManagerLocals
{
	static TOptional<FGizmosParameters> OptDefaultParameters;
	static UEditorInteractiveGizmoManager::FOnUsesNewTRSGizmosChanged OnUsesNewTRSGizmosChanged;
	static UEditorInteractiveGizmoManager::FOnGizmosParametersChanged OnGizmosParametersChanged;
}

bool UEditorInteractiveGizmoManager::UsesNewTRSGizmos()
{
	if (const UTransformGizmoEditorSettings* TransformGizmoEditorSettings = GetDefault<UTransformGizmoEditorSettings>())
	{
		return TransformGizmoEditorSettings->UsesNewTRSGizmo();
	}

	return false;
}

void UEditorInteractiveGizmoManager::SetUsesNewTRSGizmos(const bool bUseNewTRSGizmos)
{
	if (UTransformGizmoEditorSettings* const TransformGizmoEditorSettings = GetMutableDefault<UTransformGizmoEditorSettings>())
	{
		if (bUseNewTRSGizmos != TransformGizmoEditorSettings->UsesNewTRSGizmo())
		{
			TransformGizmoEditorSettings->SetUseExperimentalGizmo(bUseNewTRSGizmos);
		}
	}
}

UEditorInteractiveGizmoManager::FOnUsesNewTRSGizmosChanged& UEditorInteractiveGizmoManager::OnUsesNewTRSGizmosChangedDelegate()
{
	return GizmoManagerLocals::OnUsesNewTRSGizmosChanged;
}

void UEditorInteractiveGizmoManager::SetGizmosParameters(const FGizmosParameters& InParameters)
{
	if (UTransformGizmoEditorSettings* const TransformGizmoEditorSettings = GetMutableDefault<UTransformGizmoEditorSettings>())
	{
		TransformGizmoEditorSettings->SetGizmosParameters(InParameters);
	}
}

UEditorInteractiveGizmoManager::FOnGizmosParametersChanged& UEditorInteractiveGizmoManager::OnGizmosParametersChangedDelegate()
{
	return GizmoManagerLocals::OnGizmosParametersChanged;
}

const TOptional<FGizmosParameters>& UEditorInteractiveGizmoManager::GetDefaultGizmosParameters()
{
	if (const UTransformGizmoEditorSettings* TransformGizmoEditorSettings = GetDefault<UTransformGizmoEditorSettings>())
	{
		GizmoManagerLocals::OptDefaultParameters = TransformGizmoEditorSettings->GizmosParameters;
	}

	static const TOptional<FGizmosParameters> Invalid;
	return GizmoManagerLocals::OptDefaultParameters.IsSet() ? GizmoManagerLocals::OptDefaultParameters : Invalid;
}

bool UEditorInteractiveGizmoManager::IsExplicitModeEnabled()
{
	return GetDefaultGizmosParameters().IsSet() ? GetDefaultGizmosParameters()->bEnableExplicit : false;
}

UEditorInteractiveGizmoManager::UEditorInteractiveGizmoManager() :
	UInteractiveGizmoManager()
{
	Registry = NewObject<UEditorInteractiveGizmoRegistry>();
	bShowEditorGizmos = UsesNewTRSGizmos();
}


void UEditorInteractiveGizmoManager::InitializeWithEditorModeManager(IToolsContextQueriesAPI* QueriesAPIIn, IToolsContextTransactionsAPI* TransactionsAPIIn, UInputRouter* InputRouterIn, FEditorModeTools* InEditorModeManager)
{
	Super::Initialize(QueriesAPIIn, TransactionsAPIIn, InputRouterIn);
	EditorModeManager = InEditorModeManager;

	UModeManagerInteractiveToolsContext* InteractiveToolsContext = EditorModeManager->GetInteractiveToolsContext();
	check(InteractiveToolsContext);

	EditorModeManager->OnEditorModeIDChanged().AddUObject(this, &UEditorInteractiveGizmoManager::OnEditorModeChanged);

	if (InteractiveToolsContext)
	{
		UE::Editor::Gizmos::RegisterSceneSnappingManager(InteractiveToolsContext);
	}
}

void UEditorInteractiveGizmoManager::Shutdown()
{
	if (EditorModeManager)
	{
		EditorModeManager->OnEditorModeIDChanged().RemoveAll(this);
	}

	Registry->Shutdown();
	Super::Shutdown();
}

void UEditorInteractiveGizmoManager::RegisterEditorGizmoType(EEditorGizmoCategory InGizmoCategory, UInteractiveGizmoBuilder* InGizmoBuilder)
{
	check(Registry);
	Registry->RegisterEditorGizmoType(InGizmoCategory, InGizmoBuilder);
}

void UEditorInteractiveGizmoManager::DeregisterEditorGizmoType(EEditorGizmoCategory InGizmoCategory, UInteractiveGizmoBuilder* InGizmoBuilder)
{
	check(Registry);
	Registry->DeregisterEditorGizmoType(InGizmoCategory, InGizmoBuilder);
}

void UEditorInteractiveGizmoManager::GetQualifiedEditorGizmoBuilders(EEditorGizmoCategory InGizmoCategory, const FToolBuilderState& InToolBuilderState, TArray<UInteractiveGizmoBuilder*>& InFoundBuilders)
{
	check(Registry);
	Registry->GetQualifiedEditorGizmoBuilders(InGizmoCategory, InToolBuilderState, InFoundBuilders);

	FEditorGizmoTypePriority FoundPriority = 0;

	if (!bSearchLocalBuildersOnly)
	{
		UEditorInteractiveGizmoSubsystem* GizmoSubsystem = GEditor->GetEditorSubsystem<UEditorInteractiveGizmoSubsystem>();
		if (ensure(GizmoSubsystem))
		{
			GizmoSubsystem->GetQualifiedGlobalEditorGizmoBuilders(InGizmoCategory, InToolBuilderState, InFoundBuilders);
		}
	}
}

UTransformGizmo* UEditorInteractiveGizmoManager::FindDefaultTransformGizmo() const
{
	return Cast<UTransformGizmo>( FindGizmoByInstanceIdentifier(TransformInstanceIdentifier()) );
}

void UEditorInteractiveGizmoManager::OnEditorModeChanged(const FName& InModeID, bool bInIsEnteringMode)
{
	// When a mode is switched, shutdown is called after initializing the other mode.
	// When the system is shared between modes (ie. snapping), this can de-register the snapping AFTER being registered in init
	// This callback allows us to de-register things before the other mode registers

	// Only execute on exit (not enter)
	if (!bInIsEnteringMode && EditorModeManager)
	{
		if (const UModeManagerInteractiveToolsContext* InteractiveToolsContext = EditorModeManager->GetInteractiveToolsContext())
		{
			UE::Editor::Gizmos::DeregisterSceneSnappingManager(InteractiveToolsContext);
		}
	}
}

bool UEditorInteractiveGizmoManager::DestroyEditorGizmo(UInteractiveGizmo* Gizmo)
{
	auto Pred = [Gizmo](const FActiveEditorGizmo& ActiveEditorGizmo) {return ActiveEditorGizmo.Gizmo == Gizmo; };
	if (!ensure(ActiveEditorGizmos.FindByPredicate(Pred)))
	{
		return false;
	}

	OnGizmosParametersChangedDelegate().RemoveAll(Gizmo);
	
	InputRouter->ForceTerminateSource(Gizmo);

	Gizmo->Shutdown();

	InputRouter->DeregisterSource(Gizmo);

	ActiveEditorGizmos.RemoveAll(Pred);

	PostInvalidation();

	return true;
}

void UEditorInteractiveGizmoManager::DestroyAllEditorGizmos()
{
	for (int i = 0; i < ActiveEditorGizmos.Num(); i++)
	{
		UInteractiveGizmo* Gizmo = ActiveEditorGizmos[i].Gizmo;
		if (ensure(Gizmo))
		{
			DestroyEditorGizmo(Gizmo);
		}
	}

	ActiveEditorGizmos.Reset();

	PostInvalidation();
}

UInteractiveGizmo* UEditorInteractiveGizmoManager::CreateGizmo(const FString& BuilderIdentifier, const FString& InstanceIdentifier, void* Owner)
{
	if (BuilderIdentifier == TransformBuilderIdentifier() && InstanceIdentifier == TransformInstanceIdentifier())
	{
		// return the default transform gizmo if it already exists.
		if (UTransformGizmo* ExistingGizmo = FindDefaultTransformGizmo())
		{
			return ExistingGizmo;
		}

		// create a new one
		UInteractiveGizmo* NewGizmo = Super::CreateGizmo(BuilderIdentifier, InstanceIdentifier, Owner);
		if (!NewGizmo)
		{
			return nullptr;
		}
		
		if (IEditorInteractiveGizmoSelectionBuilder* SelectionBuilder = Cast<IEditorInteractiveGizmoSelectionBuilder>(GizmoBuilders[BuilderIdentifier]))
		{
			FToolBuilderState CurrentSceneState;
			QueriesAPI->GetCurrentSelectionState(CurrentSceneState);
			
			SelectionBuilder->UpdateGizmoForSelection(NewGizmo, CurrentSceneState);
		}

		return NewGizmo;
	}
	
	return Super::CreateGizmo(BuilderIdentifier, InstanceIdentifier, Owner);
}

bool UEditorInteractiveGizmoManager::DestroyGizmo(UInteractiveGizmo* InGizmo)
{
	const bool bHasGizmo = ActiveGizmos.ContainsByPredicate([InGizmo](const FActiveGizmo& ActiveGizmo)
	{
		return ActiveGizmo.Gizmo == InGizmo;
	});
	if (bHasGizmo)
	{
		OnGizmosParametersChangedDelegate().RemoveAll(InGizmo);
	}
	
	return Super::DestroyGizmo(InGizmo);
}

// @todo move this to a gizmo context object
bool UEditorInteractiveGizmoManager::GetShowEditorGizmos()
{
	return bShowEditorGizmos;
}

bool UEditorInteractiveGizmoManager::GetShowEditorGizmosForView(IToolsContextRenderAPI* RenderAPI)
{
	const bool bEngineShowFlagsModeWidget = (RenderAPI && RenderAPI->GetSceneView() && 
											 RenderAPI->GetSceneView()->Family &&
											 RenderAPI->GetSceneView()->Family->EngineShowFlags.ModeWidgets);
	return bShowEditorGizmos && bEngineShowFlagsModeWidget;
}

void UEditorInteractiveGizmoManager::UpdateActiveEditorGizmos()
{
	const bool bEnableEditorGizmos = UsesNewTRSGizmos();
	if (!bEnableEditorGizmos)
	{
		if (bShowEditorGizmos)
		{
			if (UTransformGizmo* Gizmo = FindDefaultTransformGizmo())
			{
				DestroyGizmo(Gizmo);
			}
			DestroyAllEditorGizmos();
		}
		
		bShowEditorGizmos = false;
		return;
	}
	
	
	const bool bEditorModeToolsSupportsWidgetDrawing = EditorModeManager ? EditorModeManager->GetShowWidget() : true;
	const bool bNewShowEditorGizmos = bEditorModeToolsSupportsWidgetDrawing && bEnableEditorGizmos;

	if (bShowEditorGizmos != bNewShowEditorGizmos)
	{
		bShowEditorGizmos = bNewShowEditorGizmos;

		if (UTransformGizmo* Gizmo = FindDefaultTransformGizmo())
		{
			Gizmo->SetVisibility(bShowEditorGizmos ? bEditorModeToolsSupportsWidgetDrawing : false);

			if (!bEnableEditorGizmos)
			{
				DestroyGizmo(Gizmo);	
			}
		}

		if (!bShowEditorGizmos)
		{
			DestroyAllEditorGizmos();
		}
	}
}

void UEditorInteractiveGizmoManager::Tick(float DeltaTime)
{
	UpdateActiveEditorGizmos();
	
	Super::Tick(DeltaTime);

	for (FActiveEditorGizmo& ActiveEditorGizmo : ActiveEditorGizmos)
	{
		ActiveEditorGizmo.Gizmo->Tick(DeltaTime);
	}
}


void UEditorInteractiveGizmoManager::Render(IToolsContextRenderAPI* RenderAPI)
{
	Super::Render(RenderAPI);

	if (GetShowEditorGizmosForView(RenderAPI))
	{
		for (FActiveEditorGizmo& ActiveEditorGizmo : ActiveEditorGizmos)
		{
			ActiveEditorGizmo.Gizmo->Render(RenderAPI);
		}
	}
}

void UEditorInteractiveGizmoManager::DrawHUD(FCanvas* Canvas, IToolsContextRenderAPI* RenderAPI)
{
	Super::DrawHUD(Canvas, RenderAPI);

	if (GetShowEditorGizmosForView(RenderAPI))
	{
		for (FActiveEditorGizmo& ActiveEditorGizmo : ActiveEditorGizmos)
		{
			ActiveEditorGizmo.Gizmo->DrawHUD(Canvas, RenderAPI);
		}
	}
}

const FString& UEditorInteractiveGizmoManager::TransformInstanceIdentifier()
{
	static const FString Identifier(TEXT("EditorTransformGizmoInstance"));
	return Identifier;	
}

const FString& UEditorInteractiveGizmoManager::TransformBuilderIdentifier()
{
	static const FString Identifier(TEXT("EditorTransformGizmoBuilder"));
	return Identifier;
}

#undef LOCTEXT_NAMESPACE
