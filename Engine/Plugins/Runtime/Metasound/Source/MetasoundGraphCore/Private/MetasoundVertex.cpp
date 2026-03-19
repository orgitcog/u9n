// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundVertex.h"
#include "MetasoundVertexPrivate.h"

#include "Algo/Find.h"
#include "Containers/Array.h"

namespace Metasound
{
	namespace VertexPrivate
	{
		// Functor used for finding FSubInterfaceLayouts by name. 
		struct FEqualSubInterfaceName
		{
			const FName& NameRef;

			FEqualSubInterfaceName(const FName& InName)
			: NameRef(InName)
			{
			}

			bool operator()(const FSubInterfaceLayout& InOther) const
			{
				return NameRef == InOther.SubInterfaceName;
			}
		};

		FSubInterfaceDeclarationBuilder::FSubInterfaceDeclarationBuilder(TArray<FSubInterfaceLayout>& OutSubInterfaceLayouts)
		: SubInterfaces(&OutSubInterfaceLayouts)
		{
		}

		FSubInterfaceDeclarationBuilder::~FSubInterfaceDeclarationBuilder()
		{
			checkf(CurrentSubInterfaceIndex == INDEX_NONE, TEXT("Failed to beging/end all sub interfaces"));
		}

		void FSubInterfaceDeclarationBuilder::ReserveSubInterfaces(int32 Num)
		{
			SubInterfaces->Reserve(Num);
		}

PRAGMA_DISABLE_EXPERIMENTAL_WARNINGS
		void FSubInterfaceDeclarationBuilder::Add(FBeginSubInterface&& InSubInterface)
		{
			PushSubInterfaceDeclaration(InSubInterface.Name);
		}

		void FSubInterfaceDeclarationBuilder::Add(FEndSubInterface&& InSubInterface)
		{
			PopSubInterfaceDeclaration();
		}
PRAGMA_ENABLE_EXPERIMENTAL_WARNINGS

		void FSubInterfaceDeclarationBuilder::OnVertexAdded(const FVertexName& InName)
		{
			checkf((CurrentSubInterfaceIndex == INDEX_NONE) || (InName.GetNumber() == NAME_NO_NUMBER_INTERNAL), TEXT("Vertex %s in sub interface %s cannot have a trailing number because it is in a sub interface."), *InName.ToString(), *(*SubInterfaces)[CurrentSubInterfaceIndex].SubInterfaceName.ToString());
			CurrentNumVertices++;
		}

		void FSubInterfaceDeclarationBuilder::PushSubInterfaceDeclaration(const FName& InName)
		{
			// Sub interface declarations should only happen once because the declaration
			// needs to be consistent. Relying on developers declaring it identically
			// is error prone so it is not allowed. The default number of times a
			// sub interface is replicated can be controlled via the FSubInterfaceDescription.
			checkf(INDEX_NONE == SubInterfaces->IndexOfByPredicate(FEqualSubInterfaceName{InName}), TEXT("Sub interface %s is already declared."), *InName.ToString());

			SubInterfaces->Add(FSubInterfaceLayout{InName});
			int32 NextSubInterfaceIndex = SubInterfaces->Num() - 1;

			checkf(CurrentSubInterfaceIndex == INDEX_NONE, TEXT("Sub interface %s cannot be embedded inside sub interface %s"), *InName.ToString(), *(*SubInterfaces)[CurrentSubInterfaceIndex].SubInterfaceName.ToString());

			CurrentSubInterfaceIndex = NextSubInterfaceIndex;
			(*SubInterfaces)[CurrentSubInterfaceIndex].Instances.Add(FSubInterfaceLayout::FInstance{CurrentNumVertices, INDEX_NONE});
		}

		void FSubInterfaceDeclarationBuilder::PopSubInterfaceDeclaration()
		{
			if (ensureMsgf(SubInterfaces->IsValidIndex(CurrentSubInterfaceIndex), TEXT("Sub interface has incorrect Begin/End declaration")))
			{
				(*SubInterfaces)[CurrentSubInterfaceIndex].Instances.Last().End = CurrentNumVertices;
				CurrentSubInterfaceIndex = INDEX_NONE;
			}
		}

