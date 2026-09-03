// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VectorUtil.h"

namespace Rasterizer
{

constexpr int32 SubpixelBits = 8;
constexpr int32 SubpixelSamples = 1 << SubpixelBits;

FORCEINLINE int32 EdgeC( const FIntVector2& Edge, const FIntVector2& Vert, int32 SubpixelDilate = 0 )
{
	int64 ex = Edge.X;
	int64 ey = Edge.Y;
	int64 vx = Vert.X;
	int64 vy = Vert.Y;

	// Half-edge constants
	// 24.16 fixed point
	int64 C = ey * vx - ex * vy;

	// Correct for fill convention
	// Top left rule for CCW
	C -= ( Edge.Y < 0 || ( Edge.Y == 0 && Edge.X > 0 ) ) ? 0 : 1;

	// Dilate edges
	C += ( FMath::Abs( Edge.X ) + FMath::Abs( Edge.Y ) ) * SubpixelDilate;

	// Step in pixel increments
	// Low bits would always be the same and thus don't matter when testing sign.
	// 24.8 fixed point
	return int32( C >> SubpixelBits );
};

// Fixed point rasterization
struct FTriangle
{
	FIntVector2	Vert0;
	FIntVector2	Vert1;
	FIntVector2	Vert2;

	FIntVector2	MinPixel;
	FIntVector2	MaxPixel;

	FIntVector2	Edge01;
	FIntVector2	Edge12;
	FIntVector2	Edge20;

	int32		C0;
	int32		C1;
	int32		C2;

	bool		bBackFace;

	FTriangle( const FVector3f Verts[3], FIntVector2 ScissorMin, FIntVector2 ScissorMax, int32 SubpixelDilate = 0 )
	{
		// 24.8 fixed point
		Vert0 = RoundToInt( FVector2f( Verts[0] ) * SubpixelSamples );
		Vert1 = RoundToInt( FVector2f( Verts[1] ) * SubpixelSamples );
		Vert2 = RoundToInt( FVector2f( Verts[2] ) * SubpixelSamples );

		// Bounding rect
		FIntVector2 MinSubpixel = Min3( Vert0, Vert1, Vert2 );
		FIntVector2 MaxSubpixel = Max3( Vert0, Vert1, Vert2 );

		MinSubpixel -= SubpixelDilate;
		MaxSubpixel += SubpixelDilate;

		// Round to nearest pixel
		MinPixel = ( ( MinSubpixel + (SubpixelSamples / 2) - 1 ) ) / SubpixelSamples;
		MaxPixel = ( ( MaxSubpixel + (SubpixelSamples / 2) - 1 ) ) / SubpixelSamples;

		// Scissor
		MinPixel = Max( MinPixel, ScissorMin );
		MaxPixel = Min( MaxPixel, ScissorMax );

		// Rebase off MinPixel with half pixel offset
		// 12.8 fixed point
		// Max triangle size = 2047x2047 pixels
		const FIntVector2 BaseSubpixel = MinPixel * SubpixelSamples + (SubpixelSamples / 2);
		Vert0 -= BaseSubpixel;
		Vert1 -= BaseSubpixel;
		Vert2 -= BaseSubpixel;

		// 12.8 fixed point
		Edge01 = Vert0 - Vert1;
		Edge12 = Vert1 - Vert2;
		Edge20 = Vert2 - Vert0;

		// 24.16 fixed point
		int64 DetXY = Edge01.Y * Edge20.X - Edge01.X * Edge20.Y;
		bBackFace = DetXY >= 0;
		if( bBackFace )
		{
			// Swap winding order
			Edge01 *= -1;
			Edge12 *= -1;
			Edge20 *= -1;
		}

		C0 = EdgeC( Edge12, Vert1, SubpixelDilate );
		C1 = EdgeC( Edge20, Vert2, SubpixelDilate );
		C2 = EdgeC( Edge01, Vert0, SubpixelDilate );
	}

	bool IsCovered( int32 x, int32 y ) const
	{
		x -= MinPixel.X;
		y -= MinPixel.Y;

		int32 CX0 = C0 - x * Edge12.Y + y * Edge12.X;
		int32 CX1 = C1 - x * Edge20.Y + y * Edge20.X;
		int32 CX2 = C2 - x * Edge01.Y + y * Edge01.X;

		return ( CX0 | CX1 | CX2 ) >= 0;
	}

