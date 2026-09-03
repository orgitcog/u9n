// Copyright Epic Games, Inc. All Rights Reserved.

#include "InterchangeStaticMeshLodDataNode.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InterchangeStaticMeshLodDataNode)

namespace UE
{
	namespace Interchange
	{
		const FAttributeKey& FStaticMeshNodeLodDataStaticData::GetMeshUidsBaseKey()
		{
			static FAttributeKey MeshUids_BaseKey(TEXT("__MeshUids__Key"));
			return MeshUids_BaseKey;
		}

		const FAttributeKey& FStaticMeshNodeLodDataStaticData::GetBoxCollisionMeshUidsBaseKey()
		{
			static FAttributeKey CollisionMeshUids_BaseKey(TEXT("__BoxCollisionMeshUids__Key"));
			return CollisionMeshUids_BaseKey;
		}

		const FAttributeKey& FStaticMeshNodeLodDataStaticData::GetCapsuleCollisionMeshUidsBaseKey()
		{
			static FAttributeKey CollisionMeshUids_BaseKey(TEXT("__CapsuleCollisionMeshUids__Key"));
			return CollisionMeshUids_BaseKey;
		}

		const FAttributeKey& FStaticMeshNodeLodDataStaticData::GetSphereCollisionMeshUidsBaseKey()
		{
			static FAttributeKey CollisionMeshUids_BaseKey(TEXT("__SphereCollisionMeshUids__Key"));
			return CollisionMeshUids_BaseKey;
		}

		const FAttributeKey& FStaticMeshNodeLodDataStaticData::GetConvexCollisionMeshUidsBaseKey()
		{
			static FAttributeKey CollisionMeshUids_BaseKey(TEXT("__ConvexCollisionMeshUids__Key"));
			return CollisionMeshUids_BaseKey;
		}

	} // namespace Interchange
} // namespace UE


UInterchangeStaticMeshLodDataNode::UInterchangeStaticMeshLodDataNode()
{
	using namespace UE::Interchange;

	MeshUids.Initialize(Attributes, FStaticMeshNodeLodDataStaticData::GetMeshUidsBaseKey().ToString());
	BoxCollisionMeshUids.Initialize(Attributes.ToSharedRef(), FStaticMeshNodeLodDataStaticData::GetBoxCollisionMeshUidsBaseKey().ToString());
	CapsuleCollisionMeshUids.Initialize(Attributes.ToSharedRef(), FStaticMeshNodeLodDataStaticData::GetCapsuleCollisionMeshUidsBaseKey().ToString());
	SphereCollisionMeshUids.Initialize(Attributes.ToSharedRef(), FStaticMeshNodeLodDataStaticData::GetSphereCollisionMeshUidsBaseKey().ToString());
	ConvexCollisionMeshUids.Initialize(Attributes.ToSharedRef(), FStaticMeshNodeLodDataStaticData::GetConvexCollisionMeshUidsBaseKey().ToString());
}

/**
 * Return the node type name of the class. This is used when reporting errors.
 */
FString UInterchangeStaticMeshLodDataNode::GetTypeName() const
{
	const FString TypeName = TEXT("StaticMeshLodDataNode");
	return TypeName;
}

#if WITH_EDITOR

