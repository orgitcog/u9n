// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "InstancedStructDetails.h"

// Forward Declarations
struct FPropertyChangedEvent;
class FInstancedStructDataDetails;
class IDetailPropertyRow;
class IPropertyHandle;
class UMetasoundEditorGraphNode;

namespace Metasound::Editor
{
	class METASOUNDEDITOR_API FMetaSoundNodeConfigurationDataDetails : public FInstancedStructDataDetails
	{
	public:
		FMetaSoundNodeConfigurationDataDetails(TSharedPtr<IPropertyHandle> InStructProperty, TWeakObjectPtr<UMetasoundEditorGraphNode> InNode)
			: FInstancedStructDataDetails(InStructProperty)
			, GraphNode(InNode)
		{
		}

		virtual void OnChildRowAdded(IDetailPropertyRow& ChildRow) override;

	protected:
		void OnChildPropertyChanged(const FPropertyChangedEvent& InPropertyChangedEvent);

		TWeakObjectPtr<UMetasoundEditorGraphNode> GraphNode;
	};
} // namespace Metasound::Editor
