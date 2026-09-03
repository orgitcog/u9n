// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMetaHumanIdentityPartsEditor.h"

#include "MetaHumanIdentity.h"
#include "MetaHumanIdentityParts.h"
#include "MetaHumanIdentityPose.h"
#include "MetaHumanIdentityStyle.h"
#include "MetaHumanIdentityLog.h"
#include "MetaHumanTemplateMeshComponent.h"
#include "CaptureData.h"
#include "MetaHumanIdentityViewportClient.h"
#include "SMetaHumanIdentityPartsClassCombo.h"
#include "MetaHumanFootageComponent.h"

#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Images/SImage.h"
#include "Framework/Commands/GenericCommands.h"
#include "EditorViewportCommands.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Commands/UICommandList.h"
#include "Components/PrimitiveComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "ScopedTransaction.h"
#include "Editor/Transactor.h"
#include "PreviewScene.h"
#include "AssetToolsModule.h"
#include "Algo/AllOf.h"
#include "Misc/MessageDialog.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"


#define LOCTEXT_NAMESPACE "MetaHumanIdentityPartsEditor"

/////////////////////////////////////////////////////
// FIdentityTreeNode

FIdentityTreeNode::FIdentityTreeNode(class UMetaHumanIdentityPart* InIdentityPart, AActor* InIdentityActor, FName InPropertyName, UPrimitiveComponent* InPreviewComponent, EIdentityTreeNodeIdentifier InComponentIdentifier)
	: IdentityPart{ InIdentityPart }
	, PreviewSceneComponent{ InPreviewComponent }
	, TreeNodeIdentifier{ InComponentIdentifier }
{
	if (!InPropertyName.IsNone())
	{
		// This is a node that points to a member of InIdentityPart
		IdentityPartPropertyName = InPropertyName;

		SetupPreviewSceneComponentInstance(InIdentityActor);
	}
	else
	{
		TreeNodeIdentifier = GetTreeNodeIdentifierForPart(InIdentityPart);

		// This is a node that directly represents a Part
		if (UMetaHumanIdentityFace* Face = Cast<UMetaHumanIdentityFace>(InIdentityPart))
		{
			Children.Reserve(3);

			//1. Add Poses node
			TSharedRef<FIdentityTreeNode> PosesListNode = MakeShared<FIdentityTreeNode>(EIdentityTreeNodeIdentifier::FacePoseList);

			// Create one node for each pose already stored in the Face Part
			for (UMetaHumanIdentityPose* Pose : Face->GetPoses())
			{
				PosesListNode->Children.Add(MakeShared<FIdentityTreeNode>(Pose, InIdentityActor));
			}

			PosesListNode->bVisible = !PosesListNode->Children.IsEmpty();
			Children.Add(PosesListNode);

			//2. Add Template Node
			if (Face->TemplateMeshComponent != nullptr)
			{
				TSharedRef<FIdentityTreeNode> TemplateMeshNode = MakeShared<FIdentityTreeNode>(Face, InIdentityActor, GET_MEMBER_NAME_CHECKED(UMetaHumanIdentityFace, TemplateMeshComponent), Face->TemplateMeshComponent, EIdentityTreeNodeIdentifier::TemplateMesh);

				if (UMetaHumanTemplateMeshComponent* TemplateMeshComponentInstance = Cast<UMetaHumanTemplateMeshComponent>(TemplateMeshNode->PreviewSceneComponentInstance))
				{
					auto UpdateTemplateMeshInstance = [Face, TemplateMeshComponentInstance]
					{
						check(Face);
						check(Face->TemplateMeshComponent);
						check(TemplateMeshComponentInstance);

						// TODO: Is there a better way of handling this instances?
						// Updates the dynamic meshes of the instance component with the meshes that are currently being set in the template mesh component stored in the face part
						TemplateMeshComponentInstance->HeadMeshComponent->GetDynamicMesh()->SetMesh(*Face->TemplateMeshComponent->HeadMeshComponent->GetMesh());
						TemplateMeshComponentInstance->TeethMeshComponent->GetDynamicMesh()->SetMesh(*Face->TemplateMeshComponent->TeethMeshComponent->GetMesh());
						TemplateMeshComponentInstance->LeftEyeComponent->GetDynamicMesh()->SetMesh(*Face->TemplateMeshComponent->LeftEyeComponent->GetMesh());
						TemplateMeshComponentInstance->RightEyeComponent->GetDynamicMesh()->SetMesh(*Face->TemplateMeshComponent->RightEyeComponent->GetMesh());
						TemplateMeshComponentInstance->SetEyeMeshesVisibility(Face->TemplateMeshComponent->bShowEyes);
						TemplateMeshComponentInstance->SetTeethMeshVisibility(Face->TemplateMeshComponent->bShowTeethMesh);

						TemplateMeshComponentInstance->UpdateBounds();
					};

					UpdateTemplateMeshInstance();

					// Adding a weak lambda means that when the TemplateMeshComponentInstance is deleted, by closing the editor for example
					// the delegate will also be removed preventing a crash that could happen if the lambda is called without an instance available
					Face->TemplateMeshComponent->OnTemplateMeshChanged.AddWeakLambda(TemplateMeshComponentInstance, UpdateTemplateMeshInstance);
				}

				Children.Add(TemplateMeshNode);
			}

			//3. Add Rig Node
			if (Face->RigComponent != nullptr && Face->RigComponent->GetSkeletalMeshAsset() != nullptr && Face->RigComponent->GetSkeletalMeshAsset()->GetSkeleton() != nullptr)
			{
				Children.Add(MakeShared<FIdentityTreeNode>(Face, InIdentityActor, GET_MEMBER_NAME_CHECKED(UMetaHumanIdentityFace, RigComponent), Face->RigComponent, EIdentityTreeNodeIdentifier::SkeletalMesh));
			}
		}
	}
}

FIdentityTreeNode::FIdentityTreeNode(UMetaHumanIdentityPose* InIdentityPose, AActor* InIdentityActor)
	: IdentityPose{ InIdentityPose }
	, PreviewSceneComponent{ Cast<UPrimitiveComponent>(InIdentityPose->CaptureDataSceneComponent) } // TODO: Evaluate the impact of changing the type of the component stored in the pose
	, TreeNodeIdentifier{ GetTreeNodeIdentifierForPose(InIdentityPose->PoseType) }
{
	SetupPreviewSceneComponentInstance(InIdentityActor);
}

FIdentityTreeNode::FIdentityTreeNode(UMetaHumanIdentity* InIdentity, AActor* InIdentityActor)
	: Identity{ InIdentity }
	, TreeNodeIdentifier{ EIdentityTreeNodeIdentifier::IdentityRoot }
{
	Children.Reserve(InIdentity->Parts.Num());

	// Add one child node for each part already in the Identity
	for (UMetaHumanIdentityPart* Part : InIdentity->Parts)
	{
		Children.Add(MakeShared<FIdentityTreeNode>(Part, InIdentityActor));
	}
}

FIdentityTreeNode::FIdentityTreeNode(EIdentityTreeNodeIdentifier InIdentifier)
	: TreeNodeIdentifier{ InIdentifier }
{
}

EIdentityTreeNodeIdentifier FIdentityTreeNode::GetTreeNodeIdentifierForPart(UMetaHumanIdentityPart* InIdentityPart)
{
	if (InIdentityPart != nullptr)
	{
		if (InIdentityPart->IsA<UMetaHumanIdentityFace>())
		{
			return EIdentityTreeNodeIdentifier::FaceNode;
		}

		if (InIdentityPart->IsA<UMetaHumanIdentityBody>())
		{
			return EIdentityTreeNodeIdentifier::BodyNode;
		}
	}

	return EIdentityTreeNodeIdentifier::None;
}

