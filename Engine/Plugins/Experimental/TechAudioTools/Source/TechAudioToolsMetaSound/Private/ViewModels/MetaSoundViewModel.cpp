// Copyright Epic Games, Inc. All Rights Reserved.

#include "ViewModels/MetaSoundViewModel.h"

#include "Logging/StructuredLog.h"
#include "MetasoundDocumentBuilderRegistry.h"
#include "MetasoundOutputSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaSoundViewModel)

DEFINE_LOG_CATEGORY(LogTechAudioToolsMetaSound);

namespace TechAudioTools::MetaSoundViewModel
{
	FName GetAdjustedDataType(const FName& CurrentDataType, const bool bIsArray)
	{
		const FString DataTypeString = CurrentDataType.ToString();
		if (bIsArray)
		{
			// Append the suffix if it isn't already there
			return DataTypeString.EndsWith(TEXT(":Array")) ? CurrentDataType : FName(DataTypeString + TEXT(":Array"));
		}
		else
		{
			// Split on ':' and keep the base data type
			FString BaseDataType;
			DataTypeString.Split(TEXT(":"), &BaseDataType, nullptr);
			return FName(BaseDataType);
		}
	}
} // TechAudioTools::MetaSoundViewModel

FText UMetaSoundViewModel::GetBuilderNameAsText() const
{
	return FText::FromString(GetNameSafe(Builder));
}

TArray<UMetaSoundInputViewModel*> UMetaSoundViewModel::GetInputViewModels() const
{
	TArray<TObjectPtr<UMetaSoundInputViewModel>> InputViewModelsArray;
	InputViewModels.GenerateValueArray(InputViewModelsArray);
	return InputViewModelsArray;
}

TArray<UMetaSoundOutputViewModel*> UMetaSoundViewModel::GetOutputViewModels() const
{
	TArray<TObjectPtr<UMetaSoundOutputViewModel>> OutputViewModelsArray;
	OutputViewModels.GenerateValueArray(OutputViewModelsArray);
	return OutputViewModelsArray;
}

void UMetaSoundViewModel::InitializeMetaSound(const TScriptInterface<IMetaSoundDocumentInterface> InMetaSound)
{
	if (!InMetaSound)
	{
		Reset();
		return;
	}

	Builder = Metasound::Engine::FDocumentBuilderRegistry::GetChecked().FindBuilderObject(InMetaSound);
	Initialize(Builder);
}

void UMetaSoundViewModel::Initialize(UMetaSoundBuilderBase* InBuilder)
{
	Reset();

	if (!InBuilder)
	{
		UE_LOGFMT(LogTechAudioToolsMetaSound, Log, "Unable to initialize MetaSoundViewModel. Builder was null.");
		return;
	}

	Builder = InBuilder;

	const FMetaSoundFrontendDocumentBuilder& DocumentBuilder = InBuilder->GetConstBuilder();
	const FMetasoundFrontendDocument& FrontendDocument = DocumentBuilder.GetConstDocumentChecked();

	InitializeProperties(FrontendDocument);

	CreateMemberViewModels();
	SetIsInitialized(true);
}

void UMetaSoundViewModel::Reset()
{
	SetIsInitialized(false);
	ResetProperties();
	Builder = nullptr;

	InputViewModels.Empty();
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetInputViewModels);

	OutputViewModels.Empty();
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetOutputViewModels);
}

void UMetaSoundViewModel::InitializeProperties(const FMetasoundFrontendDocument& FrontendDocument)
{
	UE_MVVM_SET_PROPERTY_VALUE(bIsPreset, FrontendDocument.RootGraph.PresetOptions.bIsPreset);
}

void UMetaSoundViewModel::ResetProperties()
{
	UE_MVVM_SET_PROPERTY_VALUE(bIsPreset, false);
}

void UMetaSoundViewModel::CreateMemberViewModels()
{
	const FMetaSoundFrontendDocumentBuilder& DocumentBuilder = Builder->GetConstBuilder();
	const FMetasoundFrontendDocument& FrontendDocument = DocumentBuilder.GetConstDocumentChecked();

	for (const FMetasoundFrontendClassInput& Input : FrontendDocument.RootGraph.GetDefaultInterface().Inputs)
	{
		CreateInputViewModel(Input);
	}
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetInputViewModels);

	for (const FMetasoundFrontendClassOutput& Output : FrontendDocument.RootGraph.GetDefaultInterface().Outputs)
	{
		CreateOutputViewModel(Output);
	}
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetOutputViewModels);
}