FString UInterchangeStaticMeshLodDataNode::GetKeyDisplayName(const UE::Interchange::FAttributeKey& NodeAttributeKey) const
{
	FString KeyDisplayName = NodeAttributeKey.ToString();
	const FString NodeAttributeKeyString = KeyDisplayName;
	if (NodeAttributeKey == UE::Interchange::FStaticMeshNodeLodDataStaticData::GetMeshUidsBaseKey())
	{
		KeyDisplayName = TEXT("Mesh count");
		return KeyDisplayName;
	}
	else if (NodeAttributeKeyString.StartsWith(UE::Interchange::FStaticMeshNodeLodDataStaticData::GetMeshUidsBaseKey().ToString()))
	{
		KeyDisplayName = TEXT("Mesh index ");
		const FString IndexKey = UE::Interchange::TArrayAttributeHelper<FString>::IndexKey();
		int32 IndexPosition = NodeAttributeKeyString.Find(IndexKey) + IndexKey.Len();
		if (IndexPosition < NodeAttributeKeyString.Len())
		{
			KeyDisplayName += NodeAttributeKeyString.RightChop(IndexPosition);
		}
		return KeyDisplayName;
	}
	else if (NodeAttributeKey == UE::Interchange::FStaticMeshNodeLodDataStaticData::GetBoxCollisionMeshUidsBaseKey())
	{
		KeyDisplayName = TEXT("Box collision count");
		return KeyDisplayName;
	}
	else if (NodeAttributeKeyString.StartsWith(UE::Interchange::FStaticMeshNodeLodDataStaticData::GetBoxCollisionMeshUidsBaseKey().ToString()))
	{
		KeyDisplayName = TEXT("Box collision index ");
		const FString IndexKey = UE::Interchange::TArrayAttributeHelper<FString>::IndexKey();
		int32 IndexPosition = NodeAttributeKeyString.Find(IndexKey) + IndexKey.Len();
		if (IndexPosition < NodeAttributeKeyString.Len())
		{
			KeyDisplayName += NodeAttributeKeyString.RightChop(IndexPosition);
		}
		return KeyDisplayName;
	}
	else if (NodeAttributeKey == UE::Interchange::FStaticMeshNodeLodDataStaticData::GetCapsuleCollisionMeshUidsBaseKey())
	{
		KeyDisplayName = TEXT("Capsule collision count");
		return KeyDisplayName;
	}
	else if (NodeAttributeKeyString.StartsWith(UE::Interchange::FStaticMeshNodeLodDataStaticData::GetCapsuleCollisionMeshUidsBaseKey().ToString()))
	{
		KeyDisplayName = TEXT("Capsule collision index ");
		const FString IndexKey = UE::Interchange::TArrayAttributeHelper<FString>::IndexKey();
		int32 IndexPosition = NodeAttributeKeyString.Find(IndexKey) + IndexKey.Len();
		if (IndexPosition < NodeAttributeKeyString.Len())
		{
			KeyDisplayName += NodeAttributeKeyString.RightChop(IndexPosition);
		}
		return KeyDisplayName;
	}
	else if (NodeAttributeKey == UE::Interchange::FStaticMeshNodeLodDataStaticData::GetSphereCollisionMeshUidsBaseKey())
	{
		KeyDisplayName = TEXT("Sphere collision count");
		return KeyDisplayName;
	}
	else if (NodeAttributeKeyString.StartsWith(UE::Interchange::FStaticMeshNodeLodDataStaticData::GetSphereCollisionMeshUidsBaseKey().ToString()))
	{
		KeyDisplayName = TEXT("Sphere collision index ");
		const FString IndexKey = UE::Interchange::TArrayAttributeHelper<FString>::IndexKey();
		int32 IndexPosition = NodeAttributeKeyString.Find(IndexKey) + IndexKey.Len();
		if (IndexPosition < NodeAttributeKeyString.Len())
		{
			KeyDisplayName += NodeAttributeKeyString.RightChop(IndexPosition);
		}
		return KeyDisplayName;
	}
	else if (NodeAttributeKey == UE::Interchange::FStaticMeshNodeLodDataStaticData::GetConvexCollisionMeshUidsBaseKey())
	{
		KeyDisplayName = TEXT("Convex collision count");
		return KeyDisplayName;
	}
	else if (NodeAttributeKeyString.StartsWith(UE::Interchange::FStaticMeshNodeLodDataStaticData::GetConvexCollisionMeshUidsBaseKey().ToString()))
	{
		KeyDisplayName = TEXT("Convex collision index ");
		const FString IndexKey = UE::Interchange::TArrayAttributeHelper<FString>::IndexKey();
		int32 IndexPosition = NodeAttributeKeyString.Find(IndexKey) + IndexKey.Len();
		if (IndexPosition < NodeAttributeKeyString.Len())
		{
			KeyDisplayName += NodeAttributeKeyString.RightChop(IndexPosition);
		}
		return KeyDisplayName;
	}
	else if (NodeAttributeKey == Macro_CustomOneConvexHullPerUCXKey)
	{
		KeyDisplayName = TEXT("One Convex Hull Per UCX");
		return KeyDisplayName;
	}
	else if (NodeAttributeKey == Macro_CustomImportCollisionKey)
	{
		KeyDisplayName = TEXT("Import Collision");
		return KeyDisplayName;
	}

	return Super::GetKeyDisplayName(NodeAttributeKey);
}