EIdentityTreeNodeIdentifier FIdentityTreeNode::GetTreeNodeIdentifierForPose(const EIdentityPoseType InPoseType)
{
	EIdentityTreeNodeIdentifier NodeIdentifier = EIdentityTreeNodeIdentifier::None;

	switch (InPoseType)
	{
		case EIdentityPoseType::Neutral:
			NodeIdentifier = EIdentityTreeNodeIdentifier::FaceNeutralPose;
			break;
		case EIdentityPoseType::Teeth:
			NodeIdentifier = EIdentityTreeNodeIdentifier::FaceTeethPose;
			break;
		default:
			NodeIdentifier = EIdentityTreeNodeIdentifier::None;
	}

	return NodeIdentifier;
}

void FIdentityTreeNode::SetupPreviewSceneComponentInstance(AActor* InIdentityActor)
{
	if (PreviewSceneComponent.IsValid())
	{
		// The PreviewSceneComponentInstance is what is actually is displayed in the viewport. Duplicate object will duplicate the Scene component that
		// was serialized last time so the viewport is kept up-to-date and will display any changes the user has saved
		PreviewSceneComponentInstance = DuplicateObject<UPrimitiveComponent>(PreviewSceneComponent.Get(), InIdentityActor);
		check(PreviewSceneComponentInstance.IsValid());
		PreviewSceneComponentInstance->SetFlags(RF_Transient);

		// The ComponentToWorld member of UPrimitiveComponent is not a UPROPERTY, therefore its not copied by DuplicateObject.
		// This call is required as the ComponentToWorld transform is what is used to place the component in the world,
		// if not updated it will use an identity transform
		PreviewSceneComponentInstance->UpdateComponentToWorld();

		InIdentityActor->AddOwnedComponent(PreviewSceneComponentInstance.Get());

		// The PreviewSceneComponent is what the user is editing in the details panel. This will make sure any changes to the transform
		// component of will get copied to the instance that is displayed on the screen
		PreviewSceneComponent->TransformUpdated.AddLambda([this](USceneComponent* InRootComponent, EUpdateTransformFlags, ETeleportType)
		{
			if (PreviewSceneComponent.IsValid() && PreviewSceneComponentInstance.IsValid() && PreviewSceneComponent == InRootComponent)
			{
				PreviewSceneComponentInstance->SetWorldTransform(PreviewSceneComponent->GetComponentTransform());
			}
		});
	}
}

void FIdentityTreeNode::UpdateSceneComponentInstanceProperty(FProperty* InProperty)
{
	if (InProperty != nullptr && PreviewSceneComponent.IsValid() && PreviewSceneComponentInstance.IsValid())
	{
		if (PreviewSceneComponentInstance->GetClass()->HasProperty(InProperty) && PreviewSceneComponent->GetClass()->HasProperty(InProperty))
		{
			InProperty->CopyCompleteValue_InContainer(PreviewSceneComponentInstance.Get(), PreviewSceneComponent.Get());

			// Trigger the PostEdit change event to let the instance do any required internal updates
			FPropertyChangedEvent PropertyChangeEvent{ InProperty };
			PreviewSceneComponentInstance->PostEditChangeProperty(PropertyChangeEvent);

			// Re-register the component to make sure its state is up to date.
			// This is mostly required to make sure the animation control works in the Skeletal Mesh instance
			PreviewSceneComponentInstance->ReregisterComponent();

			// When the transform property changes in the details panel the actual transform, the ComponentToWorld member,
			// doesn't change immediately, so this needs to be called. This happen because the component is registered
			// to a world but calling UpdateComponentToWorld solves it
			PreviewSceneComponent->UpdateComponentToWorld();
			PreviewSceneComponentInstance->UpdateComponentToWorld();

			// Makes sure any change to rendering properties from the CopyCompleteValue_InContainer above are updated in the viewport
			PreviewSceneComponentInstance->MarkRenderStateDirty();
		}
		else
		{
			UE_LOG(LogMetaHumanIdentity, Error, TEXT("UpdateSceneComponentInstanceProperty called with a property named '%s' that doesn't exist in class %s"), *InProperty->GetFName().ToString(), *PreviewSceneComponent->GetClass()->GetName());
		}
	}
}

bool FIdentityTreeNode::CanDelete() const
{
	// Don't allow the root node to be deleted
	if (Identity.IsValid())
	{
		return false;
	}

	if (IdentityPart.IsValid())
	{
		// If this is a part that doesn't point to a property it can be deleted
		if (IdentityPartPropertyName.IsNone())
		{
			return true;
		}
	}

	// A pose can also be deleted
	if (IdentityPose.IsValid())
	{
		return true;
	}

	return false;
}

FText FIdentityTreeNode::GetDisplayText() const
{
	if (Identity.IsValid())
	{
		return FText::FromString(Identity->GetName());
	}

	else if (IdentityPart.IsValid())
	{
		if (!IdentityPartPropertyName.IsNone())
		{
			return GetObjectProperty()->GetDisplayNameText();
		}
		else
		{
			return IdentityPart->GetPartName();
		}
	}

	else if (IdentityPose.IsValid())
	{
		return IdentityPose->PoseName;
	}

	else if (TreeNodeIdentifier == EIdentityTreeNodeIdentifier::FacePoseList)
	{
		return FText::FromString("Poses");
	}

	return LOCTEXT("InvalidNodeName", "<Invalid Node>");
}

const FSlateBrush* FIdentityTreeNode::GetDisplayIconBrush() const
{
	FMetaHumanIdentityStyle& Style = FMetaHumanIdentityStyle::Get();

	if (Identity.IsValid())
	{
		return Style.GetBrush("Identity.Root");
	}

	if (IdentityPart.IsValid())
	{
		return IdentityPart->GetPartIcon(IdentityPartPropertyName).GetIcon();
	}

	if (IdentityPose.IsValid())
	{
		return IdentityPose->GetPoseIcon().GetIcon();
	}
	else if (TreeNodeIdentifier == EIdentityTreeNodeIdentifier::FacePoseList)
	{
		return Style.GetBrush("Identity.Face.Poses");
	}

	return nullptr;
}

FText FIdentityTreeNode::GetTooltip() const
{
	if (Identity.IsValid())
	{
		return LOCTEXT("IdentityTreeRootTooltip", "Identity\nHolds all the parts needed for creating a MetaHuman Identity from Capture Data.");
	}

	if (IdentityPart.IsValid())
	{
		return IdentityPart->GetPartTooltip(IdentityPartPropertyName);
	}

	if (IdentityPose.IsValid())
	{
		if (!IdentityPose->IsCaptureDataValid())
		{
			return FText::Format(LOCTEXT("IdentityTreePoseNoCapturedata", "{0}\n\nUse Details panel to set Capture Data for the pose"), IdentityPose->GetPoseTooltip());
		}
		else
		{
			return IdentityPose->GetPoseTooltip();
		}

	}
	else if (TreeNodeIdentifier == EIdentityTreeNodeIdentifier::FacePoseList)
	{
		return LOCTEXT("IdentityTreePosesTooltip", "Poses\nContains Poses with Capture Data for individual facial expressions\nneeded to create a Template Mesh resembling a person\nrepresented in the data.");
	}

	return LOCTEXT("IdentityGenericNodeTooltip", "Identity Node");
}

UObject* FIdentityTreeNode::GetObject() const
{
	if (Identity.IsValid())
	{
		return Identity.Get();
	}

	if (IdentityPart.IsValid())
	{
		if (!IdentityPartPropertyName.IsNone())
		{
			if (PreviewSceneComponent.IsValid())
			{
				return PreviewSceneComponent.Get();
			}
			else
			{
				// Gets the value of the property named IdentityPartPropertyName in the Part object
				return GetObjectProperty()->GetObjectPropertyValue_InContainer(IdentityPart.Get());
			}
		}
		else
		{
			return IdentityPart.Get();
		}
	}

	if (IdentityPose.IsValid())
	{
		return IdentityPose.Get();
	}

	return nullptr;
}

