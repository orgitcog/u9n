// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// LUP factorization using Doolittle's method with partial pivoting
template< typename T >
bool LUPFactorize( T* RESTRICT A, uint32* RESTRICT Pivot, uint32 Size, T Epsilon )
{
	for( uint32 i = 0; i < Size; i++ )
	{
		Pivot[i] = i;
	}

	for( uint32 i = 0; i < Size; i++ )
	{
		// Find largest pivot in column
		T		MaxValue = FMath::Abs( A[ Size * i + i ] );
		uint32	MaxIndex = i;

		for( uint32 j = i + 1; j < Size; j++ )
		{
			T AbsValue = FMath::Abs( A[ Size * j + i ] );
			if( AbsValue > MaxValue )
			{
				MaxValue = AbsValue;
				MaxIndex = j;
			}
		}

		if( MaxValue < Epsilon )
		{
			// Matrix is singular
			return false;
		}

		// Swap rows pivoting MaxValue to the diagonal
		if( MaxIndex != i )
		{
			Swap( Pivot[i], Pivot[ MaxIndex ] );

			for( uint32 j = 0; j < Size; j++ )
				Swap( A[ Size * i + j ], A[ Size * MaxIndex + j ] );
		}

		// Gaussian elimination
		for( uint32 j = i + 1; j < Size; j++ )
		{
			A[ Size * j + i ] /= A[ Size * i + i ];

			for( uint32 k = i + 1; k < Size; k++ )
				A[ Size * j + k ] -= A[ Size * j + i ] * A[ Size * i + k ];
		}
	}

	return true;
}

// Solve system of equations A*x = b
template< typename T >
void LUPSolve( const T* RESTRICT LU, const uint32* RESTRICT Pivot, uint32 Size, const T* RESTRICT b, T* RESTRICT x )
{
	for( uint32 i = 0; i < Size; i++ )
	{
		x[i] = b[ Pivot[i] ];

		for( uint32 j = 0; j < i; j++ )
			x[i] -= LU[ Size * i + j ] * x[j];
	}

	for( int32 i = Size - 1; i >= 0; i-- )
	{
		for( uint32 j = i + 1; j < Size; j++ )
			x[i] -= LU[ Size * i + j ] * x[j];

		// Diagonal was filled with max values, all greater than Epsilon
		x[i] /= LU[ Size * i + i ];
	}
}

// Newton's method iterative refinement.
template< typename T >
bool LUPSolveIterate( const T* RESTRICT A, const T* RESTRICT LU, const uint32* RESTRICT Pivot, uint32 Size, const T* RESTRICT b, T* RESTRICT x )
{
	T* Residual = (T*)FMemory_Alloca( 2 * Size * sizeof(T) );
	T* Error = Residual + Size;

	LUPSolve( LU, Pivot, Size, b, x );

	bool bCloseEnough = false;
	for( uint32 k = 0; k < 4; k++ )
	{
		for( uint32 i = 0; i < Size; i++ )
		{
			Residual[i] = b[i];

			for( uint32 j = 0; j < Size; j++ )
			{
				Residual[i] -= A[ Size * i + j ] * x[j];
			}
		}

		LUPSolve( LU, Pivot, Size, Residual, Error );

		T MeanSquaredError = 0.0;
		for( uint32 i = 0; i < Size; i++ )
		{
			x[i] += Error[i];
			MeanSquaredError += Error[i] * Error[i];
		}

		if( MeanSquaredError < KINDA_SMALL_NUMBER )
		{
			bCloseEnough = true;
			break;
		}
	}

	return bCloseEnough;
}


