// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MVVM/ViewModels/OutlinerColumns/OutlinerColumnBase.h"

namespace UE::Sequencer
{

/**
 * A column for local (not serialized to the asset) muting/unmuting tracks.
 */
class FDeactivateOutlinerColumn
	: public FOutlinerColumnBase
{
public:
	SEQUENCERCORE_API FDeactivateOutlinerColumn();

	bool IsItemCompatibleWithColumn(const FCreateOutlinerColumnParams& InParams) const override;
	TSharedPtr<SWidget> CreateColumnWidget(const FCreateOutlinerColumnParams& InParams, const TSharedRef<ISequencerTreeViewRow>& TreeViewRow) override;
};
	
} // namespace UE::Sequencer