		FInputVertexInterfaceDeclarationBuilder::FInputVertexInterfaceDeclarationBuilder(TArray<FInputDataVertex>& OutVertices, TArray<FSubInterfaceLayout>& OutInstances)
		: FSubInterfaceDeclarationBuilder(OutInstances)
		, Vertices(OutVertices)
		{
		}

		void FInputVertexInterfaceDeclarationBuilder::Add(FInputDataVertex&& InVertex)
		{
			using FEqualVertexName = VertexPrivate::TEqualVertexName<FInputDataVertex>;

			checkf(!Vertices.ContainsByPredicate(FEqualVertexName(InVertex.VertexName)), TEXT("Duplicate vertex name %s. Vertex names must be unique"), *InVertex.VertexName.ToString());

			Vertices.Emplace(MoveTemp(InVertex));
			OnVertexAdded(Vertices.Last().VertexName);
		}

		FOutputVertexInterfaceDeclarationBuilder::FOutputVertexInterfaceDeclarationBuilder(TArray<FOutputDataVertex>& OutVertices, TArray<FSubInterfaceLayout>& OutInstances)
		: FSubInterfaceDeclarationBuilder(OutInstances)
		, Vertices(OutVertices)
		{
		}

		void FOutputVertexInterfaceDeclarationBuilder::Add(FOutputDataVertex&& InVertex)
		{
			using FEqualVertexName = VertexPrivate::TEqualVertexName<FOutputDataVertex>;

			checkf(!Vertices.ContainsByPredicate(FEqualVertexName(InVertex.VertexName)), TEXT("Duplicate vertex name %s. Vertex names must be unique"), *InVertex.VertexName.ToString());

			Vertices.Emplace(MoveTemp(InVertex));
			OnVertexAdded(Vertices.Last().VertexName);
		}

		FEnvironmentDeclarationBuilder::FEnvironmentDeclarationBuilder(TArray<FEnvironmentVertex>& OutVertices)
		: Vertices(OutVertices)
		{
		}

		/** Builds an interface from a sub-interface configuration */
		class FSubInterfaceConfigurationBuilder
		{
		public:
			FSubInterfaceConfigurationBuilder(TArray<FSubInterfaceLayout>& OutSubInterfaceLayouts)
			: SubInterfaces(OutSubInterfaceLayouts)
			{
			}

			virtual ~FSubInterfaceConfigurationBuilder() = default;


			virtual void Build(TConstArrayView<FSubInterfaceDescription> InSubInterfaceDescriptions, TConstArrayView<FSubInterfaceConfiguration> InSubInterfaceConfigs)
			{
				/** Do nothing if there are no sub interfaces. */
				if (SubInterfaces.IsEmpty())
				{
					return;
				}


				for (int32 LayoutIndex = 0; LayoutIndex < SubInterfaces.Num(); LayoutIndex++)
				{
					FSubInterfaceLayout& Layout = SubInterfaces[LayoutIndex];

					const FSubInterfaceDescription* Description = Algo::FindBy(InSubInterfaceDescriptions, Layout.SubInterfaceName, [](const FSubInterfaceDescription& InDesc) { return InDesc.SubInterfaceName; });

					if (ensureMsgf(Description, TEXT("Missing sub interface description %s"), *Layout.SubInterfaceName.ToString()))
					{
						// Determine number of sub interface instances. 
						uint32 Num = Description->NumDefault;

						const FSubInterfaceConfiguration* Config = Algo::FindBy(InSubInterfaceConfigs, Layout.SubInterfaceName, [](const FSubInterfaceConfiguration& InConfig) { return InConfig.SubInterfaceName; });
						if (Config)
						{
							Num = FMath::Clamp(Config->Num, Description->Min, Description->Max);
						}

						// Construct sub interface instances
						BuildSubInterfaceInstances(Layout, LayoutIndex, Num);
					}
				}
			}