	template< typename FFunc >
	void ForAllCovered( FFunc&& Func ) const
	{
		int32 CY0 = C0;
		int32 CY1 = C1;
		int32 CY2 = C2;

		for( int32 y = MinPixel.Y; y < MaxPixel.Y; y++ )
		{
			int32 CX0 = CY0;
			int32 CX1 = CY1;
			int32 CX2 = CY2;

			for( int32 x = MinPixel.X; x < MaxPixel.X; x++ )
			{
				if( ( CX0 | CX1 | CX2 ) >= 0 )
					Func( x, y );

				CX0 -= Edge12.Y;
				CX1 -= Edge20.Y;
				CX2 -= Edge01.Y;
			}

			CY0 += Edge12.X;
			CY1 += Edge20.X;
			CY2 += Edge01.X;
		}
	}

	FVector3f GetBarycentrics( int32 x, int32 y ) const
	{
		FIntVector2 p = ( FIntVector2(x,y) - MinPixel ) * SubpixelSamples;
		FVector2f p0( Vert0 - p );
		FVector2f p1( Vert1 - p );
		FVector2f p2( Vert2 - p );

		// Not perspective correct
		FVector3f Barycentrics(
			(float)Edge12.Y * p1.X - (float)Edge12.X * p1.Y,
			(float)Edge20.Y * p2.X - (float)Edge20.X * p2.Y,
			(float)Edge01.Y * p0.X - (float)Edge01.X * p0.Y );
		Barycentrics /= Barycentrics[0] + Barycentrics[1] + Barycentrics[2];

		return Barycentrics;
	}
};

FORCEINLINE bool IsCovered( const FVector2f& Edge, const FVector2f& Vert, const FVector2f& Center, const FVector2f& Extent )
{
	FVector2f Point = Vert - Center;
	float Barycentric = Edge.Y * Point.X - Edge.X * Point.Y;

	Barycentric += FMath::Abs( Edge.Y ) * Extent.X;
	Barycentric += FMath::Abs( Edge.X ) * Extent.Y;

	// Correct for fill convention
	// Top left rule for CCW
	bool bTopLeft = Edge.Y < 0.0f || ( Edge.Y == 0.0f && Edge.X > 0.0f );
	bool bIsInside = Barycentric > 0.0f || ( Barycentric == 0.0f && bTopLeft );
	return bIsInside;
}

// Floating point rasterization
struct FTriangle3f
{
	FVector3f	Vert0;
	FVector3f	Vert1;
	FVector3f	Vert2;

	FVector3f	Min;
	FVector3f	Max;

	FVector3f	Edge01;
	FVector3f	Edge12;
	FVector3f	Edge20;

	FVector4f	Plane;

	FTriangle3f() {}
	FTriangle3f( const FVector3f Verts[3] )
	{
		Vert0 = Verts[0];
		Vert1 = Verts[1];
		Vert2 = Verts[2];

		Min = Min3( Vert0, Vert1, Vert2 );
		Max = Max3( Vert0, Vert1, Vert2 );

		Edge01 = Vert0 - Vert1;
		Edge12 = Vert1 - Vert2;
		Edge20 = Vert2 - Vert0;

		FVector3f Normal = Edge01 ^ Edge20;
		Plane = FVector4f( Normal, -( Normal | Vert0 ) );
	}

	FVector3f GetBarycentrics( float x, float y ) const
	{
		FVector2f p( x, y );
		FVector2f p0 = FVector2f( Vert0 ) - p;
		FVector2f p1 = FVector2f( Vert1 ) - p;
		FVector2f p2 = FVector2f( Vert2 ) - p;

		// Not perspective correct
		FVector3f Barycentrics(
			Edge12.Y * p1.X - Edge12.X * p1.Y,
			Edge20.Y * p2.X - Edge20.X * p2.Y,
			Edge01.Y * p0.X - Edge01.X * p0.Y );
		Barycentrics /= Barycentrics[0] + Barycentrics[1] + Barycentrics[2];

		return Barycentrics;
	}

	bool IsCovered( const FVector2f& Center, const FVector2f& Extent ) const
	{
		float Sign = Plane.Z >= 0.0f ? 1.0f : -1.0f;
		return
			Rasterizer::IsCovered( Sign * FVector2f( Edge12 ), FVector2f( Vert1 ), Center, Extent ) &&
			Rasterizer::IsCovered( Sign * FVector2f( Edge20 ), FVector2f( Vert2 ), Center, Extent ) &&
			Rasterizer::IsCovered( Sign * FVector2f( Edge01 ), FVector2f( Vert0 ), Center, Extent );
	}

