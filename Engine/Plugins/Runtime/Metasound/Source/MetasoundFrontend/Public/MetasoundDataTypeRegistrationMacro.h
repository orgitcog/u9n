// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IAudioProxyInitializer.h"
#include "MetasoundArrayNodesRegistration.h"
#include "MetasoundAutoConverterNode.h"
#include "MetasoundConverterNodeRegistrationMacro.h"
#include "MetasoundDataFactory.h"
#include "MetasoundDataReference.h"
#include "MetasoundDataReferenceMacro.h"
#include "MetasoundEnum.h"
#include "MetasoundFrontendDataTypeRegistry.h"
#include "MetasoundFrontendDataTypeTraits.h"
#include "MetasoundFrontendRegistries.h"
#include "MetasoundInputNode.h"
#include "MetasoundLiteral.h"
#include "MetasoundLiteralNode.h"
#include "MetasoundLog.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundOutputNode.h"
#include "MetasoundReceiveNode.h"
#include "MetasoundSendNode.h"
#include "MetasoundTransmissionRegistration.h"
#include "MetasoundVariableNodes.h"
#include "MetasoundParameterPackFixedArray.h"

#include "Templates/Casts.h"

#include <type_traits>

#define UE_API METASOUNDFRONTEND_API

namespace Metasound::Frontend
{
	namespace MetasoundDataTypeRegistrationPrivate
	{
		template<typename DataType>
		struct TDataTypeProxyConstructorDeprecation
		{
			private:
				static constexpr bool bIsParsableWithDeprecatedPtr = TIsParsable<DataType, DataFactoryPrivate::ProxyDataPtrType_DEPRECATED>::Value;
				static constexpr bool bIsParsableWithSharedProxyPtr = TIsParsable<DataType, TSharedPtr<Audio::IProxyData>>::Value;
			public:
				static constexpr bool bOnlySupportsDeprecatedProxyPtr = bIsParsableWithDeprecatedPtr && !bIsParsableWithSharedProxyPtr;
		};

		// ExecutableDataTypes are deprecated in 5.3 in favor of TPostExecutableDataTypes.
		// This class triggers a deprecation warning when a TExecutableDataType
		// is registered. 
		template<typename DataType>
		struct TExecutableDataTypeDeprecation
		{
			TExecutableDataTypeDeprecation()
			{
				if constexpr (TExecutableDataType<DataType>::bIsExecutable)
				{
					TriggerDeprecationMessage();
				}
			}

			void TriggerDeprecationMessage() 
			{
				UE_LOG(LogMetaSound, Warning, TEXT("TExecutableDataType<> is deprecated in favor of TPostExecutableDataType<>. Please update your code for data type (%s) as TExecutableDataType<> will be removed in future releases"), *GetMetasoundDataTypeString<DataType>())
			}
		};

		// Returns the Array version of a literal type if it exists.
		template<ELiteralType LiteralType>
		struct TLiteralArrayEnum 
		{
			// Default to TArray default constructor by using
			// ELiteralType::None
			static constexpr ELiteralType Value = ELiteralType::None;
		};

		// Specialization for None->NoneArray
		template<>
		struct TLiteralArrayEnum<ELiteralType::None>
		{
			static constexpr ELiteralType Value = ELiteralType::NoneArray;
		};
		 
		// Specialization for Boolean->BooleanArray
		template<>
		struct TLiteralArrayEnum<ELiteralType::Boolean>
		{
			static constexpr ELiteralType Value = ELiteralType::BooleanArray;
		};
		
		// Specialization for Integer->IntegerArray
		template<>
		struct TLiteralArrayEnum<ELiteralType::Integer>
		{
			static constexpr ELiteralType Value = ELiteralType::IntegerArray;
		};
		
		// Specialization for Float->FloatArray
		template<>
		struct TLiteralArrayEnum<ELiteralType::Float>
		{
			static constexpr ELiteralType Value = ELiteralType::FloatArray;
		};
		
		// Specialization for String->StringArray
		template<>
		struct TLiteralArrayEnum<ELiteralType::String>
		{
			static constexpr ELiteralType Value = ELiteralType::StringArray;
		};
		
		// Specialization for UObjectProxy->UObjectProxyArray
		template<>
		struct TLiteralArrayEnum<ELiteralType::UObjectProxy>
		{
			static constexpr ELiteralType Value = ELiteralType::UObjectProxyArray;
		};

		// This utility function can be used to optionally check to see if we can transmit a data type, and autogenerate send and receive nodes for that datatype.
		template<typename TDataType, typename TEnableIf<TIsTransmittable<TDataType>::Value, bool>::Type = true>
		void AttemptToRegisterSendAndReceiveNodes()
		{
			if (TEnableTransmissionNodeRegistration<TDataType>::Value)
			{
				ensureAlways(RegisterNode<Metasound::TSendNode<TDataType>>());
				ensureAlways(RegisterNode<Metasound::TReceiveNode<TDataType>>());
			}
		}

		template<typename TDataType, typename TEnableIf<!TIsTransmittable<TDataType>::Value, bool>::Type = true>
		void AttemptToRegisterSendAndReceiveNodes()
		{
			// This implementation intentionally noops, because Metasound::TIsTransmittable is false for this datatype.
			// This is either because the datatype is not trivially copyable, and thus can't be buffered between threads,
			// or it's not an audio buffer type, which we use Audio::FPatchMixerSplitter instances for.
		}