		private:

			void BuildSubInterfaceInstances(FSubInterfaceLayout& InLayout, uint32 InLayoutIndex, uint32 InNum)
			{
				// We should always be beginning from a declaration of a sub interface
				// which enforces that there is only one instance of the sub interface. 
				check(InLayout.Instances.Num() == 1);

				if (InNum == 1)
				{
					// We already have 1 instance from the declaration. Nothing to do. 
					return;
				}

				if (InNum == 0)
				{
					RemoveSubInterface(InLayout, InLayoutIndex);
				}
				else
				{
					SetNumSubInterfaces(InLayout, InLayoutIndex, InNum);
				}
			}

			void SetNumSubInterfaces(FSubInterfaceLayout& InLayout, uint32 InLayoutIndex, uint32 InNum)
			{
				// Assume we are starting with a sub interface declaration. 
				check(InLayout.Instances.Num() == 1);
				
				// Get the position of the prototype for the sub interface. 
				const int32 ProtoBegin = InLayout.Instances[0].Begin;
				const int32 ProtoEnd = InLayout.Instances[0].End;
				const int32 ProtoNum = ProtoEnd - ProtoBegin;

				// Shift the location of sub interfaces that occur after this one 
				// to account for the about-to-be added vertices. 
				int32 NumToAdd = (InNum - 1) * ProtoNum;
				for (FSubInterfaceLayout& Layout : SubInterfaces)
				{
					for (FSubInterfaceLayout::FInstance& Instance : Layout.Instances)
					{
						if (Instance.Begin > ProtoBegin)
						{
							Instance.Begin += NumToAdd;
							Instance.End += NumToAdd;
						}
					}
				}

				// Add instances
				TArray<FSubInterfaceLayout::FInstance>& Instances = InLayout.Instances;
				Instances.Reserve(InNum);
				Instances.SetNum(InNum);

				for (uint32 Cntr = 1; Cntr < InNum; Cntr++)
				{
					// Initialize the position of the sub interface instance
					const uint32 InsertIndex = ProtoBegin + (Cntr * ProtoNum);
					Instances[Cntr].Begin = InsertIndex;
					Instances[Cntr].End = InsertIndex + ProtoNum;

					// Create the vertices of the sub interface instance. 
					InsertSubInterfaceInstance(ProtoBegin, ProtoNum, InsertIndex, Cntr);
				}
			}

			void RemoveSubInterface(FSubInterfaceLayout& InLayout, uint32 InLayoutIndex)
			{
				// We should always be beginning from a declaration of a sub interface
				// which enforces that there is only one instance of the sub interface. 
				check(InLayout.Instances.Num() == 1);

				// Get the position of the prototype for the sub interface. 
				const int32 ProtoBegin = InLayout.Instances[0].Begin;
				const int32 ProtoEnd = InLayout.Instances[0].End;
				const int32 ProtoNum = ProtoEnd - ProtoBegin;

				// Remove all instances from this subinterface
				InLayout.Instances.Empty();

				if (ProtoNum > 0)
				{
					// Remove actual vertices from interface
					RemoveVerticesAt(ProtoBegin, ProtoNum);

					// Shift positions of other instances of sub interfaces
					for (FSubInterfaceLayout& Layout : SubInterfaces)
					{
						for (FSubInterfaceLayout::FInstance& Instance : Layout.Instances)
						{
							if (Instance.Begin > ProtoBegin)
							{
								Instance.Begin -= ProtoNum;
								Instance.End -= ProtoNum;
							}
						}
					}
				}
			}

			virtual void InsertSubInterfaceInstance(uint32 InProtoBegin, uint32 InProtoNum, uint32 InInsertPos, uint32 InSubInterfaceInstanceIndex) = 0;
			virtual void RemoveVerticesAt(uint32 InVertexIndexBegin, uint32 InNum) = 0;

			TArray<FSubInterfaceLayout>& SubInterfaces;
		};

