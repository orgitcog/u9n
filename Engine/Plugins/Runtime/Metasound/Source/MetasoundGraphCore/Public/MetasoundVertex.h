// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "MetasoundDataFactory.h"
#include "MetasoundDataReference.h"
#include "MetasoundEnvironment.h"
#include "MetasoundLiteral.h"

#include <type_traits>

#define UE_API METASOUNDGRAPHCORE_API

namespace Metasound
{
	/** Name of a given vertex.  Only unique for a given node interface. */
	using FVertexName = FName;

	namespace VertexPrivate
	{
		// Forward declare for friendship
		class FInputInterfaceConfigurationBuilder;
		class FOutputInterfaceConfigurationBuilder;

		struct FPrivateAccessTag;

		/** Contains a list of sub-interface spans where the span indices
		 * refer to vertex indices of an array containing vertices. */
		struct FSubInterfaceLayout
		{
			struct FInstance
			{
				int32 Begin = 0;
				int32 End = 0; //< Inclusive. 
			};

			FName SubInterfaceName;
			TArray<FInstance> Instances;
		};

		/** Functor for finding vertices with equal names */
		template<typename VertexType>
		struct TEqualVertexName
		{
			const FVertexName& NameRef;

			TEqualVertexName(const FVertexName& InName)
			: NameRef(InName)
			{
			}

			FORCEINLINE bool operator()(const VertexType& InOther) const
			{
				return InOther.VertexName == NameRef;
			}
		};

		/** constexpr count of the number of a instances of type T in a parameter pack */
		template<typename T>
		struct TNumOfTypeInPack
		{
			template<typename ... Ts>
			static constexpr uint32 Get()
			{
				constexpr uint32 Num = ((std::is_base_of_v<T, Ts> ? 1 : 0) + ...);
				return Num;
			}
		};
	}

	// Vertex metadata
	struct FDataVertexMetadata
	{
		FText Description;
		FText DisplayName;
		bool bIsAdvancedDisplay = false;
	};

	/** Describe how the vertex will access connected data. */
	enum class EVertexAccessType
	{
		Reference, //< Vertex accesses the data references
		Value      //< Vertex accesses the data value.
	};


	/** FDataVertex
	 *
	 * An FDataVertex is a named vertex of a MetaSound node which can contain data.
	 */
	class FDataVertex
	{

	public:

		FDataVertex() = default;

		/** FDataVertex Constructor
		 *
		 * @InVertexName - Name of vertex.
		 * @InDataTypeName - Name of data type.
		 * @InMetadata - Metadata pertaining to given vertex.
		 * @InAccessType - The access type of the vertex.
		 */
		FDataVertex(const FVertexName& InVertexName, const FName& InDataTypeName, const FDataVertexMetadata& InMetadata, EVertexAccessType InAccessType)
		: VertexName(InVertexName)
		, DataTypeName(InDataTypeName)
#if WITH_EDITORONLY_DATA
		, Metadata(InMetadata)
#endif // WITH_EDITORONLY_DATA
		, AccessType(InAccessType)
		{
		}


		virtual ~FDataVertex() = default;

		/** Name of vertex. */
		FVertexName VertexName;

		/** Type name of data. */
		FName DataTypeName;

#if WITH_EDITORONLY_DATA
		/** Metadata associated with vertex. */
		FDataVertexMetadata Metadata;
#endif // WITH_EDITORONLY_DATA

		/** Access type of the vertex. */
		EVertexAccessType AccessType;
	};

	/** FInputDataVertex */
	class FInputDataVertex : public FDataVertex
	{
	public:

		FInputDataVertex() = default;

		/** Construct an FInputDataVertex. */
		FInputDataVertex(const FVertexName& InVertexName, const FName& InDataTypeName, const FDataVertexMetadata& InMetadata, EVertexAccessType InAccessType=EVertexAccessType::Reference)
			: FDataVertex(InVertexName, InDataTypeName, InMetadata, InAccessType)
			, Literal(FLiteral::FNone{})
		{
		}