namespace JacobiSVD
{

///////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2002-2012, Industrial Light & Magic, a division of Lucas
// Digital Ltd. LLC
// 
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// *       Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
// *       Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
// *       Neither the name of Industrial Light & Magic nor the names of
// its contributors may be used to endorse or promote products derived
// from this software without specific prior written permission. 
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
///////////////////////////////////////////////////////////////////////////

// Jacobi solver is modified version of code from ImathMatricAlgo.cpp

template< typename T >
inline void Update( T* RESTRICT A, T s, T tau, uint32 d1, uint32 d2 )
{
	const T nu1 = A[ d1 ];
	const T nu2 = A[ d2 ];
	A[ d1 ] -= s * ( nu2 + tau * nu1 );
	A[ d2 ] += s * ( nu1 - tau * nu2 );
}

template< typename T >
bool Rotation3(
	T* RESTRICT A,
	T* RESTRICT V,
	T* RESTRICT Z,
	const T tol,
	int j, int k, int L )
{
	const T x = A[ 3*j + j ];
	const T y = A[ 3*j + k ];
	const T z = A[ 3*k + k ];

	const T mu1 = z - x;
	const T mu2 = (T)2.0 * y;

	if( FMath::Abs( mu2 ) <= tol * FMath::Abs( mu1 ) )
	{
		// We've decided that the off-diagonal entries are already small
		// enough, so we'll set them to zero.  This actually appears to result
		// in smaller errors than leaving them be, possibly because it prevents
		// us from trying to do extra rotations later that we don't need.
		A[ 3*j + k ] = 0.0;
		return false;
	}

    const T rho = mu1 / mu2;
	const T t = ( rho < (T)0.0 ? (T)-1.0 : (T)1.0 ) / ( FMath::Abs( rho ) + FMath::Sqrt( (T)1.0 + rho * rho ) );
	const T c = (T)1.0 / FMath::Sqrt( (T)1.0 + t * t );
	const T s = c * t;
	const T tau = s / ( (T)1.0 + c );
	const T h = t * y;

	// Update diagonal elements.
	Z[j] -= h;
	Z[k] += h;
	A[ 3*j + j ] -= h;
	A[ 3*k + k ] += h;
	A[ 3*j + k ] = 0.0;

	Update( A, s, tau,
		L < j ? 3*L + j : 3*j + L,
		L < k ? 3*L + k : 3*k + L );

	// Rotate right
	for( uint32 i = 0; i < 3; i++ )
	{
		Update( V, s, tau,
			3*i + j,
			3*i + k );
	}

    return true;
}

template< typename T >
bool Rotation4(
	T* RESTRICT A,
	T* RESTRICT V,
	T* RESTRICT Z,
	const T tol,
	int j, int k, int L1, int L2 )
{
	const T x = A[ 4*j + j ];
	const T y = A[ 4*j + k ];
	const T z = A[ 4*k + k ];

	const T mu1 = z - x;
	const T mu2 = (T)2.0 * y;

	// Let's see if rho^(-1) = mu2 / mu1 is less than tol
	// This test also checks if rho^2 will overflow 
	// when tol^(-1) < sqrt(limits<T>::max()).
	if( FMath::Abs( mu2 ) <= tol * FMath::Abs( mu1 ) )
	{
		A[ 4*j + k ] = 0.0;
		return false;
	}

	const T rho = mu1 / mu2;
	const T t = ( rho < (T)0.0 ? (T)-1.0 : (T)1.0 ) / ( FMath::Abs( rho ) + FMath::Sqrt( (T)1.0 + rho * rho ) );
	const T c = (T)1.0 / FMath::Sqrt( (T)1.0 + t * t );
	const T s = c * t;
	const T tau = s / ( (T)1.0 + c );
	const T h = t * y;

	Z[j] -= h;
	Z[k] += h;
	A[ 4*j + j ] -= h;
	A[ 4*k + k ] += h;
	A[ 4*j + k ] = 0.0;

	Update( A, s, tau,
		L1 < j ? 4*L1 + j : 4*j + L1,
		L1 < k ? 4*L1 + k : 4*k + L1 );

	Update( A, s, tau,
		L2 < j ? 4*L2 + j : 4*j + L2,
		L2 < k ? 4*L2 + k : 4*k + L2 );

	// Rotate right
	for( uint32 i = 0; i < 4; i++ )
	{
		Update( V, s, tau,
			4*i + j,
			4*i + k );
	}

	return true;
}

template< typename T >
inline T MaxOffDiagSymm( T* RESTRICT A, uint32 Size )
{
	T Result = 0.0;
	for( uint32 i = 0; i < Size; i++ )
		for( uint32 j = i + 1; j < Size; j++ )
			Result =  FMath::Max( Result,  FMath::Abs( A[ Size * i + j ] ) );

	return Result;
}

template< typename T >
void EigenSolver3(
	T* RESTRICT A,
	T* RESTRICT S,
	T* RESTRICT V,
	const T tol )
{
	FMemory::Memzero( V, 9 * sizeof(T) );

	for( int i = 0; i < 3; i++ )
	{
		S[i] = A[ 3*i + i ];
		V[ 3*i + i ] = 1.0;
	}

	const int maxIter = 20;  // In case we get really unlucky, prevents infinite loops
	const T absTol = tol * MaxOffDiagSymm( A, 3 );  // Tolerance is in terms of the maximum
	if( absTol != 0 )                        // _off-diagonal_ entry.
	{
		int numIter = 0;
		do
		{
			++numIter;
			// Z is for accumulating small changes (h) to diagonal entries
			// of A for one sweep. Adding h's directly to A might cause
			// a cancellation effect when h is relatively very small to 
			// the corresponding diagonal entry of A and 
			// this will increase numerical errors
			T Z[] = { 0.0, 0.0, 0.0 };
			bool changed;
			changed = Rotation3( A, V, Z, tol, 0, 1, 2 );
			changed = Rotation3( A, V, Z, tol, 0, 2, 1 ) || changed;
			changed = Rotation3( A, V, Z, tol, 1, 2, 0 ) || changed;
			// One sweep passed. Add accumulated changes (Z) to singular values (S)
			// Update diagonal elements of A for better accuracy as well.
			for( int i = 0; i < 3; i++ )
			{
				A[ 3*i + i ] = S[i] += Z[i];
			}
			if( !changed )
				break;
		}
		while( MaxOffDiagSymm( A, 3 ) > absTol && numIter < maxIter );
	}
}

template< typename T >
void EigenSolver4(
	T* RESTRICT A,
	T* RESTRICT S,
	T* RESTRICT V,
	const T tol )
{
	FMemory::Memzero( V, 16 * sizeof(T) );

	for( int i = 0; i < 4; i++ )
	{
		S[i] = A[ 4*i + i ];
		V[ 4*i + i ] = 1.0;
	}

	const int maxIter = 20;  // In case we get really unlucky, prevents infinite loops
	const T absTol = tol * MaxOffDiagSymm( A, 4 );  // Tolerance is in terms of the maximum
	if( absTol != 0.0 )                        // _off-diagonal_ entry.
	{
		int numIter = 0;
		do
		{
			++numIter;
			T Z[] = { 0.0, 0.0, 0.0, 0.0 };
			bool changed;
			changed = Rotation4( A, V, Z, tol, 0, 1, 2, 3 );
			changed = Rotation4( A, V, Z, tol, 0, 2, 1, 3 ) || changed;
			changed = Rotation4( A, V, Z, tol, 0, 3, 1, 2 ) || changed;
			changed = Rotation4( A, V, Z, tol, 1, 2, 0, 3 ) || changed;
			changed = Rotation4( A, V, Z, tol, 1, 3, 0, 2 ) || changed;
			changed = Rotation4( A, V, Z, tol, 2, 3, 0, 1 ) || changed;
			for( int i = 0; i < 4; i++ )
			{
				A[ 4*i + i ] = S[i] += Z[i];
			}
			if( !changed )
				break;
		}
		while( MaxOffDiagSymm( A, 4 ) > absTol && numIter < maxIter );
	}
}

}

