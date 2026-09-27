// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/PlatformCrt.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "Templates/SharedPointer.h"
#include "Async/Mutex.h"


namespace mu
{

	/** Types of message stored in the log. */
	typedef enum
	{
        ELMT_NONE=0,
		ELMT_ERROR,
		ELMT_WARNING,
		ELMT_INFO
	} ErrorLogMessageType;

	/** Categories of message stored in the log for the purpose of limiiting duplication of non - identical messages. */
	typedef enum
	{
		ELMSB_ALL = 0,
		ELMSB_UNKNOWN_TAG
	} ErrorLogMessageSpamBin;

	struct ErrorLogMessageAttachedDataView 
	{
		const float* UnassignedUVs = nullptr;
		int32 UnassignedUVsSize = 0;
	};

	/** Storage for mutable error, warning and information messages from several processes performed by the tools library, like compilation. 
	* It support concurrent addition of messages, but not retrieval.
	*/	
	class MUTABLETOOLS_API FErrorLog
	{
	public:

		struct FErrorData
		{
			TArray< float > UnassignedUVs;
		};

		struct FMessage
		{
			ErrorLogMessageType Type = ELMT_NONE;
			ErrorLogMessageSpamBin Spam = ELMSB_ALL;
			FString Text;
			TSharedPtr<FErrorData> Data;
			const void* Context = nullptr;
			const void* Context2 = nullptr;
		};

	private:

		/** This mutex is used to control the access to the messages array when adding them. */
		UE::FMutex MessageMutex;

		TArray<FMessage> Messages;

	public:

		//-----------------------------------------------------------------------------------------
		// Own interface
		//-----------------------------------------------------------------------------------------

		//! Get the number of messages.
		//! If a message type is provided, return the number of that type of message only.
		int32 GetMessageCount() const;

		//! Get the text of a message.
		//! \param index index of the message from 0 to GetMessageCount()-1
		const FString& GetMessageText( int32 index ) const;

		//! Get the opaque context of a message.
		//! \param index index of the message from 0 to GetMessageCount()-1
		const void* GetMessageContext(int32 index) const;
		const void* GetMessageContext2(int32 index) const;

		//!
		ErrorLogMessageType GetMessageType( int32 index ) const;

		//!
		ErrorLogMessageSpamBin GetMessageSpamBin(int32 index) const;

		//! Get the attached data of a message.
		//! \param index index of message data from 0 to GetMessageCount(ELMT_NONE)-1
		ErrorLogMessageAttachedDataView GetMessageAttachedData( int32 index ) const;

		//!
		void Log() const;

		//!
		void Merge( const FErrorLog* Other );

		//!
		void Add(const FString& Message, ErrorLogMessageType Type, const void* Context, ErrorLogMessageSpamBin SpamBin = ELMSB_ALL);
		void Add(const FString& Message, ErrorLogMessageType Type, const void* Context, const void* Context2, ErrorLogMessageSpamBin SpamBin = ELMSB_ALL);

		//!
		void Add(const FString& Message, const ErrorLogMessageAttachedDataView& Data, ErrorLogMessageType Type, const void* Context, ErrorLogMessageSpamBin SpamBin = ELMSB_ALL);

	};
	
	MUTABLETOOLS_API extern const TCHAR* s_opNames[];
}