FObjectProperty* FIdentityTreeNode::GetObjectProperty() const
{
	if (IdentityPart.IsValid() && !IdentityPartPropertyName.IsNone())
	{
		return FindFProperty<FObjectProperty>(IdentityPart->GetClass(), *IdentityPartPropertyName.ToString());
	}

	return nullptr;
}

/////////////////////////////////////////////////////
// SMetaHumanIdentityPartsEditor

void SMetaHumanIdentityPartsEditor::Construct(const FArguments& InArgs)
{
	check(InArgs._Identity);
	check(InArgs._ViewportClient);

	OnIdentityPartAddedDelegate = InArgs._OnIdentityPartAdded;
	OnIdentityPartRemovedDelegate = InArgs._OnIdentityPartRemoved;
	OnIdentityPoseAddedDelegate = InArgs._OnIdentityPoseAdded;
	OnIdentityPoseRemovedDelegate = InArgs._OnIdentityPoseRemoved;
	OnIdentityTreeSelectionChangedDelegate = InArgs._OnIdentityTreeSelectionChanged;
	OnCaptureSourceSelectionChangedDelegate = InArgs._OnCaptureSourceSelectionChanged;

	IdentityPtr = InArgs._Identity;
	ViewportClient = InArgs._ViewportClient;

	ViewportClient->OnGetAllPrimitiveComponentsDelegate.BindSP(this, &SMetaHumanIdentityPartsEditor::GetAllPrimitiveComponents);
	ViewportClient->OnGetPrimitiveComponentInstanceDelegate.BindSP(this, &SMetaHumanIdentityPartsEditor::GetPrimitiveComponent, true);
	ViewportClient->OnGetSelectedPrimitivesComponentsDelegate.BindSP(this, &SMetaHumanIdentityPartsEditor::GetSelectedComponents);
	ViewportClient->OnPrimitiveComponentClickedDelegate.BindSP(this, &SMetaHumanIdentityPartsEditor::HandleSceneComponentClicked);
	ViewportClient->OnGetSelectedPoseTypeDelegate.BindSP(this, &SMetaHumanIdentityPartsEditor::GetSelectedPoseType);

	IdentityPreviewActorInstance = InArgs._PreviewActor;
	check(IdentityPreviewActorInstance.IsValid());

	BindCommands();

	ChildSlot
	[
		SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.Padding(4.0f, 0.0f, 4.0f, 0.0f)
				.AutoWidth()
				[
					SNew(SMetaHumanIdentityPartsClassCombo)
					.Identity(IdentityPtr)
					.OnIdentityPartClassSelected(this, &SMetaHumanIdentityPartsEditor::HandleAddIdentityPartOfClass)
					.OnIdentityPoseClassSelected(this, &SMetaHumanIdentityPartsEditor::HandleAddIdentityPoseOfClass)
					.OnIsIdentityPartClassEnabled(this, &SMetaHumanIdentityPartsEditor::CanAddIdentityPartOfClass)
					.OnIsIdentityPoseClassEnabled(this, &SMetaHumanIdentityPartsEditor::CanAddIdentityPoseOfClass)
				]
				+ SHorizontalBox::Slot()
				.Padding(6.0f)
				[
					SNew(SSearchBox)
					.OnTextChanged(this, &SMetaHumanIdentityPartsEditor::HandleIdentityFilterTextChanged)
				]
			]
			+ SVerticalBox::Slot()
			[
				SAssignNew(IdentityTreeWidget, STreeView<TSharedRef<FIdentityTreeNode>>)
				.SelectionMode(ESelectionMode::Single)
				.TreeItemsSource(&RootNodes)
				.AllowInvisibleItemSelection(false)
				.OnGenerateRow(this, &SMetaHumanIdentityPartsEditor::HandleIdentityTreeGenerateRow)
				.OnGetChildren(this, &SMetaHumanIdentityPartsEditor::HandleIdentityTreeGetChildren)
				.OnSelectionChanged(this, &SMetaHumanIdentityPartsEditor::HandleIdentityTreeSelectionChanged)
				.OnSetExpansionRecursive(this, &SMetaHumanIdentityPartsEditor::HandleIdentityTreeExpansionRecursive)
				.OnContextMenuOpening(this, &SMetaHumanIdentityPartsEditor::HandleIdentityTreeContextMenu)
				.HighlightParentNodesForSelection(true)
			]
	];

	// We display neutral pose by default. Setting pose to match that
	CurrentPoseForViewport = EIdentityPoseType::Neutral;

	// Builds the Identity hierarchy with the Parts/Poses it already has
	RefreshIdentityTree();

	// Add all preview scene components from the Identity in the viewport
	AddAllPreviewSceneComponentInstances(GetIdentityRootNode());
}

SMetaHumanIdentityPartsEditor::~SMetaHumanIdentityPartsEditor() = default;

void SMetaHumanIdentityPartsEditor::AddPartsFromAsset(UObject* InAsset, bool bInIsInputConformed)
{
	if (InAsset != nullptr)
	{
		// Check early for Template2MH cases so that any parts are not created
		if (bInIsInputConformed)
		{
			ETargetTemplateCompatibility Compatibility = UMetaHumanIdentityFace::CheckTargetTemplateMesh(InAsset);
			if (ETargetTemplateCompatibility::Valid != Compatibility)
			{
				const FText Title = LOCTEXT("FailedToConformMessageTitle", "Add Components From Conformed Mesh Error");
				FMessageDialog::Open(EAppMsgType::Ok, FText::Format(LOCTEXT("FailedToConformMessage",
					"Failed to add already conformed mesh. The mesh must be compatible with MetaHuman topology for this operation. Reason: {0}"), 
					FText::FromString(UMetaHumanIdentityFace::TargetTemplateCompatibilityAsString(Compatibility))), Title);

				return;
			}
		}

		UCaptureData* CaptureData = nullptr;
		if (InAsset->IsA<UStaticMesh>() || InAsset->IsA<USkeletalMesh>())
		{
			IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();

			const FString NewCaptureDataOuterPackagePath = FPackageName::GetLongPackagePath(GetIdentity()->GetOutermost()->GetName());
			const FString NewCaptureDataProposedPath = NewCaptureDataOuterPackagePath + TEXT("/") + InAsset->GetName();
			FString NewCaptureUniqueAssetName;
			FString Ignored;
			AssetTools.CreateUniqueAssetName(NewCaptureDataProposedPath, TEXT("_CaptureData"), Ignored, NewCaptureUniqueAssetName);

			if (UMeshCaptureData* MeshCaptureData = Cast<UMeshCaptureData>(AssetTools.CreateAsset(NewCaptureUniqueAssetName, NewCaptureDataOuterPackagePath, UMeshCaptureData::StaticClass(), nullptr)))
			{
				MeshCaptureData->Modify();
				MeshCaptureData->TargetMesh = InAsset;

				CaptureData = MeshCaptureData;
			}
			else
			{
				UE_LOG(LogMetaHumanIdentity, Error, TEXT("Unable to create a CaptureData from mesh of type '%s'. It should be either a Static or Skeletal mesh"), *InAsset->GetClass()->GetName());
			}
		}
		else if (InAsset->IsA<UFootageCaptureData>())
		{
			CaptureData = CastChecked<UFootageCaptureData>(InAsset);
		}

		if (CaptureData != nullptr)
		{
			FScopedTransaction Transaction(LOCTEXT("AddFaceFromStaticMesh", "Add MetaHuman Identity Face from Static Mesh"));

			HandleAddIdentityPartOfClass(UMetaHumanIdentityFace::StaticClass());
			UMetaHumanIdentityFace* FacePart = GetIdentity()->FindPartOfClass<UMetaHumanIdentityFace>();

			HandleAddIdentityPoseOfClass(UMetaHumanIdentityPose::StaticClass(), EIdentityPoseType::Neutral);
			UMetaHumanIdentityPose* NeutralPose = FacePart->FindPoseByType(EIdentityPoseType::Neutral);
			NeutralPose->SetCaptureData(CaptureData);

			// Enable eye fitting if the input is a footage capture data
			NeutralPose->bFitEyes = CaptureData->IsA<UFootageCaptureData>();

			if (bInIsInputConformed)
			{
				UMetaHumanIdentity* Identity = GetIdentity();
				Identity->Modify();

				const EIdentityErrorCode Conformed = FacePart->Conform(EConformType::Copy);
				if (Conformed != EIdentityErrorCode::None)
				{
					UMetaHumanIdentity::HandleError(Conformed);
					return;
				}

				if (FacePart->bIsConformed)
				{
					if (TSharedPtr<FIdentityTreeNode> FoundNode = FindIdentityNodeByComponentId(EIdentityTreeNodeIdentifier::TemplateMesh, GetIdentityRootNode()))
					{
						if (UDynamicMeshComponent* TemplateMeshComponent = Cast<UDynamicMeshComponent>(FoundNode->PreviewSceneComponent))
						{
							// The TemplateMesh has been updated directly so we need to tell the component to be updated
							TemplateMeshComponent->NotifyMeshUpdated();
						}
					}
				}
			}

			TSharedPtr<FIdentityTreeNode> FaceNeutralPoseNode = FindIdentityNodeByComponentId(EIdentityTreeNodeIdentifier::FaceNeutralPose, GetIdentityRootNode());

			UpdateSceneComponentVisiblity();
			SelectAndExpandIdentityTreeNode(FaceNeutralPoseNode.ToSharedRef());
			HandleFocusToSelection();
		}
		else
		{
			UE_LOG(LogMetaHumanIdentity, Error, TEXT("Unable to create a Face from mesh of type '%s'. It should be either a Static or Skeletal mesh"), *InAsset->GetClass()->GetName());
		}
	}
	else
	{
		UE_LOG(LogMetaHumanIdentity, Error, TEXT("Error creating Components from asset. Asset is not valid"));
	}
}