	bool IsCovered( int32 x, int32 y, float PixelExtent = 0.0f ) const
	{
		return IsCovered( FVector2f( (float)x + 0.5f, (float)y + 0.5f ), FVector2f( PixelExtent ) );
	}

	bool IsCovered( FVector3f Origin, FVector3f Direction, FVector2f Time )
	{
		// Muller-Trumbore ray triangle intersect
		FVector3f Origin0 = Origin - Vert0;
		FVector3f Dirx20 = Direction ^ Edge20;

		float Det = -( Edge01 | Dirx20 );
		if( FMath::Abs( Det ) < 1e-8f )
			return false;
		float InvDet = 1.0f / Det;

		float V = InvDet * ( Origin0 | Dirx20 );
		float W = InvDet * ( Direction | ( Edge01 ^ Origin0 ) );
		float t = InvDet * ( Edge20    | ( Edge01 ^ Origin0 ) );

		if( V < 0.0f || V > 1.0f )
			return false;
		if( W < 0.0f || V + W > 1.0f )
			return false;
		if( t < Time[0] || t > Time[1] )
			return false;
		return true;
	}

	FVector3f GetDepthPlane() const
	{
		/*
			Solve for v.z
			n | (v - p) = 0;
			(n|v) - (n|p) = 0;
			(n.xy|v.xy) + n.z*z - (n|p) = 0;
			-(n.xy|v.xy)/n.z + (n|p)/n.z = v.z;
		*/
		return FVector3f( -Plane.X, -Plane.Y, -Plane.W ) / Plane.Z;
	}

	FTriangle3f Swizzle( int32 X, int32 Y, int32 Z )
	{
		FTriangle3f TriXYZ;
		TriXYZ.Vert0 = FVector3f( Vert0[X], Vert0[Y], Vert0[Z] );
		TriXYZ.Vert1 = FVector3f( Vert1[X], Vert1[Y], Vert1[Z] );
		TriXYZ.Vert2 = FVector3f( Vert2[X], Vert2[Y], Vert2[Z] );

		TriXYZ.Min = FVector3f( Min[X], Min[Y], Min[Z] );
		TriXYZ.Max = FVector3f( Max[X], Max[Y], Max[Z] );

		TriXYZ.Edge01 = FVector3f( Edge01[X], Edge01[Y], Edge01[Z] );
		TriXYZ.Edge12 = FVector3f( Edge12[X], Edge12[Y], Edge12[Z] );
		TriXYZ.Edge20 = FVector3f( Edge20[X], Edge20[Y], Edge20[Z] );

		TriXYZ.Plane = FVector4f( Plane[X], Plane[Y], Plane[Z], Plane.W );

		return TriXYZ;
	}
};

} // namespace Rasterizer

template< typename FWritePixel >
void RasterizeTri( const FVector3f Verts[3], const FIntRect& ScissorRect, uint32 SubpixelDilate, bool bBackFaceCull, FWritePixel&& WritePixel )
{
	Rasterizer::FTriangle Tri( Verts, ScissorRect.Min, ScissorRect.Max, SubpixelDilate );

	// Cull when no pixels covered
	if( Tri.MinPixel.X >= Tri.MaxPixel.X ||
		Tri.MinPixel.Y >= Tri.MaxPixel.Y )
		return;

	if( Tri.bBackFace && bBackFaceCull )
		return;

	Tri.ForAllCovered(
		[&]( int32 x, int32 y )
		{
			FVector3f Barycentrics = Tri.GetBarycentrics( x, y );

			float Depth =
				Verts[0].Z * Barycentrics[0] +
				Verts[1].Z * Barycentrics[1] +
				Verts[2].Z * Barycentrics[2];

			WritePixel( x, y, Depth, Barycentrics );
		} );
}

