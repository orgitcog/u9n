// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/TVariant.h"
#include "Math/MathFwd.h"

#include "MuR/Parameters.h"
#include "MuR/SerialisationPrivate.h"
#include "MuR/Operations.h"
#include "MuR/Image.h"
#include "MuR/Model.h"

#define UE_API MUTABLERUNTIME_API

namespace mu
{

namespace Private
{

    template<class T>
    class TIndirectObject
    {
        TUniquePtr<T> StoragePtr;

    public:
        template<typename... TArgs>
        TIndirectObject(TArgs&&... Args) : StoragePtr(MakeUnique<T>(Forward<TArgs>(Args)...)) 
        {
        }
        
        TIndirectObject(const TIndirectObject<T>& Other) : StoragePtr(MakeUnique<T>()) 
        { 
            Get() = Other; 
        }

		TIndirectObject(TIndirectObject<T>&& Other) : StoragePtr(MoveTemp(Other.StoragePtr)) 
        {
        }

        TIndirectObject(const T& Object) : StoragePtr(MakeUnique<T>()) 
        { 
            Get() = Object; 
        }
        
        TIndirectObject(T&& Object) : StoragePtr(MakeUnique<T>()) 
        { 
            Get() = MoveTemp(Object); 
        }

        TIndirectObject& operator=(TIndirectObject<T>&&) = default;

        TIndirectObject& operator=(const TIndirectObject<T>& Other) 
        { 
            Get() = Other; 
			return *this;
        }

        TIndirectObject& operator=(const T& Object) 
        { 
            Get() = Object; 
            return *this;
        }

        TIndirectObject& operator=(const T&& Object) 
        { 
            Get() = MoveTemp(Object); 
            return *this;
        }

        const T& Get() const 
        { 
            check(StoragePtr); 
            return *StoragePtr; 
        }

        T& Get() 
        { 
            check(StoragePtr); 
            return *StoragePtr; 
        }

        operator T&() 
        { 
            return Get(); 
        }

        operator const T&() const 
        { 
            return Get(); 
        }

        T* operator &() 
        { 
            return StoragePtr.Get(); 
        }

        const T* operator &() const 
        { 
            return StoragePtr.Get(); 
        }

        T& operator *() 
        { 
            return Get(); 
        }

        const T& operator *() const 
        { 
            return Get(); 
        }

        bool operator==(const TIndirectObject<T>& Other) const 
        { 
            return *StoragePtr == Other; 
        }

        bool operator==(const T& Object) const 
        { 
            return *StoragePtr == Object; 
        }

		//!
		void Serialise(FOutputArchive& Arch) const
		{
			Arch << Get();
		}

		//!
		void Unserialise(FInputArchive& Arch)
		{
			Arch >> Get();
		} 
    };

}

    MUTABLE_DEFINE_ENUM_SERIALISABLE(EParameterType)
    MUTABLE_DEFINE_ENUM_SERIALISABLE(EProjectorType)


    /** Description of a projector to project an image on a mesh. */
    struct FProjector
    {
        EProjectorType type = EProjectorType::Planar;
        FVector3f position = {0,0,0};
		FVector3f direction = {0,0,0};
		FVector3f up = {0,0,0};
		FVector3f scale = {0,0,0};
        float projectionAngle = 0.0f;

        //!
        inline void GetDirectionSideUp(FVector3f& OutDirection, FVector3f& OutSide, FVector3f& OutUp) const
        {
			OutDirection = direction;
            OutUp = up;
            OutSide = FVector3f::CrossProduct( up, direction );
			OutSide.Normalize();
        }


        //!
        void Serialise( FOutputArchive& arch ) const
        {
            arch << type;
            arch << position;
            arch << direction;
            arch << up;
            arch << scale;
            arch << projectionAngle;
        }

        //!
        void Unserialise( FInputArchive& arch )
        {
            arch >> type;
            arch >> position;
            arch >> direction;
            arch >> up;
            arch >> scale;
            arch >> projectionAngle;
        }

        bool operator==( const FProjector& o ) const
        {
            return     type==o.type
                    && position==o.position
                    && up==o.up
                    && direction==o.direction
                    && scale==o.scale
                    && projectionAngle==o.projectionAngle;
        }

    };

	//---------------------------------------------------------------------------------------------
	//! Information about a generic shape in space.
	//---------------------------------------------------------------------------------------------
	struct FShape
	{
		// Transform
		FVector3f position = FVector3f(0,0,0);
		FVector3f up = FVector3f(0, 0, 0);
		FVector3f side = FVector3f(0, 0, 0);
		
		FVector3f size = FVector3f(0, 0, 0);

		// 
		enum class Type : uint8
		{
			None = 0,
			Ellipse,
			AABox
		};
        uint8 type = 0;

		//!
		void Serialise(FOutputArchive& arch) const
		{
			arch << position;
			arch << up;
			arch << side;
			arch << size;
			arch << type;
		}

		//!
		void Unserialise(FInputArchive& arch)
		{
			arch >> position;
			arch >> up;
			arch >> side;
			arch >> size;
			arch >> type;
		}

        bool operator==( const FShape& o ) const
        {
            return type==o.type
                    && position==o.position
                    && up==o.up
                    && side==o.side
                    && size==o.size;
        }
	};