UPrimitiveComponent* SMetaHumanIdentityPartsEditor::GetSceneComponentOfType(EIdentityTreeNodeIdentifier InComponentIdentifier, bool bInInstance) const
{
	if (TSharedPtr<FIdentityTreeNode> FoundNode = FindIdentityNodeByComponentId(InComponentIdentifier, GetIdentityRootNode()))
	{
		return bInInstance ? FoundNode->PreviewSceneComponentInstance.Get() : FoundNode->PreviewSceneComponent.Get();
	}

	return nullptr;
}

UPrimitiveComponent* SMetaHumanIdentityPartsEditor::GetPrimitiveComponent(UPrimitiveComponent* InComponent, bool bInInstance) const
{
	if (TSharedPtr<FIdentityTreeNode> FoundNode = FindIdentityTreeNode(InComponent, GetIdentityRootNode()))
	{
		return bInInstance ? FoundNode->PreviewSceneComponentInstance.Get() : FoundNode->PreviewSceneComponent.Get();
	}

	return nullptr;
}

TArray<UPrimitiveComponent*> SMetaHumanIdentityPartsEditor::GetAllPrimitiveComponents() const
{
	TArray<UPrimitiveComponent*> PrimitiveComponents;
	constexpr bool bOnlyVisible = false;
	constexpr bool bInstance = false;

	FindAllPreviewSceneComponents(GetIdentityRootNode(), PrimitiveComponents, bInstance, bOnlyVisible);

	return PrimitiveComponents;
}

TArray<UPrimitiveComponent*> SMetaHumanIdentityPartsEditor::GetSelectedComponents() const
{
	TArray<UPrimitiveComponent*> Components;

	const TArray<TSharedRef<FIdentityTreeNode>> SelectedTreeItems = IdentityTreeWidget->GetSelectedItems();

	if (!SelectedTreeItems.IsEmpty())
	{
		const bool bInstances = false;
		const bool bOnlyVisible = false;
		FindAllPreviewSceneComponents(SelectedTreeItems[0], Components, bInstances, bOnlyVisible);
	}

	return Components;
}

EIdentityPoseType SMetaHumanIdentityPartsEditor::GetSelectedPoseType() const
{
	return CurrentPoseForViewport;
}

void SMetaHumanIdentityPartsEditor::SelectNode(EIdentityTreeNodeIdentifier InNodeIdentifier)
{
	if (TSharedPtr<FIdentityTreeNode> Node = FindIdentityNodeByComponentId(InNodeIdentifier, GetIdentityRootNode()))
	{
		SelectAndExpandIdentityTreeNode(Node.ToSharedRef());
	}
	else
	{
		SelectAndExpandIdentityTreeNode(GetIdentityRootNode());
	}
}

void SMetaHumanIdentityPartsEditor::UpdateCurrentPoseForViewportSelection()
{
	const TArray<TSharedRef<FIdentityTreeNode>> SelectedTreeItems = IdentityTreeWidget->GetSelectedItems();

	if (!SelectedTreeItems.IsEmpty())
	{
		const TSharedRef<FIdentityTreeNode> SelectedNode = SelectedTreeItems[0];
		EIdentityTreeNodeIdentifier NodeIdentifier = SelectedNode->TreeNodeIdentifier;

		if (NodeIdentifier == EIdentityTreeNodeIdentifier::FaceNeutralPose)
		{
			CurrentPoseForViewport = EIdentityPoseType::Neutral;
		}
		else if (NodeIdentifier == EIdentityTreeNodeIdentifier::FaceTeethPose)
		{
			CurrentPoseForViewport = EIdentityPoseType::Teeth;
		}
	}
}

void SMetaHumanIdentityPartsEditor::UpdateViewportSelectionOutlines(bool bShowSelectionOutlines)
{
	TArray<UPrimitiveComponent*> SelectedComponentInstances;
	bool bDeselectAll = true;

	const TArray<TSharedRef<FIdentityTreeNode>> SelectedTreeItems = IdentityTreeWidget->GetSelectedItems();

	if (!SelectedTreeItems.IsEmpty())
	{
		const TSharedRef<FIdentityTreeNode> SelectedNode = SelectedTreeItems[0];

		const bool bInstances = true;
		const bool bOnlyVisible = true;
		FindAllPreviewSceneComponents(SelectedNode, SelectedComponentInstances, bInstances, bOnlyVisible);

		bDeselectAll = SelectedComponentInstances.IsEmpty() || !bShowSelectionOutlines;
	}

	if (ViewportClient.IsValid())
	{
		ViewportClient->Invalidate();
	}

	if (IdentityPreviewActorInstance != nullptr)
	{
		TArray<UActorComponent*> AllComponents;
		IdentityPreviewActorInstance->GetComponents(UPrimitiveComponent::StaticClass(), AllComponents);

		for (UActorComponent* PreviewComponent : AllComponents)
		{
			if (UPrimitiveComponent* PrimitivePreviewComponent = Cast<UPrimitiveComponent>(PreviewComponent))
			{
				const bool bSelect = !bDeselectAll && SelectedComponentInstances.Contains(PrimitivePreviewComponent);
				PrimitivePreviewComponent->SelectionOverrideDelegate.BindLambda([bSelect](const UPrimitiveComponent*) { return bSelect; });
				PrimitivePreviewComponent->PushSelectionToProxy();
			}
		}
	}
}

