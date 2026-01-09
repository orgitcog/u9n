// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HierarchyTableTypeHandler.h"

#include "DefaultHierarchyTableTypeHandler.generated.h"

UCLASS()
class UHierarchyTable_TableTypeHandler_Default final : public UHierarchyTable_TableTypeHandler
{
	GENERATED_BODY()

public:
	virtual void ConstructHierarchy() override;

	virtual bool FactoryConfigureProperties(FInstancedStruct& TableType) const override;
};