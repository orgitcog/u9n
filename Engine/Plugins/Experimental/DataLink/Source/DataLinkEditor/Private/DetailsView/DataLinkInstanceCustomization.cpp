// Copyright Epic Games, Inc. All Rights Reserved.

#include "DataLinkInstanceCustomization.h"
#include "DataLinkGraph.h"
#include "DataLinkInstance.h"
#include "DataLinkInstancedStructNodeBuilder.h"
#include "DataLinkPin.h"
#include "DataLinkPinReference.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "PropertyCustomizationHelpers.h"

FDataLinkInstanceCustomization::FDataLinkInstanceCustomization(bool bInGenerateHeader)
	: bGenerateHeader(bInGenerateHeader)
{
}

FDataLinkInstanceCustomization::~FDataLinkInstanceCustomization()
{
	UDataLinkGraph::OnGraphCompiled().RemoveAll(this);
}

void FDataLinkInstanceCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> InPropertyHandle, FDetailWidgetRow& InHeaderRow, IPropertyTypeCustomizationUtils& InCustomizationUtils)
{
	UDataLinkGraph::OnGraphCompiled().AddSP(this, &FDataLinkInstanceCustomization::OnGraphCompiled);

	DataLinkGraphHandle = InPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDataLinkInstance, DataLinkGraph));
	check(DataLinkGraphHandle.IsValid());
	DataLinkGraphHandle->MarkHiddenByCustomization();
	DataLinkGraphHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FDataLinkInstanceCustomization::OnGraphChanged));

	InputDataHandle = InPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDataLinkInstance, InputData));
	check(InputDataHandle.IsValid());
	InputDataHandle->MarkHiddenByCustomization();

	if (bGenerateHeader)
	{
		InHeaderRow
			.NameContent()
			[
				InPropertyHandle->CreatePropertyNameWidget()
			]
			.ValueContent()
			[
				DataLinkGraphHandle->CreatePropertyValueWidget()
			];
	}

	UpdateInputData();
}

void FDataLinkInstanceCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> InPropertyHandle, IDetailChildrenBuilder& InChildBuilder, IPropertyTypeCustomizationUtils& InCustomizationUtils)
{
	check(InputDataHandle.IsValid());

	UpdateInputData();

	TSharedRef<FDetailArrayBuilder> ArrayBuilder = MakeShared<FDetailArrayBuilder>(InputDataHandle.ToSharedRef()
		, /*bGenerateHeader*/false
		, /*bDisplayResetToDefault*/false
		, /*DisplayElementNum*/false);

	ArrayBuilder->OnGenerateArrayElementWidget(FOnGenerateArrayElementWidget::CreateLambda(
		[&InputDisplayNames = InputDisplayNames](TSharedRef<IPropertyHandle> InElementHandle, int32 InIndex, IDetailChildrenBuilder& InChildBuilder)
		{
			if (InputDisplayNames.IsValidIndex(InIndex))
			{
				InElementHandle->SetPropertyDisplayName(InputDisplayNames[InIndex]);
			}
			InChildBuilder.AddCustomBuilder(MakeShared<FDataLinkInstancedStructNodeBuilder>(InElementHandle));
		}));

	InChildBuilder.AddCustomBuilder(ArrayBuilder);
}

void FDataLinkInstanceCustomization::OnGraphCompiled(UDataLinkGraph* InDataLinkGraph)
{
	if (InDataLinkGraph == GetDataLinkGraph())
	{
		OnGraphChanged();
	}
}

UDataLinkGraph* FDataLinkInstanceCustomization::GetDataLinkGraph() const
{
	UObject* DataLink = nullptr;
	if (DataLinkGraphHandle.IsValid() && DataLinkGraphHandle->GetValue(DataLink) == FPropertyAccess::Success)
	{
		return Cast<UDataLinkGraph>(DataLink);
	}
	return nullptr;
}

TArray<FInstancedStruct>* FDataLinkInstanceCustomization::GetInputData() const
{
	void* RawData = nullptr;
	if (InputDataHandle.IsValid() && InputDataHandle->GetValueData(RawData) == FPropertyAccess::Success)
	{
		return static_cast<TArray<FInstancedStruct>*>(RawData);
	}
	return nullptr;
}

void FDataLinkInstanceCustomization::OnGraphChanged()
{
	if (!InputDataHandle.IsValid())
	{
		return;
	}

	InputDataHandle->NotifyPreChange();
	UpdateInputData();
	InputDataHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
}

void FDataLinkInstanceCustomization::UpdateInputData()
{
	TArray<FInstancedStruct>* InputData = GetInputData();
	if (!InputData)
	{
		return;
	}

	UDataLinkGraph* DataLinkGraph = GetDataLinkGraph();
	if (!DataLinkGraph)
	{
		InputDisplayNames.Reset();
		InputData->Reset();
		return;
	}

	const int32 InputPinCount = DataLinkGraph->GetInputPinCount();
	InputDisplayNames.SetNum(InputPinCount);
	InputData->SetNum(InputPinCount);

	int32 Index = 0;
	DataLinkGraph->ForEachInputPin(
		[InputData, &Index, &InputDisplayNames = InputDisplayNames](const FDataLinkPinReference& InPinReference)->bool
		{
			InputDisplayNames[Index] = InPinReference.Pin->GetDisplayName();

			FInstancedStruct& InstanceData = (*InputData)[Index];
			if (InstanceData.GetScriptStruct() != InPinReference.Pin->Struct)
			{
				// Input Struct can be null, FInstancedStruct will just be reset
				InstanceData.InitializeAs(InPinReference.Pin->Struct);
			}

			++Index;
			return true;
		});
}