		/** Construct an FInputDataVertex with a default literal. */
		template<typename LiteralValueType>
		FInputDataVertex(const FVertexName& InVertexName, const FName& InDataTypeName, const FDataVertexMetadata& InMetadata, EVertexAccessType InAccessType, const LiteralValueType& InLiteralValue)
			: FDataVertex(InVertexName, InDataTypeName, InMetadata, InAccessType)
			, Literal(InLiteralValue)
		{
		}

		FInputDataVertex(const FVertexName& InVertexName, const FName& InDataTypeName, const FDataVertexMetadata& InMetadata, EVertexAccessType InAccessType, const FLiteral& InLiteral)
			: FDataVertex(InVertexName, InDataTypeName, InMetadata, InAccessType)
			, Literal(InLiteral)
		{
		}

		/** Returns the default literal associated with this input. */
		FLiteral GetDefaultLiteral() const 
		{
			return Literal;
		}
		
		/** Set the default literal for this vertex */
		void SetDefaultLiteral(const FLiteral& InLiteral)
		{
			Literal = InLiteral;
		}

		friend bool METASOUNDGRAPHCORE_API operator==(const FInputDataVertex& InLHS, const FInputDataVertex& InRHS);
		friend bool METASOUNDGRAPHCORE_API operator!=(const FInputDataVertex& InLHS, const FInputDataVertex& InRHS);
		friend bool METASOUNDGRAPHCORE_API operator<(const FInputDataVertex& InLHS, const FInputDataVertex& InRHS);

	private:

		FLiteral Literal;
	};

	/** Create a FInputDataVertex with a templated MetaSound data type. */
	template<typename DataType>
	class TInputDataVertex : public FInputDataVertex
	{
	public:
		TInputDataVertex() = default;

		template<typename... ArgTypes>
		TInputDataVertex(const FVertexName& InVertexName, const FDataVertexMetadata& InMetadata, ArgTypes&&... Args)
		: FInputDataVertex(InVertexName, GetMetasoundDataTypeName<DataType>(), InMetadata, EVertexAccessType::Reference, Forward<ArgTypes>(Args)...)
		{
		}
	};

	/** Create a FInputDataVertex with a templated MetaSound data type which only
	 * reads data at operator time. */
	template<typename DataType>
	class TInputConstructorVertex : public FInputDataVertex
	{
	public:
		TInputConstructorVertex() = default;

		template<typename... ArgTypes>
		TInputConstructorVertex(const FVertexName& InVertexName, const FDataVertexMetadata& InMetadata, ArgTypes&&... Args)
		: FInputDataVertex(InVertexName, GetMetasoundDataTypeName<DataType>(), InMetadata, EVertexAccessType::Value, Forward<ArgTypes>(Args)...)
		{
		}
	};
	
	/** FOutputDataVertex
	 *
	 * Vertex for outputs.
	 */
	class FOutputDataVertex : public FDataVertex
	{
	public:
		using FDataVertex::FDataVertex;

		friend bool METASOUNDGRAPHCORE_API operator==(const FOutputDataVertex& InLHS, const FOutputDataVertex& InRHS);
		friend bool METASOUNDGRAPHCORE_API operator!=(const FOutputDataVertex& InLHS, const FOutputDataVertex& InRHS);
		friend bool METASOUNDGRAPHCORE_API operator<(const FOutputDataVertex& InLHS, const FOutputDataVertex& InRHS);
	};

	/** Create a FOutputDataVertex with a templated MetaSound data type. */
	template<typename DataType>
	class TOutputDataVertex : public FOutputDataVertex
	{
	public:

		TOutputDataVertex() = default;

		template<typename... ArgTypes>
		TOutputDataVertex(const FVertexName& InVertexName, const FDataVertexMetadata& InMetadata, ArgTypes&&... Args)
		: FOutputDataVertex(InVertexName, GetMetasoundDataTypeName<DataType>(), InMetadata, EVertexAccessType::Reference, Forward<ArgTypes>(Args)...)
		{
		}
	};

	/** Create a FOutputDataVertex with a templated MetaSound data type which is only
	 * sets data at operator construction time. 
	 */
	template<typename DataType>
	class TOutputConstructorVertex : public FOutputDataVertex
	{
	public:

		TOutputConstructorVertex() = default;

