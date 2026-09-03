// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowBlueprintLibrary.h"

#include "Dataflow/DataflowObjectInterface.h"
#include "Dataflow/DataflowInstance.h"
#include "StructUtils/PropertyBag.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DataflowBlueprintLibrary)

void UDataflowBlueprintLibrary::EvaluateTerminalNodeByName(UDataflow* Dataflow, FName TerminalNodeName, UObject* ResultAsset)
{
	if (Dataflow && Dataflow->Dataflow)
	{
		if (const TSharedPtr<FDataflowNode> Node = Dataflow->Dataflow->FindFilteredNode(FDataflowTerminalNode::StaticType(), TerminalNodeName))
		{
			if (const FDataflowTerminalNode* TerminalNode = Node->AsType<const FDataflowTerminalNode>())
			{
				UE_LOG(LogChaosDataflow, Verbose, TEXT("UDataflowBlueprintLibrary::EvaluateTerminalNodeByName(): Node [%s]"), *TerminalNodeName.ToString());
				UE::Dataflow::FEngineContext Context(ResultAsset);
				// Note: If the node is deactivated and has any outputs, then these outputs might still need to be forwarded.
				//       Therefore the Evaluate method has to be called for whichever value of bActive.
				//       This however isn't the case of SetAssetValue() for which the active state needs to be checked before the call.
				TerminalNode->Evaluate(Context);
				if (TerminalNode->IsActive() && ResultAsset)
				{
					UE_LOG(LogChaosDataflow, Verbose, TEXT("FDataflowTerminalNode::SetAssetValue(): TerminalNode [%s], Asset [%s]"), *TerminalNodeName.ToString(), *ResultAsset->GetName());
					TerminalNode->SetAssetValue(ResultAsset, Context);
				}
			}
		}
		else
		{
			UE_LOG(LogChaos, Warning, TEXT("EvaluateTerminalNodeByName : Could not find terminal node : [%s], skipping evaluation"), *TerminalNodeName.ToString());
		}
	}
}

bool UDataflowBlueprintLibrary::RegenerateAssetFromDataflow(UObject* AssetToRegenerate, bool bRegenerateDependentAssets)
{
	if (IDataflowInstanceInterface* DataflowInterface = Cast<IDataflowInstanceInterface>(AssetToRegenerate))
	{
		FDataflowInstance& DataflowInstance = DataflowInterface->GetDataflowInstance();
		return DataflowInstance.UpdateOwnerAsset(bRegenerateDependentAssets);
	}
	return false;
}

bool UDataflowBlueprintLibrary::OverrideDataflowVariableBool(UObject* Asset, FName VariableName, bool VariableValue)
{
	if (IDataflowInstanceInterface* DataflowInterface = Cast<IDataflowInstanceInterface>(Asset))
	{
		return DataflowInterface->GetDataflowInstance().GetVariableOverrides().OverrideVariableBool(VariableName, VariableValue);
	}
	return false;
}

bool UDataflowBlueprintLibrary::OverrideDataflowVariableBoolArray(UObject* Asset, FName VariableName, const TArray<bool>& VariableArrayValue)
{
	if (IDataflowInstanceInterface* DataflowInterface = Cast<IDataflowInstanceInterface>(Asset))
	{
		return DataflowInterface->GetDataflowInstance().GetVariableOverrides().OverrideVariableBoolArray(VariableName, VariableArrayValue);
	}
	return false;
}

bool UDataflowBlueprintLibrary::OverrideDataflowVariableInt(UObject* Asset, FName VariableName, int64 VariableValue)
{
	if (IDataflowInstanceInterface* DataflowInterface = Cast<IDataflowInstanceInterface>(Asset))
	{
		return DataflowInterface->GetDataflowInstance().GetVariableOverrides().OverrideVariableInt(VariableName, VariableValue);
	}
	return false;
}

bool UDataflowBlueprintLibrary::OverrideDataflowVariableIntArray(UObject* Asset, FName VariableName, const TArray<int32>& VariableArrayValue)
{
	if (IDataflowInstanceInterface* DataflowInterface = Cast<IDataflowInstanceInterface>(Asset))
	{
		return DataflowInterface->GetDataflowInstance().GetVariableOverrides().OverrideVariableInt32Array(VariableName, VariableArrayValue);
	}
	return false;
}

bool UDataflowBlueprintLibrary::OverrideDataflowVariableFloat(UObject* Asset, FName VariableName, float VariableValue)
{
	if (IDataflowInstanceInterface* DataflowInterface = Cast<IDataflowInstanceInterface>(Asset))
	{
		return DataflowInterface->GetDataflowInstance().GetVariableOverrides().OverrideVariableFloat(VariableName, VariableValue);
	}
	return false;
}

bool UDataflowBlueprintLibrary::OverrideDataflowVariableFloatArray(UObject* Asset, FName VariableName, const TArray<float>& VariableArrayValue)
{
	if (IDataflowInstanceInterface* DataflowInterface = Cast<IDataflowInstanceInterface>(Asset))
	{
		return DataflowInterface->GetDataflowInstance().GetVariableOverrides().OverrideVariableFloatArray(VariableName, VariableArrayValue);
	}
	return false;
}

bool UDataflowBlueprintLibrary::OverrideDataflowVariableObject(UObject* Asset, FName VariableName, UObject* VariableValue)
{
	if (IDataflowInstanceInterface* DataflowInterface = Cast<IDataflowInstanceInterface>(Asset))
	{
		return DataflowInterface->GetDataflowInstance().GetVariableOverrides().OverrideVariableObject(VariableName, VariableValue);
	}
	return false;
}

bool UDataflowBlueprintLibrary::OverrideDataflowVariableObjectArray(UObject* Asset, FName VariableName, const TArray<UObject*>& VariableArrayValue)
{
	if (IDataflowInstanceInterface* DataflowInterface = Cast<IDataflowInstanceInterface>(Asset))
	{
		return DataflowInterface->GetDataflowInstance().GetVariableOverrides().OverrideVariableObjectArray(VariableName, VariableArrayValue);
	}
	return false;
}









