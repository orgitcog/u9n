// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointerFwd.h"

class ISequencer;
class FCurveEditor;

namespace UE::ControlRigEditor
{
/** @return The curve editor used by sequencer */
TSharedPtr<FCurveEditor> GetCurveEditorFromSequencer(const TSharedPtr<ISequencer>& InSequencer);
}