// Moore-Penrose pseudo inverse
template< typename T >
void PseudoInverse( T* RESTRICT S, uint32 Size, T Tolerance )
{
	T MaxS = 0.0;
	for( uint32 i = 0; i < Size; i++ )
		MaxS = FMath::Max( MaxS, FMath::Abs( S[i] ) );

	for( uint32 i = 0; i < Size; i++ )
	{
		if( FMath::Abs( S[i] ) > MaxS * Tolerance )
			S[i] = 1.0 / S[i];
		else
			S[i] = 0.0;
	}
}

template< typename T >
void PseudoSolve( const T* RESTRICT V, const T* RESTRICT S, uint32 Size, const T* RESTRICT b, T* RESTRICT x )
{
	for( uint32 i = 0; i < Size; i++ )
		x[i] = 0.0;

	for( uint32 i = 0; i < Size; i++ )
	{
		T SVtbi = 0.0;
		for( uint32 j = 0; j < Size; j++ )
			SVtbi += V[ Size * j + i ] * b[j];

		SVtbi *= S[i];

		for( uint32 j = 0; j < Size; j++ )
			x[j] += V[ Size * j + i ] * SVtbi;
	}
}

// Newton's method iterative refinement.
template< typename T >
bool PseudoSolveIterate( const T* RESTRICT A, const T* RESTRICT V, const T* RESTRICT S, uint32 Size, const T* RESTRICT b, T* RESTRICT x )
{
	T* Residual = (T*)FMemory_Alloca( 2 * Size * sizeof(T) );
	T* Error = Residual + Size;

	PseudoSolve( V, S, Size, b, x );

	bool bCloseEnough = false;
	for( uint32 k = 0; k < 4; k++ )
	{
		for( uint32 i = 0; i < Size; i++ )
		{
			Residual[i] = b[i];

			for( uint32 j = 0; j < Size; j++ )
			{
				Residual[i] -= A[ Size * i + j ] * x[j];
			}
		}

		PseudoSolve( V, S, Size, Residual, Error );

		T MeanSquaredError = 0.0;
		for( uint32 i = 0; i < Size; i++ )
		{
			x[i] += Error[i];
			MeanSquaredError += Error[i] * Error[i];
		}

		if( MeanSquaredError < KINDA_SMALL_NUMBER )
		{
			bCloseEnough = true;
			break;
		}
	}

	return bCloseEnough;
}