		/** TInterfaceConfigurationBuilder builds vertex interfaces from a sub interface
		 * configuration.*/
		template<typename VertexType>
		class TInterfaceConfigurationBuilder : public FSubInterfaceConfigurationBuilder
		{
		public:
			TInterfaceConfigurationBuilder(TArray<VertexType>& OutVertices, TArray<FSubInterfaceLayout>& OutSubInterfaceLayouts)
			: FSubInterfaceConfigurationBuilder(OutSubInterfaceLayouts)
			, Vertices(OutVertices)
			{
			}

			virtual void Build(TConstArrayView<FSubInterfaceDescription> InSubInterfaceDescriptions, TConstArrayView<FSubInterfaceConfiguration> InSubInterfaceConfigs) override
			{
				// Build sub interfaces using parent class.
				FSubInterfaceConfigurationBuilder::Build(InSubInterfaceDescriptions, InSubInterfaceConfigs);

#if DO_CHECK
				// Check that there are no duplicate names in the interface
				for (int32 IndexA = 0; IndexA < Vertices.Num(); IndexA++)
				{
					const FVertexName& VertexNameA = Vertices[IndexA].VertexName;
					for (int32 IndexB = IndexA + 1; IndexB < Vertices.Num(); IndexB++)
					{
						checkf(VertexNameA != Vertices[IndexB].VertexName, TEXT("Found duplicate names (%s) in interface"), *VertexNameA.ToString());
					}
				}
#endif
			}
		private:

			virtual void InsertSubInterfaceInstance(uint32 InProtoBegin, uint32 InProtoNum, uint32 InInsertPos, uint32 InSubInterfaceInstanceIndex) override
			{
				// check that the prototype exists in the vertex array.
				check((InProtoBegin + InProtoNum) <= static_cast<uint32>(Vertices.Num()));
				// check that the insert position of the sub interface instance is valid. 
				check(InInsertPos <= static_cast<uint32>(Vertices.Num()));

				if (InProtoNum > 0)
				{
					// Copy vertices from prototype vertices
					Vertices.Insert(Vertices.GetData() + InProtoBegin, InProtoNum, InInsertPos);

					// Rename vertices
					const uint32 NewEnd = InInsertPos + InProtoNum;
					for (uint32 VertexIndex = InInsertPos;  VertexIndex < NewEnd; VertexIndex++)
					{
						checkf(
							Vertices[VertexIndex].VertexName.GetNumber() == NAME_NO_NUMBER_INTERNAL, 
							TEXT("Prototype vertex %s in sub interface cannot have a trailing number because it is in a sub interface."), 
							*Vertices[VertexIndex].VertexName.ToString()
						);

						Vertices[VertexIndex].VertexName.SetNumber(1 + InSubInterfaceInstanceIndex);
					}
				}
			}

			virtual void RemoveVerticesAt(uint32 InVertexIndexBegin, uint32 InNum) override
			{
				check((InVertexIndexBegin + InNum) <= static_cast<unsigned>(Vertices.Num()));
				Vertices.RemoveAt(InVertexIndexBegin, InNum);
			}

			TArray<VertexType>& Vertices;
		};

		class FInputInterfaceConfigurationBuilder : public TInterfaceConfigurationBuilder<FInputDataVertex>
		{
		public:
			FInputInterfaceConfigurationBuilder(FInputVertexInterface& InProto)
			: TInterfaceConfigurationBuilder<FInputDataVertex>(InProto.Vertices, InProto.SubInterfaces)
			{
			}
		};

		class FOutputInterfaceConfigurationBuilder : public TInterfaceConfigurationBuilder<FOutputDataVertex>
		{
		public:
			FOutputInterfaceConfigurationBuilder(FOutputVertexInterface& InProto)
			: TInterfaceConfigurationBuilder<FOutputDataVertex>(InProto.Vertices, InProto.SubInterfaces)
			{
			}
		};
	}

	bool operator==(const FInputDataVertex& InLHS, const FInputDataVertex& InRHS)
	{
		return (InLHS.VertexName == InRHS.VertexName) && (InLHS.DataTypeName == InRHS.DataTypeName);
	}

