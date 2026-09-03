// Copyright Epic Games, Inc. All Rights Reserved.


#include "MetaHumanRigLogicUnpackLibrary.h"

#include "AnimationGraph.h"
#include "AnimGraphNode_ControlRig.h"
#include "AnimGraphNode_Root.h"
#include "Animation/AnimSequence.h"
#include "Animation/PoseAsset.h"
#include "Engine/SkeletalMesh.h"
#include "ControlRigBlueprintFactory.h"
#include "DNAAsset.h"
#include "DNACommon.h"
#include "DNAReader.h"
#include "DNAReaderAdapter.h"
#include "RigLogicDNAReader.h"
#include "ControlRigEditor/Private/ControlRigEditorModule.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Rigs/RigHierarchyController.h"
#include "MetaHumanCharacterPaletteEditorModule.h"

#define LOCTEXT_NAMESPACE "MetaHumanCharacterEditorRigLogicUnpackLibrary"


bool UMetaHumanRigLogicUnpackLibrary::UnpackRBFEvaluation(
	UAnimBlueprint* AnimBlueprint,
	USkeletalMesh* SkeletalMesh,
	TNotNull<UObject*> GeneratedAssetOuter,
	bool UnpackFingerRBFToHalfRotationControlRig,
	TArray<uint16>& HalfRotationSolvers,
	TArray<FMetaHumanBodyRigLogicGeneratedAsset>& OutGeneratedAssets
)
{
	UAssetUserData* UserData = SkeletalMesh->GetAssetUserDataOfClass(UDNAAsset::StaticClass());
	if (!UserData)
	{
		return false;
	}
	
	// Get the dna asset from the user asset data
	UDNAAsset* DNAAsset = CastChecked<UDNAAsset>(UserData);
	const TSharedPtr<IDNAReader> BehaviorReader = DNAAsset->GetBehaviorReader();

	// Convert from the dna coordinate space (right-handed Y-Up) to UE coordinate space (left-handed Z-Up) with the UESpaceWrapper
	RigLogicDNAReader BehaviorReaderInUESpace{BehaviorReader->Unwrap()};
	FDNAReader BehaviorReaderInUESpaceWrapper{&BehaviorReaderInUESpace};

	// Get the neutral joint transforms for the skeleton
	const FReferenceSkeleton& RefSkeleton = SkeletalMesh->GetSkeleton()->GetReferenceSkeleton();
	TArray<FTransform> NeutralJointTransforms = RefSkeleton.GetRefBonePose();

	// Get the neutral joint translation/rotation from the dna file
	TMap<FString, UE::Math::TVector<float>> NeutralJointTranslations = {};
	TMap<FString, UE::Math::TQuat<float>> NeutralJointRotations = {};
	for (uint16 i = 0; i < BehaviorReaderInUESpaceWrapper.GetJointCount(); i++)
	{
		const dna::Vector3 Translation = BehaviorReaderInUESpace.getNeutralJointTranslation(i);
		NeutralJointTranslations.Add(BehaviorReaderInUESpaceWrapper.GetJointName(i), UE::Math::TVector(Translation.x, Translation.y, Translation.z));

		const dna::Vector3 Rotation = BehaviorReaderInUESpace.getNeutralJointRotation(i);

		tdm::fquat q;
		if (BehaviorReaderInUESpace.getRotationUnit() == dna::RotationUnit::radians) {
			q = tdm::fquat{tdm::frad3{tdm::frad{Rotation.x}, tdm::frad{Rotation.y}, tdm::frad{Rotation.z}}, tdm::rot_seq::zyx};
		} else {
			q = tdm::fquat{tdm::frad3{tdm::frad{tdm::fdeg{Rotation.x}}, tdm::frad{tdm::fdeg{Rotation.y}}, tdm::frad{tdm::fdeg{Rotation.z}}}, tdm::rot_seq::zyx};
		}
		NeutralJointRotations.Add(BehaviorReaderInUESpaceWrapper.GetJointName(i), UE::Math::TQuat(q.x, q.y, q.z, q.w));
	}

	uint16 SolverCount = BehaviorReaderInUESpaceWrapper.GetRBFSolverCount();

	OutGeneratedAssets.Reserve(SolverCount);

	for (uint16 i = 0; i < BehaviorReaderInUESpaceWrapper.GetRBFSolverCount(); i++)
	{
		// Get the index for each driven joint
		TArrayView<const uint16> JointGroupIndices = BehaviorReaderInUESpaceWrapper.GetJointGroupJointIndices(i);

		const FString SolverName = BehaviorReaderInUESpaceWrapper.GetRBFSolverName(i);
		// If half rotation solvers are to be unpacked to control rig, add the index and skip
		if (SolverName.Contains("_half_") && UnpackFingerRBFToHalfRotationControlRig)
		{
			HalfRotationSolvers.Add(i);
			continue;
		}
		FMetaHumanBodyRigLogicGeneratedAsset GeneratedAsset;
		GeneratedAsset.SolverName = SolverName;
		UAnimSequence* AnimSequence = NewObject<UAnimSequence>(GeneratedAssetOuter);
		AnimSequence->SetSkeleton(SkeletalMesh->GetSkeleton());
		GeneratedAsset.AnimSequence = AnimSequence;
		
		// Construct the animation curve data from the transforms stored inside the dna file
		TArray<FName> PoseNames = {};
		TArray<const FString> DriverJointNames = {};
		TArray<FName> DrivenJoints = {};

		{
			IAnimationDataController& Controller = AnimSequence->GetController();

			Controller.OpenBracket(LOCTEXT("CreateAnimSequence", "Unpacking DNA Anim Sequence"), true);
			Controller.InitializeModel();
			IAnimationDataModel::FReimportScope ReimportScope(AnimSequence->GetDataModel());
			// Clear any existing bone tracks in case the file already existed
			Controller.RemoveAllBoneTracks();
			
			TArrayView<const uint16> PoseIndices = BehaviorReaderInUESpaceWrapper.GetRBFSolverPoseIndices(i);
			const int32 PoseCount = PoseIndices.Num();

			Controller.SetNumberOfFrames(PoseCount - 1);

			// Handle the creation of transforms for the poses driver transforms
			TArrayView<const uint16> RawControlIndices = BehaviorReaderInUESpaceWrapper.GetRBFSolverRawControlIndices(i);
			TArrayView<const float> RawControlValues = BehaviorReaderInUESpaceWrapper.GetRBFSolverRawControlValues(i);
			
			for (const uint16 RawControlIndex : RawControlIndices)
			{
				const FString RawControlName = BehaviorReaderInUESpaceWrapper.GetRawControlName(RawControlIndex);
				TArray<FString> Result;
				RawControlName.ParseIntoArray(Result, TEXT("."), true);
				const FString BoneName = Result[0];
				DriverJointNames.AddUnique(BoneName);
			}

			const uint16 UniqueDriverJointCount = DriverJointNames.Num();
			// Get the driven joint indices by mapping the raw control name back to the joint			
			for (uint16 ii = 0; ii < DriverJointNames.Num(); ii++)
			{
				FString BoneName = DriverJointNames[ii];
				int32 BoneIndex = RefSkeleton.FindBoneIndex(*BoneName);
				if (BoneIndex == INDEX_NONE)
				{
					continue;
				}
				FRawAnimSequenceTrack RawTrack;
				RawTrack.PosKeys.Reserve(PoseCount - 1);
				RawTrack.RotKeys.Reserve(PoseCount - 1);
				RawTrack.ScaleKeys.Reserve(PoseCount - 1);

				FTransform3f NeutralTransform = static_cast<FTransform3f>(NeutralJointTransforms[BoneIndex]);
				if (!NeutralJointRotations.Contains(BoneName) || !NeutralJointTranslations.Contains(BoneName))
				{
					continue;
				}
				// Need to calculate the offset for number of driver joints and their channels
				UE::Math::TQuat<float> NeutralRotation = *NeutralJointRotations.Find(BoneName);
				UE::Math::TVector NeutralTranslation = *NeutralJointTranslations.Find(BoneName);

				uint16 StartIndex = ii * 4;
				uint16 Gap = (UniqueDriverJointCount - 1) * 4;
				for (uint16 p = 0; p < PoseCount; p++)
				{
					RawTrack.ScaleKeys.Add(NeutralTransform.GetScale3D());
					RawTrack.PosKeys.Add(NeutralTranslation);

					UE::Math::TQuat<float> Rotation = {};
					Rotation.X = RawControlValues[StartIndex];
					Rotation.Y = RawControlValues[StartIndex + 1];
					Rotation.Z = RawControlValues[StartIndex + 2];
					Rotation.W = RawControlValues[StartIndex + 3];

					RawTrack.RotKeys.Add(NeutralRotation * Rotation);
					StartIndex += 4 + Gap;
				}

				Controller.AddBoneCurve(*BoneName, true);
				Controller.SetBoneTrackKeys(*BoneName, RawTrack.PosKeys, RawTrack.RotKeys, RawTrack.ScaleKeys, true);
			}
			
			// Handle the creation of transforms for the poses driven transforms
			TMap<const uint16, TArrayView<const uint16>> PoseJointOutputIndicesMap;
			TMap<const uint16, TArrayView<const float>> PoseJointOutputValuesMap;

			// Get the poses and cache their output data so that it doesn't need to be queried for each joint
			for (const uint16 PoseIndex : PoseIndices)
			{
				PoseJointOutputIndicesMap.Add(PoseIndex, BehaviorReaderInUESpaceWrapper.GetRBFPoseJointOutputIndices(PoseIndex));
				PoseJointOutputValuesMap.Add(PoseIndex, BehaviorReaderInUESpaceWrapper.GetRBFPoseJointOutputValues(PoseIndex));
			}

			// To generate the curve sequence in order, generate each track joint by joint
			// Iterate over each joint
			for (const uint16 JointIndex : JointGroupIndices)
			{
				// Get the bone name from the dna and map to the bone index on the skeleton
				const FString BoneName = BehaviorReaderInUESpaceWrapper.GetJointName(JointIndex);
				DrivenJoints.AddUnique(FName(BoneName));
				const int32 BoneIndex = RefSkeleton.FindBoneIndex(*BoneName);
				if (BoneIndex == INDEX_NONE)
				{
					continue;
				}
				// Create and reserve the tracks
				FRawAnimSequenceTrack RawTrack;
				RawTrack.PosKeys.Reserve(PoseCount - 1);
				RawTrack.RotKeys.Reserve(PoseCount - 1);
				RawTrack.ScaleKeys.Reserve(PoseCount - 1);

				FTransform3f NeutralTransform = static_cast<FTransform3f>(NeutralJointTransforms[BoneIndex]);
				if (!NeutralJointRotations.Contains(BoneName) || !NeutralJointTranslations.Contains(BoneName))
				{
					continue;
				}
				// Need to calculate the offset for number of driver joints and their channels
				UE::Math::TQuat<float> NeutralRotation = *NeutralJointRotations.Find(BoneName);
				UE::Math::TVector NeutralTranslation = *NeutralJointTranslations.Find(BoneName);

				// Iterate over each pose and generate the animation curve data
				for (const uint16 PoseIndex : PoseIndices)
				{
					const FString PoseName = *BehaviorReaderInUESpaceWrapper.GetRBFPoseName(PoseIndex);
					PoseNames.AddUnique(FName(PoseName));
					// Get the indices for the driven joints
					TArrayView<const uint16> PoseJointOutputIndices = BehaviorReaderInUESpaceWrapper.GetRBFPoseJointOutputIndices(PoseIndex);
					// Get the values for each driven joint
					TArrayView<const float> PoseJointOutputValues = BehaviorReaderInUESpaceWrapper.GetRBFPoseJointOutputValues(PoseIndex);

					FTransform3f Transform = FTransform3f();
					// Create an empty map to store the curve index: curve value
					TMap<uint16, float> CurveOutputMap = {};

					// Generate the default values for all the joint curves as dna does not store tracks for curves with a value of 0
					const uint16 EndIndex = JointIndex * 9 + 9;
					for (uint16 StartIndex = JointIndex * 9; StartIndex < EndIndex; StartIndex++)
					{
						CurveOutputMap.Add(StartIndex, 0.0f);
					}

					// Add/Overwrite the curve values
					for (uint16 j = 0; j < PoseJointOutputIndices.Num(); j++)
					{
						CurveOutputMap.Add(PoseJointOutputIndices[j], PoseJointOutputValues[j]);
					}

					// Construct the T/R/S values from the delta curve values + neutral values
					const uint16 StartIndex = JointIndex * 9;
					UE::Math::TVector Translation = {
						CurveOutputMap.FindChecked(StartIndex) + NeutralTranslation.X,
						CurveOutputMap.FindChecked(StartIndex + 1) + NeutralTranslation.Y,
						CurveOutputMap.FindChecked(StartIndex + 2) + NeutralTranslation.Z
					};

					Transform.SetTranslation(Translation);

					UE::Math::TRotator<float> Rotation = {};
					Rotation.Roll = CurveOutputMap.FindChecked(StartIndex + 3);
					Rotation.Pitch = CurveOutputMap.FindChecked(StartIndex + 4);
					Rotation.Yaw = CurveOutputMap.FindChecked(StartIndex + 5);
					tdm::fquat q;
					
					if (BehaviorReaderInUESpace.getRotationUnit() == dna::RotationUnit::radians) {
						q = tdm::fquat{tdm::frad3{tdm::frad{CurveOutputMap.FindChecked(StartIndex + 3)}, tdm::frad{CurveOutputMap.FindChecked(StartIndex + 4)}, tdm::frad{CurveOutputMap.FindChecked(StartIndex + 5)}}, tdm::rot_seq::zyx};
					} else {
						q = tdm::fquat{tdm::frad3{tdm::frad{tdm::fdeg{CurveOutputMap.FindChecked(StartIndex + 3)}}, tdm::frad{tdm::fdeg{CurveOutputMap.FindChecked(StartIndex + 4)}}, tdm::frad{tdm::fdeg{CurveOutputMap.FindChecked(StartIndex + 5)}}}, tdm::rot_seq::zyx};
					}

					UE::Math::TQuat RotationQuat{q.x, q.y, q.z, q.w};

					Transform.SetRotation(NeutralRotation * RotationQuat);

					UE::Math::TVector Scale = {
						CurveOutputMap.FindChecked(StartIndex + 6),
						CurveOutputMap.FindChecked(StartIndex + 7),
						CurveOutputMap.FindChecked(StartIndex + 8)
					};

					Transform.SetScale3D(NeutralTransform.GetScale3D() + Scale);


					RawTrack.ScaleKeys.Add(Transform.GetScale3D());
					RawTrack.RotKeys.Add(Transform.GetRotation());
					RawTrack.PosKeys.Add(Transform.GetTranslation());
				}
				// Add the track
				Controller.AddBoneCurve(*BoneName, true);
				Controller.SetBoneTrackKeys(*BoneName, RawTrack.PosKeys, RawTrack.RotKeys, RawTrack.ScaleKeys, true);
			}

			Controller.NotifyPopulated();
			Controller.CloseBracket(true);
		}

		UPoseAsset* PoseAsset = NewObject<UPoseAsset>(GeneratedAssetOuter);
		if (!IsValid(PoseAsset))
		{
			UE_LOG(LogTemp, Error, TEXT("Unable to create PoseAsset for solver: %s"), *SolverName);
			continue;
		}
		
		PoseAsset->SetSkeleton(SkeletalMesh->GetSkeleton());
		PoseAsset->SetRetargetSourceAsset(SkeletalMesh);
		GeneratedAsset.PoseAsset = PoseAsset;

		OutGeneratedAssets.Add(GeneratedAsset);
		PoseAsset->SourceAnimation = AnimSequence;
		PoseAsset->UpdatePoseFromAnimation(AnimSequence);
		for (int p = 0; p < PoseNames.Num(); p++)
		{
			FName CurrentName = PoseAsset->GetPoseNameByIndex(p);
			PoseAsset->ModifyPoseName(CurrentName, PoseNames[p]);
		}
		// Generate the pose asset node inside the anim blueprint
		if (PoseAsset && IsValid(AnimBlueprint))
		{
			TArray<FName> DriverJoints = {};
			for (const FString& DriverJointName : DriverJointNames)
			{
				DriverJoints.AddUnique(*DriverJointName);
			}

			UAnimGraphNode_PoseDriver* PoseDriverNode = GetPoseDriverWithTag(FName(*SolverName), AnimBlueprint);
			if (!IsValid(PoseDriverNode))
			{
				PoseDriverNode = GetPoseDriverWithDrivers(DriverJoints, AnimBlueprint);
			}
			// If that still doesn't return results, create a new pose driver node and connect it
			if (!IsValid(PoseDriverNode))
			{
				PoseDriverNode = CreatePoseDriverNode(AnimBlueprint, true);

				// If creation fails, skip this solver
				if (!IsValid(PoseDriverNode))
				{
					UE_LOG(LogTemp, Error, TEXT("Unable to create a pose driver node for %s"), *SolverName);
					continue;
				}
			}
			if (IsValid(PoseDriverNode))
			{
				// We have a valid pose driver node, time to update it with all the settings
				PoseDriverNode->SetTag(FName(SolverName));
				PoseDriverNode->SetSourceBones(DriverJoints);
				PoseDriverNode->SetDrivingBones(DrivenJoints);
				PoseDriverNode->SetAnimationAsset(PoseAsset);
				PoseDriverNode->Node.bEvalFromRefPose = true;
				PoseDriverNode->CopyTargetsFromPoseAsset();
				FRBFParams RBFParams;

				EAutomaticRadius SolverAutomaticRadius = BehaviorReaderInUESpaceWrapper.GetRBFSolverAutomaticRadius(i);
				RBFParams.bAutomaticRadius = false;
				if (SolverAutomaticRadius == EAutomaticRadius::On)
				{
					RBFParams.bAutomaticRadius = true;
				}
				RBFParams.SolverType = BehaviorReaderInUESpaceWrapper.GetRBFSolverType(i);
				RBFParams.DistanceMethod = BehaviorReaderInUESpaceWrapper.GetRBFSolverDistanceMethod(i);
				RBFParams.Function = BehaviorReaderInUESpaceWrapper.GetRBFSolverFunctionType(i);
				RBFParams.NormalizeMethod = BehaviorReaderInUESpaceWrapper.GetRBFSolverNormalizeMethod(i);
				ETwistAxis TwistAxis = BehaviorReaderInUESpaceWrapper.GetRBFSolverTwistAxis(i);
				if (TwistAxis == ETwistAxis::X)
				{
					RBFParams.TwistAxis = BA_X;
				}
				else if (TwistAxis == ETwistAxis::Y)
				{
					RBFParams.TwistAxis = BA_Y;
				}
				else if (TwistAxis == ETwistAxis::Z)
				{
					RBFParams.TwistAxis = BA_Z;
				}

				RBFParams.Radius = BehaviorReaderInUESpaceWrapper.GetRBFSolverRadius(i);
				RBFParams.WeightThreshold = BehaviorReaderInUESpaceWrapper.GetRBFSolverWeightThreshold(i);
				PoseDriverNode->SetRBFParameters(RBFParams);
				PoseDriverNode->SetPoseDriverSource(EPoseDriverSource::Rotation);
			}
		}
	}
	return true;
}

