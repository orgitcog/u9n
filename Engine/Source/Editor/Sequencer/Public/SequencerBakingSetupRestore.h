// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Templates/SharedPointer.h"
#include "Misc/Optional.h"

class ISequencer;

namespace UE::Sequencer
{

	class SEQUENCER_API FSequencerBakingSetupRestore 
	{
	public:
		FSequencerBakingSetupRestore() = delete;
		FSequencerBakingSetupRestore(TSharedPtr<ISequencer>& SequencerPtr);
		~FSequencerBakingSetupRestore();
		
	private:
		TWeakPtr<ISequencer> WeakSequencer;
		TOptional<bool> bRestoreShouldEvaluateSubSequencesInIsolation;
		
	};

} // namespace UE::Sequencer