// Copyright Epic Games, Inc. All Rights Reserved.

#if 0

#pragma once

#include "MetasoundFacade.h"
#include "MetasoundNodeInterface.h"
#include "Templates/SharedPointer.h"

namespace Metasound
{
	class FCatMixerNode final : public FNodeFacade
	{
	public:
		explicit FCatMixerNode(FNodeData InNodeData, TSharedRef<const FNodeClassMetadata> InClassMetadata);
		virtual ~FCatMixerNode() override = default;

		static FNodeClassMetadata CreateNodeClassMetadata();
	};
}

#endif //