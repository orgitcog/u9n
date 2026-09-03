// Copyright Epic Games, Inc. All Rights Reserved.

#include "Stateless/NiagaraStatelessEmitterDetails.h"
#include "Stateless/NiagaraStatelessEmitter.h"
#include "NiagaraSystem.h"

#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
//#include "IDetailChildrenBuilder.h"
//#include "IDetailGroup.h"
//#include "DetailWidgetRow.h"

#define LOCTEXT_NAMESPACE "NiagaraStatelessEmitter"

TSharedRef<IDetailCustomization> FNiagaraStatelessEmitterDetails::MakeInstance()
{
	return MakeShared<FNiagaraStatelessEmitterDetails>();
}

void FNiagaraStatelessEmitterDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	UNiagaraStatelessEmitter* Emitter = nullptr;
	{
		TArray<TWeakObjectPtr<UObject>> ObjectsBeingCustomized;
		DetailBuilder.GetObjectsBeingCustomized(ObjectsBeingCustomized);
		Emitter = ObjectsBeingCustomized.Num() == 1 ? Cast<UNiagaraStatelessEmitter>(ObjectsBeingCustomized[0]) : nullptr;
	}
	if (Emitter == nullptr)
	{
		return;
	}
	WeakEmitter = Emitter;

	static const FName NAME_EmitterProperties("Emitter Properties");
	static const FName NAME_FixedBounds("FixedBounds");

	IDetailCategoryBuilder& DetailCategory = DetailBuilder.EditCategory(NAME_EmitterProperties);

	TArray<TSharedRef<IPropertyHandle>> CategoryProperties;
	DetailCategory.GetDefaultProperties(CategoryProperties);

	for (TSharedRef<IPropertyHandle>& PropertyHandle : CategoryProperties)
	{
		const FName PropertyName = PropertyHandle->GetProperty() ? PropertyHandle->GetProperty()->GetFName() : NAME_None;
		if (PropertyName == NAME_FixedBounds)
		{
			IDetailPropertyRow& PropertyRow = DetailCategory.AddProperty(PropertyHandle);
			PropertyRow.IsEnabled(TAttribute<bool>::CreateSP(this, &FNiagaraStatelessEmitterDetails::GetFixedBoundsEnabled));
		}
		else
		{
			DetailCategory.AddProperty(PropertyHandle);
		}
	}
}

bool FNiagaraStatelessEmitterDetails::GetFixedBoundsEnabled() const
{
	if (const UNiagaraStatelessEmitter* Emitter = WeakEmitter.Get())
	{
		if (const UNiagaraSystem* System = Emitter->GetTypedOuter<UNiagaraSystem>())
		{
			return System->bFixedBounds == false;
		}
	}
	return false;
}

#undef LOCTEXT_NAMESPACE