		template<typename... ArgTypes>
		TOutputConstructorVertex(const FVertexName& InVertexName, const FDataVertexMetadata& InMetadata, ArgTypes&&... Args)
		: FOutputDataVertex(InVertexName, GetMetasoundDataTypeName<DataType>(), InMetadata, EVertexAccessType::Value, Forward<ArgTypes>(Args)...)
		{
		}
	};


	/** FEnvironmentVertex
	 *
	 * A vertex for environment variables. 
	 */
	class FEnvironmentVertex
	{
	public:
		/** FEnvironmentVertex Construtor
		 *
		 * @InVertexName - Name of vertex.
		 * @InDescription - Human readable vertex description.
		 */
		FEnvironmentVertex(const FVertexName& InVertexName, const FText& InDescription)
		:	VertexName(InVertexName)
#if WITH_EDITORONLY_DATA
		,	Description(InDescription)
#endif // WITH_EDITORONLY_DATA
		{
		}

		virtual ~FEnvironmentVertex() = default;

		/** Name of vertex. */
		FVertexName VertexName;

#if WITH_EDITORONLY_DATA
		/** Description of the vertex. */
		FText Description;
#endif // WITH_EDITORONLY_DATA

		friend bool METASOUNDGRAPHCORE_API operator==(const FEnvironmentVertex& InLHS, const FEnvironmentVertex& InRHS);
		friend bool METASOUNDGRAPHCORE_API operator!=(const FEnvironmentVertex& InLHS, const FEnvironmentVertex& InRHS);
		friend bool METASOUNDGRAPHCORE_API operator<(const FEnvironmentVertex& InLHS, const FEnvironmentVertex& InRHS);
	};

	/** TVertexInterfaceImpls encapsulates multiple related data vertices. It 
	 * requires that each vertex in the group have a unique FVertexName.
	 */
	template<typename VertexType>
	class TVertexInterfaceImpl
	{
		using FEqualVertexName = VertexPrivate::TEqualVertexName<VertexType>;

		void AddOrUpdateVertex(VertexType&& InVertex)
		{
			if (VertexType* Vertex = Find(InVertex.VertexName))
			{
				*Vertex = MoveTemp(InVertex);
			}
			else
			{
				Vertices.Add(MoveTemp(InVertex));
			}
		}
	public:

		using RangedForConstIteratorType = typename TArray<VertexType>::RangedForConstIteratorType;

		TVertexInterfaceImpl() = default;

		/** Construct with prebuilt array of vertices. */
		explicit TVertexInterfaceImpl(TArray<VertexType> InVertices)
		: Vertices(MoveTemp(InVertices))
		{
		}
		
		/** TVertexInterfaceImpl constructor with variadic list of vertex
		 * models.
		 */
		template<typename... VertexTypes>
		UE_DEPRECATED(5.6, "Use the constructors of derived classes instead (FInputVertexInterface, FOutputVertexInterface, etc).")
		explicit TVertexInterfaceImpl(VertexTypes&&... InVertices)
		{
			static_assert(
				(std::is_constructible_v<VertexType, VertexTypes&&> && ...),
				"Vertex types must be move constructible from the group's base type");
			
			// Reserve array to hold exact number of vertices to avoid
			// over allocation.
			Vertices.Reserve(sizeof...(VertexTypes));

			// Unfold parameter pack and add t
			(AddOrUpdateVertex(Forward<VertexTypes>(InVertices)), ...);
		}

		/** Add a vertex to the group. */
		void Add(const VertexType& InVertex)
		{
			AddOrUpdateVertex(VertexType(InVertex));
		}

		void Add(VertexType&& InVertex)
		{
			AddOrUpdateVertex(MoveTemp(InVertex));
		}

		void Append(TArrayView<const VertexType> InVertices)
		{
			for (const VertexType& Vertex : InVertices)
			{
				Add(Vertex);
			}
		}

		/** Remove a vertex by key. */
		bool Remove(const FVertexName& InKey)
		{
			int32 NumRemoved = Vertices.RemoveAll(FEqualVertexName(InKey));
			return (NumRemoved > 0);
		}