	bool operator!=(const FInputDataVertex& InLHS, const FInputDataVertex& InRHS)
	{
		return !(InLHS == InRHS);
	}

	bool operator<(const FInputDataVertex& InLHS, const FInputDataVertex& InRHS)
	{
		if (InLHS.VertexName == InRHS.VertexName)
		{
			return InLHS.DataTypeName.FastLess(InRHS.DataTypeName);
		}
		else
		{
			return InLHS.VertexName.FastLess(InRHS.VertexName);
		}
	}

	bool operator==(const FOutputDataVertex& InLHS, const FOutputDataVertex& InRHS)
	{
		return (InLHS.VertexName == InRHS.VertexName) && (InLHS.DataTypeName == InRHS.DataTypeName);
	}

	bool operator!=(const FOutputDataVertex& InLHS, const FOutputDataVertex& InRHS)
	{
		return !(InLHS == InRHS);
	}

	bool operator<(const FOutputDataVertex& InLHS, const FOutputDataVertex& InRHS)
	{
		if (InLHS.VertexName == InRHS.VertexName)
		{
			return InLHS.DataTypeName.FastLess(InRHS.DataTypeName);
		}
		else
		{
			return InLHS.VertexName.FastLess(InRHS.VertexName);
		}
	}

	bool operator==(const FEnvironmentVertex& InLHS, const FEnvironmentVertex& InRHS)
	{
		return (InLHS.VertexName == InRHS.VertexName);
	}

	bool operator!=(const FEnvironmentVertex& InLHS, const FEnvironmentVertex& InRHS)
	{
		return !(InLHS == InRHS);
	}

	bool operator<(const FEnvironmentVertex& InLHS, const FEnvironmentVertex& InRHS)
	{
		return InLHS.VertexName.FastLess(InRHS.VertexName);
	}

	FInputVertexInterface::FInputVertexInterface(TArray<FInputDataVertex> InVertices, TArray<VertexPrivate::FSubInterfaceLayout> InSubInterfaces)
	: TVertexInterfaceImpl<FInputDataVertex>(MoveTemp(InVertices))
	, SubInterfaces(MoveTemp(InSubInterfaces))
	{
	}

	void FInputVertexInterface::ForEachSubInterfaceInstance(const FName& InSubInterfaceName, TFunctionRef<void (TConstArrayView<FInputDataVertex>)> Callable) const
	{
		using namespace VertexPrivate;

		if (const FSubInterfaceLayout* Instances = FindSubInterfaceLayout(InSubInterfaceName))
		{
			for (const FSubInterfaceLayout::FInstance& Instance : Instances->Instances)
			{
				check(Vertices.IsValidIndex(Instance.Begin) && Vertices.IsValidIndex(Instance.End - 1));
				Callable(MakeConstArrayView<FInputDataVertex>(Vertices.GetData() + Instance.Begin, Instance.End - Instance.Begin));
			}
		}
	}

	void FInputVertexInterface::ForEachSubInterfaceInstance(const FName& InSubInterfaceName, TFunctionRef<void (TArrayView<FInputDataVertex>)> Callable)
	{
		using namespace VertexPrivate;

		if (const FSubInterfaceLayout* Instances = FindSubInterfaceLayout(InSubInterfaceName))
		{
			for (const FSubInterfaceLayout::FInstance& Instance : Instances->Instances)
			{
				check(Vertices.IsValidIndex(Instance.Begin) && Vertices.IsValidIndex(Instance.End - 1));
				Callable(MakeArrayView<FInputDataVertex>(Vertices.GetData() + Instance.Begin, Instance.End - Instance.Begin));
			}
		}
	}

	TConstArrayView<VertexPrivate::FSubInterfaceLayout> FInputVertexInterface::GetSubInterfaces(const VertexPrivate::FPrivateAccessTag& InTag) const
	{
		return SubInterfaces;
	}