	using FParamBoolType = bool;
	using FParamIntType = int32;
	using FParamFloatType = float;
	using FParamColorType = FVector4f;
	using FParamProjectorType = Private::TIndirectObject<FProjector>;
	using FParamAssetType = FName;
	using FParamStringType = Private::TIndirectObject<FString>;
	using FParamMatrixType = Private::TIndirectObject<FMatrix44f>;
	
	using FParameterValue = TVariant<
            FParamBoolType, FParamIntType, FParamFloatType, FParamColorType, FParamProjectorType, FParamAssetType, FParamStringType, FParamMatrixType>;

    // static_assert to track ParameterValue size changes. It is ok to change if needed.
    static_assert(sizeof(FParameterValue) == 8*4, "ParameterValue size has changed.");

	// TVariant currently does not support this operator. Once supported remove it.
	inline bool operator==(const FParameterValue& ValueA, const FParameterValue& ValueB)
	{
		const int32 IndexA = ValueA.GetIndex();
		const int32 IndexB = ValueB.GetIndex();

		if (IndexA != IndexB)
		{
			return false;
		}

		return Visit([&ValueB](const auto& StoredValueA)
		{
			using Type = typename TDecay<decltype(StoredValueA)>::Type;
			return StoredValueA == ValueB.Get<Type>();
		}, ValueA);
	}
	

    struct FParameterDesc
    {
        FString Name;

        //! Unique id (provided externally, so no actual guarantee that it is unique.)
		FGuid UID;

        EParameterType Type = EParameterType::None;

        FParameterValue DefaultValue;

        //! Ranges, if the parameter is multi-dimensional. The indices refer to the Model's program
        //! vector of range descriptors.
		TArray<uint32> Ranges;

        //! Possible values of the parameter in case of being an integer, and its names
        struct FIntValueDesc
        {
            int16 Value;
			FString Name;

            //!
            bool operator==( const FIntValueDesc& Other ) const
            {
                return Value== Other.Value &&
						Name== Other.Name;
            }

            //!
            void Serialise( FOutputArchive& Arch ) const
            {
				Arch << Value;
				Arch << Name;
            }

            //!
            void Unserialise( FInputArchive& Arch)
            {
				Arch >> Value;
				Arch >> Name;
            }
        };

        //! For integer parameters, this contains the description of the possible values.
        //! If empty, the integer may have any value.
		TArray<FIntValueDesc> PossibleValues;

        //!
        bool operator==( const FParameterDesc& other ) const
        {
            return Name == other.Name && UID == other.UID && Type == other.Type &&
                   DefaultValue == other.DefaultValue &&
                   Ranges == other.Ranges &&
                   PossibleValues == other.PossibleValues;
        }

        //!
        void Serialise( FOutputArchive& arch ) const
        {
			arch << Name;
			arch << UID;
            arch << Type;
            arch << DefaultValue;
            arch << Ranges;
            arch << PossibleValues;
        }

        //!
        void Unserialise( FInputArchive& arch )
        {
			arch >> Name;
			arch >> UID;
            arch >> Type;
			arch >> DefaultValue;
            arch >> Ranges;
			arch >> PossibleValues;
        }
    };


    struct FRangeDesc
    {
		FString Name;
		FString UID;

		/** Parameter that controls the size of this range, if any. */
		int32 DimensionParameter = -1;

        //!
        bool operator==( const FRangeDesc& other ) const
        {
            return Name==other.Name
				&&
				UID == other.UID 
				&&
				DimensionParameter == other.DimensionParameter;
        }

        //!
        void Serialise( FOutputArchive& Arch ) const
        {
			Arch << Name;
			Arch << UID;
			Arch << DimensionParameter;
        }

        //!
        void Unserialise( FInputArchive& Arch)
        {
			Arch >> Name;
			Arch >> UID;
			Arch >> DimensionParameter;
        }
    };


    class FParameters::Private
    {
    public:

        //! Warning: update FParameters::Clone method if this members change.

        //! Run-time data
        TSharedPtr<const FModel> Model;
 
        //! Values for the parameters if they are not multidimensional.
		TArray<FParameterValue> Values;


        //! If the parameter is multidemensional, the values are stored here.
        //! The key of the map is the vector of values stored in a FRangeIndex
		TArray< TMap< TArray<int32>, FParameterValue > > MultiValues;


        //!
        void Serialise( FOutputArchive& arch ) const
        {
            arch << Values;
            arch << MultiValues;
        }

        //!
        void Unserialise( FInputArchive& arch )
        {
		    arch >> Values;
			arch >> MultiValues;
        }

        //!
        UE_API int32 Find( const FString& Name ) const;

        //!
        UE_API FProjector GetProjectorValue( int32 index, const FRangeIndex* ) const;

		/** Return true if the parameter has any multi-dimensional values set. This is independent to if the model
		* accepts multi-dimensional parameters for this particular parameter.
		*/
		inline bool HasMultipleValues(int32 ParamIndex) const
		{
			if (ParamIndex >= MultiValues.Num())
			{
				return false;
			}

			return MultiValues[ParamIndex].Num()>0;
		}
    };

}

#undef UE_API
