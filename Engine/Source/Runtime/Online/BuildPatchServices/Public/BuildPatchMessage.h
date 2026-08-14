// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "Misc/Guid.h"
#include "Templates/Function.h"

namespace BuildPatchServices
{
	/**
	 * A message describing an event that occurred for a chunk source.
	 */
	struct FChunkSourceEvent
	{
		// Describes the event type.
		enum class EType : uint32
		{
			// Access was lost to the source.
			AccessLost = 0,
			// Access has been regained after being lost.
			AccessRegained,
		};

		// The type of event that occurred.
		EType Event;
		// The location context for the source, could be cloud root, install location, chunkdb file etc.
		FString Location;
	};

	/**
	 * A message describing an action taken to an installation file.
	 */
	struct FInstallationFileAction
	{
		// Describes the action type.
		enum class EType : uint32
		{
			// The file was removed.
			Removed = 0,
			// The file was added.
			Added,
			// The file was updated.
			Updated,
		};

		// The type of action that occurred.
		EType Action;
		// The filename affected, relative to the install location.
		FString Filename;
	};

	/**
	 * A Request for a chunk
	 */
	struct FChunkUriRequest
	{
		FString CloudDirectory;
		FString RelativePath;
	};

	/**
	 * A Response containing the actual location of the chunk
	 */
	struct FChunkUriResponse
	{
		// Set this to true if the response can not be fulfilled. This will fail the chunk requests and subsequently
		// the installation.
		bool bFailed = false;
		FString Uri;
		
		// These headers <name, value> will be added to the HTTP request.
		TMap<FString, FString> AdditionalHeaders;
	};

	struct FGenericMessage
	{
		// Set of informational messages the installer can send to the client to update
		// UI or otherwise react.
		enum class EType : uint32
		{
			// Posted when a chunk is requested of the cloud source ONLY WHEN CHUNKDBS ARE PRESENT. This is useful if the installer
			// is not expected to be downloading anything independently and you want to log this case.
			// Note that even when chunks are fully provided, cancellation and resumption can lose "harvested"
			// chunks that were resident only in non-persistent backing store causing a download
			CloudSourceUsed,

			// Posted when a CDN/CloudDir has failed a download and is dropped in priority.
			// Payload1 = CDN that failed,
			// Payload2 = CDN that is considered "best" after this failure.
			CDNDownloadFailed,
		};

		EType Type;
		FGuid ChunkId;
		FString Payload1, Payload2;
	};
	

	// Flag for which Requests a message handler expects to receive, allows for internal implementation optimisation.
	enum class EMessageRequests : uint32
	{
		// Does not respond to any requests - message listener only.
		None = 0,
		// Will respond to chunk URI requests.
		ChunkUriRequest = 0x1,
		// Further request types to follow in future.

	};
	ENUM_CLASS_FLAGS(EMessageRequests);

	/**
	 * Base class of a message handler, this should be inherited from and passed to an installer to receive messages that you want to handle.
	 */
	class FMessageHandler
	{
	public:
		FMessageHandler(EMessageRequests InMessageRequests)
			: MessageRequests(InMessageRequests)
		{}

		virtual ~FMessageHandler() = default;

		/**
		* Handle generic information posting. This can be called from any thread and should not
		* take appreciable time as it blocks further installation.
		* See FGenericMessage for payload contents and types
		*/
		virtual void HandleMessage(const FGenericMessage& Message) {}

		/**
		 * Handles a chunk source event message.
		 * @param Message   The message to be handled.
		 */
		virtual void HandleMessage(const FChunkSourceEvent& Message) {}

		/**
		 * Handles an installation file action message.
		 * @param Message   The message to be handled.
		 */
		virtual void HandleMessage(const FInstallationFileAction& Message) {}

		/**
		 * Handles responding to a chunk Uri request
		 * @param Request   The request for a chunk
		 * @param OnResponse   The function to callback once the chunk has been found
		 */
		virtual bool HandleRequest(const FChunkUriRequest& Request, TFunction<void(FChunkUriResponse)> OnResponse) { return false; }

		/**
		 * @return the message request flags.
		 */
		EMessageRequests GetMessageRequests() const { return MessageRequests; }

	private:
		const EMessageRequests MessageRequests;
	};

	class FDefaultMessageHandler
		: public FMessageHandler
	{
	public:
		// Default constructor passes flags to always support all requests
		FDefaultMessageHandler()
			: FMessageHandler(static_cast<EMessageRequests>(0xFFFFFFFF))
		{}

		// This gets called if all registered handlers return false to HandleRequest. If there are
		// _no_ handlers then this is entirely sidestepped in the cloud chunk source.
		virtual bool HandleRequest(const FChunkUriRequest& Request, TFunction<void(FChunkUriResponse)> OnResponse) override
		{
			FChunkUriResponse Response;
			Response.Uri = Request.CloudDirectory / Request.RelativePath;
			OnResponse(MoveTemp(Response));
			return true;
		}
	};
}