	const VertexPrivate::FSubInterfaceLayout* FInputVertexInterface::FindSubInterfaceLayout(const FName& InName) const
	{
		using namespace VertexPrivate;

		return SubInterfaces.FindByPredicate(VertexPrivate::FEqualSubInterfaceName(InName));
	}

	FOutputVertexInterface::FOutputVertexInterface(TArray<FOutputDataVertex> InVertices, TArray<VertexPrivate::FSubInterfaceLayout> InSubInterfaces)
	: TVertexInterfaceImpl<FOutputDataVertex>(MoveTemp(InVertices))
	, SubInterfaces(MoveTemp(InSubInterfaces))
	{
	}

	void FOutputVertexInterface::ForEachSubInterfaceInstance(const FName& InSubInterfaceName, TFunctionRef<void (TConstArrayView<FOutputDataVertex>)> Callable) const
	{
		using namespace VertexPrivate;

		if (const FSubInterfaceLayout* Instances = FindSubInterfaceLayout(InSubInterfaceName))
		{
			for (const FSubInterfaceLayout::FInstance& Instance : Instances->Instances)
			{
				check(Vertices.IsValidIndex(Instance.Begin) && Vertices.IsValidIndex(Instance.End - 1));
				Callable(MakeConstArrayView<FOutputDataVertex>(Vertices.GetData() + Instance.Begin, Instance.End - Instance.Begin));
			}
		}
	}

	void FOutputVertexInterface::ForEachSubInterfaceInstance(const FName& InSubInterfaceName, TFunctionRef<void (TArrayView<FOutputDataVertex>)> Callable)
	{
		using namespace VertexPrivate;

		if (const FSubInterfaceLayout* Instances = FindSubInterfaceLayout(InSubInterfaceName))
		{
			for (const FSubInterfaceLayout::FInstance& Instance : Instances->Instances)
			{
				check(Vertices.IsValidIndex(Instance.Begin) && Vertices.IsValidIndex(Instance.End - 1));
				Callable(MakeArrayView<FOutputDataVertex>(Vertices.GetData() + Instance.Begin, Instance.End - Instance.Begin));
			}
		}
	}

	TConstArrayView<VertexPrivate::FSubInterfaceLayout> FOutputVertexInterface::GetSubInterfaces(const VertexPrivate::FPrivateAccessTag& InTag) const
	{
		return SubInterfaces;
	}

	const VertexPrivate::FSubInterfaceLayout* FOutputVertexInterface::FindSubInterfaceLayout(const FName& InName) const
	{
		return SubInterfaces.FindByPredicate(VertexPrivate::FEqualSubInterfaceName(InName));
	}

	FEnvironmentVertexInterface::FEnvironmentVertexInterface(TArray<FEnvironmentVertex> InVertices)
	: TVertexInterfaceImpl<FEnvironmentVertex>(MoveTemp(InVertices))
	{
	}

	FVertexInterface::FVertexInterface(const FInputVertexInterface& InInputs, const FOutputVertexInterface& InOutputs)
	:	InputInterface(InInputs)
	,	OutputInterface(InOutputs)
	{
	}

	FVertexInterface::FVertexInterface(FInputVertexInterface InInputs, FOutputVertexInterface InOutputs, FEnvironmentVertexInterface InEnvironmentVariables)
	:	InputInterface(MoveTemp(InInputs))
	,	OutputInterface(MoveTemp(InOutputs))
	,	EnvironmentInterface(MoveTemp(InEnvironmentVariables))
	{
	}

	const FInputVertexInterface& FVertexInterface::GetInputInterface() const
	{
		return InputInterface;
	}

	FInputVertexInterface& FVertexInterface::GetInputInterface()
	{
		return InputInterface;
	}

	const FInputDataVertex& FVertexInterface::GetInputVertex(const FVertexName& InKey) const
	{
		return InputInterface[InKey];
	}

	bool FVertexInterface::ContainsInputVertex(const FVertexName& InKey) const
	{
		return InputInterface.Contains(InKey);
	}

