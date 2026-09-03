// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "Internationalization/Text.h"
#include "Templates/SharedPointer.h"
#include "Templates/ValueOrError.h"

#include "Templates/PimplPtr.h"

class CAPTUREMANAGERPIPELINE_API FCaptureManagerPipelineError
{
public:
	FCaptureManagerPipelineError(FText InMessage, int32 InCode = 0);

	const FText& GetMessage() const;

	int32 GetCode() const;

private:

	FText Message;
	int32 Code;
};

class CAPTUREMANAGERPIPELINE_API FCaptureManagerPipelineNode : public TSharedFromThis<FCaptureManagerPipelineNode>
{
public:

	using FResult = TValueOrError<void, FCaptureManagerPipelineError>;

	FCaptureManagerPipelineNode(const FString& InName);
	virtual ~FCaptureManagerPipelineNode();

	FString GetName() const;

	FResult Execute();
	void Cancel();

protected:

	virtual FResult Prepare() = 0;
	virtual FResult Run() = 0;
	virtual FResult Validate() = 0;

private:
	class FCaptureManagerPipelineNodeImpl;
	TPimplPtr<FCaptureManagerPipelineNodeImpl> Impl;
};