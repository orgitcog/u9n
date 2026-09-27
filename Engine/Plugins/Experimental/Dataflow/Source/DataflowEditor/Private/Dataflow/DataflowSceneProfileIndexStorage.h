// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Dataflow/DataflowPreviewProfileController.h"

class FDataflowConstructionScene;
class FDataflowSimulationScene;

class FDataflowConstructionSceneProfileIndexStorage : public FDataflowPreviewProfileController::IProfileIndexStorage
{
public:

	FDataflowConstructionSceneProfileIndexStorage(FDataflowConstructionScene* ConstructionScene);
	virtual ~FDataflowConstructionSceneProfileIndexStorage() = default;

	virtual void StoreProfileIndex(int32 Index) override;
	virtual int32 RetrieveProfileIndex() override;

private:

	FDataflowConstructionScene* ConstructionScene;
};

class FDataflowSimulationSceneProfileIndexStorage : public FDataflowPreviewProfileController::IProfileIndexStorage
{
public:

	FDataflowSimulationSceneProfileIndexStorage(FDataflowSimulationScene* SimulationScene);
	virtual ~FDataflowSimulationSceneProfileIndexStorage() = default;

	virtual void StoreProfileIndex(int32 Index) override;
	virtual int32 RetrieveProfileIndex() override;

private:

	FDataflowSimulationScene* SimulationScene;
};