	const FOutputVertexInterface& FVertexInterface::GetOutputInterface() const
	{
		return OutputInterface;
	}

	FOutputVertexInterface& FVertexInterface::GetOutputInterface()
	{
		return OutputInterface;
	}

	const FOutputDataVertex& FVertexInterface::GetOutputVertex(const FVertexName& InName) const
	{
		return OutputInterface[InName];
	}

	bool FVertexInterface::ContainsOutputVertex(const FVertexName& InName) const
	{
		return OutputInterface.Contains(InName);
	}

	const FEnvironmentVertexInterface& FVertexInterface::GetEnvironmentInterface() const
	{
		return EnvironmentInterface;
	}

	FEnvironmentVertexInterface& FVertexInterface::GetEnvironmentInterface()
	{
		return EnvironmentInterface;
	}

	const FEnvironmentVertex& FVertexInterface::GetEnvironmentVertex(const FVertexName& InKey) const
	{
		return EnvironmentInterface[InKey];
	}

	bool FVertexInterface::ContainsEnvironmentVertex(const FVertexName& InKey) const
	{
		return EnvironmentInterface.Contains(InKey);
	}

	bool operator==(const FVertexInterface& InLHS, const FVertexInterface& InRHS)
	{
		const bool bIsEqual = (InLHS.InputInterface == InRHS.InputInterface) && 
			(InLHS.OutputInterface == InRHS.OutputInterface) && 
			(InLHS.EnvironmentInterface == InRHS.EnvironmentInterface);

		return bIsEqual;
	}

	bool operator!=(const FVertexInterface& InLHS, const FVertexInterface& InRHS)
	{
		return !(InLHS == InRHS);
	}

PRAGMA_DISABLE_EXPERIMENTAL_WARNINGS
	FClassVertexInterface::FClassVertexInterface(FVertexInterface InInterface)
	: FClassVertexInterface(MoveTemp(InInterface.GetInputInterface()), MoveTemp(InInterface.GetOutputInterface()), MoveTemp(InInterface.GetEnvironmentInterface()))
	{
	}

	FClassVertexInterface::FClassVertexInterface(FInputVertexInterface InInputs, FOutputVertexInterface InOutputs, FEnvironmentVertexInterface InEnvironment)
	: FClassVertexInterface(TArray<FSubInterfaceDescription>{}, MoveTemp(InInputs), MoveTemp(InOutputs), MoveTemp(InEnvironment))
	{
	}

	FClassVertexInterface::FClassVertexInterface(TArray<FSubInterfaceDescription> InSubInterfaceDescriptions, FInputVertexInterface InInputs, FOutputVertexInterface InOutputs, FEnvironmentVertexInterface InEnvironment)
	: SubInterfaces(MoveTemp(InSubInterfaceDescriptions))
	, Inputs(MoveTemp(InInputs))
	, Outputs(MoveTemp(InOutputs))
	, Environment(MoveTemp(InEnvironment))
	{

	}

	FVertexInterface FClassVertexInterface::CreateVertexInterface(TConstArrayView<FSubInterfaceConfiguration> InSubInterfaceConfigurations) const
	{
		using namespace VertexPrivate;

		FInputVertexInterface NewInputs(Inputs);
		FInputInterfaceConfigurationBuilder(NewInputs).Build(SubInterfaces, InSubInterfaceConfigurations);
		
		FOutputVertexInterface NewOutputs(Outputs);
		FOutputInterfaceConfigurationBuilder(NewOutputs).Build(SubInterfaces, InSubInterfaceConfigurations);

		return FVertexInterface(MoveTemp(NewInputs), MoveTemp(NewOutputs), Environment);
	}
PRAGMA_ENABLE_EXPERIMENTAL_WARNINGS
}

FString LexToString(Metasound::EVertexAccessType InAccessType)
{
	using namespace Metasound;

	switch (InAccessType)
	{
		case EVertexAccessType::Value:
			return TEXT("Value");

		case EVertexAccessType::Reference:
		default:
			return TEXT("Reference");
	}
}