		// This utility function can be used to check to see if we can static cast between two types, and autogenerate a node for that static cast.
		template<typename TFromDataType, typename TToDataType, typename std::enable_if<TIsAutoConvertible<TFromDataType, TToDataType>::Value, bool>::type = true>
		void AttemptToRegisterConverter()
		{
			using FConverterNode = Metasound::TAutoConverterNode<TFromDataType, TToDataType>;

			if (TEnableAutoConverterNodeRegistration<TFromDataType, TToDataType>::Value)
			{
				const FNodeClassMetadata& Metadata = FConverterNode::GetAutoConverterNodeMetadata();
				const Metasound::Frontend::FNodeRegistryKey Key(Metadata);

				if (!std::is_same<TFromDataType, TToDataType>::value && !FMetasoundFrontendRegistryContainer::Get()->IsNodeRegistered(Key))
				{
					ensureAlways(RegisterNode<FConverterNode>(Metadata));
					
					bool bSucessfullyRegisteredConversionNode = RegisterConversionNode<FConverterNode, TFromDataType, TToDataType>(FConverterNode::GetInputName(), FConverterNode::GetOutputName(), Metadata);
					ensureAlways(bSucessfullyRegisteredConversionNode);
				}
			}
		}

		template<typename TFromDataType, typename TToDataType, typename std::enable_if<!TIsAutoConvertible<TFromDataType, TToDataType>::Value, int>::type = 0>
		void AttemptToRegisterConverter()
		{
			// This implementation intentionally noops, because static_cast<TFromDataType>(TToDataType&) is invalid.
		}

		// Here we attempt to infer and autogenerate conversions for basic datatypes.
		template<typename TDataType>
		void RegisterConverterNodes()
		{
			// Conversions to this data type:
			AttemptToRegisterConverter<bool, TDataType>();
			AttemptToRegisterConverter<int32, TDataType>();
			AttemptToRegisterConverter<float, TDataType>();
			AttemptToRegisterConverter<FString, TDataType>();

			// Conversions from this data type:
			AttemptToRegisterConverter<TDataType, bool>();
			AttemptToRegisterConverter<TDataType, int32>();
			AttemptToRegisterConverter<TDataType, float>();
			AttemptToRegisterConverter<TDataType, FString>();
		}

		/** Creates the FDataTypeRegistryInfo for a data type.
		 * 
		 * @tparam TDataType - The data type to create info for.
		 * @tparam PreferredArgType - The preferred constructor argument type to use when creating an instance of the data type.
		 * @tparam UClassToUse - The preferred UObject class to use when constructing from an Audio::IProxyData.
		 */
		template<typename TDataType, ELiteralType PreferredArgType, typename UClassToUse>
		Frontend::FDataTypeRegistryInfo CreateDataTypeInfo()
		{
			Frontend::FDataTypeRegistryInfo RegistryInfo;

			RegistryInfo.DataTypeName = GetMetasoundDataTypeName<TDataType>();
			RegistryInfo.DataTypeDisplayText = GetMetasoundDataTypeDisplayText<TDataType>();
			RegistryInfo.PreferredLiteralType = PreferredArgType;

			RegistryInfo.bIsParsable = TLiteralTraits<TDataType>::bIsParsableFromAnyLiteralType;
			RegistryInfo.bIsArrayParseable = TLiteralTraits<TDataType>::bIsParseableFromAnyArrayLiteralType;

			RegistryInfo.bIsArrayType = TIsArrayType<TDataType>::Value;

			RegistryInfo.bIsDefaultParsable = TIsParsable<TDataType, FLiteral::FNone>::Value;
			RegistryInfo.bIsBoolParsable = TIsParsable<TDataType, bool>::Value;
			RegistryInfo.bIsIntParsable = TIsParsable<TDataType, int32>::Value;
			RegistryInfo.bIsFloatParsable = TIsParsable<TDataType, float>::Value;
			RegistryInfo.bIsStringParsable = TIsParsable<TDataType, FString>::Value;
			RegistryInfo.bIsProxyParsable = TIsParsable<TDataType, const TSharedPtr<Audio::IProxyData>&>::Value;

			RegistryInfo.bIsUniquePtrProxyParsable_DEPRECATED = TIsParsable<TDataType, const TUniquePtr<Audio::IProxyData>&>::Value;

			RegistryInfo.bIsDefaultArrayParsable = TIsParsable<TDataType, TArray<FLiteral::FNone>>::Value;
			RegistryInfo.bIsBoolArrayParsable = TIsParsable<TDataType, TArray<bool>>::Value;
			RegistryInfo.bIsIntArrayParsable = TIsParsable<TDataType, TArray<int32>>::Value;
			RegistryInfo.bIsFloatArrayParsable = TIsParsable<TDataType, TArray<float>>::Value;
			RegistryInfo.bIsStringArrayParsable = TIsParsable<TDataType, TArray<FString>>::Value;
			RegistryInfo.bIsProxyArrayParsable = TIsParsable<TDataType, const TArray<TSharedPtr<Audio::IProxyData>>& >::Value;

			RegistryInfo.bIsUniquePtrProxyArrayParsable_DEPRECATED = TIsParsable<TDataType, const TArray<TUniquePtr<Audio::IProxyData>>& >::Value;

			RegistryInfo.bIsEnum = TEnumTraits<TDataType>::bIsEnum;
			RegistryInfo.bIsExplicit = TIsExplicit<TDataType>::Value;
			RegistryInfo.bIsVariable = TIsVariable<TDataType>::Value;
			RegistryInfo.bIsTransmittable = TIsTransmittable<TDataType>::Value;
			RegistryInfo.bIsConstructorType = TIsConstructorVertexSupported<TDataType>::Value;
			
			if constexpr (std::is_base_of<UObject, UClassToUse>::value)
			{
				RegistryInfo.ProxyGeneratorClass = UClassToUse::StaticClass();
			}
			else
			{
				static_assert(std::is_same<UClassToUse, void>::value, "Only UObject derived classes can supply proxy interfaces.");
				RegistryInfo.ProxyGeneratorClass = nullptr;
			}

			return RegistryInfo;
		}

