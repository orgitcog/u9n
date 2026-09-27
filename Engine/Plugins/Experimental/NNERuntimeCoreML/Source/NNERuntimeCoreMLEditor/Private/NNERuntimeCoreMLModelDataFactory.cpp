// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNERuntimeCoreMLModelDataFactory.h"

#include "Editor.h"
#include "NNEModelData.h"
#include "Misc/Paths.h"
#include "Subsystems/ImportSubsystem.h"


UNNERuntimeCoreMLModelDataFactory::UNNERuntimeCoreMLModelDataFactory(const FObjectInitializer& ObjectInitializer) : UFactory(ObjectInitializer)
{
	bCreateNew = false;
	bEditorImport = true;
	SupportedClass = UNNEModelData::StaticClass();
	ImportPriority = DefaultImportPriority;
	Formats.Add(TEXT("mlmodel;Core ML model format"));
}

UObject* UNNERuntimeCoreMLModelDataFactory::FactoryCreateBinary(UClass* InClass, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, const TCHAR * Type, const uint8 *& Buffer, const uint8 * BufferEnd, FFeedbackContext* Warn)
{
	GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPreImport(this, InClass, InParent, Name, Type);

	if (!Type || !Buffer || !BufferEnd || BufferEnd - Buffer <= 0)
	{
		GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPostImport(this, nullptr);
		return nullptr;
	}

	TConstArrayView<uint8> BufferView = MakeArrayView(Buffer, BufferEnd - Buffer);
	UNNEModelData* ModelData = NewObject<UNNEModelData>(InParent, InClass, Name, Flags);
	check(ModelData)

	ModelData->Init(Type, BufferView);

	GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPostImport(this, ModelData);

	return ModelData;
}

bool UNNERuntimeCoreMLModelDataFactory::FactoryCanImport(const FString & Filename)
{
	return Filename.EndsWith(FString(TEXT("mlmodel")));
}