FString UInterchangeStaticMeshLodDataNode::GetAttributeCategory(const UE::Interchange::FAttributeKey& NodeAttributeKey) const
{
	if (NodeAttributeKey.ToString().StartsWith(UE::Interchange::FStaticMeshNodeLodDataStaticData::GetMeshUidsBaseKey().ToString()))
	{
		return FString(TEXT("Meshes"));
	}
	else if (NodeAttributeKey.ToString().StartsWith(UE::Interchange::FStaticMeshNodeLodDataStaticData::GetBoxCollisionMeshUidsBaseKey().ToString()))
	{
		return FString(TEXT("Box Collisions"));
	}
	else if (NodeAttributeKey.ToString().StartsWith(UE::Interchange::FStaticMeshNodeLodDataStaticData::GetCapsuleCollisionMeshUidsBaseKey().ToString()))
	{
		return FString(TEXT("Capsule Collisions"));
	}
	else if (NodeAttributeKey.ToString().StartsWith(UE::Interchange::FStaticMeshNodeLodDataStaticData::GetSphereCollisionMeshUidsBaseKey().ToString()))
	{
		return FString(TEXT("Sphere Collisions"));
	}
	else if (NodeAttributeKey.ToString().StartsWith(UE::Interchange::FStaticMeshNodeLodDataStaticData::GetConvexCollisionMeshUidsBaseKey().ToString()))
	{
		return FString(TEXT("Convex Collisions"));
	}
	return Super::GetAttributeCategory(NodeAttributeKey);
}

#endif //WITH_EDITOR

int32 UInterchangeStaticMeshLodDataNode::GetMeshUidsCount() const
{
	return MeshUids.GetCount();
}

void UInterchangeStaticMeshLodDataNode::GetMeshUids(TArray<FString>& OutMeshNames) const
{
	MeshUids.GetItems(OutMeshNames);
}

bool UInterchangeStaticMeshLodDataNode::AddMeshUid(const FString& MeshName)
{
	return MeshUids.AddItem(MeshName);
}

bool UInterchangeStaticMeshLodDataNode::RemoveMeshUid(const FString& MeshName)
{
	return MeshUids.RemoveItem(MeshName);
}

bool UInterchangeStaticMeshLodDataNode::RemoveAllMeshes()
{
	return MeshUids.RemoveAllItems();
}

int32 UInterchangeStaticMeshLodDataNode::GetBoxCollisionMeshUidsCount() const
{
	return BoxCollisionMeshUids.GetCount();
}

TMap<FString, FString> UInterchangeStaticMeshLodDataNode::GetBoxCollisionMeshMap() const
{
	return BoxCollisionMeshUids.ToMap();
}

// Deprecated
void UInterchangeStaticMeshLodDataNode::GetBoxCollisionMeshUids(TArray<FString>& OutColliderUids) const
{
	TMap<FString, FString> Map = GetBoxCollisionMeshMap();
	Map.GetKeys(OutColliderUids);
}

void UInterchangeStaticMeshLodDataNode::GetBoxColliderRenderMeshUid(const FString& InColliderMeshUid, FString& OutRenderMeshUid) const
{
	BoxCollisionMeshUids.GetValue(InColliderMeshUid, OutRenderMeshUid);
}

bool UInterchangeStaticMeshLodDataNode::AddBoxCollisionMeshUid(const FString& ColliderMeshUid)
{
	return false;
}