TObjectPtr<UControlRigBlueprint> UMetaHumanRigLogicUnpackLibrary::UnpackControlRigEvaluation(
	UAnimBlueprint* AnimBlueprint,
	USkeletalMesh* SkeletalMesh,
	TObjectPtr<UControlRigBlueprint> ControlRig,
	TNotNull<UObject*> GeneratedAssetOuter,
	bool UnpackSwingTwistEvaluation,
	TArray<uint16>& HalfRotationSolvers)
{
	const FName AssetName = "CR_Body_Procedural";
	bool bControlRigCreated = false;
	if (!IsValid(ControlRig))
	{
		ControlRig = CastChecked<UControlRigBlueprint>(FKismetEditorUtilities::CreateBlueprint(UControlRig::StaticClass(), GeneratedAssetOuter, AssetName, BPTYPE_Normal, UControlRigBlueprint::StaticClass(), URigVMBlueprintGeneratedClass::StaticClass(), NAME_None));
		FControlRigEditorModule::Get().CreateRootGraphIfRequired(ControlRig);
		bControlRigCreated = true;
	}
	// Grab the dna user data
	UAssetUserData* UserData = SkeletalMesh->GetAssetUserDataOfClass(UDNAAsset::StaticClass());
	if (!UserData)
	{
		if (bControlRigCreated || IsValid(ControlRig))
		{
			return ControlRig;
		}
		return nullptr;
	}

	UDNAAsset* DNAAsset = CastChecked<UDNAAsset>(UserData);
	TSharedPtr<IDNAReader> BehaviorReader = DNAAsset->GetBehaviorReader();

	RigLogicDNAReader BehaviorReaderInUESpace{BehaviorReader->Unwrap()};
	FDNAReader BehaviorReaderInUESpaceWrapper{&BehaviorReaderInUESpace};


	URigVMController* RigController = ControlRig->GetController();
	if (!IsValid(RigController))
	{
		FFormatNamedArguments FormatArguments;
		FormatArguments.Add(TEXT("ControlRigPath"), FText::FromString(ControlRig->GetPathName()));

		FText Message = FText::Format(
			LOCTEXT("RigLogicUnpackError", "Unable to unpack RigLogic to control rig. {ControlRigPath} is invalid. Asset may need saving."),
			FormatArguments);
		
		FMessageLog(UE::MetaHuman::MessageLogName).Error(Message);
		return nullptr;
	}
	
	URigHierarchyController* HierarchyController = ControlRig->GetHierarchyController();
	// Ensure that the hierarchy matches the incoming skeleton
	HierarchyController->ImportBonesFromAsset(SkeletalMesh->GetSkeleton()->GetPathName(), "None");

	// Gather the existing nodes in the graph
	URigVMGraph* TopLevelGraph = RigController->GetTopLevelGraph();
	TArray<URigVMNode*> GraphNodes = TopLevelGraph->GetNodes();
	TArray<URigVMNode*> TwistNodes;
	TArray<URigVMNode*> SwingNodes;
	TArray<URigVMNode*> HalfRotationNodes;

	TArray<URigVMNode*> GeneratedNodes;

	for (URigVMNode* GraphNode : GraphNodes)
	{
		if (GraphNode->GetNodeTitle() == "ComputeTwist")
		{
			TwistNodes.Add(GraphNode);
		}
		else if (GraphNode->GetNodeTitle() == "ComputeSwing")
		{
			SwingNodes.Add(GraphNode);
		}
		else if (GraphNode->GetNodeTitle() == "ComputeHalfFingers")
		{
			HalfRotationNodes.Add(GraphNode);
		}
	}

	if (UnpackSwingTwistEvaluation)
	{
		// Generate the twist nodes from the dna data
		for (uint16 i = 0; i < BehaviorReaderInUESpaceWrapper.GetTwistCount(); i++)
		{
			TArrayView<const uint16> TwistControlIndices = BehaviorReaderInUESpaceWrapper.GetTwistInputControlIndices(i);
			if (TwistControlIndices.IsEmpty())
			{
				continue;
			}
			// Get the input joint name
			const FString RawControlName = BehaviorReaderInUESpaceWrapper.GetRawControlName(TwistControlIndices[0]);
			TArray<FString> SplitName;
			RawControlName.ParseIntoArray(SplitName, TEXT("."), true);
			if (SplitName.IsEmpty())
			{
				continue;
			}

			const FString InputJointName = SplitName[0];

			// Get the output joint names
			TArrayView<const uint16> OutputJointIndices = BehaviorReaderInUESpaceWrapper.GetTwistOutputJointIndices(i);
			TArray<FString> OutputJointNames;
			for (const uint16 JointIndex : OutputJointIndices)
			{
				OutputJointNames.AddUnique(BehaviorReaderInUESpaceWrapper.GetJointName(JointIndex));
			}

			// Get the blend values
			TArrayView<const float> BlendValues = BehaviorReaderInUESpaceWrapper.GetTwistBlendWeights(i);
			const ETwistAxis TwistAxis = BehaviorReaderInUESpaceWrapper.GetTwistSetupTwistAxis(i);
			URigVMNode* TwistGraphNode = nullptr;

			// Try to get an existing twist node
			if (!TwistNodes.IsEmpty())
			{
				for (URigVMNode* GraphNode : TwistNodes)
				{
					if (const URigVMPin* InputBonePin = GraphNode->FindPin(TEXT("InputBone")))
					{
						if (const URigVMPin* NamePin = InputBonePin->FindSubPin(TEXT("Name")))
						{
							if (NamePin->GetDefaultValue() != InputJointName)
							{
								continue;
							}
							TwistGraphNode = GraphNode;
						}
					}
				}
			}
			// Create the node
			if (!IsValid(TwistGraphNode))
			{
				URigVMFunctionReferenceNode* NewNode = RigController->AddExternalFunctionReferenceNode(
					"/MetaHumanCharacter/Controls/CR_MH_Function_Library.CR_MH_Function_Library", "ComputeTwist");
				TwistGraphNode = CastChecked<URigVMNode>(NewNode);
				GeneratedNodes.Add(TwistGraphNode);
			}

			if (!IsValid(TwistGraphNode))
			{
				UE_LOG(LogTemp, Error, TEXT("Unable to create Twist setup for %s"), *InputJointName);
				continue;
			}
			// Set the node pin values from the dna data
			if (URigVMPin* InputBonePin = TwistGraphNode->FindPin(TEXT("InputBone")))
			{
				if (URigVMPin* TypePin = InputBonePin->FindSubPin(TEXT("Type")))
				{
					RigController->SetPinDefaultValue(TypePin->GetPinPath(), TEXT("Bone"));
				}
				if (URigVMPin* NamePin = InputBonePin->FindSubPin(TEXT("Name")))
				{
					RigController->SetPinDefaultValue(NamePin->GetPinPath(), InputJointName);
				}
			}
			if (URigVMPin* TwistBonesPin = TwistGraphNode->FindPin(TEXT("TwistBones")))
			{
				RigController->ClearArrayPin(TwistBonesPin->GetPinPath());

				for (uint16 j = 0; j < OutputJointNames.Num(); j++)
				{
					FString PinPathRoot = TwistBonesPin->GetPinPath() + "." + FString::FromInt(j);
					FString TypePinPath = PinPathRoot + ".Type";
					FString NamePinPath = PinPathRoot + ".Name";
					RigController->AddArrayPin(TwistBonesPin->GetPinPath());
					RigController->SetPinDefaultValue(TypePinPath, "Bone");
					RigController->SetPinDefaultValue(NamePinPath, OutputJointNames[j]);
				}
			}
			if (URigVMPin* TwistBlendPin = TwistGraphNode->FindPin(TEXT("TwistBlend")))
			{
				RigController->ClearArrayPin(TwistBlendPin->GetPinPath());

				for (uint16 j = 0; j < BlendValues.Num(); j++)
				{
					FString PinPathRoot = TwistBlendPin->GetPinPath() + "." + FString::FromInt(j);
					RigController->AddArrayPin(TwistBlendPin->GetPinPath());
					RigController->SetPinDefaultValue(PinPathRoot, FString::SanitizeFloat(BlendValues[j]));
				}
			}
			if (URigVMPin* TwistAxisPin = TwistGraphNode->FindPin(TEXT("TwistAxis")))
			{
				if (TwistAxis == ETwistAxis::X)
				{
					RigController->SetPinDefaultValue(TwistAxisPin->GetPinPath(), "(X=1.0, Y=0.0, Z=0.0)");
				}
				else if (TwistAxis == ETwistAxis::Y)
				{
					RigController->SetPinDefaultValue(TwistAxisPin->GetPinPath(), "(X=0.0, Y=1.0, Z=0.0)");
				}
				else
				{
					RigController->SetPinDefaultValue(TwistAxisPin->GetPinPath(), "(X=0.0, Y=0.0, Z=1.0)");
				}
			}

			if (URigVMPin* TwistFromEndPin = TwistGraphNode->FindPin(TEXT("TwistFromEnd")))
			{
				URigHierarchy* Hierarchy = HierarchyController->GetHierarchy();
				FRigElementKey InputBone;
				InputBone.Type = ERigElementType::Bone;
				InputBone.Name = *InputJointName;
				TArray<FRigElementKey> Children = Hierarchy->GetChildren(InputBone, true);
				bool bMatch = false;
				for (FRigElementKey Child : Children)
				{
					if (OutputJointNames.Contains(Child.Name))
					{
						bMatch = true;
						break;
					}
				}
				if (!bMatch)
				{
					RigController->SetPinDefaultValue(TwistFromEndPin->GetPinPath(), "true");
				}
			}
		}
		// Build the swing nodes
		for (uint16 i = 0; i < BehaviorReaderInUESpaceWrapper.GetSwingCount(); i++)
		{
			TArrayView<const uint16> SwingControlIndices = BehaviorReaderInUESpaceWrapper.GetSwingInputControlIndices(i);
			if (SwingControlIndices.IsEmpty())
			{
				continue;
			}
			// Get the input joint name
			const FString RawControlName = BehaviorReaderInUESpaceWrapper.GetRawControlName(SwingControlIndices[0]);
			TArray<FString> SplitName;
			RawControlName.ParseIntoArray(SplitName, TEXT("."), true);
			if (SplitName.IsEmpty())
			{
				continue;
			}

			const FString InputJointName = SplitName[0];

			// Get the output joint names
			TArrayView<const uint16> OutputJointIndices = BehaviorReaderInUESpaceWrapper.GetSwingOutputJointIndices(i);
			TArray<FString> OutputJointNames;
			for (const uint16 JointIndex : OutputJointIndices)
			{
				OutputJointNames.AddUnique(BehaviorReaderInUESpaceWrapper.GetJointName(JointIndex));
			}

			// Get the blend values
			TArrayView<const float> BlendValues = BehaviorReaderInUESpaceWrapper.GetSwingBlendWeights(i);
			ETwistAxis TwistAxis = BehaviorReaderInUESpaceWrapper.GetSwingSetupTwistAxis(i);
			URigVMNode* SwingGraphNode = nullptr;

			if (!SwingNodes.IsEmpty())
			{
				for (URigVMNode* GraphNode : SwingNodes)
				{
					if (URigVMPin* InputBonePin = GraphNode->FindPin(TEXT("InputBone")))
					{
						if (URigVMPin* NamePin = InputBonePin->FindSubPin(TEXT("Name")))
						{
							if (NamePin->GetDefaultValue() != InputJointName)
							{
								continue;
							}
							SwingGraphNode = GraphNode;
						}
					}
				}
			}
			if (!IsValid(SwingGraphNode))
			{
				URigVMFunctionReferenceNode* NewNode = RigController->AddExternalFunctionReferenceNode(
					"/MetaHumanCharacter/Controls/CR_MH_Function_Library.CR_MH_Function_Library", "ComputeSwing");
				SwingGraphNode = CastChecked<URigVMNode>(NewNode);
				GeneratedNodes.Add(SwingGraphNode);
			}

			if (!IsValid(SwingGraphNode))
			{
				UE_LOG(LogTemp, Error, TEXT("Unable to create Swing setup for %s"), *InputJointName);
				continue;
			}

			if (URigVMPin* InputBonePin = SwingGraphNode->FindPin(TEXT("InputBone")))
			{
				if (URigVMPin* TypePin = InputBonePin->FindSubPin(TEXT("Type")))
				{
					RigController->SetPinDefaultValue(TypePin->GetPinPath(), TEXT("Bone"));
				}
				if (URigVMPin* NamePin = InputBonePin->FindSubPin(TEXT("Name")))
				{
					RigController->SetPinDefaultValue(NamePin->GetPinPath(), InputJointName);
				}
			}
			if (URigVMPin* CorrectiveBonePin = SwingGraphNode->FindPin(TEXT("CorrectiveBone")))
			{
				for (uint16 j = 0; j < OutputJointNames.Num();)
				{
					FString PinPathRoot = CorrectiveBonePin->GetPinPath();
					FString TypePinPath = PinPathRoot + ".Type";
					FString NamePinPath = PinPathRoot + ".Name";
					RigController->SetPinDefaultValue(TypePinPath, "Bone");
					RigController->SetPinDefaultValue(NamePinPath, OutputJointNames[j]);
					break;
				}
			}
			if (URigVMPin* SwingBlendPin = SwingGraphNode->FindPin(TEXT("SwingBlend")))
			{
				for (uint16 j = 0; j < BlendValues.Num();)
				{
					FString PinPathRoot = SwingBlendPin->GetPinPath();
					RigController->SetPinDefaultValue(PinPathRoot, FString::SanitizeFloat(BlendValues[j]));
					break;
				}
			}
			if (URigVMPin* TwistAxisPin = SwingGraphNode->FindPin(TEXT("TwistAxis")))
			{
				if (TwistAxis == ETwistAxis::X)
				{
					RigController->SetPinDefaultValue(TwistAxisPin->GetPinPath(), "(X=1.0, Y=0.0, Z=0.0)");
				}
				else if (TwistAxis == ETwistAxis::Y)
				{
					RigController->SetPinDefaultValue(TwistAxisPin->GetPinPath(), "(X=0.0, Y=1.0, Z=0.0)");
				}
				else
				{
					RigController->SetPinDefaultValue(TwistAxisPin->GetPinPath(), "(X=0.0, Y=0.0, Z=1.0)");
				}
			}
		}
	}
	// Build the half rotation setup for fingers
	if (!HalfRotationSolvers.IsEmpty())
	{
		TArray<FString> DriverJointNames;
		
		for (uint16 SolverIndex : HalfRotationSolvers)
		{
			// Get the index for each driven joint
			TArrayView<const uint16> JointGroupIndices = BehaviorReaderInUESpaceWrapper.GetJointGroupJointIndices(SolverIndex);
			
			for (const uint16 JointIndex : JointGroupIndices)
			{
				FString JointName = BehaviorReaderInUESpaceWrapper.GetRawControlName(JointIndex);
				// Get the bone name from the dna and map to the bone index on the skeleton
				FString BoneName = BehaviorReaderInUESpaceWrapper.GetJointName(JointIndex);
				DriverJointNames.AddUnique(BoneName);
			}
		}
		
		URigVMNode* HalfRotationNode = nullptr;
		if (HalfRotationNodes.IsEmpty())
		{
			URigVMFunctionReferenceNode* NewNode = RigController->AddExternalFunctionReferenceNode(
	"/MetaHumanCharacter/Controls/CR_MH_Function_Library.CR_MH_Function_Library", "ComputeHalfFingers");
			HalfRotationNode = CastChecked<URigVMNode>(NewNode);
			GeneratedNodes.Add(HalfRotationNode);
		}
		else
		{
			HalfRotationNode = HalfRotationNodes[0];
		}
		if (IsValid(HalfRotationNode))
		{
			if (URigVMPin* HalfBonesPin = HalfRotationNode->FindPin(TEXT("HalfBones")))
			{
				RigController->ClearArrayPin(HalfBonesPin->GetPinPath());

				for (uint16 j = 0; j < DriverJointNames.Num(); j++)
				{
					FString PinPathRoot = HalfBonesPin->GetPinPath() + "." + FString::FromInt(j);
					FString TypePinPath = PinPathRoot + ".Type";
					FString NamePinPath = PinPathRoot + ".Name";
					RigController->AddArrayPin(HalfBonesPin->GetPinPath());
					RigController->SetPinDefaultValue(TypePinPath, "Bone");
					RigController->SetPinDefaultValue(NamePinPath, DriverJointNames[j]);
				}
			}
		}
	}
	// Connect up all the newly generated nodes
	URigVMNode* ExecutionNode = TopLevelGraph->FindNodeByName("BeginExecution");
	if (!IsValid(ExecutionNode))
	{
		ExecutionNode = RigController->AddUnitNodeFromStructPath("/Script/ControlRig.RigUnit_BeginExecution", "Execute", FVector2D::ZeroVector,
		                                                         "BeginExecution");
	}

	URigVMPin* OutputPin = ExecutionNode->FindExecutePin();
	TArray<URigVMLink*> Links = ExecutionNode->FindExecutePin()->GetLinks();
	URigVMPin* ExistingExecutePin = nullptr;
	if (Links.Num() > 0)
	{
		ExistingExecutePin = Links[0]->GetTargetPin();
		RigController->BreakLink(OutputPin, ExistingExecutePin);
	}


	for (URigVMNode* NewNode : GeneratedNodes)
	{
		URigVMPin* InputPin = NewNode->FindExecutePin();
		RigController->AddLink(OutputPin, InputPin);
		OutputPin = InputPin;
	}
	if (IsValid(ExistingExecutePin))
	{
		RigController->AddLink(OutputPin, ExistingExecutePin);
	}

	if (IsValid(AnimBlueprint))
	{
		// Add the control rig to the anim blueprint
		UClass* ControlRigClass = ControlRig->CreateControlRig()->GetClass();
		for (UEdGraph* Graph : AnimBlueprint->FunctionGraphs)
		{
			if (UAnimationGraph* AnimGraph = Cast<UAnimationGraph>(Graph))
			{
				TArray<UAnimGraphNode_ControlRig*> ControlRigNodes;
	
				AnimGraph->GetNodesOfClass(ControlRigNodes);
				UAnimGraphNode_ControlRig* ControlRigNode = nullptr;
				// Check for an existing control rig
				for (UAnimGraphNode_ControlRig* Node : ControlRigNodes)
				{
					if (Node->GetTag() == AssetName)
					{
						ControlRigNode = Node;
					}
					else if (Node->Node.GetControlRigClass()->GetPathName() == ControlRig->GetPathName())
					{
						ControlRigNode = Node;
					}
				}
	
				if (ControlRigNode == nullptr)
				{
					// Create a new control rig
					UEdGraphNode* UEdControlRigNode = NewObject<UAnimGraphNode_ControlRig>(AnimGraph);
					ControlRigNode = Cast<UAnimGraphNode_ControlRig>(UEdControlRigNode);
					AnimGraph->AddNode(UEdControlRigNode, true);
					ControlRigNode->CreateNewGuid();
					ControlRigNode->PostPlacedNewNode();
					ControlRigNode->AllocateDefaultPins();				
					TArray<UAnimGraphNode_Root*> ResultGraphNodes;
					AnimGraph->GetNodesOfClass(ResultGraphNodes);
					ControlRigNode->SetTag(AssetName);
					if (!ResultGraphNodes.IsEmpty())
					{
						UAnimGraphNode_Root* ResultGraphNode = ResultGraphNodes[0];
	
						for (UEdGraphPin* Pin : ControlRigNode->Pins)
						{
							switch (Pin->Direction)
							{
							case EGPD_Input:
								// Make link to the previous input or start node
									for (UEdGraphPin* ResultPin : ResultGraphNode->Pins)
									{
										TArray<UEdGraphPin*> LinkedPins = ResultPin->LinkedTo;
										for (UEdGraphPin* LinkedPin : LinkedPins)
										{
											ResultPin->BreakLinkTo(LinkedPin);
										}
										for (UEdGraphPin* LinkedPin : LinkedPins)
										{
											LinkedPin->MakeLinkTo(Pin);
											// Simple auto layout
											ControlRigNode->NodePosX = LinkedPin->GetOwningNode()->NodePosX + ControlRigNode->NodeWidth + 200;
											ControlRigNode->NodePosY = LinkedPin->GetOwningNode()->NodePosY;
										}
									}
								break;
	
							case EGPD_Output:
								// Make link to the result node
									for (UEdGraphPin* ResultPin : ResultGraphNode->Pins)
									{
										Pin->MakeLinkTo(ResultPin);
									}
								break;
	
							default: break;
							}
						}
					}
				}

				if (IsValid(ControlRigNode))
				{
					ControlRigNode->Node.SetControlRigClass(ControlRigClass);
				}
			}
		}
	}
	if (bControlRigCreated)
	{
		return ControlRig;
	}
	return nullptr;
}