void UMetaSoundViewModel::CreateInputViewModel(const FMetasoundFrontendClassInput& InInput)
{
	UMetaSoundInputViewModel* InputViewModel = CreateInputViewModelInstance();
	check(InputViewModel);

	InputViewModel->InitializeInput(Builder.Get(), InInput);
	InputViewModels.Emplace(InInput.Name, InputViewModel);

	// Prevent spamming field notify broadcasts while we're still initializing.
	if (bIsInitialized)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetInputViewModels);
	}
}

UMetaSoundInputViewModel* UMetaSoundViewModel::CreateInputViewModelInstance()
{
	return NewObject<UMetaSoundInputViewModel>(this, UMetaSoundInputViewModel::StaticClass());
}

void UMetaSoundViewModel::CreateOutputViewModel(const FMetasoundFrontendClassOutput& InOutput)
{
	UMetaSoundOutputViewModel* OutputViewModel = CreateOutputViewModelInstance();
	check(OutputViewModel);

	OutputViewModel->InitializeOutput(Builder.Get(), InOutput);
	OutputViewModels.Emplace(InOutput.Name, OutputViewModel);

	if (bIsInitialized)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetOutputViewModels);
	}
}

UMetaSoundOutputViewModel* UMetaSoundViewModel::CreateOutputViewModelInstance()
{
	return NewObject<UMetaSoundOutputViewModel>(this, UMetaSoundOutputViewModel::StaticClass());
}

void UMetaSoundInputViewModel::InitializeInput(UMetaSoundBuilderBase* InBuilder, const FMetasoundFrontendClassInput& InInput)
{
	ResetInput();
	Builder = InBuilder;

	UE_MVVM_SET_PROPERTY_VALUE(InputName, InInput.Name);
	UE_MVVM_SET_PROPERTY_VALUE(DataType, InInput.TypeName);

	const FGuid PageID = Metasound::Engine::FDocumentBuilderRegistry::GetChecked().ResolveTargetPageID(InInput);
	const FMetasoundFrontendLiteral& DefaultLiteral = InInput.FindConstDefaultChecked(PageID);

	UE_MVVM_SET_PROPERTY_VALUE(LiteralType, DefaultLiteral.GetType());
	UE_MVVM_SET_PROPERTY_VALUE(Literal, DefaultLiteral);
	UE_MVVM_SET_PROPERTY_VALUE(bIsArray, DefaultLiteral.IsArray());

	SetIsInitialized(true);
}

void UMetaSoundInputViewModel::ResetInput()
{
	SetIsInitialized(false);
	Builder = nullptr;

	UE_MVVM_SET_PROPERTY_VALUE(InputName, FName());
	UE_MVVM_SET_PROPERTY_VALUE(DataType, FName());
	UE_MVVM_SET_PROPERTY_VALUE(LiteralType, EMetasoundFrontendLiteralType::Invalid);
	UE_MVVM_SET_PROPERTY_VALUE(Literal, FMetasoundFrontendLiteral());
	UE_MVVM_SET_PROPERTY_VALUE(bIsArray, false);
}

void UMetaSoundInputViewModel::SetIsInitialized(const bool bInIsInitialized)
{
	UE_MVVM_SET_PROPERTY_VALUE(bIsInitialized, bInIsInitialized);
}

void UMetaSoundInputViewModel::SetInputName(const FName& InInputName)
{
	if (!Builder)
	{
		UE_LOGFMT(LogTechAudioToolsMetaSound, Log, "Failed to set input name {new} for {old}. Builder was null.", InInputName, InputName);
		return;
	}

	const FName OldName = InputName;
	if (UE_MVVM_SET_PROPERTY_VALUE(InputName, InInputName))
	{
		EMetaSoundBuilderResult Result;
		Builder->SetGraphInputName(OldName, InInputName, Result);

		if (Result != EMetaSoundBuilderResult::Succeeded)
		{
			UE_LOGFMT(LogTechAudioToolsMetaSound, Log, "Failed to set input name {new} for {old}. Builder was null.", InInputName, InputName);
		}
	}
}

void UMetaSoundInputViewModel::SetDataType(const FName& InDataType)
{
	if (!Builder)
	{
		UE_LOGFMT(LogTechAudioToolsMetaSound, Log, "Failed to set data type for {input}. Builder was null.", InputName);
		return;
	}

	FMetaSoundFrontendDocumentBuilder& DocumentBuilder = Builder->GetBuilder();
	if (!DocumentBuilder.IsValid())
	{
		UE_LOGFMT(LogTechAudioToolsMetaSound, Log, "Failed to set data type for {input}. DocumentBuilder was invalid.", InputName);
		return;
	}

	if (UE_MVVM_SET_PROPERTY_VALUE(DataType, InDataType))
	{
		DocumentBuilder.SetGraphInputDataType(InputName, InDataType);
	}
}

