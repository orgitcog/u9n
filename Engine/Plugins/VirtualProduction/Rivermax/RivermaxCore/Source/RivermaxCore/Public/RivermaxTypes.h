// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Misc/FrameRate.h"
#include "RivermaxFormats.h"


namespace UE::RivermaxCore
{
	constexpr TCHAR DefaultStreamAddress[] = TEXT("228.1.1.1");


	enum class ERivermaxAlignmentMode
	{
		/** Aligns scheduling with ST2059 frame boundary formula */
		AlignmentPoint,

		/** Aligns scheduling with frame creation */
		FrameCreation,
	};

	inline const TCHAR* LexToString(ERivermaxAlignmentMode InValue)
	{
		switch (InValue)
		{
			case ERivermaxAlignmentMode::AlignmentPoint:
			{
				return TEXT("Alignment point");
			}
			case ERivermaxAlignmentMode::FrameCreation:
			{
				return TEXT("Frame creation");
			}
			default:
			{
				checkNoEntry();
			}
		}

		return TEXT("<Unknown ERivermaxAlignmentMode>");
	}

	enum class EFrameLockingMode : uint8
	{
		/** If no frame available, continue */
		FreeRun,

		/** Blocks when reserving a frame slot. */
		BlockOnReservation,
	};

	inline const TCHAR* LexToString(EFrameLockingMode InValue)
	{
		switch (InValue)
		{
			case EFrameLockingMode::FreeRun:
			{
				return TEXT("Freerun");
			}
			case EFrameLockingMode::BlockOnReservation:
			{
				return TEXT("Blocking");
			}
			default:
			{
				checkNoEntry();
			}
		}

		return TEXT("<Unknown EFrameLockingMode>");
	}

	struct FRivermaxInputStreamOptions
	{
		/** Stream FrameRate */
		FFrameRate FrameRate = { 24,1 };

		/** Interface IP to bind to */
		FString InterfaceAddress;

		/** IP of the stream. Defaults to multicast group IP. */
		FString StreamAddress = DefaultStreamAddress;

		/** Port to be used by stream */
		uint32 Port = 50000;

		/** Desired stream pixel format */
		ESamplingType PixelFormat = ESamplingType::RGB_10bit;

		/** Sample count to buffer. */
		int32 NumberOfBuffers = 2;
		
		/** If true, don't use auto detected video format */
		bool bEnforceVideoFormat = false;

		/** Enforced resolution aligning with pgroup of sampling type */
		FIntPoint EnforcedResolution = FIntPoint::ZeroValue;

		/** Whether to leverage GPUDirect (Cuda) capability to transfer memory to NIC if available */
		bool bUseGPUDirect = true;
	};

	struct FRivermaxOutputStreamOptions
	{
		/** Stream FrameRate */
		FFrameRate FrameRate = { 24,1 };

		/** Interface IP to bind to */
		FString InterfaceAddress;

		/** IP of the stream. Defaults to multicast group IP. */
		FString StreamAddress = DefaultStreamAddress;

		/** Port to be used by stream */
		uint32 Port = 50000;

		/** Used by RivermaxOutStream when it calls to the library to assign Media Block Index in SDP. */
		uint64 StreamIndex = 0;
	};

	struct FRivermaxVideoOutputOptions : public FRivermaxOutputStreamOptions
	{
		/** Desired stream resolution */
		FIntPoint Resolution = { 1920, 1080 };

		/** Desired stream pixel format */
		ESamplingType PixelFormat = ESamplingType::RGB_10bit;

		/** Resolution aligning with pgroup of sampling type */
		FIntPoint AlignedResolution = FIntPoint::ZeroValue;

		/** Whether to leverage GPUDirect (Cuda) capability to transfer memory to NIC if available */
		bool bUseGPUDirect = true;
	};

	struct FRivermaxAncOutputOptions : public FRivermaxOutputStreamOptions
	{
		/** DID value is specified by SMPTE 291 standard. 0x60 - Ancillary timecode. */
		uint16 DID = 0x60;

		/** SDID value is specified by SMPTE 291 standard. 0x60 - Ancillary timecode. */
		uint16 SDID = 0x60;

		FRivermaxAncOutputOptions()
		{
			Port = 50010;
		};
	};

	enum class ERivermaxStreamType : uint8
	{
		VIDEO_2110_20_STREAM,
		AUDIO_2110_30_STREAM,
		ANC_2110_40_STREAM,
		MAX
	};

	struct FRivermaxOutputOptions
	{
		/** If not nullptr indicates that anc stream needs to be created. Contains all anc stream related options. */
		TStaticArray<TSharedPtr<FRivermaxOutputStreamOptions>, static_cast<uint32>(ERivermaxStreamType::MAX)> StreamOptions;

		/** Sample count to buffer. */
		int32 NumberOfBuffers = 2;

		/** Method used to align output stream */
		ERivermaxAlignmentMode AlignmentMode = ERivermaxAlignmentMode::AlignmentPoint;

		/** Defines how frame requests are handled. Whether they can block or not. */
		EFrameLockingMode FrameLockingMode = EFrameLockingMode::FreeRun;

		/** Whether the stream will output a frame at every frame interval, repeating last frame if no new one provided */
		bool bDoContinuousOutput = true;

		/** Whether to use frame's frame number instead of standard timestamping */
		bool bDoFrameCounterTimestamping = true;

		/** Returns stream options for this stream. */
		template <typename T>
		TSharedPtr<T> GetStreamOptions(ERivermaxStreamType InStreamType)
		{
			return StaticCastSharedPtr<T>(StreamOptions[static_cast<uint8>(InStreamType)]);
		}

		TSharedPtr<FRivermaxOutputStreamOptions> GetStreamOptions(ERivermaxStreamType InStreamType)
		{
			return StreamOptions[static_cast<uint8>(InStreamType)];
		}
	};
}