		/** Returns an IEnumDataTypeInterface pointer for the data type. If the
		 * data type has no IEnumDataTypeInterface, the returned pointer will be
		 * invalid.
		 *
		 * @tparam TDataType - The data type to create the interface for.
		 *
		 * @return A shared pointer to the IEnumDataTypeInterface. If the TDataType
		 *         does not have an IEnumDataTypeInterface, returns an invalid pointer.
		 */
		template<typename TDataType>
		TSharedPtr<Frontend::IEnumDataTypeInterface> GetEnumDataTypeInterface()
		{
			TSharedPtr<Frontend::IEnumDataTypeInterface> EnumInterfacePtr;

			using FEnumTraits = TEnumTraits<TDataType>;

			// Check if data type is an enum.
			if constexpr (FEnumTraits::bIsEnum)
			{
				using InnerType = typename FEnumTraits::InnerType;
				using FStringHelper = TEnumStringHelper<InnerType>;

				struct FEnumHandler : Metasound::Frontend::IEnumDataTypeInterface
				{
					FName GetNamespace() const override
					{
						return FStringHelper::GetNamespace();
					}

					int32 GetDefaultValue() const override
					{
						return static_cast<int32>(TEnumTraits<TDataType>::DefaultValue);
					}

					const TArray<FGenericInt32Entry>& GetAllEntries() const override
					{
						auto BuildIntEntries = []()
						{
							// Convert to int32 representation 
							TArray<FGenericInt32Entry> IntEntries;
							IntEntries.Reserve(FStringHelper::GetAllEntries().Num());
							for (const TEnumEntry<InnerType>& i : FStringHelper::GetAllEntries())
							{
								IntEntries.Emplace(i);
							}
							return IntEntries;
						};
						static const TArray<FGenericInt32Entry> IntEntries = BuildIntEntries();
						return IntEntries;
					}
				};

				EnumInterfacePtr = MakeShared<FEnumHandler>();
			}

			return EnumInterfacePtr;
		}

		template <typename T, typename V = void>
		struct HasRawParameterAssignmentOp { bool value = false; };

		template <typename T>
		struct HasRawParameterAssignmentOp<T, decltype(&T::AssignRawParameter, void())> { bool value = true; };

		// Base registry entry for any data type
		class FDataTypeRegistryEntryBase : public IDataTypeRegistryEntry
		{
		public:
			UE_API FDataTypeRegistryEntryBase(const Frontend::FDataTypeRegistryInfo& Info, const TSharedPtr<Frontend::IEnumDataTypeInterface>& EnumInterface);
			UE_API virtual ~FDataTypeRegistryEntryBase();

			UE_API virtual const Frontend::FDataTypeRegistryInfo& GetDataTypeInfo() const override;
			UE_API virtual TSharedPtr<const Frontend::IEnumDataTypeInterface> GetEnumInterface() const override;
			UE_API virtual const FMetasoundFrontendClass& GetFrontendInputClass() const override;
			UE_API virtual TSharedPtr<const FNodeClassMetadata> GetInputClassMetadata() const override;
			UE_API virtual const FMetasoundFrontendClass& GetFrontendConstructorInputClass() const override;
			UE_API virtual TSharedPtr<const FNodeClassMetadata> GetConstructorInputClassMetadata() const override;
			UE_API virtual const FMetasoundFrontendClass& GetFrontendLiteralClass() const override;
			UE_API virtual const FMetasoundFrontendClass& GetFrontendOutputClass() const override;
			UE_API virtual TSharedPtr<const FNodeClassMetadata> GetOutputClassMetadata() const override;
			UE_API virtual const FMetasoundFrontendClass& GetFrontendConstructorOutputClass() const override;
			UE_API virtual TSharedPtr<const FNodeClassMetadata> GetConstructorOutputClassMetadata() const override;
			UE_API virtual const FMetasoundFrontendClass& GetFrontendVariableClass() const override;
			UE_API virtual TSharedPtr<const FNodeClassMetadata> GetVariableClassMetadata() const override;
			UE_API virtual const FMetasoundFrontendClass& GetFrontendVariableMutatorClass() const override;
			UE_API virtual TSharedPtr<const FNodeClassMetadata> GetVariableMutatorClassMetadata() const override;
			UE_API virtual const FMetasoundFrontendClass& GetFrontendVariableAccessorClass() const override;
			UE_API virtual TSharedPtr<const FNodeClassMetadata> GetVariableAccessorClassMetadata() const override;
			UE_API virtual const FMetasoundFrontendClass& GetFrontendVariableDeferredAccessorClass() const override;
			UE_API virtual TSharedPtr<const FNodeClassMetadata> GetVariableDeferredAccessorClassMetadata() const override;
			UE_API virtual const Frontend::IParameterAssignmentFunction& GetRawAssignmentFunction() const override;
			UE_API virtual Frontend::FLiteralAssignmentFunction GetLiteralAssignmentFunction() const override;
			UE_API virtual TUniquePtr<INode> CreateOutputNode(FNodeData InNodeData) const override;
			UE_API virtual TUniquePtr<INode> CreateConstructorOutputNode(FNodeData InNodeData) const override;

