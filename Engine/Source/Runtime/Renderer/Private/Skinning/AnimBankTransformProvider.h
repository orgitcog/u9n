// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animation/AnimBank.h"
#include "Delegates/Delegate.h"
#include "Delegates/DelegateCombinations.h"
#include "RendererPrivateUtils.h"
#include "SceneExtensions.h"
#include "SkinningTransformProvider.h"
#include "SpanAllocator.h"

class FAnimBankTransformProvider : public ISceneExtension
{
	DECLARE_SCENE_EXTENSION(RENDERER_API, FAnimBankTransformProvider);

public:
	using ISceneExtension::ISceneExtension;

	static bool ShouldCreateExtension(FScene& InScene);

	virtual void InitExtension(FScene& InScene) override;

	RENDERER_API FAnimBankRecordHandle RegisterBank(const FAnimBankDesc& Desc);
	RENDERER_API void UnregisterBank(const FAnimBankRecordHandle& Handle);

	RENDERER_API static FAnimBankRecordHandle ComputeDescHash(const FAnimBankDesc& Desc);

private:
	void AdvanceAnimation(FSkinningTransformProvider::FProviderContext& Context);
	void ScatterAnimation(
		FSkinningTransformProvider::FProviderContext& Context,
		const TConstArrayView<uint32> IdToOffsetMapping,
		FRDGBufferRef SrcTransformBuffer
	);

	void ProvideGPUBankTransforms(FSkinningTransformProvider::FProviderContext& Context);
	void ProvideCPUBankTransforms(FSkinningTransformProvider::FProviderContext& Context);

private:
	FAnimBankRecordMap BankRecordMap;
	FSpanAllocator BankAllocator;
};

RENDERER_API const FSkinningTransformProvider::FProviderId& GetAnimBankProviderId();