bool UInterchangeStaticMeshLodDataNode::AddBoxCollisionMeshUids(const FString& ColliderMeshUid, const FString& RenderMeshUid)
{
	return BoxCollisionMeshUids.SetKeyValue(ColliderMeshUid, RenderMeshUid);
}

bool UInterchangeStaticMeshLodDataNode::RemoveBoxCollisionMeshUid(const FString& ColliderMeshUid)
{
	return BoxCollisionMeshUids.RemoveKey(ColliderMeshUid);
}

bool UInterchangeStaticMeshLodDataNode::RemoveAllBoxCollisionMeshes()
{
	bool bHadItems = GetBoxCollisionMeshUidsCount() > 0;
	BoxCollisionMeshUids.Empty();
	return bHadItems;
}

int32 UInterchangeStaticMeshLodDataNode::GetCapsuleCollisionMeshUidsCount() const
{
	return CapsuleCollisionMeshUids.GetCount();
}

TMap<FString, FString> UInterchangeStaticMeshLodDataNode::GetCapsuleCollisionMeshMap() const
{
	return CapsuleCollisionMeshUids.ToMap();
}

// Deprecated
void UInterchangeStaticMeshLodDataNode::GetCapsuleCollisionMeshUids(TArray<FString>& OutColliderUids) const
{
	TMap<FString, FString> Map = GetCapsuleCollisionMeshMap();
	Map.GetKeys(OutColliderUids);
}

void UInterchangeStaticMeshLodDataNode::GetCapsuleColliderRenderMeshUid(const FString& InColliderMeshUid, FString& OutRenderMeshUid) const
{
	CapsuleCollisionMeshUids.GetValue(InColliderMeshUid, OutRenderMeshUid);
}

bool UInterchangeStaticMeshLodDataNode::AddCapsuleCollisionMeshUid(const FString& ColliderMeshUid)
{
	return false;
}

bool UInterchangeStaticMeshLodDataNode::AddCapsuleCollisionMeshUids(const FString& ColliderMeshUid, const FString& RenderMeshUid)
{
	return CapsuleCollisionMeshUids.SetKeyValue(ColliderMeshUid, RenderMeshUid);
}

bool UInterchangeStaticMeshLodDataNode::RemoveCapsuleCollisionMeshUid(const FString& ColliderMeshUid)
{
	return CapsuleCollisionMeshUids.RemoveKey(ColliderMeshUid);
}

bool UInterchangeStaticMeshLodDataNode::RemoveAllCapsuleCollisionMeshes()
{
	bool bHadItems = GetCapsuleCollisionMeshUidsCount() > 0;
	CapsuleCollisionMeshUids.Empty();
	return bHadItems;
}

int32 UInterchangeStaticMeshLodDataNode::GetSphereCollisionMeshUidsCount() const
{
	return SphereCollisionMeshUids.GetCount();
}

TMap<FString, FString> UInterchangeStaticMeshLodDataNode::GetSphereCollisionMeshMap() const
{
	return SphereCollisionMeshUids.ToMap();
}

// Deprecated
void UInterchangeStaticMeshLodDataNode::GetSphereCollisionMeshUids(TArray<FString>& OutColliderUids) const
{
	TMap<FString, FString> Map = GetSphereCollisionMeshMap();
	Map.GetKeys(OutColliderUids);
}

void UInterchangeStaticMeshLodDataNode::GetSphereColliderRenderMeshUid(const FString& InColliderMeshUid, FString& OutRenderMeshUid) const
{
	SphereCollisionMeshUids.GetValue(InColliderMeshUid, OutRenderMeshUid);
}

bool UInterchangeStaticMeshLodDataNode::AddSphereCollisionMeshUid(const FString& ColliderMeshUid)
{
	return false;
}

bool UInterchangeStaticMeshLodDataNode::AddSphereCollisionMeshUids(const FString& ColliderMeshUid, const FString& RenderMeshUid)
{
	return SphereCollisionMeshUids.SetKeyValue(ColliderMeshUid, RenderMeshUid);
}