		protected:
			Frontend::FDataTypeRegistryInfo Info;
			FMetasoundFrontendClass InputClass;
			TSharedPtr<const FNodeClassMetadata> InputClassMetadata;
			FMetasoundFrontendClass ConstructorInputClass;
			TSharedPtr<const FNodeClassMetadata> ConstructorInputClassMetadata;
			FMetasoundFrontendClass OutputClass;
			TSharedPtr<const FNodeClassMetadata> OutputClassMetadata;
			FMetasoundFrontendClass ConstructorOutputClass;
			TSharedPtr<const FNodeClassMetadata> ConstructorOutputClassMetadata;
			
			FMetasoundFrontendClass LiteralClass;

			FMetasoundFrontendClass VariableClass;
			TSharedPtr<const FNodeClassMetadata> VariableClassMetadata;
			FMetasoundFrontendClass VariableMutatorClass;
			TSharedPtr<const FNodeClassMetadata> VariableMutatorClassMetadata;
			FMetasoundFrontendClass VariableAccessorClass;
			TSharedPtr<const FNodeClassMetadata> VariableAccessorClassMetadata;
			FMetasoundFrontendClass VariableDeferredAccessorClass;
			TSharedPtr<const FNodeClassMetadata> VariableDeferredAccessorClassMetadata;
			TSharedPtr<Frontend::IEnumDataTypeInterface> EnumInterface;
			Frontend::IParameterAssignmentFunction RawAssignmentFunction;
			Frontend::FLiteralAssignmentFunction LiteralAssignmentFunction = nullptr;
		};