void SMetaHumanIdentityPartsEditor::NotifyPostChange(const FPropertyChangedEvent& InPropertyChangedEvent, FProperty* InPropertyThatChanged)
{
	if (InPropertyThatChanged != nullptr)
	{
		const TArray<TSharedRef<FIdentityTreeNode>> SelectedTreeItems = IdentityTreeWidget->GetSelectedItems();
		if (SelectedTreeItems.Num() == 1)
		{
			TSharedRef<FIdentityTreeNode> SelectedItem = SelectedTreeItems[0];
			if (SelectedItem->PreviewSceneComponent.IsValid() && SelectedItem->PreviewSceneComponentInstance.IsValid())
			{
				if (InPropertyChangedEvent.MemberProperty != nullptr &&
					SelectedItem->PreviewSceneComponent->GetClass()->HasProperty(InPropertyChangedEvent.MemberProperty) &&
					SelectedItem->PreviewSceneComponentInstance->GetClass()->HasProperty(InPropertyChangedEvent.MemberProperty))
				{
					SelectedItem->UpdateSceneComponentInstanceProperty(InPropertyChangedEvent.MemberProperty);
				}
				else if (SelectedItem->PreviewSceneComponent->GetClass()->HasProperty(InPropertyThatChanged) &&
						 SelectedItem->PreviewSceneComponentInstance->GetClass()->HasProperty(InPropertyThatChanged))
				{
					SelectedItem->UpdateSceneComponentInstanceProperty(InPropertyThatChanged);
				}
			}
		}
	}
}

FReply SMetaHumanIdentityPartsEditor::OnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// Function required to process keyboard events in the tree view
	if (CommandList->ProcessCommandBindings(InKeyEvent))
	{
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

void SMetaHumanIdentityPartsEditor::BindCommands()
{
	CommandList = MakeShared<FUICommandList>();

	CommandList->MapAction(FGenericCommands::Get().Delete,
						   FUIAction(FExecuteAction::CreateSP(this, &SMetaHumanIdentityPartsEditor::HandleIdentityTreeDeleteSelectedNode),
									 FCanExecuteAction::CreateSP(this, &SMetaHumanIdentityPartsEditor::CanDeleteSelectedIdentityTreeNode)));

	CommandList->MapAction(FEditorViewportCommands::Get().FocusViewportToSelection,
						   FUIAction(FExecuteAction::CreateSP(this, &SMetaHumanIdentityPartsEditor::HandleFocusToSelection),
									 FCanExecuteAction::CreateSP(this, &SMetaHumanIdentityPartsEditor::CanFocusToSelection)));
}

void SMetaHumanIdentityPartsEditor::FindAllPreviewSceneComponents(const TSharedRef<FIdentityTreeNode>& InNode, TArray<UPrimitiveComponent*>& OutPreviewComponents, bool bInInstances, bool bInOnlyVisible) const
{
	if ((InNode->PreviewSceneComponent.IsValid() && !bInInstances) ||
		(InNode->PreviewSceneComponentInstance.IsValid() && bInInstances))
	{
		UPrimitiveComponent* Component = bInInstances ? InNode->PreviewSceneComponentInstance.Get() : InNode->PreviewSceneComponent.Get();

		if (bInOnlyVisible && Component->IsVisible())
		{
			OutPreviewComponents.Add(Component);
		}
		else if (!bInOnlyVisible)
		{
			OutPreviewComponents.Add(Component);
		}
	}

	for (const TSharedRef<FIdentityTreeNode>& ChildNode : InNode->Children)
	{
		FindAllPreviewSceneComponents(ChildNode, OutPreviewComponents, bInInstances, bInOnlyVisible);
	}
}

void SMetaHumanIdentityPartsEditor::AddAllPreviewSceneComponentInstances(const TSharedRef<FIdentityTreeNode>& InNode)
{
	const bool bInstances = true;
	const bool bOnlyVisible = false;
	TArray<UPrimitiveComponent*> SceneComponents;
	FindAllPreviewSceneComponents(InNode, SceneComponents, bInstances, bOnlyVisible);

	for (UPrimitiveComponent* SceneComponent : SceneComponents)
	{
		SceneComponent->RegisterComponent();

		if (UMetaHumanFootageComponent* FootageSceneComponent = Cast<UMetaHumanFootageComponent>(SceneComponent))
		{
			for (UPrimitiveComponent* FootagePlaneComponent : FootageSceneComponent->GetFootagePlaneComponents())
			{
				FootagePlaneComponent->RegisterComponent();
			}
		}
	}
}

void SMetaHumanIdentityPartsEditor::RemoveAllPreviewSceneComponents(const TSharedRef<FIdentityTreeNode>& InNode)
{
	const bool bInstances = true;
	const bool bOnlyVisible = false;
	TArray<UPrimitiveComponent*> SceneComponents;
	FindAllPreviewSceneComponents(InNode, SceneComponents, bInstances, bOnlyVisible);

	for (UPrimitiveComponent* SceneComponent : SceneComponents)
	{
		SceneComponent->UnregisterComponent();

		if (UMetaHumanFootageComponent* FootageComponent = Cast<UMetaHumanFootageComponent>(SceneComponent))
		{
			for (UStaticMeshComponent* FootagePlane : FootageComponent->GetFootagePlaneComponents())
			{
				FootagePlane->UnregisterComponent();
			}
		}
	}
}

void SMetaHumanIdentityPartsEditor::RefreshWidget()
{
	if (IsIdentityTreeValid())
	{
		RemoveAllPreviewSceneComponents(GetIdentityRootNode());
	}

	// Builds the Identity hierarchy with the Parts/Poses it already has
	RefreshIdentityTree();

	// Add all preview scene components from the Identity in the viewport
	AddAllPreviewSceneComponentInstances(GetIdentityRootNode());

	if (IdentityPreviewActorInstance != nullptr)
	{
		IdentityPreviewActorInstance->MarkComponentsRenderStateDirty();
	}
}

void SMetaHumanIdentityPartsEditor::RefreshIdentityTree()
{
	UMetaHumanIdentity* Identity = GetIdentity();

	// Register a delegate to handle changes in the capture source of a pose
	if (UMetaHumanIdentityFace* FacePart = Identity->FindPartOfClass<UMetaHumanIdentityFace>())
	{
		for (UMetaHumanIdentityPose* Pose : FacePart->GetPoses())
		{
			// Creates or destroys scene component depending if the capture data is valid
			Pose->UpdateCaptureDataSceneComponent();

			Pose->OnCaptureDataChanged().AddSP(this, &SMetaHumanIdentityPartsEditor::HandleIdentityPoseCaptureDataChanged, Pose);
		}
	}

	// Rebuild the Identity hierarchy
	RootNodes = { MakeShared<FIdentityTreeNode>(Identity, IdentityPreviewActorInstance.Get()) };

	// Expand all the nodes
	HandleIdentityTreeExpansionRecursive(GetIdentityRootNode(), true);
}

void SMetaHumanIdentityPartsEditor::HandleAddIdentityPartOfClass(TSubclassOf<UMetaHumanIdentityPart> InIdentityPartClass)
{
	UMetaHumanIdentity* Identity = GetIdentity();

	const FScopedTransaction Transaction(UMetaHumanIdentity::IdentityTransactionContext, LOCTEXT("AddIdentityPart", "Add Part to the MetaHuman Identity"), Identity);

	Identity->Modify();

	if (UMetaHumanIdentityPart* NewIdentityPart = Identity->GetOrCreatePartOfClass(InIdentityPartClass))
	{
		// Add the new Part to the tree view, this will create its preview scene component
		TSharedRef<FIdentityTreeNode> NewPartNode = MakeShared<FIdentityTreeNode>(NewIdentityPart, IdentityPreviewActorInstance.Get(), NAME_None, nullptr);

		GetIdentityRootNode()->Children.Add(NewPartNode);

		AddAllPreviewSceneComponentInstances(NewPartNode);

		SelectAndExpandIdentityTreeNode(NewPartNode);

		// Update the visibility of the preview scene components
		UpdateSceneComponentVisiblity();

		// Notify that a new Part was added
		OnIdentityPartAddedDelegate.ExecuteIfBound(NewIdentityPart);
	}
	else
	{
		UE_LOG(LogMetaHumanIdentity, Error, TEXT("Trying to add a Part that the MetaHuman Identity already has: '%s'"), *InIdentityPartClass->GetName());
	}
}

void SMetaHumanIdentityPartsEditor::HandleAddIdentityPoseOfClass(TSubclassOf<UMetaHumanIdentityPose> InIdentityPose, EIdentityPoseType InPoseType)
{
	UMetaHumanIdentity* Identity = GetIdentity();

	if (Identity->CanAddPoseOfClass(InIdentityPose, InPoseType))
	{
		if (UMetaHumanIdentityFace* FacePart = Identity->FindPartOfClass<UMetaHumanIdentityFace>())
		{
			// At the moment all poses are related to the face, so add the pose directly there
			// TODO: Handle cases where poses can be added to other Parts, might need to check which Identity Part is selected in the tree view when
			// this gets called

			if (TSharedPtr<FIdentityTreeNode> FacePartNode = FindIdentityTreeNode(FacePart, GetIdentityRootNode()))
			{
				const FScopedTransaction Transaction(UMetaHumanIdentity::IdentityTransactionContext, LOCTEXT("AddIdentityPose", "Add Pose to the MetaHuman Identity"), Identity);

				FacePart->Modify();

				UMetaHumanIdentityPose* NewIdentityPose = NewObject<UMetaHumanIdentityPose>(FacePart, InIdentityPose, NAME_None, RF_Transactional);

				FacePart->AddPoseOfType(InPoseType, NewIdentityPose);

				NewIdentityPose->OnCaptureDataChanged().AddSP(this, &SMetaHumanIdentityPartsEditor::HandleIdentityPoseCaptureDataChanged, NewIdentityPose);

				// Add the new pose to the pose list
				TSharedRef<FIdentityTreeNode> NewPoseNode = MakeShared<FIdentityTreeNode>(NewIdentityPose, IdentityPreviewActorInstance.Get());

				TSharedPtr<FIdentityTreeNode> FacePoseList = FindIdentityNodeByComponentId(EIdentityTreeNodeIdentifier::FacePoseList, FacePartNode.ToSharedRef());
				FacePoseList->bVisible = true;
				FacePoseList->Children.Add(NewPoseNode);

				AddAllPreviewSceneComponentInstances(NewPoseNode);

				// Automatically select the newly created node in the tree view
				SelectAndExpandIdentityTreeNode(FacePoseList.ToSharedRef());
				IdentityTreeWidget->SetSelection(NewPoseNode);
				IdentityTreeWidget->RequestTreeRefresh();

				// Notify that a new pose was added to the given part
				OnIdentityPoseAddedDelegate.ExecuteIfBound(NewIdentityPose, FacePartNode->IdentityPart.Get());

				// Update the visibility of the preview scene component
				UpdateSceneComponentVisiblity();
			}
			else
			{
				UE_LOG(LogMetaHumanIdentity, Error, TEXT("Failed to find the Face node to add the new Pose to."));
			}
		}
		else
		{
			UE_LOG(LogMetaHumanIdentity, Error, TEXT("Trying to add a Pose that the MetaHuman Identity already has: '%s' of type '%s'"), *InIdentityPose->GetName(), *UMetaHumanIdentityPose::PoseTypeAsString(InPoseType));
		}
	}
}

TSharedRef<ITableRow> SMetaHumanIdentityPartsEditor::HandleIdentityTreeGenerateRow(TSharedRef<FIdentityTreeNode> InNode, const TSharedRef<STableViewBase>& InOwnerTable)
{
	return SNew(STableRow<TSharedRef<FIdentityTreeNode>>, InOwnerTable)
		.Content()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(SImage)
				.ColorAndOpacity(FSlateColor::UseForeground())
				.Image(InNode, &FIdentityTreeNode::GetDisplayIconBrush)
				.ToolTipText(InNode, &FIdentityTreeNode::GetTooltip)
			]
			+ SHorizontalBox::Slot()
			[
				SNew(STextBlock)
				.Margin(4)
				.Text(InNode, &FIdentityTreeNode::GetDisplayText)
				.ToolTipText(InNode, &FIdentityTreeNode::GetTooltip)
			]
		];
}

