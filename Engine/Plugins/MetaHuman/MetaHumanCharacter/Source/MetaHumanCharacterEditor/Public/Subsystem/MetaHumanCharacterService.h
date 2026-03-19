// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Notifications/NotificationManager.h"
#include "Templates/SharedPointer.h"
#include "MetaHumanCharacterIdentity.h"
#include "Cloud/MetaHumanTextureSynthesisServiceRequest.h"

namespace UE::MetaHuman
{
	class FAutoRigServiceRequest;
	class FAutorigResponse;
	struct FTargetSolveParameters;
}

/**
 * Struct holding requests to the MetaHuman Cloud Services
 */
struct FMetaHumanCharacterEditorCloudRequests
{
	// A request to obtain high resolution textures
	TSharedPtr<UE::MetaHuman::FFaceTextureSynthesisServiceRequest> TextureSynthesis;

	// A request to obtain high resolution body textures
	TSharedPtr<UE::MetaHuman::FBodyTextureSynthesisServiceRequest> BodyTextures;
	
	// A request to auto rig a character
	TSharedPtr<UE::MetaHuman::FAutoRigServiceRequest> AutoRig;

	// The start time of the texture synthesis request
	double TextureSynthesisStartTime = 0.0f;

	// The start time of the body texture request
	double BodyTextureStartTime = 0.0f;

	// The start time of the auto rig request
	double AutoRiggingStartTime = 0.0f;

	// Handle used to update the progress on the texture download
	FProgressNotificationHandle TextureSynthesisProgressHandle;
	
	// Handle used to update the progress on the body texture download
	FProgressNotificationHandle BodyTextureProgressHandle;

	// Handle used to update the progress of an auto rigging request
	FProgressNotificationHandle AutoRiggingProgressHandle;

	// Permanent notification item displayed while downloading high resolution textures
	TWeakPtr<SNotificationItem> TextureSynthesisNotificationItem;

	// Permanent notification item displayed while downloading high resolution body textures
	TWeakPtr<SNotificationItem> BodyTextureNotificationItem;

	// Permanent notification item displayed while the auto rigging request is live
	TWeakPtr<SNotificationItem> AutoRiggingNotificationItem;

	/**
	* Marks the texture synthesis request as finished and reset all of the state associated with it
	*/
	void TextureSynthesisRequestFinished();

	/**
	* Marks the body texture request as finished and reset all of the state associated with it
	*/
	void BodyTextureRequestFinished();

	/**
	* Marks the auto rigging request as finished and reset all of the state associated with it
	*/
	void AutoRiggingRequestFinished();

	/**
	* Returns true if there is an active texture synthesis or auto-rigging request
	*/
	bool HasActiveRequest() const;

	/**
	* Initializes the passed FTargetSolveParameters from the input face data
	*/
	static void InitFaceAutoRigParams(
		const FMetaHumanCharacterIdentity::FState& InFaceState,
		TSharedRef<const IDNAReader> InFaceDNAReader,
		UE::MetaHuman::FTargetSolveParameters& OutAutoRigParameters);

	/**
	* Generates the face textures from the data in the service respond. 
	* Normals and cavity are assigned directly, while albedos are synthesized using InFaceTextureSynthesizer. 
	* 
	* Returns true if the Character texture objects were updated
	*/
	static bool GenerateTexturesFromResponse(
		TSharedPtr<UE::MetaHuman::FFaceHighFrequencyData> InResponse,
		const class FMetaHumanFaceTextureSynthesizer& InFaceTextureSynthesizer,
		TSharedRef<struct FMetaHumanCharacterEditorData> InCharacterData,
		TNotNull<class UMetaHumanCharacter*> InMetaHumanCharacter);

	static bool GenerateBodyTexturesFromResponse(
		TSharedPtr<UE::MetaHuman::FBodyHighFrequencyData> InResponse,
		TNotNull<class UMetaHumanCharacter*> InMetaHumanCharacter);
};