		/** Registers a data type with the MetaSound Frontend. This allows the data type
		 * to be used in Input and Output nodes by informing the Frontend how to
		 * instantiate an instance.
		 *
		 * @tparam TDataType - The data type to register.
		 * @tparam PreferredArgType - The preferred constructor argument type to use when creating an instance of the data type.
		 * @tparam UClassToUse - The preferred UObject class to use when constructing from an Audio::IProxyDataPtr. If the type is not
		 *                       constructible with an Audio::IProxyDataPtr, then this should be void.
		 *
		 * @return True on success, false on failure.
		 */
		template<typename TDataType, ELiteralType PreferredArgType = ELiteralType::None, typename UClassToUse = void>
		bool RegisterDataTypeWithFrontendInternal()
		{
			static constexpr bool bIsParsable = TLiteralTraits<TDataType>::bIsParsableFromAnyLiteralType;
			static constexpr bool bIsConstructorType = TIsConstructorVertexSupported<TDataType>::Value;

			static bool bAlreadyRegisteredThisDataType = false;
			if (bAlreadyRegisteredThisDataType)
			{
				UE_LOG(LogMetaSound, Display, TEXT("Tried to call REGISTER_METASOUND_DATATYPE twice with the same class %s. ignoring the second call. Likely because REGISTER_METASOUND_DATATYPE is in a header that's used in multiple modules. Consider moving it to a private header or cpp file."), TDataReferenceTypeInfo<TDataType>::TypeName)
				return false;
			}

	
			if constexpr (MetasoundDataTypeRegistrationPrivate::TDataTypeProxyConstructorDeprecation<TDataType>::bOnlySupportsDeprecatedProxyPtr)
			{
				// TUniquePtr<Audio::IProxyData> deprecated in 5.2. Log a warning
				// during data type registration to warn users to update their MetaSound
				// data type constructor. 
				UE_LOG(LogMetaSound, Warning, TEXT("MetaSound data type \"%s\" supports construction from deprecated TUniquePtr<Audio::IProxyData>. Please update the constructor to accept a \"const TSharedPtr<Audio::IProxyData>& \""), TDataReferenceTypeInfo<TDataType>::TypeName);
			}
			
			// TExecutableDataTypes are deprecated as of 5.3. This call triggers
			// a deprecation warning in case the TExecutableDataType<> template
			// was specialized.
			MetasoundDataTypeRegistrationPrivate::TExecutableDataTypeDeprecation<TDataType>();

			bAlreadyRegisteredThisDataType = true;


			class FDataTypeRegistryEntry : public FDataTypeRegistryEntryBase
			{
				void InitRawAssignmentFunction()
				{
					if constexpr (HasRawParameterAssignmentOp<TDataType>().value)
					{
						this->RawAssignmentFunction = [](const void* src, void* dest) { reinterpret_cast<TDataType*>(dest)->AssignRawParameter(src); };
					}
					else if constexpr (std::is_copy_assignable_v<TDataType>)
					{
						if constexpr (!TIsArrayType<TDataType>::Value)
						{
							this->RawAssignmentFunction = [](const void* Src, void* Dest)
								{
									*(reinterpret_cast<TDataType*>(Dest)) = *(reinterpret_cast<const TDataType*>(Src)); 
								};
						}
						else
						{
							this->RawAssignmentFunction = [](const void* Src, void* Dest)
								{
									//***********************************************************************************************************
									// sanity check to be sure memory layout is the same regardless of number of elements in the fixed array...
									using FSmallFixedArray = TParamPackFixedArray<typename TDataType::ElementType, 1>;
									using FLargeFixedArray = TParamPackFixedArray<typename TDataType::ElementType, 200>;
									static_assert(offsetof(FSmallFixedArray,InlineData) > offsetof(FSmallFixedArray, NumValidElements));
									static_assert(offsetof(FSmallFixedArray,InlineData) == offsetof(FLargeFixedArray,InlineData));
									//***********************************************************************************************************
									TDataType& DestinationArray = *(reinterpret_cast<TDataType*>(Dest));
									const FSmallFixedArray& SourceArray = *(reinterpret_cast<const FSmallFixedArray*>(Src));
									SourceArray.CopyToArray(DestinationArray);
								};
						}
					}
				}

				void InitLiteralAssignmentFunction()
				{
					if constexpr (std::is_copy_assignable_v<TDataType> && bIsParsable)
					{
						this->LiteralAssignmentFunction = [](const FOperatorSettings& InOperatorSettings, const FLiteral& InLiteral, const FAnyDataReference& OutDataRef)
						{
							*OutDataRef.GetWritableValue<TDataType>() = TDataTypeLiteralFactory<TDataType>::CreateExplicitArgs(InOperatorSettings, InLiteral);
						};
					}
				}

				void InitNodeClasses()
				{
					// Create class info using prototype node
					// TODO: register nodes with static class info instead of prototype instance.

					if constexpr (bIsParsable)
					{
						const FName DataTypeName = GetMetasoundDataTypeName<TDataType>();
						const FName UnnamedVertex;


						this->InputClassMetadata = MakeShared<FNodeClassMetadata>(TInputNode<TDataType, EVertexAccessType::Reference>::CreateNodeClassMetadata(UnnamedVertex));
						this->InputClass = Metasound::Frontend::GenerateClass(*this->InputClassMetadata, EMetasoundFrontendClassType::Input);

						this->OutputClassMetadata = MakeShared<FNodeClassMetadata>(FOutputNode::CreateNodeClassMetadata(UnnamedVertex, DataTypeName, EVertexAccessType::Reference));
						this->OutputClass = Metasound::Frontend::GenerateClass(*this->OutputClassMetadata, EMetasoundFrontendClassType::Output);

						TLiteralNode<TDataType> LiteralPrototype(TEXT(""), FGuid(), FLiteral());
						this->LiteralClass = Metasound::Frontend::GenerateClass(LiteralPrototype.GetMetadata(), EMetasoundFrontendClassType::Literal);

						this->VariableClassMetadata = MakeShared<FNodeClassMetadata>(TVariableNode<TDataType>::CreateNodeClassMetadata());
						this->VariableClass = Metasound::Frontend::GenerateClass(*this->VariableClassMetadata, EMetasoundFrontendClassType::Variable);

						this->VariableMutatorClassMetadata = MakeShared<FNodeClassMetadata>(TVariableMutatorNode<TDataType>::CreateNodeClassMetadata());
						this->VariableMutatorClass = Metasound::Frontend::GenerateClass(*this->VariableMutatorClassMetadata, EMetasoundFrontendClassType::VariableMutator);

						this->VariableAccessorClassMetadata = MakeShared<FNodeClassMetadata>(TVariableAccessorNode<TDataType>::CreateNodeClassMetadata());
						this->VariableAccessorClass = Metasound::Frontend::GenerateClass(*this->VariableAccessorClassMetadata, EMetasoundFrontendClassType::VariableAccessor);

						this->VariableDeferredAccessorClassMetadata = MakeShared<FNodeClassMetadata>(TVariableDeferredAccessorNode<TDataType>::CreateNodeClassMetadata());
						this->VariableDeferredAccessorClass = Metasound::Frontend::GenerateClass(*this->VariableDeferredAccessorClassMetadata, EMetasoundFrontendClassType::VariableDeferredAccessor);

						if constexpr (bIsConstructorType)
						{
							this->ConstructorInputClassMetadata = MakeShared<FNodeClassMetadata>(TInputNode<TDataType, EVertexAccessType::Value>::CreateNodeClassMetadata(UnnamedVertex));
							this->ConstructorInputClass = Metasound::Frontend::GenerateClass(*this->ConstructorInputClassMetadata, EMetasoundFrontendClassType::Input);
							this->ConstructorOutputClassMetadata = MakeShared<FNodeClassMetadata>(FOutputNode::CreateNodeClassMetadata(UnnamedVertex, DataTypeName, EVertexAccessType::Value));
							this->ConstructorOutputClass = Metasound::Frontend::GenerateClass(*this->ConstructorOutputClassMetadata, EMetasoundFrontendClassType::Output);
						}
					}
				}


			public:

				FDataTypeRegistryEntry()
					: FDataTypeRegistryEntryBase(CreateDataTypeInfo<TDataType, PreferredArgType, UClassToUse>(), GetEnumDataTypeInterface<TDataType>())
				{
					InitRawAssignmentFunction();
					InitLiteralAssignmentFunction();
					InitNodeClasses();
				}

				UE_DEPRECATED(5.6, "Create input nodes using FNodeData")
				virtual TUniquePtr<INode> CreateInputNode(FInputNodeConstructorParams&& InParams) const override
				{
					if constexpr (bIsParsable)
					{
						return MakeUnique<TInputNode<TDataType, EVertexAccessType::Reference>>(MoveTemp(InParams));
					}
					else
					{
						return TUniquePtr<INode>(nullptr);
					}
				}

				virtual TUniquePtr<INode> CreateInputNode(FNodeData InNodeData) const override
				{
					if constexpr (bIsParsable)
					{
						if (InputClassMetadata)
						{
							const FInputVertexInterface& Inputs = InNodeData.Interface.GetInputInterface();
							if (ensure(Inputs.Num() == 1))
							{
								const FVertexName& VertexName = Inputs.At(0).VertexName;
								return MakeUnique<TInputNode<TDataType, EVertexAccessType::Reference>>(VertexName, MoveTemp(InNodeData), InputClassMetadata.ToSharedRef());
							}
						}
					}
					return TUniquePtr<INode>(nullptr);
				}

				UE_DEPRECATED(5.6, "Create constructor input nodes using FNodeData")
				virtual TUniquePtr<INode> CreateConstructorInputNode(FInputNodeConstructorParams&& InParams) const override
				{
					if constexpr (bIsParsable && bIsConstructorType)
					{
						return MakeUnique<TInputNode<TDataType, EVertexAccessType::Value>>(MoveTemp(InParams));
					}
					else
					{
						return TUniquePtr<INode>(nullptr);
					}
				}

				virtual TUniquePtr<INode> CreateConstructorInputNode(FNodeData InNodeData) const override
				{
					if constexpr (bIsParsable && bIsConstructorType)
					{
						if (ConstructorInputClassMetadata)
						{
							const FInputVertexInterface& Inputs = InNodeData.Interface.GetInputInterface();
							if (ensure(Inputs.Num() == 1))
							{
								const FVertexName& VertexName = Inputs.At(0).VertexName;
								return MakeUnique<TInputNode<TDataType, EVertexAccessType::Value>>(VertexName, MoveTemp(InNodeData), ConstructorInputClassMetadata.ToSharedRef());
							}
						}
					}
					return TUniquePtr<INode>(nullptr);
				}

				using FDataTypeRegistryEntryBase::CreateOutputNode; // Unhide override

				UE_DEPRECATED(5.6, "Create output nodes using FNodeData")
				virtual TUniquePtr<INode> CreateOutputNode(FOutputNodeConstructorParams&& InParams) const override
				{
					if constexpr (bIsParsable)
					{
						return MakeUnique<TOutputNode<TDataType, EVertexAccessType::Reference>>(InParams.NodeName, InParams.InstanceID, InParams.VertexName);
					}
					else
					{
						return TUniquePtr<INode>(nullptr);
					}
				}

				using FDataTypeRegistryEntryBase::CreateConstructorOutputNode; // Unhide override
																			   
				UE_DEPRECATED(5.6, "Create constructor output nodes using FNodeData")
				virtual TUniquePtr<INode> CreateConstructorOutputNode(FOutputNodeConstructorParams&& InParams) const override
				{
					if constexpr (bIsParsable && bIsConstructorType)
					{
						return MakeUnique<TOutputNode<TDataType, EVertexAccessType::Value>>(InParams.NodeName, InParams.InstanceID, InParams.VertexName);
					}
					else
					{
						return TUniquePtr<INode>(nullptr);
					}
				}


				UE_DEPRECATED(5.6, "Direct creation of literal nodes will no longer be supported")
				virtual TUniquePtr<INode> CreateLiteralNode(FLiteralNodeConstructorParams&& InParams) const override
				{
					if constexpr (bIsParsable)
					{
						return MakeUnique<TLiteralNode<TDataType>>(InParams.NodeName, InParams.InstanceID, MoveTemp(InParams.Literal));
					}
					else
					{
						return TUniquePtr<INode>(nullptr);
					}
				}

				UE_DEPRECATED(5.6, "Direct creation of receive nodes will no longer be supported")
				virtual TUniquePtr<INode> CreateReceiveNode(const FNodeInitData& InParams) const override
				{
					if constexpr (bIsParsable)
					{
						return MakeUnique<TReceiveNode<TDataType>>(InParams);
					}
					else
					{
						return TUniquePtr<INode>(nullptr);
					}
				}

				UE_DEPRECATED(5.6, "Create variable nodes using FNodeData")
				virtual TUniquePtr<INode> CreateVariableNode(FVariableNodeConstructorParams&& InParams) const override
				{
					if constexpr (bIsParsable)
					{
						return MakeUnique<TVariableNode<TDataType>>(InParams.NodeName, InParams.InstanceID, MoveTemp(InParams.Literal));
					}
					else
					{
						return TUniquePtr<INode>(nullptr);
					}
				}

				virtual TUniquePtr<INode> CreateVariableNode(FLiteral InLiteral, FNodeData InNodeData) const override
				{
					if constexpr (bIsParsable)
					{
						if (VariableClassMetadata)
						{
							return MakeUnique<TVariableNode<TDataType>>(MoveTemp(InLiteral), MoveTemp(InNodeData), VariableClassMetadata.ToSharedRef());
						}
					}
					return TUniquePtr<INode>(nullptr);
				}
 
				UE_DEPRECATED(5.6, "Create variable mutator nodes using FNodeData")
				virtual TUniquePtr<INode> CreateVariableMutatorNode(const FNodeInitData& InParams) const override
				{
					if constexpr (bIsParsable)
					{
						return MakeUnique<TVariableMutatorNode<TDataType>>(InParams);
					}
					else
					{
						return TUniquePtr<INode>(nullptr);
					}
				}

				virtual TUniquePtr<INode> CreateVariableMutatorNode(FNodeData InNodeData) const override
				{
					if constexpr (bIsParsable)
					{
						if (VariableMutatorClassMetadata)
						{
							return MakeUnique<TVariableMutatorNode<TDataType>>(MoveTemp(InNodeData), VariableMutatorClassMetadata.ToSharedRef());
						}
					}
					return TUniquePtr<INode>(nullptr);
				}

				UE_DEPRECATED(5.6, "Create variable accessor nodes using FNodeData")
				virtual TUniquePtr<INode> CreateVariableAccessorNode(const FNodeInitData& InParams) const override
				{
					if constexpr (bIsParsable)
					{
						return MakeUnique<TVariableAccessorNode<TDataType>>(InParams);
					}
					else
					{
						return TUniquePtr<INode>(nullptr);
					}
				}

				virtual TUniquePtr<INode> CreateVariableAccessorNode(FNodeData InNodeData) const override
				{
					if constexpr (bIsParsable)
					{
						if (VariableAccessorClassMetadata)
						{
							return MakeUnique<TVariableAccessorNode<TDataType>>(MoveTemp(InNodeData), VariableAccessorClassMetadata.ToSharedRef());
						}
					}
					return TUniquePtr<INode>(nullptr);
				}

				UE_DEPRECATED(5.6, "Create variable deferred accessor nodes using FNodeData")
				virtual TUniquePtr<INode> CreateVariableDeferredAccessorNode(const FNodeInitData& InParams) const override
				{
					if constexpr (bIsParsable)
					{
						return MakeUnique<TVariableDeferredAccessorNode<TDataType>>(InParams);
					}
					else
					{
						return TUniquePtr<INode>(nullptr);
					}
				}

				virtual TUniquePtr<INode> CreateVariableDeferredAccessorNode(FNodeData InNodeData) const override
				{
					if constexpr (bIsParsable)
					{
						if (VariableAccessorClassMetadata)
						{
							return MakeUnique<TVariableDeferredAccessorNode<TDataType>>(MoveTemp(InNodeData), VariableAccessorClassMetadata.ToSharedRef());
						}
					}
					return TUniquePtr<INode>(nullptr);
				}

				virtual TSharedPtr<Audio::IProxyData> CreateProxy(UObject* InObject) const override
				{
					// Only attempt to create proxy if the `UClassToUse` is not void.
					if constexpr (!std::is_same<UClassToUse, void>::value)
					{
						static_assert(std::is_base_of<IAudioProxyDataFactory, UClassToUse>::value, "If a Metasound Datatype uses a UObject as a literal, the UClass of that object needs to also derive from Audio::IProxyDataFactory. See USoundWave as an example.");
						if (Frontend::IDataTypeRegistry::Get().IsUObjectProxyFactory(InObject))
						{
							IAudioProxyDataFactory* ObjectAsFactory = Audio::CastToProxyDataFactory<UClassToUse>(InObject);
							if (ensureAlways(ObjectAsFactory))
							{
								Audio::FProxyDataInitParams ProxyInitParams;
								ProxyInitParams.NameOfFeatureRequestingProxy = "MetaSound";

								return ObjectAsFactory->CreateProxyData(ProxyInitParams);
							}
						}
					}

					return TSharedPtr<Audio::IProxyData>(nullptr);
				}

				virtual TOptional<FAnyDataReference> CreateDataReference(EDataReferenceAccessType InAccessType, const FLiteral& InLiteral, const FOperatorSettings& InOperatorSettings) const override
				{
					if constexpr (bIsParsable)
					{
						switch (InAccessType)
						{
						case EDataReferenceAccessType::Read:
							return FAnyDataReference{ TDataReadReferenceLiteralFactory<TDataType>::CreateExplicitArgs(InOperatorSettings, InLiteral) };

						case EDataReferenceAccessType::Write:
							return FAnyDataReference{ TDataWriteReferenceLiteralFactory<TDataType>::CreateExplicitArgs(InOperatorSettings, InLiteral) };

						case EDataReferenceAccessType::Value:
							return FAnyDataReference{ TDataValueReferenceLiteralFactory<TDataType>::CreateExplicitArgs(InOperatorSettings, InLiteral) };

						default:
							break;
						}
					}
					return TOptional<FAnyDataReference>();
				}

				virtual TSharedPtr<IDataChannel, ESPMode::ThreadSafe> CreateDataChannel(const FOperatorSettings& InOperatorSettings) const override
				{
					if constexpr (bIsParsable)
					{
						return FTransmissionDataChannelFactory::CreateDataChannel<TDataType>(InOperatorSettings);
					}
					else
					{
						return TSharedPtr<IDataChannel, ESPMode::ThreadSafe>(nullptr);
					}
				}
			};

			bool bSucceeded = Frontend::IDataTypeRegistry::Get().RegisterDataType(MakeUnique<FDataTypeRegistryEntry>());
			ensureAlwaysMsgf(bSucceeded, TEXT("Failed to register data type %s in the node registry!"), *GetMetasoundDataTypeString<TDataType>());

			if (bSucceeded)
			{
				RegisterConverterNodes<TDataType>();
				AttemptToRegisterSendAndReceiveNodes<TDataType>();
			}
			
			return bSucceeded;
		}