// 6-separating voxelization
template< typename FWriteVoxel >
void VoxelizeTri6( const FVector3f Verts[3], FWriteVoxel&& WriteVoxel )
{
	Rasterizer::FTriangle3f Tri( Verts );

	for( int32 SwizzleZ = 0; SwizzleZ < 3; SwizzleZ++ )
	{
		const int32 SwizzleX = ( 1 << SwizzleZ ) & 3;
		const int32 SwizzleY = ( 1 << SwizzleX ) & 3;

		Rasterizer::FTriangle3f TriZ = Tri.Swizzle( SwizzleX, SwizzleY, SwizzleZ );

		FVector3f DepthPlane = TriZ.GetDepthPlane();

		FIntVector3 MinVoxel = RoundToInt( TriZ.Min );
		FIntVector3 MaxVoxel = RoundToInt( TriZ.Max );	//exclusive
		
		for( int32 y = MinVoxel.Y; y < MaxVoxel.Y; y++ )
		{
			for( int32 x = MinVoxel.X; x < MaxVoxel.X; x++ )
			{
				FVector3f Barycentrics = TriZ.GetBarycentrics( (float)x + 0.5f, (float)y + 0.5f );

				if( !TriZ.IsCovered( x, y ) )
					continue;

				float CenterZ = DepthPlane | FVector3f( (float)x + 0.5f, (float)y + 0.5f, 1.0f );

				int32 z = FMath::FloorToInt( CenterZ );

				FIntVector3 Unswizzle;
				Unswizzle[ SwizzleX ] = x;
				Unswizzle[ SwizzleY ] = y;
				Unswizzle[ SwizzleZ ] = z;

				WriteVoxel( Unswizzle, Barycentrics );
			}
		}
	}
}

// 26-separating voxelization, conservative
template< typename FWriteVoxel >
void VoxelizeTri26( const FVector3f Verts[3], FWriteVoxel&& WriteVoxel )
{
	Rasterizer::FTriangle3f Tri( Verts );

	// Dominant direction
	const int32 SwizzleZ = FMath::Max3Index(
		FMath::Abs( Tri.Plane.X ),
		FMath::Abs( Tri.Plane.Y ),
		FMath::Abs( Tri.Plane.Z ) );
	const int32 SwizzleX = ( 1 << SwizzleZ ) & 3;
	const int32 SwizzleY = ( 1 << SwizzleX ) & 3;

	Rasterizer::FTriangle3f TriX = Tri.Swizzle( SwizzleY, SwizzleZ, SwizzleX );	// YZX
	Rasterizer::FTriangle3f TriY = Tri.Swizzle( SwizzleZ, SwizzleX, SwizzleY );	// ZXY
	Rasterizer::FTriangle3f TriZ = Tri.Swizzle( SwizzleX, SwizzleY, SwizzleZ );	// XYZ
		
	FVector3f DepthPlane = TriZ.GetDepthPlane();

	const float PixelExtent = 0.5f;
	float ExtentZ = PixelExtent * ( FMath::Abs( DepthPlane.X ) + FMath::Abs( DepthPlane.Y ) );

	FIntVector3 MinVoxel = RoundToInt( TriZ.Min - PixelExtent );
	FIntVector3 MaxVoxel = RoundToInt( TriZ.Max + PixelExtent );	//exclusive
		
	for( int32 y = MinVoxel.Y; y < MaxVoxel.Y; y++ )
	{
		for( int32 x = MinVoxel.X; x < MaxVoxel.X; x++ )
		{
			FVector3f Barycentrics = TriZ.GetBarycentrics( (float)x + 0.5f, (float)y + 0.5f );

			if( !TriZ.IsCovered( x, y, PixelExtent ) )
				continue;

			float CenterZ = DepthPlane | FVector3f( (float)x + 0.5f, (float)y + 0.5f, 1.0f );

			int32 MinZ = FMath::FloorToInt( CenterZ - ExtentZ );
			int32 MaxZ = FMath::FloorToInt( CenterZ + ExtentZ );

			MinZ = FMath::Max( MinZ, MinVoxel.Z );
			MaxZ = FMath::Min( MaxZ, MaxVoxel.Z - 1 );

			for( int32 z = MinZ; z <= MaxZ; z++ )
			{
				if( !TriX.IsCovered( y, z, PixelExtent ) )
					continue;
				if( !TriY.IsCovered( z, x, PixelExtent ) )
					continue;

				FIntVector3 Unswizzle;
				Unswizzle[ SwizzleX ] = x;
				Unswizzle[ SwizzleY ] = y;
				Unswizzle[ SwizzleZ ] = z;

				WriteVoxel( Unswizzle, Barycentrics );
			}
		}
	}
}