bool UInterchangeStaticMeshLodDataNode::RemoveSphereCollisionMeshUid(const FString& ColliderMeshUid)
{
	return SphereCollisionMeshUids.RemoveKey(ColliderMeshUid);
}

bool UInterchangeStaticMeshLodDataNode::RemoveAllSphereCollisionMeshes()
{
	bool bHadItems = GetSphereCollisionMeshUidsCount() > 0;
	SphereCollisionMeshUids.Empty();
	return bHadItems;
}

int32 UInterchangeStaticMeshLodDataNode::GetConvexCollisionMeshUidsCount() const
{
	return ConvexCollisionMeshUids.GetCount();
}

TMap<FString, FString> UInterchangeStaticMeshLodDataNode::GetConvexCollisionMeshMap() const
{
	return ConvexCollisionMeshUids.ToMap();
}

// Deprecated
void UInterchangeStaticMeshLodDataNode::GetConvexCollisionMeshUids(TArray<FString>& OutColliderUids) const
{
	TMap<FString, FString> Map = GetConvexCollisionMeshMap();
	Map.GetKeys(OutColliderUids);
}

void UInterchangeStaticMeshLodDataNode::GetConvexColliderRenderMeshUid(const FString& InColliderMeshUid, FString& OutRenderMeshUid) const
{
	ConvexCollisionMeshUids.GetValue(InColliderMeshUid, OutRenderMeshUid);
}

bool UInterchangeStaticMeshLodDataNode::AddConvexCollisionMeshUid(const FString& ColliderMeshUid)
{
	return false;
}

bool UInterchangeStaticMeshLodDataNode::AddConvexCollisionMeshUids(const FString& ColliderMeshUid, const FString& RenderMeshUid)
{
	return ConvexCollisionMeshUids.SetKeyValue(ColliderMeshUid, RenderMeshUid);
}

bool UInterchangeStaticMeshLodDataNode::RemoveConvexCollisionMeshUid(const FString& ColliderMeshUid)
{
	return ConvexCollisionMeshUids.RemoveKey(ColliderMeshUid);
}

bool UInterchangeStaticMeshLodDataNode::RemoveAllConvexCollisionMeshes()
{
	bool bHadItems = GetConvexCollisionMeshUidsCount() > 0;
	ConvexCollisionMeshUids.Empty();
	return bHadItems;
}

bool UInterchangeStaticMeshLodDataNode::GetOneConvexHullPerUCX(bool& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(OneConvexHullPerUCX, bool);
}

bool UInterchangeStaticMeshLodDataNode::SetOneConvexHullPerUCX(bool AttributeValue)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER_NODELEGATE(OneConvexHullPerUCX, bool)
}

bool UInterchangeStaticMeshLodDataNode::GetImportCollisionType(EInterchangeMeshCollision& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(ImportCollisionType, EInterchangeMeshCollision);
}

bool UInterchangeStaticMeshLodDataNode::SetImportCollisionType(EInterchangeMeshCollision AttributeValue)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER_NODELEGATE(ImportCollisionType, EInterchangeMeshCollision)
}

bool UInterchangeStaticMeshLodDataNode::GetForceCollisionPrimitiveGeneration(bool& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(ForceCollisionPrimitiveGeneration, bool);
}

bool UInterchangeStaticMeshLodDataNode::SetForceCollisionPrimitiveGeneration(bool AttributeValue)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER_NODELEGATE(ForceCollisionPrimitiveGeneration, bool);
}

bool UInterchangeStaticMeshLodDataNode::GetImportCollision(bool& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(ImportCollision, bool);
}

bool UInterchangeStaticMeshLodDataNode::SetImportCollision(bool AttributeValue)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER_NODELEGATE(ImportCollision, bool);
}

bool UInterchangeStaticMeshLodDataNode::IsEditorOnlyDataDefined()
{
#if WITH_EDITORONLY_DATA
	return true;
#else
	return false;
#endif
}