		/** Registers an array of a data type with the MetaSound Frontend. This allows 
		 * an array of the data type to be used in Input, Output, Send and Receive 
		 * nodes by informing the Frontend how to instantiate an instance. 
		 *
		 * @tparam TDataType - The data type to register.
		 * @tparam PreferredArgType - The preferred constructor argument type to use when creating an instance of the data type.
		 * @tparam UClassToUse - The preferred UObject class to use when constructing from an Audio::IProxyDataPtr. If the type is not
		 *                       constructible with an Audio::IProxyDataPtr, then this should be void.
		 *
		 * @return True on success, false on failure.
		 */
		template<typename TDataType, ELiteralType PreferredArgType>
		bool RegisterDataTypeArrayWithFrontend()
		{
			using namespace MetasoundDataTypeRegistrationPrivate;
			using TArrayType = TArray<TDataType>;

			if constexpr (TEnableAutoArrayTypeRegistration<TDataType>::Value)
			{
				constexpr bool bIsArrayType = true;
				bool bSuccess = RegisterDataTypeWithFrontendInternal<TArrayType, TLiteralArrayEnum<PreferredArgType>::Value>();
				bSuccess = bSuccess && RegisterArrayNodes<TArrayType>();
				bSuccess = bSuccess && RegisterDataTypeWithFrontendInternal<TVariable<TArrayType>>();
				return bSuccess;
			}
			else
			{
				return true;
			}
		}
	}
	
