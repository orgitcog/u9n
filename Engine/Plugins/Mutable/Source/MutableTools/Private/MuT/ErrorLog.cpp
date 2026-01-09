// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ErrorLog.h"

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "HAL/PlatformMath.h"
#include "HAL/UnrealMemory.h"
#include "Logging/LogCategory.h"
#include "Logging/LogMacros.h"
#include "Misc/AssertionMacros.h"
#include "MuR/Layout.h"
#include "MuR/Mesh.h"
#include "MuR/Model.h"
#include "MuR/ModelPrivate.h"
#include "MuR/MutableMath.h"
#include "MuR/Operations.h"
#include "MuR/ParametersPrivate.h"
#include "MuR/Platform.h"
#include "MuR/Skeleton.h"
#include "MuR/System.h"
#include "MuR/MutableRuntimeModule.h"
#include "Templates/Tuple.h"
#include "Trace/Detail/Channel.h"


namespace mu
{

    int32 FErrorLog::GetMessageCount() const
	{
		return Messages.Num();
	}


	const FString& FErrorLog::GetMessageText( int32 Index ) const
	{
		if (Messages.IsValidIndex(Index))
		{
			return Messages[Index].Text;
		}

		static FString Empty;
		return Empty;
	}


	const void* FErrorLog::GetMessageContext(int32 Index) const
	{
		const void* Result = 0;

		if (Messages.IsValidIndex(Index))
		{
			Result = Messages[Index].Context;
		}

		return Result;
	}


	const void* FErrorLog::GetMessageContext2(int32 Index) const
	{
		const void* Result = 0;

		if (Messages.IsValidIndex(Index))
		{
			Result = Messages[Index].Context2;
		}

		return Result;
	}


	ErrorLogMessageType FErrorLog::GetMessageType( int32 Index ) const
	{
		ErrorLogMessageType Result = ELMT_NONE;

		if (Messages.IsValidIndex(Index))
		{
			Result = Messages[Index].Type;
		}

		return Result;
	}


	ErrorLogMessageSpamBin FErrorLog::GetMessageSpamBin(int32 Index) const
	{
		ErrorLogMessageSpamBin Result = ELMSB_ALL;

		if (Messages.IsValidIndex(Index))
		{
			Result = Messages[Index].Spam;
		}

		return Result;
	}


	ErrorLogMessageAttachedDataView FErrorLog::GetMessageAttachedData( int32 Index ) const
	{
        ErrorLogMessageAttachedDataView Result;

		if (Messages.IsValidIndex(Index))
		{
			const FMessage& message = Messages[Index];
            
            if ( message.Data ) 
            {
                Result.UnassignedUVs = message.Data->UnassignedUVs.GetData();
			    Result.UnassignedUVsSize = message.Data->UnassignedUVs.Num();
            }
		}

		return Result;
	}


	void FErrorLog::Add(const FString& InMessage,
		ErrorLogMessageType InType,
		const void* InContext,
		ErrorLogMessageSpamBin InSpamBin)
	{
		UE::TUniqueLock Lock(MessageMutex);

		FMessage& Msg = Messages.Emplace_GetRef();
		Msg.Type = InType;
		Msg.Spam = InSpamBin;
		Msg.Text = InMessage;
		Msg.Context = InContext;
	}


	void FErrorLog::Add(const FString& InMessage,
		ErrorLogMessageType InType,
		const void* InContext,
		const void* InContext2,
		ErrorLogMessageSpamBin InSpamBin)
	{
		UE::TUniqueLock Lock(MessageMutex);

		FMessage& Msg = Messages.Emplace_GetRef();
		Msg.Type = InType;
		Msg.Spam = InSpamBin;
		Msg.Text = InMessage;
		Msg.Context = InContext;
		Msg.Context2 = InContext2;
	}


	void FErrorLog::Add(const FString& InMessage,
                                const ErrorLogMessageAttachedDataView& InDataView,
                                ErrorLogMessageType InType, 
								const void* InContext,
								ErrorLogMessageSpamBin InSpamBin)
	{
		UE::TUniqueLock Lock(MessageMutex);

		FMessage& Msg = Messages.Emplace_GetRef();
		Msg.Type = InType;
		Msg.Spam = InSpamBin;
		Msg.Text = InMessage;
		Msg.Context = InContext;
		Msg.Data = MakeShared<FErrorData>();

        if ( InDataView.UnassignedUVs && InDataView.UnassignedUVsSize > 0 )
        {
			// \TODO: Review
			Msg.Data->UnassignedUVs.Append(InDataView.UnassignedUVs, InDataView.UnassignedUVsSize);
        }
	}
	
	
	void FErrorLog::Log() const
	{
		UE_LOG(LogMutableCore, Log, TEXT(" Error Log :\n"));

		for ( const FMessage& msg : Messages )
		{
			switch ( msg.Type )
			{
			case ELMT_ERROR: 	UE_LOG(LogMutableCore, Log, TEXT("  ERR  %s\n"), *msg.Text); break;
			case ELMT_WARNING: 	UE_LOG(LogMutableCore, Log, TEXT("  WRN  %s\n"), *msg.Text); break;
			case ELMT_INFO: 	UE_LOG(LogMutableCore, Log, TEXT("  INF  %s\n"), *msg.Text); break;
			default: 			UE_LOG(LogMutableCore, Log, TEXT("  NON  %s\n"), *msg.Text); break;
			}
		}
	}


	void FErrorLog::Merge( const FErrorLog* Other )
	{
		Messages.Append(Other->Messages);
	}

    
    // clang-format off

