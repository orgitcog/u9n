// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCOE/Nodes/CustomizableObjectNode.h"

#include "CustomizableObjectNodeComponentMeshBase.generated.h"


UENUM()
enum class ECustomizableObjectAutomaticLODStrategy : uint8
{
	// Use the same strategy than the parent component. If root, then use "Manual".
	Inherited = 0 UMETA(DisplayName = "Inherit from parent component"),
	// Don't try to generate LODs automatically for the child nodes. Only the ones tha explicitely define them will be used.
	Manual = 1 UMETA(DisplayName = "Only manually created LODs"),
	/** Automatic LODs from Mesh will try to replicate the explicit graph connections to higher LODs.
	 *
	 * For example, given a Mesh with n LODs connected to a Component m-th LOD, LODs m1, m2... will be automatically connected
	 * using Mesh LODs n1, n2.
	 *
	 * Note: If the mesh section is disabled at LOD 'n' or doesn't exist, it will not be added from that LOD onwards.
	 * Note: If the mesh section uses a different MaterialSlot at LOD 'n', it will not be added from that LOD onwards.
	 */
	AutomaticFromMesh = 2 UMETA(DisplayName = "Automatic from mesh")
};


UINTERFACE(MinimalAPI, Blueprintable)
class UCustomizableObjectNodeComponentMeshInterface : public UInterface
{
	GENERATED_BODY()
};


class ICustomizableObjectNodeComponentMeshInterface
{
	GENERATED_BODY()

public:

	/** Returns the num of Lods set in the Mesh Component Node */
	virtual int32 GetNumLODs() = 0;

	/** Returns the Lod Strategy set in the Mesh Component Node */
	virtual ECustomizableObjectAutomaticLODStrategy GetAutoLODStrategy() = 0;

	/** Returns a reference to an array with all the LOD pins of the Mesh Component Node */
	virtual TArray<FEdGraphPinReference>& GetLODPins();

	/** Returns a const reference to an array with all the LOD pins of the Mesh Component Node */
	virtual const TArray<FEdGraphPinReference>& GetLODPins() const = 0;

	/** Returns the output pin of the mesh component node */
	virtual UEdGraphPin* GetOutputPin() const = 0;

	/** Sets the output pin of the mesh component node */
	virtual void SetOutputPin(const UEdGraphPin* Pin) = 0;

	/** Returns a pointer to the node that owns this interface */
	virtual const UCustomizableObjectNode* GetOwningNode() const = 0;
};