		/** Returns true if the group contains a vertex with a matching key. */
		bool Contains(const FVertexName& InKey) const
		{
			return Vertices.ContainsByPredicate(FEqualVertexName(InKey));
		}

		/** Find a vertex with a given VertexName */
		VertexType* Find(const FVertexName& InKey)
		{
			return Vertices.FindByPredicate(FEqualVertexName(InKey));
		}

		/** Find a vertex with a given VertexName */
		const VertexType* Find(const FVertexName& InKey) const
		{
			return Vertices.FindByPredicate(FEqualVertexName(InKey));
		}

		/** Return the sort order index of a vertex with the given name.
		 *
		 * @param InKey - FVertexName of vertex of interest.
		 *
		 * @return The index of the vertex. INDEX_NONE if the vertex does not exist. 
		 */
		int32 GetSortOrderIndex(const FVertexName& InKey) const
		{
			return Vertices.IndexOfByPredicate(FEqualVertexName(InKey));
		}

		/** Return the vertex for a given vertex key. */
		const VertexType& operator[](const FVertexName& InName) const
		{
			const VertexType* Vertex = Find(InName);
			checkf(nullptr != Vertex, TEXT("Vertex with name '%s' does not exist"), *InName.ToString());
			return *Vertex;
		}

		/** Iterator for ranged for loops. */
		RangedForConstIteratorType begin() const
		{
			return Vertices.begin();
		}

		/** Iterator for ranged for loops. */
		RangedForConstIteratorType end() const
		{
			return Vertices.end();
		}

		/** Returns the number of vertices in the group. */
		int32 Num() const
		{
			return Vertices.Num();
		}

		/** Return a vertex at an index. */
		const VertexType& At(int32 InIndex) const
		{
			return Vertices[InIndex];
		}
		
		/** Return a vertex at an index. */
		VertexType& At(int32 InIndex)
		{
			return Vertices[InIndex];
		}

		/** Compare whether two vertex groups are equal. */
		friend bool operator==(const TVertexInterfaceImpl<VertexType>& InLHS, const TVertexInterfaceImpl<VertexType>& InRHS)
		{
			return InLHS.Vertices == InRHS.Vertices;
		}

		/** Compare whether two vertex groups are unequal. */
		friend bool operator!=(const TVertexInterfaceImpl<VertexType>& InLHS, const TVertexInterfaceImpl<VertexType>& InRHS)
		{
			return !(InLHS == InRHS);
		}

	protected:

		TArray<VertexType> Vertices;
	};

	template<typename VertexType>
	using TVertexInterfaceGroup UE_DEPRECATED(5.6, "Use TVertexInterfaceImpl instead") = TVertexInterfaceImpl<VertexType>;

	/** Declare the beginning of a sub interface in a vertex interface declaration. */
	struct UE_EXPERIMENTAL(5.6, "Sub interfaces are experimental") FBeginSubInterface;
	struct FBeginSubInterface
	{
		FName Name; //< Name of the sub interface. 
	};

	/** Declare the ending of a sub interface in a vertex interface declaration. */
	struct UE_EXPERIMENTAL(5.6, "Sub interfaces are experimental") FEndSubInterface;
	struct FEndSubInterface
	{
	};

	namespace VertexPrivate
	{
		/** Interface Declaration Builders create vertex interfaces from a template
		 * parameter pack. This allows developers to express their node interfaces 
		 * as declarations as opposed to requiring them to sequentially construct
		 * their interfaces.  
		 *
		 * For example, we can do:
		 * 
		 *  FInputVertexInterface Interface
		 *  {
		 *  	TInputDataVertex<float>(...),
		 *  	FBeginSubInterface(...),
		 *  		TInputDataVertex<int32>(...),
		 *  	FEndSubInterface(...),
		 *  	TInputDataVertex<float>(...)
		 *  };
		 *
		 * As opposed to:
		 *  FInputVertexInterface Interface;
		 * 
		 * 	Interface.Add(TInputDataVertex<float>(...));
		 * 	Interface.Add(FBeginSubInterface(...));
	     * 	Interface.Add(TInputDataVertex<int32>(...));
		 * 	Interface.Add(FEndSubInterface(...));
		 * 	Interface.Add(TInputDataVertex<float>(...);
		 *
		 * 
		 * An essential element of the builders is that they use member function 
		 * overloading of Add(...) to handle the variety of different objects which
		 * might be used to describe an interface. 
		 *
		 * The builders also size memory allocations exactly to remove any addition
		 * slack. In general, there can be many interfaces in memory and minimizing
		 * their memory footprint is important. 
		 */

