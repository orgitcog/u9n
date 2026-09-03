// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace UE::Math
{

template< typename T > FORCEINLINE TVector2<T> operator+( T S, const TVector2<T>& V ) { return TVector2<T>(S) + V; }
template< typename T > FORCEINLINE TVector <T> operator+( T S, const TVector <T>& V ) { return TVector <T>(S) + V; }
template< typename T > FORCEINLINE TVector4<T> operator+( T S, const TVector4<T>& V ) { return TVector4<T>(S) + V; }

template< typename T > FORCEINLINE TVector2<T> operator-( T S, const TVector2<T>& V ) { return TVector2<T>(S) - V; }
template< typename T > FORCEINLINE TVector <T> operator-( T S, const TVector <T>& V ) { return TVector <T>(S) - V; }
template< typename T > FORCEINLINE TVector4<T> operator-( T S, const TVector4<T>& V ) { return TVector4<T>(S) - V; }

template< typename T > FORCEINLINE TIntVector2<T> operator+( const TIntVector2<T>& V, T S ) { return V + TIntVector2<T>(S); }
template< typename T > FORCEINLINE TIntVector3<T> operator+( const TIntVector3<T>& V, T S ) { return V + TIntVector3<T>(S); }
template< typename T > FORCEINLINE TIntVector4<T> operator+( const TIntVector4<T>& V, T S ) { return V + TIntVector4<T>(S); }

template< typename T > FORCEINLINE TIntVector2<T> operator-( const TIntVector2<T>& V, T S ) { return V - TIntVector2<T>(S); }
template< typename T > FORCEINLINE TIntVector3<T> operator-( const TIntVector3<T>& V, T S ) { return V - TIntVector3<T>(S); }
template< typename T > FORCEINLINE TIntVector4<T> operator-( const TIntVector4<T>& V, T S ) { return V - TIntVector4<T>(S); }

template< typename T > FORCEINLINE TIntVector2<T> operator+( T S, const TIntVector2<T>& V ) { return TIntVector2<T>(S) + V; }
template< typename T > FORCEINLINE TIntVector3<T> operator+( T S, const TIntVector3<T>& V ) { return TIntVector3<T>(S) + V; }
template< typename T > FORCEINLINE TIntVector4<T> operator+( T S, const TIntVector4<T>& V ) { return TIntVector4<T>(S) + V; }

template< typename T > FORCEINLINE TIntVector2<T> operator-( T S, const TIntVector2<T>& V ) { return TIntVector2<T>(S) - V; }
template< typename T > FORCEINLINE TIntVector3<T> operator-( T S, const TIntVector3<T>& V ) { return TIntVector3<T>(S) - V; }
template< typename T > FORCEINLINE TIntVector4<T> operator-( T S, const TIntVector4<T>& V ) { return TIntVector4<T>(S) - V; }

template< typename T > FORCEINLINE TIntVector2<T> operator*( T S, const TIntVector2<T>& V ) { return TIntVector2<T>(S) * V; }
template< typename T > FORCEINLINE TIntVector3<T> operator*( T S, const TIntVector3<T>& V ) { return TIntVector3<T>(S) * V; }
template< typename T > FORCEINLINE TIntVector4<T> operator*( T S, const TIntVector4<T>& V ) { return TIntVector4<T>(S) * V; }

template< typename T > FORCEINLINE FVector2f operator+( const TIntVector2<T>& V, float S ) { return FVector2f(V) + S; }
template< typename T > FORCEINLINE FVector3f operator+( const TIntVector3<T>& V, float S ) { return FVector3f(V) + S; }
template< typename T > FORCEINLINE FVector4f operator+( const TIntVector4<T>& V, float S ) { return FVector4f(V) + S; }

template< typename T > FORCEINLINE FVector2f operator-( const TIntVector2<T>& V, float S ) { return FVector2f(V) - S; }
template< typename T > FORCEINLINE FVector3f operator-( const TIntVector3<T>& V, float S ) { return FVector3f(V) - S; }
template< typename T > FORCEINLINE FVector4f operator-( const TIntVector4<T>& V, float S ) { return FVector4f(V) - S; }

template< typename T > FORCEINLINE FVector2f operator+( float S, const TIntVector2<T>& V ) { return S + FVector2f(V); }
template< typename T > FORCEINLINE FVector3f operator+( float S, const TIntVector3<T>& V ) { return S + FVector3f(V); }
template< typename T > FORCEINLINE FVector4f operator+( float S, const TIntVector4<T>& V ) { return S + FVector4f(V); }

template< typename T > FORCEINLINE FVector2f operator-( float S, const TIntVector2<T>& V ) { return S - FVector2f(V); }
template< typename T > FORCEINLINE FVector3f operator-( float S, const TIntVector3<T>& V ) { return S - FVector3f(V); }
template< typename T > FORCEINLINE FVector4f operator-( float S, const TIntVector4<T>& V ) { return S - FVector4f(V); }

template< typename T > FORCEINLINE FVector2f operator*( const TIntVector2<T>& V, float S ) { return FVector2f(V) * S; }
template< typename T > FORCEINLINE FVector3f operator*( const TIntVector3<T>& V, float S ) { return FVector3f(V) * S; }
template< typename T > FORCEINLINE FVector4f operator*( const TIntVector4<T>& V, float S ) { return FVector4f(V) * S; }

template< typename T > FORCEINLINE FVector2f operator*( float S, const TIntVector2<T>& V ) { return S * FVector2f(V); }
template< typename T > FORCEINLINE FVector3f operator*( float S, const TIntVector3<T>& V ) { return S * FVector3f(V); }
template< typename T > FORCEINLINE FVector4f operator*( float S, const TIntVector4<T>& V ) { return S * FVector4f(V); }

template< typename T > FORCEINLINE FVector2f operator/( float S, const TVector2<T>& V ) { return TVector2<T>( S / V.X, S / V.Y ); }
template< typename T > FORCEINLINE FVector3f operator/( float S, const TVector <T>& V ) { return TVector <T>( S / V.X, S / V.Y, S / V.Z ); }
template< typename T > FORCEINLINE FVector4f operator/( float S, const TVector4<T>& V ) { return TVector4<T>( S / V.X, S / V.Y, S / V.Z, S / V.W ); }

template< typename T > FORCEINLINE TIntVector2<T>& operator+=( TIntVector2<T>& V, T S ) { return V += TIntVector2<T>(S); }
template< typename T > FORCEINLINE TIntVector3<T>& operator+=( TIntVector3<T>& V, T S ) { return V += TIntVector3<T>(S); }
template< typename T > FORCEINLINE TIntVector4<T>& operator+=( TIntVector4<T>& V, T S ) { return V += TIntVector4<T>(S); }

template< typename T > FORCEINLINE TIntVector2<T>& operator-=( TIntVector2<T>& V, T S ) { return V -= TIntVector2<T>(S); }
template< typename T > FORCEINLINE TIntVector3<T>& operator-=( TIntVector3<T>& V, T S ) { return V -= TIntVector3<T>(S); }
template< typename T > FORCEINLINE TIntVector4<T>& operator-=( TIntVector4<T>& V, T S ) { return V -= TIntVector4<T>(S); }

template< typename T > FORCEINLINE TIntVector2<T> Abs( const TIntVector2<T>& V ) { return TIntVector2<T>( FMath::Abs( V.X ), FMath::Abs( V.Y ) ); }
template< typename T > FORCEINLINE TIntVector3<T> Abs( const TIntVector3<T>& V ) { return TIntVector3<T>( FMath::Abs( V.X ), FMath::Abs( V.Y ), FMath::Abs( V.Z ) ); }
template< typename T > FORCEINLINE TIntVector4<T> Abs( const TIntVector4<T>& V ) { return TIntVector4<T>( FMath::Abs( V.X ), FMath::Abs( V.Y ), FMath::Abs( V.Z ), FMath::Abs( V.W ) ); }

template< typename T > FORCEINLINE TIntVector2<T> Min( const TIntVector2<T>& A, const TIntVector2<T>& B ) { return A.ComponentMin(B); }
template< typename T > FORCEINLINE TIntVector3<T> Min( const TIntVector3<T>& A, const TIntVector3<T>& B ) { return A.ComponentMin(B); }
template< typename T > FORCEINLINE TIntVector4<T> Min( const TIntVector4<T>& A, const TIntVector4<T>& B ) { return A.ComponentMin(B); }

template< typename T > FORCEINLINE TIntVector2<T> Max( const TIntVector2<T>& A, const TIntVector2<T>& B ) { return A.ComponentMax(B); }
template< typename T > FORCEINLINE TIntVector3<T> Max( const TIntVector3<T>& A, const TIntVector3<T>& B ) { return A.ComponentMax(B); }
template< typename T > FORCEINLINE TIntVector4<T> Max( const TIntVector4<T>& A, const TIntVector4<T>& B ) { return A.ComponentMax(B); }

template< typename T > FORCEINLINE TIntVector2<T> Min3( const TIntVector2<T>& A, const TIntVector2<T>& B, const TIntVector2<T>& C ) { return A.ComponentMin(B).ComponentMin(C); }
template< typename T > FORCEINLINE TIntVector3<T> Min3( const TIntVector3<T>& A, const TIntVector3<T>& B, const TIntVector3<T>& C ) { return A.ComponentMin(B).ComponentMin(C); }
template< typename T > FORCEINLINE TIntVector4<T> Min3( const TIntVector4<T>& A, const TIntVector4<T>& B, const TIntVector4<T>& C ) { return A.ComponentMin(B).ComponentMin(C); }

template< typename T > FORCEINLINE TIntVector2<T> Max3( const TIntVector2<T>& A, const TIntVector2<T>& B, const TIntVector2<T>& C ) { return A.ComponentMax(B).ComponentMax(C); }
template< typename T > FORCEINLINE TIntVector3<T> Max3( const TIntVector3<T>& A, const TIntVector3<T>& B, const TIntVector3<T>& C ) { return A.ComponentMax(B).ComponentMax(C); }
template< typename T > FORCEINLINE TIntVector4<T> Max3( const TIntVector4<T>& A, const TIntVector4<T>& B, const TIntVector4<T>& C ) { return A.ComponentMax(B).ComponentMax(C); }

template< typename T > FORCEINLINE TVector2<T> Abs( const TVector2<T>& V ) { return TVector2<T>( FMath::Abs( V.X ), FMath::Abs( V.Y ) ); }
template< typename T > FORCEINLINE TVector <T> Abs( const TVector <T>& V ) { return TVector <T>( FMath::Abs( V.X ), FMath::Abs( V.Y ), FMath::Abs( V.Z ) ); }
template< typename T > FORCEINLINE TVector4<T> Abs( const TVector4<T>& V ) { return TVector4<T>( FMath::Abs( V.X ), FMath::Abs( V.Y ), FMath::Abs( V.Z ), FMath::Abs( V.W ) ); }

template< typename T > FORCEINLINE TVector2<T> Min( const TVector2<T>& A, const TVector2<T>& B ) { return A.ComponentMin(B); }
template< typename T > FORCEINLINE TVector <T> Min( const TVector <T>& A, const TVector <T>& B ) { return A.ComponentMin(B); }
template< typename T > FORCEINLINE TVector4<T> Min( const TVector4<T>& A, const TVector4<T>& B ) { return A.ComponentMin(B); }

template< typename T > FORCEINLINE TVector2<T> Max( const TVector2<T>& A, const TVector2<T>& B ) { return A.ComponentMax(B); }
template< typename T > FORCEINLINE TVector <T> Max( const TVector <T>& A, const TVector <T>& B ) { return A.ComponentMax(B); }
template< typename T > FORCEINLINE TVector4<T> Max( const TVector4<T>& A, const TVector4<T>& B ) { return A.ComponentMax(B); }

template< typename T > FORCEINLINE TVector2<T> Min3( const TVector2<T>& A, const TVector2<T>& B, const TVector2<T>& C ) { return A.ComponentMin(B).ComponentMin(C); }
template< typename T > FORCEINLINE TVector <T> Min3( const TVector <T>& A, const TVector <T>& B, const TVector <T>& C ) { return A.ComponentMin(B).ComponentMin(C); }
template< typename T > FORCEINLINE TVector4<T> Min3( const TVector4<T>& A, const TVector4<T>& B, const TVector4<T>& C ) { return A.ComponentMin(B).ComponentMin(C); }

template< typename T > FORCEINLINE TVector2<T> Max3( const TVector2<T>& A, const TVector2<T>& B, const TVector2<T>& C ) { return A.ComponentMax(B).ComponentMax(C); }
template< typename T > FORCEINLINE TVector <T> Max3( const TVector <T>& A, const TVector <T>& B, const TVector <T>& C ) { return A.ComponentMax(B).ComponentMax(C); }
template< typename T > FORCEINLINE TVector4<T> Max3( const TVector4<T>& A, const TVector4<T>& B, const TVector4<T>& C ) { return A.ComponentMax(B).ComponentMax(C); }

template< typename T > FORCEINLINE TVector2<T> Floor( const TVector2<T>& V ) { return TVector2<T>( FMath::FloorToFloat( V.X ), FMath::FloorToFloat( V.Y ) ); }
template< typename T > FORCEINLINE TVector <T> Floor( const TVector <T>& V ) { return TVector <T>( FMath::FloorToFloat( V.X ), FMath::FloorToFloat( V.Y ), FMath::FloorToFloat( V.Z ) ); }
template< typename T > FORCEINLINE TVector4<T> Floor( const TVector4<T>& V ) { return TVector4<T>( FMath::FloorToFloat( V.X ), FMath::FloorToFloat( V.Y ), FMath::FloorToFloat( V.Z ), FMath::FloorToFloat( V.W ) ); }

template< typename T > FORCEINLINE TVector2<T> Ceil( const TVector2<T>& V ) { return TVector2<T>( FMath::CeilToFloat( V.X ), FMath::CeilToFloat( V.Y ) ); }
template< typename T > FORCEINLINE TVector <T> Ceil( const TVector <T>& V ) { return TVector <T>( FMath::CeilToFloat( V.X ), FMath::CeilToFloat( V.Y ), FMath::CeilToFloat( V.Z ) ); }
template< typename T > FORCEINLINE TVector4<T> Ceil( const TVector4<T>& V ) { return TVector4<T>( FMath::CeilToFloat( V.X ), FMath::CeilToFloat( V.Y ), FMath::CeilToFloat( V.Z ), FMath::CeilToFloat( V.W ) ); }

FORCEINLINE FIntVector2 FloorToInt( const FVector2f& V ) { return FIntVector2( FMath::FloorToInt( V.X ), FMath::FloorToInt( V.Y ) ); }
FORCEINLINE FIntVector3 FloorToInt( const FVector3f& V ) { return FIntVector3( FMath::FloorToInt( V.X ), FMath::FloorToInt( V.Y ), FMath::FloorToInt( V.Z ) ); }
FORCEINLINE FIntVector4 FloorToInt( const FVector4f& V ) { return FIntVector4( FMath::FloorToInt( V.X ), FMath::FloorToInt( V.Y ), FMath::FloorToInt( V.Z ), FMath::FloorToInt( V.W ) ); }

FORCEINLINE FIntVector2 RoundToInt( const FVector2f& V ) { return FIntVector2( FMath::RoundToInt( V.X ), FMath::RoundToInt( V.Y ) ); }
FORCEINLINE FIntVector3 RoundToInt( const FVector3f& V ) { return FIntVector3( FMath::RoundToInt( V.X ), FMath::RoundToInt( V.Y ), FMath::RoundToInt( V.Z ) ); }
FORCEINLINE FIntVector4 RoundToInt( const FVector4f& V ) { return FIntVector4( FMath::RoundToInt( V.X ), FMath::RoundToInt( V.Y ), FMath::RoundToInt( V.Z ), FMath::RoundToInt( V.W ) ); }

FORCEINLINE FIntVector2 CeilToInt( const FVector2f& V ) { return FIntVector2( FMath::CeilToInt( V.X ), FMath::CeilToInt( V.Y ) ); }
FORCEINLINE FIntVector3 CeilToInt( const FVector3f& V ) { return FIntVector3( FMath::CeilToInt( V.X ), FMath::CeilToInt( V.Y ), FMath::CeilToInt( V.Z ) ); }
FORCEINLINE FIntVector4 CeilToInt( const FVector4f& V ) { return FIntVector4( FMath::CeilToInt( V.X ), FMath::CeilToInt( V.Y ), FMath::CeilToInt( V.Z ), FMath::CeilToInt( V.W ) ); }

// Useful for templated code that uses both vector and scalar types
namespace Scalar
{
	template< typename T > FORCEINLINE T Abs( const T A ) { return FMath::Abs( A ); }
	template< typename T > FORCEINLINE T Min( const T A, const T B ) { return FMath::Min( A, B ); }
	template< typename T > FORCEINLINE T Max( const T A, const T B ) { return FMath::Max( A, B ); }
	template< typename T > FORCEINLINE T Min3( const T A, const T B, const T C ) { return FMath::Min3( A, B, C ); }
	template< typename T > FORCEINLINE T Max3( const T A, const T B, const T C ) { return FMath::Max3( A, B, C ); }
	template< typename T > FORCEINLINE int32 FloorToInt( const T A ) { return FMath::FloorToInt32( A ); }
	template< typename T > FORCEINLINE int32 RoundToInt( const T A ) { return FMath::RoundToInt32( A ); }
	template< typename T > FORCEINLINE int32 CeilToInt( const T A ) { return FMath::CeilToInt32( A ); }
}

} // namespace UE::Math