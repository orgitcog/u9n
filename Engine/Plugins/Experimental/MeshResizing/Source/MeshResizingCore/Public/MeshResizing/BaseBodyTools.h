// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 
#include "UObject/NameTypes.h"
#include "Containers/Array.h"

namespace UE::Geometry
{
	class FDynamicMesh3;
}
namespace UE::MeshResizing
{
	struct MESHRESIZINGCORE_API FBaseBodyTools
	{
		static const FName ImportedVertexVIDsAttrName;
		static const FName RawPointIndicesVIDsAttrName;

		/** @return success */
		static bool AttachVertexMappingData(const FName& AttrName, const TArray<int32>& Data, UE::Geometry::FDynamicMesh3& Mesh);

		/** @return success */
		static bool GenerateResizableProxyFromVertexMappingData(const UE::Geometry::FDynamicMesh3& SourceMesh, const FName& SourceMappingName, const UE::Geometry::FDynamicMesh3& TargetMesh, const FName& TargetMappingName, UE::Geometry::FDynamicMesh3& ProxyMesh);

		/** @return success */
		static bool InterpolateResizableProxy(const UE::Geometry::FDynamicMesh3& SourceMesh, const UE::Geometry::FDynamicMesh3& TargetMesh, float BlendAlpha, UE::Geometry::FDynamicMesh3& ProxyMesh);


	};
}