		/** Base class for an interface builder which supports sub interfaces. */
		class FSubInterfaceDeclarationBuilder
		{
		public:
			UE_API FSubInterfaceDeclarationBuilder(TArray<FSubInterfaceLayout>& OutSubInterfaceLayouts);
			UE_API virtual ~FSubInterfaceDeclarationBuilder();

			// Handler for the beginning of a sub interface. 
UE_API PRAGMA_DISABLE_EXPERIMENTAL_WARNINGS
			void Add(FBeginSubInterface&& InSubInterface);

			// Handler for the ending of a sub interface. 
			UE_API void Add(FEndSubInterface&& InSubInterface);
UE_API PRAGMA_ENABLE_EXPERIMENTAL_WARNINGS

			// Allocate memory for a known number of sub interfaces.
			void ReserveSubInterfaces(int32 Num);

		protected:
			// Called when a vertex is added to adjust internal counters. 
			UE_API void OnVertexAdded(const FVertexName& InVertexName);

		private:

			void PushSubInterfaceDeclaration(const FName& InName);
			void PopSubInterfaceDeclaration();

			int32 CurrentNumVertices = 0;
			TArray<FSubInterfaceLayout>* SubInterfaces;
			int32 CurrentSubInterfaceIndex = INDEX_NONE;
		};

		/** Interface builder for FInputVertexInterface declarations. */
		class FInputVertexInterfaceDeclarationBuilder : public FSubInterfaceDeclarationBuilder
		{
		public:
			UE_API FInputVertexInterfaceDeclarationBuilder(TArray<FInputDataVertex>& OutVertices, TArray<FSubInterfaceLayout>& OutInstances);
			template<typename... ArgTypes>
			void Build(ArgTypes&&... InArgs)
			{
				// Size vertices exactly to avoid wasted memory in array allocations
				constexpr uint32 NumVertices = TNumOfTypeInPack<FInputDataVertex>::Get<ArgTypes...>();
				if constexpr (NumVertices > 0)
				{
					Vertices.Reserve(NumVertices);
				}

PRAGMA_DISABLE_EXPERIMENTAL_WARNINGS
				// Size sub interfaces exactly to avoid wasted memory in array allocations
				constexpr uint32 NumSubInterfaces = TNumOfTypeInPack<FBeginSubInterface>::Get<ArgTypes...>();
PRAGMA_ENABLE_EXPERIMENTAL_WARNINGS
				if constexpr (NumSubInterfaces > 0)
				{
					ReserveSubInterfaces(NumSubInterfaces);
				}

				// Add all the elements of the vertex interface. This uses a fold expression
				// to call Add(...) on each input. The various overloads of the Add(...) method
				// then assemble the appropriate structures.
				(Add(Forward<ArgTypes>(InArgs)), ...);
			}

		private:

			using FSubInterfaceDeclarationBuilder::Add; // Unhide Add(...) function overloads in FSubInterfaceDeclarationBuilder
			UE_API void Add(FInputDataVertex&& InVertex);

			TArray<FInputDataVertex>& Vertices;
		};

		/** Interface builder for FOutputVertexInterface declarations. */
		class FOutputVertexInterfaceDeclarationBuilder : public FSubInterfaceDeclarationBuilder
		{
		public:
			UE_API FOutputVertexInterfaceDeclarationBuilder(TArray<FOutputDataVertex>& OutVertices, TArray<FSubInterfaceLayout>& OutInstances);