    const TCHAR* s_opNames[] =
	{
		TEXT("NONE             "),

		TEXT("BO_CONSTANT      "),
        TEXT("NU_CONSTANT      "),
        TEXT("SC_CONSTANT      "),
        TEXT("CO_CONSTANT      "),
		TEXT("IM_CONSTANT      "),
		TEXT("ME_CONSTANT      "),
		TEXT("LA_CONSTANT      "),
		TEXT("PR_CONSTANT      "),
		TEXT("ST_CONSTANT      "),
		TEXT("ED_CONSTANT      "),
		TEXT("MA_CONSTANT      "),

		TEXT("BO_PARAMETER     "),
		TEXT("NU_PARAMETER     "),
		TEXT("SC_PARAMETER     "),
		TEXT("CO_PARAMETER     "),
		TEXT("PR_PARAMETER     "),
		TEXT("IM_PARAMETER     "),
		TEXT("ME_PARAMETER     "),
		TEXT("ST_PARAMETER     "),
		TEXT("MA_PARAMETER     "),
		
		TEXT("IM_REFERENCE     "),
		TEXT("ME_REFERENCE     "),

		TEXT("NU_CONDITIONAL   "),
		TEXT("SC_CONDITIONAL   "),
		TEXT("CO_CONDITIONAL   "),
		TEXT("IM_CONDITIONAL   "),
		TEXT("ME_CONDITIONAL   "),
		TEXT("LA_CONDITIONAL   "),
		TEXT("IN_CONDITIONAL   "),
		TEXT("ED_CONDITIONAL   "),
		
		TEXT("NU_SWITCH        "),
		TEXT("SC_SWITCH        "),
		TEXT("CO_SWITCH        "),
		TEXT("IM_SWITCH        "),
		TEXT("ME_SWITCH        "),
		TEXT("LA_SWITCH        "),
		TEXT("IN_SWITCH        "),
		TEXT("ED_SWITCH        "),

		TEXT("BO_EQUAL_SC_CONST"),
		TEXT("BO_AND           "),
		TEXT("BO_OR            "),
		TEXT("BO_NOT           "),
		
		TEXT("SC_ARITHMETIC    "),
		TEXT("SC_CURVE         "),
		
		TEXT("CO_SAMPLEIMAGE   "),
		TEXT("CO_SWIZZLE       "),
		TEXT("CO_FROMSCALARS   "),
		TEXT("CO_ARITHMETIC    "),

		TEXT("IM_LAYER         "),
		TEXT("IM_LAYERCOLOUR   "),
		TEXT("IM_PIXELFORMAT   "),
		TEXT("IM_MIPMAP        "),
		TEXT("IM_RESIZE        "),
		TEXT("IM_RESIZELIKE    "),
		TEXT("IM_RESIZEREL     "),
		TEXT("IM_BLANKLAYOUT   "),
		TEXT("IM_COMPOSE       "),
		TEXT("IM_INTERPOLATE   "),
		TEXT("IM_SATURATE      "),
		TEXT("IM_LUMINANCE     "),
		TEXT("IM_SWIZZLE       "),
		TEXT("IM_COLOURMAP     "),
		TEXT("IM_BINARISE      "),
		TEXT("IM_PLAINCOLOUR   "),
		TEXT("IM_CROP          "),
		TEXT("IM_PATCH         "),
		TEXT("IM_RASTERMESH    "),
		TEXT("IM_MAKEGROWMAP   "),
		TEXT("IM_DISPLACE      "),
		TEXT("IM_MULTILAYER    "),
		TEXT("IM_INVERT        "),
		TEXT("IM_NORMAL_COMPO  "),
		TEXT("IM_TRANSFORM     "),

		TEXT("ME_APPLYLAYOUT   "),
		TEXT("ME_PREPARELAYOUT "),
		TEXT("ME_DIFFERENCE    "),
		TEXT("ME_MORPH         "),
		TEXT("ME_MERGE         "),
		TEXT("ME_MASKCLIPMESH  "),
		TEXT("ME_MASKCLIPUVMASK"),
		TEXT("ME_MASKDIFF      "),
        TEXT("ME_REMOVEMASK    "),
        TEXT("ME_FORMAT        "),
        TEXT("ME_EXTRACTLABLOCK"),
		TEXT("ME_TRANSFORM     "),
		TEXT("ME_CLIPMORPHPLANE"),
        TEXT("ME_CLIPWITHMESH  "),
        TEXT("ME_SETSKELETON   "),
        TEXT("ME_PROJECT       "),
        TEXT("ME_APPLYPOSE     "),
		TEXT("ME_BINDSHAPE	   "),
		TEXT("ME_APPLYSHAPE	   "),
		TEXT("ME_CLIPDEFORM	   "),
		TEXT("ME_MORPHRESHAPE  "),
		TEXT("ME_OPTIMIZESKIN  "),
		TEXT("ME_ADDTAGS       "),
		TEXT("ME_TRANSFORMWITHMESH"),

		TEXT("IN_ADDMESH       "),
		TEXT("IN_ADDIMAGE      "),
		TEXT("IN_ADDVECTOR     "),
		TEXT("IN_ADDSCALAR     "),
		TEXT("IN_ADDSTRING     "),
        TEXT("IN_ADDSURFACE    "),
        TEXT("IN_ADDCOMPONENT  "),
        TEXT("IN_ADDLOD        "),
		TEXT("IN_ADDEXTENSIDATA"),
		TEXT("IN_ADDOVERLAYMATERIAL"),
		
		TEXT("LA_PACK          "),
		TEXT("LA_MERGE         "),
		TEXT("LA_REMOVEBLOCKS  "),
		TEXT("LA_FROMMESH	   "),
	};

	static_assert(UE_ARRAY_COUNT(s_opNames) == int32(EOpType::COUNT));

    // clang-format on

}
