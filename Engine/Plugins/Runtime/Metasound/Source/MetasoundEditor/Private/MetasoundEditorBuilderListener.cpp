// Copyright Epic Games, Inc. All Rights Reserved.
#include "MetasoundEditorBuilderListener.h"

void UMetaSoundEditorBuilderListener::Init(const TWeakObjectPtr<UMetaSoundBuilderBase> InBuilder)
{
	if (InBuilder.IsValid())
	{
		Builder = InBuilder;
		FMetaSoundFrontendDocumentBuilder& DocumentBuilder = Builder->GetBuilder();
		Metasound::Frontend::FDocumentModifyDelegates& BuilderDelegates = DocumentBuilder.GetDocumentDelegates();
		BuilderListener = MakeShared<FEditorBuilderListener>(this);
		Builder->AddTransactionListener(BuilderListener->AsShared());
	}
}

void UMetaSoundEditorBuilderListener::OnGraphInputAdded(int32 Index)
{
	if (Builder.IsValid())
	{
		const FMetaSoundFrontendDocumentBuilder& ConstBuilder = Builder->GetConstBuilder();
		const FMetasoundFrontendGraphClass& GraphClass = ConstBuilder.GetConstDocumentChecked().RootGraph;
		const FMetasoundFrontendClassInput& GraphInput = GraphClass.GetDefaultInterface().Inputs[Index];

		OnGraphInputAddedDelegate.Broadcast(GraphInput.Name, GraphInput.TypeName);
	}
}

void UMetaSoundEditorBuilderListener::OnGraphInputDefaultChanged(int32 Index)
{
	if (Builder.IsValid())
	{
		const FMetaSoundFrontendDocumentBuilder& ConstBuilder = Builder->GetConstBuilder();
		const FMetasoundFrontendGraphClass& GraphClass = ConstBuilder.GetConstDocumentChecked().RootGraph;

		// todo: support paged graph once frontend delegates support paged literals
		const FMetasoundFrontendGraph& Graph = GraphClass.GetConstDefaultGraph();
		FName PageName = Metasound::Frontend::DefaultPageName;

		const FMetasoundFrontendClassInput& GraphInput = GraphClass.GetDefaultInterface().Inputs[Index];
		const FMetasoundFrontendLiteral* DefaultLiteral = GraphInput.FindConstDefault(Metasound::Frontend::DefaultPageID);
		if (ensure(DefaultLiteral))
		{
			OnGraphInputDefaultChangedDelegate.Broadcast(GraphInput.Name, *DefaultLiteral, PageName);
		}
	}
}

void UMetaSoundEditorBuilderListener::OnRemovingGraphInput(int32 Index)
{
	if (Builder.IsValid())
	{
		const FMetaSoundFrontendDocumentBuilder& ConstBuilder = Builder->GetConstBuilder();
		const FMetasoundFrontendGraphClass& GraphClass = ConstBuilder.GetConstDocumentChecked().RootGraph;
		const FMetasoundFrontendClassOutput& GraphInput = GraphClass.GetDefaultInterface().Inputs[Index];

		OnRemovingGraphInputDelegate.Broadcast(GraphInput.Name, GraphInput.TypeName);
	}
}

void UMetaSoundEditorBuilderListener::OnGraphOutputAdded(int32 Index)
{
	if (Builder.IsValid())
	{
		const FMetaSoundFrontendDocumentBuilder& ConstBuilder = Builder->GetConstBuilder();
		const FMetasoundFrontendGraphClass& GraphClass = ConstBuilder.GetConstDocumentChecked().RootGraph;
		const FMetasoundFrontendClassOutput& GraphOutput = GraphClass.GetDefaultInterface().Outputs[Index];

		OnGraphOutputAddedDelegate.Broadcast(GraphOutput.Name, GraphOutput.TypeName);
	}
}

void UMetaSoundEditorBuilderListener::OnRemovingGraphOutput(int32 Index)
{
	if (Builder.IsValid())
	{
		const FMetaSoundFrontendDocumentBuilder& ConstBuilder = Builder->GetConstBuilder();
		const FMetasoundFrontendGraphClass& GraphClass = ConstBuilder.GetConstDocumentChecked().RootGraph;
		const FMetasoundFrontendClassOutput& GraphOutput = GraphClass.GetDefaultInterface().Outputs[Index];

		OnRemovingGraphOutputDelegate.Broadcast(GraphOutput.Name, GraphOutput.TypeName);
	}
}


void UMetaSoundEditorBuilderListener::OnGraphInputDataTypeChanged(int32 Index)
{
	if (Builder.IsValid())
	{
		const FMetaSoundFrontendDocumentBuilder& ConstBuilder = Builder->GetConstBuilder();
		const FMetasoundFrontendGraphClass& GraphClass = ConstBuilder.GetConstDocumentChecked().RootGraph;
		const FMetasoundFrontendClassOutput& GraphInput = GraphClass.GetDefaultInterface().Inputs[Index];

		OnGraphInputDataTypeChangedDelegate.Broadcast(GraphInput.Name, GraphInput.TypeName);
	}
}