	/** Registers a data type with the MetaSound Frontend. This allows the data type 
	 * to be used in Input, Output, Send and Receive  nodes by informing the 
	 * Frontend how to instantiate an instance. 
	 *
	 * @tparam TDataType - The data type to register.
	 * @tparam PreferredArgType - The preferred constructor argument type to use when creating an instance of the data type.
	 * @tparam UClassToUse - The preferred UObject class to use when constructing from an Audio::IProxyDataPtr. If the type is not
	 *                       constructible with an Audio::IProxyDataPtr, then this should be void.
	 *
	 * @return True on success, false on failure.
	 */
	template<typename TDataType, ELiteralType PreferredArgType = ELiteralType::None, typename UClassToUse = void>
	bool RegisterDataType()
	{
		using namespace MetasoundDataTypeRegistrationPrivate;

		// Register TDataType as a metasound data type.
		bool bSuccess = RegisterDataTypeWithFrontendInternal<TDataType, PreferredArgType, UClassToUse>();
		ensure(bSuccess);
		bSuccess = bSuccess && RegisterDataTypeWithFrontendInternal<TVariable<TDataType>>();
		ensure(bSuccess);

		// Register TArray<TDataType> as a metasound data type.
		bSuccess = bSuccess && RegisterDataTypeArrayWithFrontend<TDataType, PreferredArgType>();
		ensure(bSuccess);

		return bSuccess;
	}