void SMetaHumanIdentityPartsEditor::HandleIdentityTreeGetChildren(TSharedRef<FIdentityTreeNode> InItem, TArray<TSharedRef<FIdentityTreeNode>>& OutChildren)
{
	for (const TSharedRef<FIdentityTreeNode>& Child : InItem->Children)
	{
		if (Child->bVisible)
		{
			OutChildren.Add(Child);
		}
	}
}

void SMetaHumanIdentityPartsEditor::HandleIdentityTreeSelectionChanged(TSharedPtr<FIdentityTreeNode> InItem, ESelectInfo::Type InSelectInfo)
{
	UpdateCurrentPoseForViewportSelection();

	if (InItem.IsValid())
	{
		OnIdentityTreeSelectionChangedDelegate.ExecuteIfBound(InItem->GetObject(), InItem->TreeNodeIdentifier);
	}
	else
	{
		OnIdentityTreeSelectionChangedDelegate.ExecuteIfBound(nullptr, EIdentityTreeNodeIdentifier::None);
	}

	if (IdentityPtr.IsValid())
	{
		const EIdentityTreeNodeIdentifier SelectedNodeIdentifier = InItem.IsValid() ? InItem->TreeNodeIdentifier : EIdentityTreeNodeIdentifier::None;
		if (IdentityPtr->ViewportSettings->SelectedTreeNode != SelectedNodeIdentifier)
		{
			// Save the current tree view selection
			IdentityPtr->ViewportSettings->SelectedTreeNode = SelectedNodeIdentifier;
		}
	}

	UpdateViewportSelectionOutlines();
}

void SMetaHumanIdentityPartsEditor::HandleIdentityTreeExpansionRecursive(TSharedRef<FIdentityTreeNode> InItem, bool bInShouldExpand)
{
	if (IdentityTreeWidget != nullptr)
	{
		IdentityTreeWidget->SetItemExpansion(InItem, bInShouldExpand);

		for (const TSharedRef<FIdentityTreeNode>& Child : InItem->Children)
		{
			HandleIdentityTreeExpansionRecursive(Child, bInShouldExpand);
		}
	}
}

void SMetaHumanIdentityPartsEditor::HandleIdentityTreeDeleteSelectedNode()
{
	const TArray<TSharedRef<FIdentityTreeNode>> SelectedItems = IdentityTreeWidget->GetSelectedItems();

	if (SelectedItems.Num() == 1)
	{
		UMetaHumanIdentity* Identity = GetIdentity();
		TSharedRef<FIdentityTreeNode> IdentityNode = GetIdentityRootNode();

		const TSharedRef<FIdentityTreeNode>& Node = SelectedItems[0];
		if (Node->IdentityPart.IsValid())
		{
			UMetaHumanIdentityPart* IdentityPart = Node->IdentityPart.Get();

			const FScopedTransaction Transaction(UMetaHumanIdentity::IdentityTransactionContext, LOCTEXT("RemoveIdentityPart", "Remove Part from MetaHuman Identity"), Identity);
			Identity->Modify();

			// Remove the Part from the Identity and the tree view
			if (Identity->Parts.Remove(IdentityPart))
			{
				IdentityNode->Children.Remove(Node);

				// Remove all preview scene components from the node that was just removed
				RemoveAllPreviewSceneComponents(Node);

				OnIdentityPartRemovedDelegate.ExecuteIfBound(IdentityPart);
			}
			else
			{
				UE_LOG(LogMetaHumanIdentity, Error, TEXT("Failed to remove MetaHuman Identity Part '%s'"), *IdentityPart->GetPartName().ToString())
			}
		}
		else if (Node->IdentityPose.IsValid())
		{
			if (TSharedPtr<FIdentityTreeNode> FaceNode = FindIdentityPartNodeByClass(UMetaHumanIdentityFace::StaticClass(), GetIdentityRootNode()))
			{
				if (UMetaHumanIdentityFace* FacePart = Cast<UMetaHumanIdentityFace>(FaceNode->IdentityPart.Get()))
				{
					const FScopedTransaction Transaction(UMetaHumanIdentity::IdentityTransactionContext, LOCTEXT("RemoveIdentityPose", "Remove Pose from MetaHuman Identity"), Identity);
					FacePart->Modify();

					UMetaHumanIdentityPose* Pose = Node->IdentityPose.Get();
					TSharedPtr<FIdentityTreeNode> FacePoseListNode = FindIdentityNodeByComponentId(EIdentityTreeNodeIdentifier::FacePoseList, FaceNode.ToSharedRef());

					// Remove the Pose from the Identity and the tree view
					if (FacePart->RemovePose(Pose))
					{
						FacePoseListNode->Children.Remove(Node);
						FacePoseListNode->bVisible = !FacePoseListNode->Children.IsEmpty();

						// Remove all preview scene components from the node that was just removed
						RemoveAllPreviewSceneComponents(Node);

						OnIdentityPoseRemovedDelegate.ExecuteIfBound(Pose, FacePart);
					}
					else
					{
						UE_LOG(LogMetaHumanIdentity, Error, TEXT("Failed to remove MetaHuman Identity Pose '%s'"), *Pose->PoseName.ToString())
					}
				}
			}
		}

		IdentityTreeWidget->RequestTreeRefresh();
	}
}

