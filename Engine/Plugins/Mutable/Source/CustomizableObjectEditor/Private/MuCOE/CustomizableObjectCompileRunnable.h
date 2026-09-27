// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Runnable.h"
#include "MuCO/CustomizableObjectPrivate.h"
#include "MuCO/CustomizableObjectCompilerTypes.h"
#include "MuCO/UnrealToMutableTextureConversionUtils.h"
#include "MuR/Image.h"
#include "MuR/Mesh.h"
#include "MuR/Ptr.h"
#include "MuT/Node.h"
#include "MuCOE/CustomizableObjectEditorLogger.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSource.h"

#include "DerivedDataCacheKey.h"
#include "DerivedDataCachePolicy.h"

#include <atomic>

struct FCompilationRequest;
class ITargetPlatform;

class FCustomizableObjectCompileRunnable : public FRunnable
{
public:

	struct FErrorAttachedData
	{
		TArray<float> UnassignedUVs;
	};

	struct FError
	{
		EMessageSeverity::Type Severity = EMessageSeverity::Error;
		ELoggerSpamBin SpamBin = ELoggerSpamBin::ShowAll;
		FText Message;
		TSharedPtr<FErrorAttachedData> AttachedData;
		TObjectPtr<const UObject> Context = nullptr;
		TObjectPtr<const UObject> Context2 = nullptr;

		FError(const EMessageSeverity::Type InSeverity, const FText& InMessage, const UObject* InContext, const UObject* InContext2=nullptr, const ELoggerSpamBin InSpamBin = ELoggerSpamBin::ShowAll )
			: Severity(InSeverity), SpamBin(InSpamBin), Message(InMessage), Context(InContext), Context2(InContext2) {}
		FError(const EMessageSeverity::Type InSeverity, const FText& InMessage, const TSharedPtr<FErrorAttachedData>& InAttachedData, const UObject* InContext, const ELoggerSpamBin InSpamBin = ELoggerSpamBin::ShowAll)
			: Severity(InSeverity), SpamBin(InSpamBin), Message(InMessage), AttachedData(InAttachedData), Context(InContext) {}
	};

private:

	mu::Ptr<mu::Node> MutableRoot;
	TArray<FError> ArrayErrors;

	TSharedPtr<mu::FImage> LoadImageResourceReferenced(int32 ID);

public:

	FCustomizableObjectCompileRunnable(mu::Ptr<mu::Node> Root, const TSharedRef<FCustomizableObjectCompiler>& InCompiler);

	// FRunnable interface
	uint32 Run() override;

	// Own interface

	//
	bool IsCompleted() const;

	//
	const TArray<FError>& GetArrayErrors() const;

	TSharedPtr<mu::FModel, ESPMode::ThreadSafe> Model;

	FCompilationOptions Options;

	TWeakPtr<FCustomizableObjectCompiler> WeakCompiler;

	TArray<FMutableSourceTextureData> ReferencedTextures;
	TArray<FMutableSourceMeshData> ReferencedMeshes;

	FString ErrorMsg;

	// Whether the thread has finished running
	std::atomic<bool> bThreadCompleted;
};


class FCustomizableObjectSaveDDRunnable : public FRunnable
{
public:

	FCustomizableObjectSaveDDRunnable(const TSharedPtr<FCompilationRequest>& InRequest, TSharedPtr<MutablePrivate::FMutableCachedPlatformData>& InPlatformData);

	// FRunnable interface
	uint32 Run() override;

	//
	bool IsCompleted() const;


	const ITargetPlatform* GetTargetPlatform() const;

private:

	void CachePlatformData();

	void StoreCachedPlatformDataInDDC(bool& bStoredSuccessfully);

	void StoreCachedPlatformDataToDisk();

	FCompilationOptions Options;

	MutableCompiledDataStreamHeader CustomizableObjectHeader;

	FString CustomizableObjectName;

	// Used to save files to disk
	FString FullFileName;

	UE::DerivedData::FCacheKey DDCKey;
	UE::DerivedData::ECachePolicy DefaultDDCPolicy;

	// Whether the thread has finished running
	std::atomic<bool> bThreadCompleted = false;

public:

	TArray64<uint8> ModelData;
	TArray64<uint8> ModelResourcesData;

	// Cached platform data
	TSharedPtr<MutablePrivate::FMutableCachedPlatformData> PlatformData;

	// DDC Helpers
	TArray<MutablePrivate::FFile> BulkDataFilesDDC;
};
