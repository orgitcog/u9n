// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IntVectorTypes.h"
#include "Pipeline/Node.h"
#include "Pipeline/PipelineData.h"

class UTextureRenderTarget2D;

namespace UE::MetaHuman
{
struct IFramePathResolver;
}

namespace UE::MetaHuman::Pipeline
{

class METAHUMANPIPELINECORE_API FUEImageLoadNode : public FNode
{
public:

	FUEImageLoadNode(const FString& InName);
	virtual ~FUEImageLoadNode() override;

	virtual bool Process(const TSharedPtr<FPipelineData>& InPipelineData) override;

	TUniquePtr<IFramePathResolver> FramePathResolver;
	bool bFailOnMissingFile = false;

	enum ErrorCode
	{
		FailedToLoadFile = 0,
		FailedToFindFile,
		NoFramePathResolver,
		BadFilePath,
	};
};

class METAHUMANPIPELINECORE_API FUEImageSaveNode : public FNode
{
public:

	FUEImageSaveNode(const FString& InName);

	virtual bool Process(const TSharedPtr<FPipelineData>& InPipelineData) override;

	FString FilePath;
	int32 FrameNumberOffset = 0;

	enum ErrorCode
	{
		FailedToSaveFile = 0,
		FailedToCompressData,
		MissingFrameFormatSpecifier
	};
};

class METAHUMANPIPELINECORE_API FUEImageResizeNode : public FNode
{
public:

	FUEImageResizeNode(const FString& InName);

	virtual bool Process(const TSharedPtr<FPipelineData>& InPipelineData) override;

	int32 MaxSize = 1;
};

class METAHUMANPIPELINECORE_API FUEImageCropNode : public FNode
{
public:

	FUEImageCropNode(const FString& InName);

	virtual bool Process(const TSharedPtr<FPipelineData>& InPipelineData) override;

	int32 X = -1;
	int32 Y = -1;
	int32 Width = -1;
	int32 Height = -1;

	enum ErrorCode
	{
		BadValues = 0
	};
};

class METAHUMANPIPELINECORE_API FUEImageRotateNode : public FNode
{
public:

	FUEImageRotateNode(const FString& InName);

	virtual bool Process(const TSharedPtr<FPipelineData>& InPipelineData) override;

	void SetAngle(float InAngle);
	float GetAngle();

	enum ErrorCode
	{
		UnsupportedAngle = 0
	};

private:

	float Angle = 0;
	FCriticalSection AngleMutex;
};

class METAHUMANPIPELINECORE_API FUEImageCompositeNode : public FNode
{
public:

	FUEImageCompositeNode(const FString& InName);

	virtual bool Process(const TSharedPtr<FPipelineData>& InPipelineData) override;
};

class METAHUMANPIPELINECORE_API FUEImageToUEGrayImageNode : public FNode
{
public:

	FUEImageToUEGrayImageNode(const FString& InName);

	virtual bool Process(const TSharedPtr<FPipelineData>& InPipelineData) override;
};

class METAHUMANPIPELINECORE_API FUEGrayImageToUEImageNode : public FNode
{
public:

	FUEGrayImageToUEImageNode(const FString& InName);

	virtual bool Process(const TSharedPtr<FPipelineData>& InPipelineData) override;
};

class METAHUMANPIPELINECORE_API FUEImageToHSImageNode : public FNode
{
public:

	FUEImageToHSImageNode(const FString& InName);

	virtual bool Process(const TSharedPtr<FPipelineData>& InPipelineData) override;
};

class METAHUMANPIPELINECORE_API FBurnContoursNode : public FNode
{
public:

	FBurnContoursNode(const FString& InName);

	virtual bool Process(const TSharedPtr<FPipelineData>& InPipelineData) override;

	int32 Size = 1;
	int32 LineWidth = 0; // set this to a value > 0 to connect the contour points by lines
};

class METAHUMANPIPELINECORE_API FDepthLoadNode : public FNode
{
public:

	FDepthLoadNode(const FString& InName);
	virtual ~FDepthLoadNode() override;

	virtual bool Process(const TSharedPtr<FPipelineData>& InPipelineData) override;

	TUniquePtr<IFramePathResolver> FramePathResolver;
	bool bFailOnMissingFile = false;

	enum ErrorCode
	{
		FailedToLoadFile = 0,
		FailedToFindFile,
		NoFramePathResolver
	};
};

class METAHUMANPIPELINECORE_API FDepthSaveNode : public FNode
{
public:

	FDepthSaveNode(const FString& InName);

	virtual bool Process(const TSharedPtr<FPipelineData>& InPipelineData) override;

	FString FilePath;
	int32 FrameNumberOffset = 0;
	bool bShouldCompressFiles = true;

	enum ErrorCode
	{
		FailedToSaveFile = 0,
		FailedToCompressData,
		MissingFrameFormatSpecifier
	};
};

class METAHUMANPIPELINECORE_API FDepthQuantizeNode : public FNode
{
public:

	FDepthQuantizeNode(const FString& InName);

	virtual bool Process(const TSharedPtr<FPipelineData>& InPipelineData) override;

	float Factor = 80; // Eightieth of a cm (0.125mm). This equals the oodle compression quantization. 
};

class METAHUMANPIPELINECORE_API FDepthResizeNode : public FNode
{
public:

	FDepthResizeNode(const FString& InName);

	virtual bool Process(const TSharedPtr<FPipelineData>& InPipelineData) override;

	int32 Factor = 1;
};

class METAHUMANPIPELINECORE_API FDepthToUEImageNode : public FNode
{
public:

	FDepthToUEImageNode(const FString& InName);

	virtual bool Process(const TSharedPtr<FPipelineData>& InPipelineData) override;

	float Min = 0.0f;
	float Max = 1.0f;

	enum ErrorCode
	{
		BadRange = 0
	};
};

class METAHUMANPIPELINECORE_API FFColorToUEImageNode : public FNode
{
public:
	FFColorToUEImageNode(const FString& InName);
	
	virtual bool Process(const TSharedPtr<FPipelineData>& InPipelineData) override;
	
	TArray<FColor> Samples;
	int32 Width = -1;
	int32 Height = -1;

	enum ErrorCode
	{
		NoInputImage = 0
	};
}; 

class METAHUMANPIPELINECORE_API FCopyImagesNode : public FNode
{
public:
	FCopyImagesNode(const FString& InName);

	virtual bool Process(const TSharedPtr<FPipelineData>& InPipelineData) override;

	int32 FrameNumberOffset = 0;

	FString InputFilePath;
	FString OutputDirectoryPath;

	enum ErrorCode
	{
		FailedToFindFile = 0,
		FailedToCopyFile,
		MissingFrameFormatSpecifier
	};
};

}