TSharedPtr<SWidget> SMetaHumanIdentityPartsEditor::HandleIdentityTreeContextMenu()
{
	const TArray<TSharedRef<FIdentityTreeNode>> SelectedItems = IdentityTreeWidget->GetSelectedItems();

	if (SelectedItems.Num() == 1)
	{
		const bool bShouldCloseAfterMenuSelection = true;
		FMenuBuilder MenuBuilder{ bShouldCloseAfterMenuSelection, CommandList };

		MenuBuilder.BeginSection(TEXT("PartCommandsParts"), LOCTEXT("PartsCommandPartsSectionLabel", "Part Options"));
		{
			MenuBuilder.AddMenuEntry(FGenericCommands::Get().Delete);
		}
		MenuBuilder.EndSection();

		MenuBuilder.BeginSection(TEXT("PartCommandsView"), LOCTEXT("PartCommandsViewSectionLabel", "View Options"));
		{
			MenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().FocusViewportToSelection);
		}
		MenuBuilder.EndSection();

		return MenuBuilder.MakeWidget();
	}

	return TSharedPtr<SWidget>{};
}

bool SMetaHumanIdentityPartsEditor::HandleUndoOrRedoTransaction(const FTransaction* InTransaction)
{
	bool bEditorModified = false;

	if (InTransaction != nullptr)
	{
		if (InTransaction->GetPrimaryObject() == IdentityPtr)
		{
			// Something happened to the Identity so react by rebuilding the tree hierarchy and the viewport components
			RefreshWidget();
			bEditorModified = true;
		}
		else
		{
			// Something happened to the objects we are editing so iterate over the changes recorded in
			// the transaction to make sure the instances being displayed in the viewport are in sync
			// with what's changed

			TArray<UObject*> AffectedObjects;
			InTransaction->GetTransactionObjects(AffectedObjects);

			const FTransactionDiff Diff = InTransaction->GenerateDiff();
			for (const TPair<FName, TSharedPtr<FTransactionObjectEvent>>& DiffMapPair : Diff.DiffMap)
			{
				const FName& ObjectName = DiffMapPair.Key;
				const TSharedPtr<FTransactionObjectEvent>& TransactionObjectEvent = DiffMapPair.Value;

				if (TransactionObjectEvent->HasPropertyChanges())
				{
					const int32 ObjectIndex = AffectedObjects.IndexOfByPredicate([ObjectName](const UObject* InObject)
					{
						return InObject != nullptr && InObject->GetPathName() == ObjectName.ToString();
					});

					if (ObjectIndex != INDEX_NONE)
					{
						UObject* AffectedObject = AffectedObjects[ObjectIndex];

						// Find the node in the tree view that holds the object that was affected
						if (TSharedPtr<FIdentityTreeNode> Node = FindIdentityTreeNode(AffectedObject, GetIdentityRootNode()))
						{
							// Checks if the affected object is the preview scene component
							if (Node->PreviewSceneComponent.IsValid() && Node->PreviewSceneComponentInstance.IsValid() &&
								Node->PreviewSceneComponent == AffectedObject)
							{
								// Finally iterate over all the properties that changed and update the value in the
								// scene component instance
								for (const FName& PropertyNameThatChanged : TransactionObjectEvent->GetChangedProperties())
								{
									FProperty* PropertyThatChanged = FindFProperty<FProperty>(AffectedObject->GetClass(), PropertyNameThatChanged);
									Node->UpdateSceneComponentInstanceProperty(PropertyThatChanged);
									bEditorModified = true;
								}
							}
						}
					}
				}
			}
		}
	}

	return bEditorModified;
}

void SMetaHumanIdentityPartsEditor::HandleSceneComponentClicked(const UPrimitiveComponent* InSceneComponent)
{
	if (InSceneComponent != nullptr)
	{
		// Try to search the node that stores the component or its attach parent if we can't find it directly
		TSharedPtr<FIdentityTreeNode> Node = FindIdentityTreeNode(InSceneComponent, GetIdentityRootNode());
		if (!Node.IsValid())
		{
			Node = FindIdentityTreeNode(InSceneComponent->GetAttachParent(), GetIdentityRootNode());
		}

		if (Node.IsValid())
		{
			SelectAndExpandIdentityTreeNode(Node.ToSharedRef());
		}
	}
}

void SMetaHumanIdentityPartsEditor::ClearIdentityTreeFilter(TSharedRef<FIdentityTreeNode> InNode)
{
	InNode->bVisible = true;

	for (TSharedRef<FIdentityTreeNode> Child : InNode->Children)
	{
		ClearIdentityTreeFilter(Child);
	}
}

bool SMetaHumanIdentityPartsEditor::FilterIdentityTree(TSharedRef<FIdentityTreeNode> InNode, const FString& InFilterString)
{
	// Set the state of the current node based on the filter string
	if (InNode->GetDisplayText().ToString().Contains(InFilterString))
	{
		InNode->bVisible = true;
		SelectAndExpandIdentityTreeNode(InNode);
	}
	else
	{
		InNode->bVisible = false;
	}

	// If any child of this node is visible, set this node to be visible as well
	for (TSharedRef<FIdentityTreeNode> Child : InNode->Children)
	{
		InNode->bVisible |= FilterIdentityTree(Child, InFilterString);
	}

	return InNode->bVisible;
}

void SMetaHumanIdentityPartsEditor::HandleIdentityFilterTextChanged(const FText& InFilterText)
{
	TSharedRef<FIdentityTreeNode> RootNode = GetIdentityRootNode();
	ClearIdentityTreeFilter(RootNode);

	if (!InFilterText.IsEmpty())
	{
		const FString FilterString = FText::TrimPrecedingAndTrailing(InFilterText).ToString();

		for (TSharedRef<FIdentityTreeNode> Child : RootNode->Children)
		{
			FilterIdentityTree(Child, FilterString);
		}
	}

	IdentityTreeWidget->RequestTreeRefresh();
}

void SMetaHumanIdentityPartsEditor::SelectAndExpandIdentityTreeNode(TSharedRef<FIdentityTreeNode> InNode)
{
	// Expand the root and the new node so they are visible in the tree view
	IdentityTreeWidget->SetItemExpansion(GetIdentityRootNode(), true);
	IdentityTreeWidget->SetItemExpansion(InNode, true);

	// Finally select the new node automatically
	IdentityTreeWidget->SetSelection(InNode);
}