UAnimGraphNode_PoseDriver* UMetaHumanRigLogicUnpackLibrary::CreatePoseDriverNode(const UAnimBlueprint* AnimBlueprint, const bool bAutoConnect)
{
	for (UEdGraph* Graph : AnimBlueprint->FunctionGraphs)
	{
		if (UAnimationGraph* AnimGraph = Cast<UAnimationGraph>(Graph))
		{
			// Create a pose driver graph node
			UEdGraphNode* UEdPoseDriverNode = NewObject<UAnimGraphNode_PoseDriver>(AnimGraph);
			UAnimGraphNode_PoseDriver* PoseDriverNode = Cast<UAnimGraphNode_PoseDriver>(UEdPoseDriverNode);

			// Add the node to the graph
			AnimGraph->AddNode(UEdPoseDriverNode, true);
			PoseDriverNode->CreateNewGuid();
			PoseDriverNode->PostPlacedNewNode();
			PoseDriverNode->AllocateDefaultPins();
			if (bAutoConnect)
			{
				// Try to find a results graph node
				TArray<UAnimGraphNode_Root*> ResultGraphNodes;
				AnimGraph->GetNodesOfClass(ResultGraphNodes);
				if (ResultGraphNodes.IsEmpty())
				{
					return PoseDriverNode;
				}

				UAnimGraphNode_Root* ResultGraphNode = ResultGraphNodes[0];

				for (UEdGraphPin* Pin : PoseDriverNode->Pins)
				{
					switch (Pin->Direction)
					{
					case EGPD_Input:
						// Make link to the previous input or start node
						for (UEdGraphPin* ResultPin : ResultGraphNode->Pins)
						{
							TArray<UEdGraphPin*> LinkedPins = ResultPin->LinkedTo;
							for (UEdGraphPin* LinkedPin : LinkedPins)
							{
								ResultPin->BreakLinkTo(LinkedPin);
							}
							for (UEdGraphPin* LinkedPin : LinkedPins)
							{
								LinkedPin->MakeLinkTo(Pin);
								// Simple auto layout
								PoseDriverNode->NodePosX = LinkedPin->GetOwningNode()->NodePosX + PoseDriverNode->NodeWidth + 200;
								PoseDriverNode->NodePosY = LinkedPin->GetOwningNode()->NodePosY;
							}
						}
						break;

					case EGPD_Output:
						// Make link to the result node
						for (UEdGraphPin* ResultPin : ResultGraphNode->Pins)
						{
							Pin->MakeLinkTo(ResultPin);
						}
						break;

					default: break;
					}
				}
			}

			return PoseDriverNode;
		}
	}
	return nullptr;
}

