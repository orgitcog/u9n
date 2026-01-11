// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Parameters.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "Templates/Function.h"
#include "Templates/SharedPointer.h"

#define UE_API MUTABLERUNTIME_API


namespace mu
{
	class FInputArchive;
	class FModelWriter;
	class FOutputArchive;
	class FSystem;
    class FModelParametersGenerator;


    /** A Model represents a customisable object with any number of parameters.
    * When values are given to the parameters, specific Instances can be built, which hold the
    * built application-usable data.
	*/
    class FModel
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------
		UE_API FModel();

		//! Don't call directly. Manage with a TSharedPtr.
		UE_API ~FModel();

		static UE_API void Serialise( const FModel*, FOutputArchive& );
		static UE_API TSharedPtr<FModel> StaticUnserialise( FInputArchive& );

		/** Special serialise operation that serialises the data in separate "files". An object
         * with the ModelStreamer interface is responsible of storing this data and providing
		 * the "file" concept. 
		 * If bDropData is set to true, the rom data will be freed as it is serialized.
		 */
        static UE_API void Serialise( FModel*, FModelWriter&, bool bDropData=false );

		//! Return true if the model has external data in other files. This kind of models will
		//! require data streaming when used.
		UE_API bool HasExternalData() const;

#if WITH_EDITOR
		//! Return true unless the streamed resources were destroyed, which could happen in the
		//! editor after recompiling the CO.
		UE_API bool IsValid() const;

		//! Invalidate the Model. Compiling a compiled CO will invalidate the model kept by previously
		//! generated resources, like streamed textures.
		UE_API void Invalidate();
#endif

		//-----------------------------------------------------------------------------------------
		// Own interface
		//-----------------------------------------------------------------------------------------

		//! Create a set of new parameters of the model with the default values.
		//! If old parameters are provided, they will be reused when possible instead of the
		//! default values.
        static UE_API TSharedPtr<FParameters> NewParameters( TSharedPtr<const FModel> Model, const FParameters* Old = nullptr );

		/** Return true if the parameter is multi-dimensional */
		UE_API bool IsParameterMultidimensional(int32 ParameterIndex) const;

		//! Get the number of states in the model.
		UE_API int32 GetStateCount() const;

		//! Get a state name by state index from 0 to GetStateCount-1
		UE_API const FString& GetStateName( int32 StateIndex ) const;

		//! Find a state index by state name
		UE_API int32 FindState( const FString& Name ) const;

		//! Get the number of parameters available in a particular state.
		UE_API int32 GetStateParameterCount( int32 StateIndex ) const;

		//! Get the index of one of the parameters in the given state. The index refers to the
		//! parameters in a FParameters object obtained from this model with NewParameters.
		UE_API int32 GetStateParameterIndex( int32 StateIndex, int32 ParamIndex ) const;

        //! Free memory used by streaming data that may be loaded again when needed.
        UE_API void UnloadExternalData();
    	
		//! Return the default value of a boolean parameter.
		//! \pre The parameter specified by index is a T_BOOL.
		//! \param Index Index of the parameter from 0 to GetCount()-1
    	UE_API bool GetBoolDefaultValue(int32 Index) const;

   		//! Return the default value of a integer parameter.
		//! \pre The parameter specified by index is a T_INT.
		//! \param Index Index of the parameter from 0 to GetCount()-1
    	UE_API int32 GetIntDefaultValue(int32 Index) const;

		//! Return the default value of a float parameter.
		//! \pre The parameter specified by index is a T_FLOAT.
		//! \param Index Index of the parameter from 0 to GetCount()-1
        UE_API float GetFloatDefaultValue(int32 Index) const;

		//! Return the default value of a colour parameter.
		//! \pre The parameter specified by index is a T_FLOAT.
        //! \param Index Index of the parameter from 0 to GetCount()-1
        //! \param R,G,B Pointers to values where every resulting colour channel will be stored
    	UE_API void GetColourDefaultValue(int32 Index, FVector4f& OutValue) const;

		//! Return the default value of a colour parameter.
		//! \pre The parameter specified by index is a T_FLOAT.
        //! \param Index Index of the parameter from 0 to GetCount()-1
    	UE_API FMatrix44f GetMatrixDefaultValue(int32 Index) const;

    	//! Return the default value of a projector parameter, as a 4x4 matrix. The matrix is supposed to be
		//! a linear transform in column-major.
		//! \pre The parameter specified by index is a T_PROJECTOR.
        //! \param Index Index of the parameter from 0 to GetCount()-1
        //! \param OutPos Pointer to where the object-space position coordinates of the projector will be stored.
        //! \param OutDir Pointer to where the object-space direction vector of the projector will be stored.
        //! \param OutUp Pointer to where the object-space vertically up direction vector
        //!         of the projector will be stored. This controls the "roll" angle of the
        //!         projector.
        //! \param OutScale Pointer to the projector-space scaling of the projector.
    	UE_API void GetProjectorDefaultValue(int32 Index, EProjectorType* OutProjectionType, FVector3f* OutPos,
    	 	FVector3f* OutDir, FVector3f* OutUp, FVector3f* OutScale, float* OutProjectionAngle) const;

        //! Return the default value of an image parameter.
        //! \pre The parameter specified by index is a T_IMAGE.
        //! \param Index Index of the parameter from 0 to GetCount()-1
		//! \return The externalId specified when setting the image value (\see SetImageValue)
		UE_API FName GetImageDefaultValue(int32 Index) const;

    	UE_API int32 GetRomCount() const;

#if WITH_EDITOR
		UE_API uint32 GetRomSourceId(int32 Index) const;
#endif

		UE_API uint32 GetRomSize(int32 Index) const;

		UE_API bool IsMeshData(int32 Index) const;

		UE_API bool IsRomHighRes(int32 Index) const;
   
		/** 
		 * Returns the ConstantImageIndex LODIndex rom id. In case ConstantImageIndex does not have LODIndex 
		 * returns -1.
		 */
    	UE_API int32 GetConstantImageRomId(int32 ConstantImageIndex, int32 LODIndex) const;

		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;

		UE_API Private* GetPrivate() const;

	private:

		Private* m_pD;

	};

}

#undef UE_API
