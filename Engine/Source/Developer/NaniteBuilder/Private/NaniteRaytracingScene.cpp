// Copyright Epic Games, Inc. All Rights Reserved.

#include "NaniteRayTracingScene.h"

//VOXELTODO: Implement support for occlusion ray tests?
//VOXELTODO: Investigate if Embree gives deterministic results that we want in builder code

namespace Nanite
{

LLM_DECLARE_TAG(Embree);

// userPtr is provided during callback registration, it points to the associated FEmbreeScene
static bool EmbreeMemoryMonitorRTScene(void* userPtr, ssize_t bytes, bool post)
{
	LLM_SCOPE_BYTAG(Embree);
	LLM_IF_ENABLED(FLowLevelMemTracker::Get().OnLowLevelChangeInMemoryUse(ELLMTracker::Default, static_cast<int64>(bytes)));

	return true;
}

FRayTracingScene::FRayTracingScene( TArray< FCluster >& Clusters, uint32 ClusterOffset, uint32 NumClusters )
{
	uint32 TotalVerts	= 0;
	uint32 TotalTris	= 0;

	for( uint32 ClusterIndex = ClusterOffset; ClusterIndex < ClusterOffset + NumClusters; ClusterIndex++ )
	{
		TotalVerts	+= Clusters[ClusterIndex].NumVerts;
		TotalTris	+= Clusters[ClusterIndex].NumTris;
	}

	ClusterTriRefs.SetNumUninitialized(TotalTris);

	Device = rtcNewDevice(NULL);
	LLM_IF_ENABLED(rtcSetDeviceMemoryMonitorFunction(Device, EmbreeMemoryMonitorRTScene, NULL));

	Scene = rtcNewScene(Device);

	RTCGeometry Geom = rtcNewGeometry( Device, RTC_GEOMETRY_TYPE_TRIANGLE );

	FVector3f* VertexPtr = (FVector3f*)	rtcSetNewGeometryBuffer( Geom, RTC_BUFFER_TYPE_VERTEX,	0, RTC_FORMAT_FLOAT3,	sizeof( FVector3f ),	TotalVerts );
	uint32*		IndexPtr = (uint32*)	rtcSetNewGeometryBuffer( Geom, RTC_BUFFER_TYPE_INDEX,	0, RTC_FORMAT_UINT3,	3 * sizeof( uint32 ),	TotalTris );

	uint32 NextTri = 0;
	uint32 NextIndex = 0;
	for( uint32 ClusterIndex = ClusterOffset; ClusterIndex < ClusterOffset + NumClusters; ClusterIndex++ )
	{
		const FCluster& Cluster = Clusters[ ClusterIndex ];

		for( uint32 i = 0; i < Cluster.NumVerts; i++ )
		{
			VertexPtr[ i ] = Cluster.GetPosition( i );
		}
		
		for( uint32 i = 0; i < Cluster.NumTris * 3; i++ )
		{
			IndexPtr[ i ] = NextIndex + Cluster.Indexes[ i ];
		}

		for( uint32 i = 0; i < Cluster.NumTris; i++ )
		{
			ClusterTriRefs[ NextTri + i ].ClusterIndex	= ClusterIndex;
			ClusterTriRefs[ NextTri + i ].TriIndex		= i;				// VOXELTODO: Optimize? This is very redundant
		}
		
		VertexPtr	+= Cluster.NumVerts;
		IndexPtr	+= Cluster.NumTris * 3;
		NextIndex	+= Cluster.NumVerts;
		NextTri		+= Cluster.NumTris;
	}

	rtcCommitGeometry( Geom );
	rtcAttachGeometry( Scene, Geom );
	rtcReleaseGeometry( Geom );
	rtcCommitScene( Scene );
}

FRayTracingScene::~FRayTracingScene()
{
	rtcReleaseScene( Scene );
	rtcReleaseDevice( Device );
}

} // namespace Nanite