bool SMetaHumanIdentityPartsEditor::IdentityPartOfClassExists(TSubclassOf<class UMetaHumanIdentityPart> InIdentityPartClass) const
{
	return GetIdentity()->FindPartOfClass<UMetaHumanIdentityFace>() != nullptr;
}

bool SMetaHumanIdentityPartsEditor::CanAddIdentityPartOfClass(TSubclassOf<class UMetaHumanIdentityPart> InIdentityPartClass) const
{
	return GetIdentity()->CanAddPartOfClass(InIdentityPartClass);
}

bool SMetaHumanIdentityPartsEditor::CanAddIdentityPoseOfClass(TSubclassOf<class UMetaHumanIdentityPose> InIdentityPoseClass, EIdentityPoseType InPoseType) const
{
	return GetIdentity()->CanAddPoseOfClass(InIdentityPoseClass, InPoseType);
}

bool SMetaHumanIdentityPartsEditor::CanDeleteSelectedIdentityTreeNode() const
{
	const TArray<TSharedRef<FIdentityTreeNode>> SelectedItems = IdentityTreeWidget->GetSelectedItems();

	if (SelectedItems.Num() == 1)
	{
		const TSharedRef<FIdentityTreeNode>& Node = SelectedItems[0];
		return Node->CanDelete();
	}

	return false;
}

bool SMetaHumanIdentityPartsEditor::CanFocusToSelection() const
{
	// Determine if the selected node in the tree view allows focusing in the selection,
	// for nodes that don't have an associated preview component, focusing is disabled
	const TArray<TSharedRef<FIdentityTreeNode>> SelectedItems = IdentityTreeWidget->GetSelectedItems();

	if (SelectedItems.Num() == 1 && ViewportClient.IsValid())
	{
		const bool bInstances = true;
		const bool bOnlyVisible = true;
		TArray<UPrimitiveComponent*> VisibleComponents;
		FindAllPreviewSceneComponents(SelectedItems[0], VisibleComponents, bInstances, bOnlyVisible);

		if (!VisibleComponents.IsEmpty())
		{
			const bool bIsWorldValidForAllVisibleComponents = Algo::AllOf(VisibleComponents, [](UPrimitiveComponent* Component)
			{
				return Component->GetWorld() != nullptr;
			});

			return bIsWorldValidForAllVisibleComponents;
		}
	}

	return false;
}

bool SMetaHumanIdentityPartsEditor::IsIdentityTreeValid() const
{
	return !RootNodes.IsEmpty();
}

void SMetaHumanIdentityPartsEditor::HandleIdentityPoseCaptureDataChanged(bool bInResetRanges, UMetaHumanIdentityPose* InIdentityPose)
{
	if (TSharedPtr<FIdentityTreeNode> PoseNode = FindIdentityTreeNode(InIdentityPose, GetIdentityRootNode()))
	{
		FPreviewScene* PreviewScene = GetPreviewScene();

		if (PoseNode->PreviewSceneComponentInstance.IsValid())
		{
			RemoveAllPreviewSceneComponents(PoseNode.ToSharedRef());

			PoseNode->PreviewSceneComponentInstance = nullptr;
			PoseNode->PreviewSceneComponent = nullptr;
		}

		if (UPrimitiveComponent* NewPreviewComponent = Cast<UPrimitiveComponent>(InIdentityPose->CaptureDataSceneComponent))
		{
			// Update the Pose node with information from the new capture data
			PoseNode->TreeNodeIdentifier = PoseNode->IdentityPose->PoseType == EIdentityPoseType::Neutral ? EIdentityTreeNodeIdentifier::FaceNeutralPose : EIdentityTreeNodeIdentifier::FaceTeethPose;
			PoseNode->PreviewSceneComponent = NewPreviewComponent;
			PoseNode->SetupPreviewSceneComponentInstance(IdentityPreviewActorInstance.Get());

			check(PoseNode->PreviewSceneComponentInstance.IsValid());

			// Add the new one to the scene and store a reference to it in the tree node
			AddAllPreviewSceneComponentInstances(PoseNode.ToSharedRef());

			// Update the preview scene components visibility to make sure the new capture data visibility state is reflected in the viewport
			UpdateSceneComponentVisiblity();

			if (IdentityTreeWidget->IsItemSelected(PoseNode.ToSharedRef()) && ViewportClient.IsValid())
			{
				HandleFocusToSelection();
			}
		}

		OnCaptureSourceSelectionChangedDelegate.ExecuteIfBound(InIdentityPose->GetCaptureData(), InIdentityPose->TimecodeAlignment, InIdentityPose->Camera, bInResetRanges);
	}
}

void SMetaHumanIdentityPartsEditor::HandleFocusToSelection()
{
	if (ViewportClient.IsValid())
	{
		ViewportClient->FocusViewportOnSelection();
	}
}

TSharedRef<FIdentityTreeNode> SMetaHumanIdentityPartsEditor::GetIdentityRootNode() const
{
	check(IsIdentityTreeValid());
	return RootNodes[0];
}

UMetaHumanIdentity* SMetaHumanIdentityPartsEditor::GetIdentity() const
{
	check(IdentityPtr.IsValid());
	return IdentityPtr.Get();
}

FPreviewScene* SMetaHumanIdentityPartsEditor::GetPreviewScene() const
{
	check(ViewportClient.IsValid());
	return ViewportClient->GetPreviewScene();
}

TSharedPtr<FIdentityTreeNode> SMetaHumanIdentityPartsEditor::FindIdentityPartNodeByClass(TSubclassOf<UMetaHumanIdentityPart> InIdentityPart, const TSharedRef<FIdentityTreeNode>& InNode) const
{
	if (InNode->IdentityPart.IsValid() && InNode->IdentityPart->IsA(InIdentityPart))
	{
		return InNode;
	}
	else
	{
		// Look on all the children
		for (const TSharedRef<FIdentityTreeNode>& ChildNode : InNode->Children)
		{
			if (TSharedPtr<FIdentityTreeNode> FoundNode = FindIdentityPartNodeByClass(InIdentityPart, ChildNode))
			{
				return FoundNode;
			}
		}
	}

	return nullptr;
}

TSharedPtr<FIdentityTreeNode> SMetaHumanIdentityPartsEditor::FindIdentityTreeNode(const UObject* InObject, const TSharedRef<FIdentityTreeNode>& InNode) const
{
	if (InNode->GetObject() == InObject || InNode->PreviewSceneComponentInstance == InObject || InNode->PreviewSceneComponent == InObject)
	{
		return InNode;
	}
	else
	{
		// Look on all the children
		for (const TSharedRef<FIdentityTreeNode>& ChildNode : InNode->Children)
		{
			if (TSharedPtr<FIdentityTreeNode> FoundNode = FindIdentityTreeNode(InObject, ChildNode))
			{
				return FoundNode;
			}
		}
	}

	return nullptr;
}

TSharedPtr<FIdentityTreeNode> SMetaHumanIdentityPartsEditor::FindIdentityNodeByComponentId(EIdentityTreeNodeIdentifier InComponentIdentifier, const TSharedRef<FIdentityTreeNode>& InNode) const
{
	if (InNode->TreeNodeIdentifier == InComponentIdentifier)
	{
		return InNode;
	}
	else
	{
		for (const TSharedRef<FIdentityTreeNode>& ChildNode : InNode->Children)
		{
			if (TSharedPtr<FIdentityTreeNode> FoundNode = FindIdentityNodeByComponentId(InComponentIdentifier, ChildNode))
			{
				return FoundNode;
			}
		}
	}

	return nullptr;
}

void SMetaHumanIdentityPartsEditor::UpdateSceneComponentVisiblity()
{
	if (ViewportClient.IsValid())
	{
		ViewportClient->UpdateABVisibility();
	}
}

#undef LOCTEXT_NAMESPACE