	/** Registration info for a data type.
	 *
	 * @tparam DataType - The data type to be registered. 
	 */
	template<typename DataType>
	struct TMetasoundDataTypeRegistration
	{
		static_assert(std::is_same<DataType, typename std::decay<DataType>::type>::value, "DataType and decayed DataType must be the same");
		
		// To register a data type, an input node must be able to instantiate it.
		static constexpr bool bCanRegister = TInputNode<DataType, EVertexAccessType::Reference>::bCanRegister;

		// If a data type has been successfully registered, this will be true.
		static const bool bSuccessfullyRegistered;
	};
}

namespace Metasound
{
	template<typename DataType>
	using TMetasoundDataTypeRegistration UE_DEPRECATED(5.6, "Use Frontend::TMetasoundDataTypeRegistration") = Frontend::TMetasoundDataTypeRegistration<DataType>;

	template<typename TDataType, ELiteralType PreferredArgType = ELiteralType::None, typename UClassToUse = void>
	UE_DEPRECATED(5.6, "Use Frontend::RegisterDataType instead")
	bool RegisterDataTypeWithFrontend()
	{
		return Frontend::RegisterDataType<TDataType, PreferredArgType, UClassToUse>();
	}
}


#define CANNOT_REGISTER_METASOUND_DATA_TYPE_ASSERT_STRING(DataType) \
"To register " #DataType " to be used as a Metasounds input or output type, it needs a default constructor or one of the following constructors must be implemented:  " \
#DataType "(), " \
#DataType "(bool InValue), " \
#DataType "(int32 InValue), " \
#DataType "(float InValue), " \
#DataType "(const FString& InString)" \
#DataType "(const Audio::IProxyDataPtr& InData),  or " \
#DataType "(const TArray<Audio::IProxyDataPtr>& InProxyArray)."\
#DataType "(const ::Metasound::FOperatorSettings& InSettings), " \
#DataType "(const ::Metasound::FOperatorSettings& InSettings, bool InValue), " \
#DataType "(const ::Metasound::FOperatorSettings& InSettings, int32 InValue), " \
#DataType "(const ::Metasound::FOperatorSettings& InSettings, float InValue), " \
#DataType "(const ::Metasound::FOperatorSettings& InSettings, const FString& InString)" \
#DataType "(const ::Metasound::FOperatorSettings& InSettings, const Audio::IProxyDataPtr& InData),  or " \
#DataType "(const ::Metasound::FOperatorSettings& InSettings, const TArray<Audio::IProxyDataPtr>& InProxyArray)."

#define ENQUEUE_METASOUND_DATATYPE_REGISTRATION_COMMAND(DataType, DataTypeName, ...) \
	static_assert(::Metasound::Frontend::TMetasoundDataTypeRegistration<DataType>::bCanRegister, CANNOT_REGISTER_METASOUND_DATA_TYPE_ASSERT_STRING(DataType)); \
	template<> const bool ::Metasound::Frontend::TMetasoundDataTypeRegistration<DataType>::bSuccessfullyRegistered = ::Metasound::Frontend::INodeClassRegistry::Get()->EnqueueInitCommand([](){ ::Metasound::Frontend::RegisterDataType<DataType, ##__VA_ARGS__>(); }); // This static bool is useful for debugging, but also is the only way the compiler will let us call this function outside of an expression.

// This should be used to expose a datatype as a potential input or output for a metasound graph.
// The first argument to the macro is the class to expose.
// the second argument is the display name of that type in the Metasound editor.
// Optionally, a Metasound::ELiteralType can be passed in to designate a preferred literal type-
// For example, if Metasound::ELiteralType::Float is passed in, we will default to using a float parameter to create this datatype.
// If no argument is passed in, we will infer a literal type to use.
// If 
// Metasound::ELiteralType::Invalid can be used to enforce that we don't provide space for a literal, in which case you should have a default constructor or a constructor that takes [const FOperatorSettings&] implemented.
// If you pass in a preferred arg type, please make sure that the passed in datatype has a matching constructor, since we won't check this until runtime.
#define REGISTER_METASOUND_DATATYPE(DataType, DataTypeName, ...) \
	DEFINE_METASOUND_DATA_TYPE(DataType, DataTypeName); \
	ENQUEUE_METASOUND_DATATYPE_REGISTRATION_COMMAND(DataType, DataTypeName, ##__VA_ARGS__)

#undef UE_API