			template<typename... ArgTypes>
			void Build(ArgTypes&&... InArgs)
			{
				// Size vertices exactly to avoid wasted memory in array allocations
				constexpr uint32 NumVertices = TNumOfTypeInPack<FOutputDataVertex>::Get<ArgTypes...>();
				if constexpr (NumVertices > 0)
				{
					Vertices.Reserve(NumVertices);
				}

PRAGMA_DISABLE_EXPERIMENTAL_WARNINGS
				// Size sub interfaces exactly to avoid wasted memory in array allocations
				constexpr uint32 NumSubInterfaces = TNumOfTypeInPack<FBeginSubInterface>::Get<ArgTypes...>();
PRAGMA_ENABLE_EXPERIMENTAL_WARNINGS
				if constexpr (NumSubInterfaces > 0)
				{
					ReserveSubInterfaces(NumSubInterfaces);
				}

				// Add all the elements of the vertex interface. This uses a fold expression
				// to call Add(...) on each input. The various overloads of the Add(...) method
				// then assemble the appropriate structures.
				(Add(Forward<ArgTypes>(InArgs)), ...);
			}

		private:
			using FSubInterfaceDeclarationBuilder::Add; // Unhide Add(...) function overloads in FSubInterfaceDeclarationBuilder
			UE_API void Add(FOutputDataVertex&& InVertex);

			TArray<FOutputDataVertex>& Vertices;
		};

		/** Interface builder for FEnvironmentInterface declarations. */
		class FEnvironmentDeclarationBuilder 
		{
		public:
			UE_API FEnvironmentDeclarationBuilder(TArray<FEnvironmentVertex>& OutVertices);

			template<typename... ArgTypes>
			void Build(ArgTypes&&... InArgs)
			{
				// Size vertices exactly to avoid wasted memory in array allocations
				constexpr uint32 NumVertices = TNumOfTypeInPack<FEnvironmentVertex>::Get<ArgTypes...>();
				if constexpr (NumVertices > 0)
				{
					Vertices.Reserve(NumVertices);
				}

				// Add all the elements of the vertex interface. 
				(Vertices.Emplace(MoveTemp(InArgs)), ...);
			}
		private:

			TArray<FEnvironmentVertex>& Vertices;
		};
	}

	/** Interface representing the inputs of a node. */
	class FInputVertexInterface : public TVertexInterfaceImpl<FInputDataVertex>
	{
	public:
		FInputVertexInterface() = default;

		/** Construct an FInputVertexInterface from a parameter pack. This allows 
		 * node interfaces to be declared in a single function.
		 *
		 * For example:
		 * 
		 *  FInputVertexInterface Interface
		 *  {
		 *  	TInputDataVertex<float>(...),
		 *  	FBeginSubInterface(...),
		 *  		TInputDataVertex<int32>(...),
		 *  	FEndSubInterface(...),
		 *  	TInputDataVertex<float>(...)
		 *  };
		 */
		template<typename... ArgTypes>
		explicit FInputVertexInterface(ArgTypes && ... InArgs)
		{
			VertexPrivate::FInputVertexInterfaceDeclarationBuilder InterfaceBuilder(Vertices, SubInterfaces);
			InterfaceBuilder.Build(Forward<ArgTypes>(InArgs)...);
		}

		UE_API FInputVertexInterface(TArray<FInputDataVertex> InVertices, TArray<VertexPrivate::FSubInterfaceLayout> InSubInterfaces={});

		/** Iterate through all repetitions of a sub interface. */
		UE_API void ForEachSubInterfaceInstance(const FName& InSubInterfaceName, TFunctionRef<void (TConstArrayView<FInputDataVertex>)> Callable) const;
		/** Iterate through all repetitions of a sub interface. */
		UE_API void ForEachSubInterfaceInstance(const FName& InSubInterfaceName, TFunctionRef<void (TArrayView<FInputDataVertex>)> Callable);
		UE_API TConstArrayView<VertexPrivate::FSubInterfaceLayout> GetSubInterfaces(const VertexPrivate::FPrivateAccessTag& InTag) const;

	private:
		friend class VertexPrivate::FInputInterfaceConfigurationBuilder;
		UE_API const VertexPrivate::FSubInterfaceLayout* FindSubInterfaceLayout(const FName& InName) const;

		TArray<VertexPrivate::FSubInterfaceLayout> SubInterfaces;
	};

