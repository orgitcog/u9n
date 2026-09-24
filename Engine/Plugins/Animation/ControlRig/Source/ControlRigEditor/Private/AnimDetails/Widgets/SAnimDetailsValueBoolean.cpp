// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAnimDetailsValueBoolean.h"

#include "AnimDetails/AnimDetailsMultiEditUtil.h"
#include "AnimDetails/AnimDetailsProxyManager.h"
#include "AnimDetails/Proxies/AnimDetailsProxyBase.h"
#include "EditMode/ControlRigEditMode.h"
#include "EditorModeManager.h"
#include "PropertyHandle.h"
#include "SAnimDetailsPropertySelectionBorder.h"
#include "Widgets/Input/SCheckBox.h"

namespace UE::ControlRigEditor
{
	SAnimDetailsValueBoolean::~SAnimDetailsValueBoolean()
	{
		FAnimDetailsMultiEditUtil::Get().Leave(WeakPropertyHandle);
	}

	void SAnimDetailsValueBoolean::Construct(const FArguments& InArgs, const TSharedRef<IPropertyHandle>& InPropertyHandle)
	{
		FControlRigEditMode* EditMode = static_cast<FControlRigEditMode*>(GLevelEditorModeTools().GetActiveMode(FControlRigEditMode::ModeName));
		UAnimDetailsProxyManager* ProxyManager = EditMode ? EditMode->GetAnimDetailsProxyManager() : nullptr;
		if (!ProxyManager)
		{
			return;
		}

		WeakProxyManager = ProxyManager;
		WeakPropertyHandle = InPropertyHandle;

		ChildSlot
		[
			SAssignNew(SelectionBorder, SAnimDetailsPropertySelectionBorder, InPropertyHandle)
			.RequiresModifierKeys(true)
			[
				SNew(SCheckBox)
				.Type(ESlateCheckBoxType::CheckBox)
				.IsChecked(this, &SAnimDetailsValueBoolean::GetCheckState)
				.OnCheckStateChanged(this, &SAnimDetailsValueBoolean::OnCheckStateChanged)
			]
		];

		FAnimDetailsMultiEditUtil::Get().Join(ProxyManager, InPropertyHandle);
	}

	ECheckBoxState SAnimDetailsValueBoolean::GetCheckState() const
	{
		bool bChecked{};
		if (WeakPropertyHandle.IsValid() && 
			WeakPropertyHandle.Pin()->GetValue(bChecked) == FPropertyAccess::Success)
		{
			return bChecked ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		}

		return ECheckBoxState::Undetermined;
	}

	void SAnimDetailsValueBoolean::OnCheckStateChanged(ECheckBoxState CheckBoxState)
	{
		if (WeakProxyManager.IsValid() && WeakPropertyHandle.IsValid())
		{
			const bool bEnabled = CheckBoxState == ECheckBoxState::Checked;
			UE::ControlRigEditor::FAnimDetailsMultiEditUtil::Get().MultiEditSet<bool>(*WeakProxyManager.Get(), bEnabled, WeakPropertyHandle.Pin().ToSharedRef());
		}
	}
}