void UMetaSoundEditorBuilderListener::OnGraphOutputDataTypeChanged(int32 Index)
{
	if (Builder.IsValid())
	{
		const FMetaSoundFrontendDocumentBuilder& ConstBuilder = Builder->GetConstBuilder();
		const FMetasoundFrontendGraphClass& GraphClass = ConstBuilder.GetConstDocumentChecked().RootGraph;
		const FMetasoundFrontendClassOutput& GraphOutput = GraphClass.GetDefaultInterface().Outputs[Index];

		OnGraphOutputDataTypeChangedDelegate.Broadcast(GraphOutput.Name, GraphOutput.TypeName);
	}
}

void UMetaSoundEditorBuilderListener::OnGraphInputNameChanged(FName OldName, FName NewName)
{
	if (Builder.IsValid())
	{
		OnGraphInputNameChangedDelegate.Broadcast(OldName, NewName);
	}
}

void UMetaSoundEditorBuilderListener::OnGraphOutputNameChanged(FName OldName, FName NewName)
{
	if (Builder.IsValid())
	{
		OnGraphOutputNameChangedDelegate.Broadcast(OldName, NewName);
	}
}

void UMetaSoundEditorBuilderListener::RemoveAllDelegates()
{
	// Remove multicast BP delegates 
	OnGraphInputAddedDelegate.RemoveAll(this);
	OnGraphInputDefaultChangedDelegate.RemoveAll(this);
	OnRemovingGraphInputDelegate.RemoveAll(this);

	OnGraphOutputAddedDelegate.RemoveAll(this);
	OnRemovingGraphOutputDelegate.RemoveAll(this);

	OnGraphInputDataTypeChangedDelegate.RemoveAll(this);
	OnGraphOutputDataTypeChangedDelegate.RemoveAll(this);
	OnGraphInputNameChangedDelegate.RemoveAll(this);
	OnGraphOutputNameChangedDelegate.RemoveAll(this);

	// Remove document delegates 
	if (Builder.IsValid())
	{
		FMetaSoundFrontendDocumentBuilder& DocumentBuilder = Builder->GetBuilder();
		Metasound::Frontend::FDocumentModifyDelegates& BuilderDelegates = DocumentBuilder.GetDocumentDelegates();
		Metasound::Frontend::FInterfaceModifyDelegates& InterfaceDelegates = BuilderDelegates.InterfaceDelegates;

		InterfaceDelegates.OnInputAdded.Remove(OnInputAddedHandle);
		InterfaceDelegates.OnInputDefaultChanged.Remove(OnInputDefaultChangedHandle);
		InterfaceDelegates.OnRemovingInput.Remove(OnRemovingInputHandle);

		InterfaceDelegates.OnOutputAdded.Remove(OnOutputAddedHandle);
		InterfaceDelegates.OnRemovingOutput.Remove(OnRemovingOutputHandle);

		InterfaceDelegates.OnInputDataTypeChanged.Remove(OnInputDataTypeChangedHandle);
		InterfaceDelegates.OnOutputDataTypeChanged.Remove(OnOutputDataTypeChangedHandle);
		InterfaceDelegates.OnInputNameChanged.Remove(OnInputNameChangedHandle);
		InterfaceDelegates.OnOutputNameChanged.Remove(OnOutputNameChangedHandle);
	}
}

void UMetaSoundEditorBuilderListener::FEditorBuilderListener::OnBuilderReloaded(Metasound::Frontend::FDocumentModifyDelegates& OutDelegates)
{
	if (Parent)
	{
		Metasound::Frontend::FInterfaceModifyDelegates& InterfaceDelegates = OutDelegates.InterfaceDelegates;

		Parent->OnInputAddedHandle = InterfaceDelegates.OnInputAdded.AddUObject(Parent, &UMetaSoundEditorBuilderListener::OnGraphInputAdded);
		Parent->OnInputDefaultChangedHandle = InterfaceDelegates.OnInputDefaultChanged.AddUObject(Parent, &UMetaSoundEditorBuilderListener::OnGraphInputDefaultChanged);
		Parent->OnRemovingInputHandle = InterfaceDelegates.OnRemovingInput.AddUObject(Parent, &UMetaSoundEditorBuilderListener::OnRemovingGraphInput);

		Parent->OnOutputAddedHandle = InterfaceDelegates.OnOutputAdded.AddUObject(Parent, &UMetaSoundEditorBuilderListener::OnGraphOutputAdded);
		Parent->OnRemovingOutputHandle = InterfaceDelegates.OnRemovingOutput.AddUObject(Parent, &UMetaSoundEditorBuilderListener::OnRemovingGraphOutput);

		Parent->OnInputDataTypeChangedHandle = InterfaceDelegates.OnInputDataTypeChanged.AddUObject(Parent, &UMetaSoundEditorBuilderListener::OnGraphInputDataTypeChanged);
		Parent->OnOutputDataTypeChangedHandle = InterfaceDelegates.OnOutputDataTypeChanged.AddUObject(Parent, &UMetaSoundEditorBuilderListener::OnGraphOutputDataTypeChanged);
		Parent->OnInputNameChangedHandle = InterfaceDelegates.OnInputNameChanged.AddUObject(Parent, &UMetaSoundEditorBuilderListener::OnGraphInputNameChanged);
		Parent->OnOutputNameChangedHandle = InterfaceDelegates.OnOutputNameChanged.AddUObject(Parent, &UMetaSoundEditorBuilderListener::OnGraphOutputNameChanged);
	}
}