UAnimGraphNode_PoseDriver* UMetaHumanRigLogicUnpackLibrary::GetPoseDriverWithDrivers(const TArray<FName>& DriverJointNames,
                                                                                     const UAnimBlueprint* AnimBlueprint)
{
	for (UEdGraph* Graph : AnimBlueprint->FunctionGraphs)
	{
		if (const UAnimationGraph* AnimGraph = Cast<UAnimationGraph>(Graph))
		{
			TArray<UAnimGraphNode_PoseDriver*> PoseDriverNodes;
			AnimGraph->GetNodesOfClass(PoseDriverNodes);
			for (UAnimGraphNode_PoseDriver* Node : PoseDriverNodes)
			{
				TArray<FName> SourceBoneNames;
				Node->GetSourceBoneNames(SourceBoneNames);
				if (SourceBoneNames == DriverJointNames)
				{
					return Node;
				}
			}
		}
	}
	return nullptr;
}

UAnimGraphNode_PoseDriver* UMetaHumanRigLogicUnpackLibrary::GetPoseDriverWithTag(const FName& DriverTag, const UAnimBlueprint* AnimBlueprint)
{
	for (UEdGraph* Graph : AnimBlueprint->FunctionGraphs)
	{
		if (const UAnimationGraph* AnimGraph = Cast<UAnimationGraph>(Graph))
		{
			TArray<UAnimGraphNode_PoseDriver*> PoseDriverNodes;
			AnimGraph->GetNodesOfClass(PoseDriverNodes);
			for (UAnimGraphNode_PoseDriver* Node : PoseDriverNodes)
			{
				if (Node->GetTag() == DriverTag)
				{
					return Node;
				}
			}
		}
	}
	return nullptr;
}

#undef LOCTEXT_NAMESPACE
