// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundNodeInterface.h"

#include "Containers/UnrealString.h"
#include "Internationalization/Text.h"
#include "Misc/Guid.h"
#include "UObject/NameTypes.h"

namespace Metasound
{
	FNodeData::FNodeData(const FName& InName, const FGuid& InID, FVertexInterface InInterface)
	: FNodeData(InName, InID, MoveTemp(InInterface), TSharedPtr<const IOperatorData>())
	{
	}

	FNodeData::FNodeData(const FName& InName, const FGuid& InID, FVertexInterface InInterface, TSharedPtr<const IOperatorData> InOperatorData)
	: Name(InName)
	, ID(InID)
	, Interface(MoveTemp(InInterface))
	, OperatorData(MoveTemp(InOperatorData))
	{
	}

	const FString PluginAuthor = TEXT("Epic Games, Inc.");
#if WITH_EDITOR
	const FText PluginNodeMissingPrompt = NSLOCTEXT("MetasoundGraphCore", "Metasound_DefaultMissingNodePrompt", "The node was likely removed, renamed, or the Metasound plugin is not loaded.");
#else 
	const FText PluginNodeMissingPrompt = FText::GetEmpty();
#endif // WITH_EDITOR

	const FNodeClassName FNodeClassName::InvalidNodeClassName;

	FNodeClassName::FNodeClassName()
	{
	}

	FNodeClassName::FNodeClassName(const FName& InNamespace, const FName& InName, const FName& InVariant)
	: Namespace(InNamespace)
	, Name(InName)
	, Variant(InVariant)
	{
	}

	/** Namespace of node class. */
	const FName& FNodeClassName::GetNamespace() const
	{
		return Namespace;
	}

	/** Name of node class. */
	const FName& FNodeClassName::GetName() const
	{
		return Name;
	}

	/** Variant of node class. */
	const FName& FNodeClassName::GetVariant() const
	{
		return Variant;
	}

	/** The full name of the Node formatted Namespace.Name[.Variant] */
	const FString FNodeClassName::ToString() const
	{
		FNameBuilder Builder;
		FormatFullName(Builder, Namespace, Name, Variant);
		return *Builder;
	}

	FName FNodeClassName::FormatFullName(const FName& InNamespace, const FName& InName, const FName& InVariant)
	{
		FNameBuilder Builder;
		FormatFullName(Builder, InNamespace, InName, InVariant);
		return *Builder;
	}

	FName FNodeClassName::FormatScopedName(const FName& InNamespace, const FName& InName)
	{
		FNameBuilder Builder;
		FormatScopedName(Builder, InNamespace, InName);
		return *Builder;
	}

	void FNodeClassName::FormatFullName(FNameBuilder& InBuilder, const FName& InNamespace, const FName& InName, const FName& InVariant)
	{
		FormatScopedName(InBuilder, InNamespace, InName);

		if (InVariant != NAME_None)
		{
			InBuilder.Append(".");
			InVariant.AppendString(InBuilder);
		}
	}

	void FNodeClassName::FormatScopedName(FNameBuilder& InBuilder, const FName& InNamespace, const FName& InName)
	{
		InNamespace.AppendString(InBuilder);
		InBuilder.Append(".");
		InName.AppendString(InBuilder);
	}

	bool FNodeClassName::IsValid() const
	{
		return *this != InvalidNodeClassName;
	}

	const FNodeClassMetadata& FNodeClassMetadata::GetEmpty()
	{
		static const FNodeClassMetadata EmptyInfo;
		return EmptyInfo;
	}

	bool operator==(const FOutputDataSource& InLeft, const FOutputDataSource& InRight)
	{
		return (InLeft.Node == InRight.Node) && (InLeft.Vertex == InRight.Vertex);
	}

	bool operator!=(const FOutputDataSource& InLeft, const FOutputDataSource& InRight)
	{
		return !(InLeft == InRight);
	}

	bool operator<(const FOutputDataSource& InLeft, const FOutputDataSource& InRight)
	{
		if (InLeft.Node == InRight.Node)
		{
			return InLeft.Vertex < InRight.Vertex;
		}
		else
		{
			return InLeft.Node < InRight.Node;
		}
	}

	/** Check if two FInputDataDestinations are equal. */
	bool operator==(const FInputDataDestination& InLeft, const FInputDataDestination& InRight)
	{
		return (InLeft.Node == InRight.Node) && (InLeft.Vertex == InRight.Vertex);
	}

	bool operator!=(const FInputDataDestination& InLeft, const FInputDataDestination& InRight)
	{
		return !(InLeft == InRight);
	}

	bool operator<(const FInputDataDestination& InLeft, const FInputDataDestination& InRight)
	{
		if (InLeft.Node == InRight.Node)
		{
			return InLeft.Vertex < InRight.Vertex;
		}
		else
		{
			return InLeft.Node < InRight.Node;
		}
	}

	/** Check if two FDataEdges are equal. */
	bool operator==(const FDataEdge& InLeft, const FDataEdge& InRight)
	{
		return (InLeft.From == InRight.From) && (InLeft.To == InRight.To);
	}

	bool operator!=(const FDataEdge& InLeft, const FDataEdge& InRight)
	{
		return !(InLeft == InRight);
	}

	bool operator<(const FDataEdge& InLeft, const FDataEdge& InRight)
	{
		if (InLeft.From == InRight.From)
		{
			return InLeft.To < InRight.To;	
		}
		else
		{
			return InLeft.From < InRight.From;
		}
	}

#if !UE_METASOUND_PURE_VIRTUAL_SET_DEFAULT_INPUT
	void INodeBase::SetDefaultInput(const FVertexName& InVertexName, const FLiteral& InLiteral)
	{
		static bool bDidWarn = false;
		if (!bDidWarn)
		{
			UE_LOG(LogMetaSound, Warning, TEXT("Ignoring input default for vertex %s. Please implement INodeInterface::SetDefaultInput(...) for the class representing node %s. This method will become pure virtual in future releases. Define UE_METASOUND_PURE_VIRTUAL_SET_DEFAULT_INPUT in order to build with this method as a pure virtual on the interface."), *InVertexName.ToString(), *GetInstanceName().ToString());
			bDidWarn = true;
		}
	}
#endif

#if !UE_METASOUND_PURE_VIRTUAL_GET_OPERATOR_DATA
	TSharedPtr<const IOperatorData> INodeBase::GetOperatorData() const
	{
		static bool bDidWarn = false;
		if (!bDidWarn)
		{
			UE_LOG(LogMetaSound, Warning, TEXT("Please implement INodeInterface::GetOperatorData(...) for the class representing node %s. This method will become pure virtual in future releases. Define UE_METASOUND_PURE_VIRTUAL_GET_OPERATOR_DATA in order to build with this method as a pure virtual on the interface."), *GetInstanceName().ToString());
			bDidWarn = true;
		}
		return {};
	}
#endif
}