	/** Interface representing the outputs of a node. */
	class FOutputVertexInterface : public TVertexInterfaceImpl<FOutputDataVertex>
	{
	public:
		FOutputVertexInterface() = default;

		/** Construct an FOutputVertexInterface from a parameter pack. This allows 
		 * node interfaces to be declared in a single function.
		 *
		 * For example:
		 * 
		 *  FOutputVertexInterface Interface
		 *  {
		 *  	TOutputDataVertex<float>(...),
		 *  	FBeginSubInterface(...),
		 *  		TOutputDataVertex<int32>(...),
		 *  	FEndSubInterface(...),
		 *  	TOutputDataVertex<float>(...)
		 *  };
		 */
		template<typename... ArgTypes>
		explicit FOutputVertexInterface(ArgTypes && ... InArgs)
		{
			VertexPrivate::FOutputVertexInterfaceDeclarationBuilder InterfaceBuilder(Vertices, SubInterfaces);
			InterfaceBuilder.Build(Forward<ArgTypes>(InArgs)...);
		}

		UE_API FOutputVertexInterface(TArray<FOutputDataVertex> InVertices, TArray<VertexPrivate::FSubInterfaceLayout> InSubInterfaces={});

		/** Iterate through all repetitions of a sub interface. */
		UE_API void ForEachSubInterfaceInstance(const FName& InSubInterfaceName, TFunctionRef<void (TConstArrayView<FOutputDataVertex>)> Callable) const;
		/** Iterate through all repetitions of a sub interface. */
		UE_API void ForEachSubInterfaceInstance(const FName& InSubInterfaceName, TFunctionRef<void (TArrayView<FOutputDataVertex>)> Callable);

		UE_API TConstArrayView<VertexPrivate::FSubInterfaceLayout> GetSubInterfaces(const VertexPrivate::FPrivateAccessTag& InTag) const;

	private:

		friend class VertexPrivate::FOutputInterfaceConfigurationBuilder;

		UE_API const VertexPrivate::FSubInterfaceLayout* FindSubInterfaceLayout(const FName& InName) const;

		TArray<VertexPrivate::FSubInterfaceLayout> SubInterfaces;
	};

	/** Interface representing the environment variables used by a node. */
	class FEnvironmentVertexInterface : public TVertexInterfaceImpl<FEnvironmentVertex>
	{
	public:
		FEnvironmentVertexInterface() = default;

		template<typename... ArgTypes>
		explicit FEnvironmentVertexInterface(ArgTypes && ... InArgs)
		{
			VertexPrivate::FEnvironmentDeclarationBuilder InterfaceBuilder(Vertices);
			InterfaceBuilder.Build(Forward<ArgTypes>(InArgs)...);
		}

		UE_API explicit FEnvironmentVertexInterface(TArray<FEnvironmentVertex> InVertices);
	};

	/** FVertexInterface provides access to a collection of input and output vertex
	 * interfaces. 
	 */
	class FVertexInterface
	{
		public:

			/** Default constructor. */
			FVertexInterface() = default;

			/** Construct with an input and output interface. */
			UE_API FVertexInterface(const FInputVertexInterface& InInputs, const FOutputVertexInterface& InOutputs);

			/** Construct with input, output and environment interface. */
			UE_API FVertexInterface(FInputVertexInterface InInputs, FOutputVertexInterface InOutputs, FEnvironmentVertexInterface InEnvironmentVariables);

			/** Return the input interface. */
			UE_API const FInputVertexInterface& GetInputInterface() const;

			/** Return the input interface. */
			UE_API FInputVertexInterface& GetInputInterface();

			/** Return an input vertex. */
			UE_API const FInputDataVertex& GetInputVertex(const FVertexName& InKey) const;

			/** Returns true if an input vertex with the given key exists. */
			UE_API bool ContainsInputVertex(const FVertexName& InKey) const;

			/** Return the output interface. */
			UE_API const FOutputVertexInterface& GetOutputInterface() const;

			/** Return the output interface. */
			UE_API FOutputVertexInterface& GetOutputInterface();

			/** Return an output vertex. */
			UE_API const FOutputDataVertex& GetOutputVertex(const FVertexName& InName) const;