void UMetaSoundInputViewModel::SetIsArray(const bool bInIsArray)
{
	if (UE_MVVM_SET_PROPERTY_VALUE(bIsArray, bInIsArray))
	{
		SetDataType(TechAudioTools::MetaSoundViewModel::GetAdjustedDataType(DataType, bInIsArray));
	}
}

FText UMetaSoundInputViewModel::GetLiteralValueAsText() const
{
	return FText::FromString(Literal.ToString());
}

void UMetaSoundInputViewModel::SetLiteral(const FMetasoundFrontendLiteral& InLiteral)
{
	if (!Builder)
	{
		UE_LOGFMT(LogTechAudioToolsMetaSound, Log, "Failed to set literal for {input}. Builder was null.", InputName);
		return;
	}

	if (UE_MVVM_SET_PROPERTY_VALUE(Literal, InLiteral))
	{
		EMetaSoundBuilderResult Result;
		Builder->SetGraphInputDefault(InputName, InLiteral, Result);
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetLiteralValueAsText);
	}
}

void UMetaSoundOutputViewModel::InitializeOutput(UMetaSoundBuilderBase* InBuilder, const FMetasoundFrontendClassOutput& InOutput)
{
	ResetOutput();
	Builder = InBuilder;

	UE_MVVM_SET_PROPERTY_VALUE(OutputName, InOutput.Name);
	UE_MVVM_SET_PROPERTY_VALUE(DataType, InOutput.TypeName);
	SetMetaSoundOutput(FMetaSoundOutput());
	SetIsInitialized(true);
}

void UMetaSoundOutputViewModel::ResetOutput()
{
	SetMetaSoundOutput(FMetaSoundOutput());
}

void UMetaSoundOutputViewModel::SetIsInitialized(const bool bInIsInitialized)
{
	UE_MVVM_SET_PROPERTY_VALUE(bIsInitialized, bInIsInitialized);
}

void UMetaSoundOutputViewModel::SetOutputName(const FName& InOutputName)
{
	if (!Builder)
	{
		UE_LOGFMT(LogTechAudioToolsMetaSound, Log, "Failed to set output name for {output}. Builder was null.", OutputName);
		return;
	}

	const FName OldName = OutputName;
	if (UE_MVVM_SET_PROPERTY_VALUE(OutputName, InOutputName))
	{
		EMetaSoundBuilderResult Result;
		Builder->SetGraphOutputName(OldName, InOutputName, Result);

		if (Result != EMetaSoundBuilderResult::Succeeded)
		{
			UE_LOGFMT(LogTechAudioToolsMetaSound, Log, "Failed to set output name for {output}", OutputName);
		}
	}
}

void UMetaSoundOutputViewModel::SetDataType(const FName& InDataType)
{
	if (!Builder)
	{
		UE_LOGFMT(LogTechAudioToolsMetaSound, Log, "Failed to set data type for {output}. Builder was null.", OutputName);
		return;
	}

	FMetaSoundFrontendDocumentBuilder& DocumentBuilder = Builder->GetBuilder();
	if (!DocumentBuilder.IsValid())
	{
		UE_LOGFMT(LogTechAudioToolsMetaSound, Log, "Failed to set data type for {output}. DocumentBuilder was invalid.", OutputName);
		return;
	}

	if (UE_MVVM_SET_PROPERTY_VALUE(DataType, InDataType))
	{
		DocumentBuilder.SetGraphOutputDataType(OutputName, InDataType);
	}
}

void UMetaSoundOutputViewModel::SetIsArray(const bool bInIsArray)
{
	if (UE_MVVM_SET_PROPERTY_VALUE(bIsArray, bInIsArray))
	{
		SetDataType(TechAudioTools::MetaSoundViewModel::GetAdjustedDataType(DataType, bInIsArray));
	}
}

void UMetaSoundOutputViewModel::SetMetaSoundOutput(const FMetaSoundOutput& InMetaSoundOutput)
{
	MetaSoundOutput = InMetaSoundOutput;
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(MetaSoundOutput);
}

void UMetaSoundOutputViewModel::OnOutputValueChanged(const FName InOutputName, const FMetaSoundOutput& InOutput)
{
	if (OutputName == InOutputName)
	{
		SetMetaSoundOutput(InOutput);
	}
}
