// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if USE_EMBREE_MAJOR_VERSION >= 4
	#include <embree4/rtcore.h>
#else
	#include <embree3/rtcore.h>
#endif

#include "Cluster.h"

namespace Nanite
{

struct FRay1 : public RTCRayHit
{
	void SetRay( FVector3f Origin, FVector3f Direction, FVector2f Time )
	{
		ray.org_x	= Origin.X;
		ray.org_y	= Origin.Y;
		ray.org_z	= Origin.Z;
		ray.dir_x	= Direction.X;
		ray.dir_y	= Direction.Y;
		ray.dir_z	= Direction.Z;
		ray.tnear	= Time[0];
		ray.tfar 	= Time[1];
		ray.mask	= 0xFFFFFFFF;
		hit.geomID	= RTC_INVALID_GEOMETRY_ID;
		hit.primID	= RTC_INVALID_GEOMETRY_ID;
	}
};

template< typename T >
struct TRayN : public T
{
	void SetRay( uint32 Index, FVector3f Origin, FVector3f Direction, FVector2f Time )
	{
		this->ray.org_x[ Index ]	= Origin.X;
		this->ray.org_y[ Index ]	= Origin.Y;
		this->ray.org_z[ Index ]	= Origin.Z;
		this->ray.dir_x[ Index ]	= Direction.X;
		this->ray.dir_y[ Index ]	= Direction.Y;
		this->ray.dir_z[ Index ]	= Direction.Z;
		this->ray.tnear[ Index ]	= Time[0];
		this->ray.tfar [ Index ]	= Time[1];
		this->ray.mask [ Index ]	= 0xFFFFFFFF;
		this->hit.geomID[ Index ]	= RTC_INVALID_GEOMETRY_ID;
		this->hit.primID[ Index ]	= RTC_INVALID_GEOMETRY_ID;
	}
};

using FRay4  = TRayN< RTCRayHit4 >;
using FRay8  = TRayN< RTCRayHit8 >;
using FRay16 = TRayN< RTCRayHit16 >;

class FCluster;
class FRayTracingScene
{
	RTCDevice	Device	= nullptr;
	RTCScene	Scene	= nullptr;

	struct FClusterTriRef
	{
		uint32 ClusterIndex	: 25;
		uint32 TriIndex		: 7;
	};
	TArray< FClusterTriRef > ClusterTriRefs;
public:
	FRayTracingScene( TArray< FCluster >& Clusters, uint32 ClusterOffset, uint32 NumClusters );
	FRayTracingScene( TArray< FCluster >& Clusters ) : FRayTracingScene(Clusters, 0, Clusters.Num()) {}
	~FRayTracingScene();

	void	Intersect1( FRay1& Ray ) const
	{
#if USE_EMBREE_MAJOR_VERSION >= 4
		rtcIntersect1( Scene, &Ray );
#else
		RTCIntersectContext Context;
		rtcInitIntersectContext( &Context );
		rtcIntersect1( Scene, &Context, &Ray );
#endif
	}

	void	Intersect16( FRay16& Ray ) const
	{
		int Valid[16];
		FMemory::Memset( Valid, 0xff );

#if USE_EMBREE_MAJOR_VERSION >= 4
		rtcIntersect16( Valid, Scene, &Ray );
#else
		RTCIntersectContext Context;
		rtcInitIntersectContext( &Context );
		rtcIntersect16( Valid, Scene, &Context, &Ray );
#endif
	}

	bool	GetHit( const FRay1& Ray, uint32& HitClusterIndex, uint32& HitTriIndex, FVector3f& HitBarycentrics ) const
	{
		if( Ray.hit.geomID != RTC_INVALID_GEOMETRY_ID && Ray.hit.primID != RTC_INVALID_GEOMETRY_ID )
		{
			const FClusterTriRef& Ref = ClusterTriRefs[ Ray.hit.primID ];
			HitClusterIndex		= Ref.ClusterIndex;
			HitTriIndex			= Ref.TriIndex;
			HitBarycentrics.X	= 1.0f - Ray.hit.u - Ray.hit.v;
			HitBarycentrics.Y	= Ray.hit.u;
			HitBarycentrics.Z	= Ray.hit.v;
			return true;
		}

		return false;
	}

	template< typename T >
	bool	GetHit( const TRayN<T>& Ray, uint32 Index, uint32& HitClusterIndex, uint32& HitTriIndex, FVector3f& HitBarycentrics ) const
	{
		if( Ray.hit.geomID[ Index ] != RTC_INVALID_GEOMETRY_ID && Ray.hit.primID[ Index ] != RTC_INVALID_GEOMETRY_ID )
		{
			const FClusterTriRef& Ref = ClusterTriRefs[ Ray.hit.primID[ Index ] ];
			HitClusterIndex		= Ref.ClusterIndex;
			HitTriIndex			= Ref.TriIndex;
			HitBarycentrics.X	= 1.0f - Ray.hit.u[ Index ] - Ray.hit.v[ Index ];
			HitBarycentrics.Y	= Ray.hit.u[ Index ];
			HitBarycentrics.Z	= Ray.hit.v[ Index ];
			return true;
		}
		return false;
	}
};

} // namespace Nanite