			/** Returns true if an output vertex with the given name exists. */
			UE_API bool ContainsOutputVertex(const FVertexName& InName) const;

			/** Return the output interface. */
			UE_API const FEnvironmentVertexInterface& GetEnvironmentInterface() const;

			/** Return the output interface. */
			UE_API FEnvironmentVertexInterface& GetEnvironmentInterface();

			/** Return an output vertex. */
			UE_API const FEnvironmentVertex& GetEnvironmentVertex(const FVertexName& InKey) const;

			/** Returns true if an output vertex with the given key exists. */
			UE_API bool ContainsEnvironmentVertex(const FVertexName& InKey) const;

			/** Test for equality between two interfaces. */
			friend bool METASOUNDGRAPHCORE_API operator==(const FVertexInterface& InLHS, const FVertexInterface& InRHS);

			/** Test for inequality between two interfaces. */
			friend bool METASOUNDGRAPHCORE_API operator!=(const FVertexInterface& InLHS, const FVertexInterface& InRHS);

		private:

			FInputVertexInterface InputInterface;
			FOutputVertexInterface OutputInterface;
			FEnvironmentVertexInterface EnvironmentInterface;
	};

	/** A description of a sub interface which is used when declaring a FClassVertexInterface */
	struct UE_EXPERIMENTAL(5.6, "Sub interfaces are experimental") FSubInterfaceDescription;
	struct FSubInterfaceDescription
	{
		FName SubInterfaceName;
		uint32 Min; 		//< Minimum number of instances of the sub interface. 
		uint32 Max; 		//< Maximum number of instances of the sub interface. 
		uint32 NumDefault; 	//< Default number of instances of the sub interface if desired num is unspecified. 
	};

	/** A sub interface configuration is used to create a FVertexInterface from 
	 * a FClassVertexInterface. */
	struct UE_EXPERIMENTAL(5.6, "Sub interfaces are experimental") FSubInterfaceConfiguration;
	struct FSubInterfaceConfiguration
	{
		FName SubInterfaceName;
		uint32 Num;
	};

	/** FClassVertexInterface describes the interface of a node class. It is an 
	 * immutable factory for FVertexInterfaces. */
	class UE_EXPERIMENTAL(5.6, "FClassVertexInterfaces are experimental") FClassVertexInterface;

	class FClassVertexInterface
	{
	public:
PRAGMA_DISABLE_EXPERIMENTAL_WARNINGS
		FClassVertexInterface() = default;
		~FClassVertexInterface() = default;
		UE_API FClassVertexInterface(FVertexInterface InInterface);
		UE_API FClassVertexInterface(FInputVertexInterface InInputs, FOutputVertexInterface InOutputs, FEnvironmentVertexInterface InEnvironment={});
		UE_API FClassVertexInterface(TArray<FSubInterfaceDescription> InSubInterfactDescriptions, FInputVertexInterface InInputs, FOutputVertexInterface InOutputs, FEnvironmentVertexInterface InEnvironment={});

		/** Create a FVertexInterface with the given configuration. */
		UE_API FVertexInterface CreateVertexInterface(TConstArrayView<FSubInterfaceConfiguration> InSubInterfaceConfigurations) const;
PRAGMA_ENABLE_EXPERIMENTAL_WARNINGS

	private:

PRAGMA_DISABLE_EXPERIMENTAL_WARNINGS
		TArray<FSubInterfaceDescription> SubInterfaces;
PRAGMA_ENABLE_EXPERIMENTAL_WARNINGS
		FInputVertexInterface Inputs;
		FOutputVertexInterface Outputs;
		FEnvironmentVertexInterface Environment;
	};

	/**
	 * This struct is used to pass in any arguments required for constructing a single node instance.
	 * because of this, all FNode implementations have to implement a constructor that takes an FNodeInitData instance.
	 */
	struct FNodeInitData
	{
		FVertexName InstanceName;
		FGuid InstanceID;
	};
}

/** Convert EVertexAccessType to string */
METASOUNDGRAPHCORE_API FString LexToString(Metasound::EVertexAccessType InAccessType);

#undef UE_API
