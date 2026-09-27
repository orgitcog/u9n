// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/PlatformMath.h"
#include "MuR/ExtensionData.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuR/Types.h"

#define UE_API MUTABLERUNTIME_API


namespace mu
{

    /** A customised object created from a model and a set of parameter values.
    * It corresponds to an "engine object" but the contents of its data depends on the Model, and
    * it may contain any number of LODs, components, surfaces, meshes and images, even none.
	*/
	class FInstance : public FResource
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		UE_API FInstance();

        //! Clone this instance
		UE_API TSharedPtr<FInstance> Clone() const;

		// Resource interface
		UE_API int32 GetDataSize() const override;

		/** */
		UE_API ~FInstance();

		//-----------------------------------------------------------------------------------------
		// Own interface
		//-----------------------------------------------------------------------------------------

        //! Type for the instance unique identifiers.
        typedef uint32 FID;

        //! Get a unique identifier for this instance. It doesn't change during the entire
        //! lifecycle of each instance. This identifier can be used in the System methods to update
        //! or release the instance.
        UE_API FInstance::FID GetId() const;

		//! Get the number of components of this instance.
		UE_API int32 GetComponentCount() const;

		//! Get the Id of a component
		//! \param ComponentIndex Index of the component, from 0 to GetComponentCount()-1
		UE_API uint16 GetComponentId(int32 ComponentIndex) const;

        //! Get the number of LODs in a component
        //! \param ComponentIndex Index of the component, from 0 to GetComponentCount()-1
        UE_API int32 GetLODCount( int32 ComponentIndex ) const;

        //! Get the number of surfaces in a component
        //! \param ComponentIndex Index of the component, from 0 to GetComponentCount()-1
		//! \param LODIndex Index of the level of detail, from 0 to GetLODCount()-1
		UE_API int32 GetSurfaceCount( int32 ComponentIndex, int32 LODIndex) const;

        //! Get an id that can be used to match the surface data with the mesh surface data.
        //! \param ComponentIndex Index of the component, from 0 to GetComponentCount()-1
		//! \param LODIndex Index of the level of detail, from 0 to GetLODCount()-1
		//! \param SurfaceIndex Index of the surface, from 0 to GetSurfaceCount()-1
        UE_API uint32 GetSurfaceId( int32 ComponentIndex, int32 LODIndex, int32 SurfaceIndex) const;

        //! Find a surface index from the internal id (as returned by GetSurfaceId).
        //! \param ComponentIndex Index of the component, from 0 to GetComponentCount()-1
		//! \param LODIndex Index of the level of detail, from 0 to GetLODCount()-1
		//! \param id ID of the surface to look for.
        UE_API int32 FindSurfaceById( int32 ComponentIndex, int32 LODIndex, uint32 id ) const;

		//! Find the base surface index and Lod index when reusing surfaces between LODs. Return the surface index
		//! and the LOD it belongs to.
		//! \param ComponentIndex - Index of the component, from 0 to GetComponentCount()-1
		//! \param SharedSurfaceId - Id of the surface to look for (as returned by GetSharedSurfaceId).
		//! \param OutSurfaceIndex - Index of the surface in the OutLODIndex lod. 
		//! \param OutLODIndex - Index of the first LOD where the surface can be found. 
		UE_API void FindBaseSurfaceBySharedId(int32 ComponentIndex, int32 SharedId, int32& OutSurfaceIndex, int32& OutLODIndex) const;

		//! Get an id that can be used to find the same surface on other LODs
		//! \param ComponentIndex Index of the component, from 0 to GetComponentCount()-1
		//! \param lod Index of the level of detail, from 0 to GetLODCount()-1
		//! \param SurfaceIndex Index of the surface, from 0 to GetSurfaceCount()-1
		UE_API int32 GetSharedSurfaceId(int32 ComponentIndex, int32 LODIndex, int32 SurfaceIndex) const;

        //! Get an optional, opaque application-defined identifier for this surface. The meaning of
        //! this ID depends on each application, and it is specified when creating the source data
        //! that generates this surface.
        //! See NodeSurfaceNew::SetCustomID.
		//! \param ComponentIndex Index of the component, from 0 to GetComponentCount()-1
		//! \param LODIndex Index of the level of detail, from 0 to GetLODCount()-1
		//! \param SurfaceIndex Index of the surface, from 0 to GetSurfaceCount()-1
        UE_API uint32 GetSurfaceCustomId( int32 ComponentIndex, int32 LODIndex, int32 SurfaceIndex) const;

        //! Get the mesh resource id from a component
        //! \param ComponentIndex Index of the component, from 0 to GetComponentCount()-1
		//! \param LODIndex Index of the level of detail, from 0 to GetLODCount()-1
  		UE_API FResourceID GetMeshId( int32 ComponentIndex, int32 LODIndex ) const;

		//! Get the number of images in a component
		//! \param ComponentIndex Index of the component, from 0 to GetComponentCount()-1
		//! \param LODIndex Index of the level of detail, from 0 to GetLODCount()-1
		//! \param SurfaceIndex Index of the surface, from 0 to GetSurfaceCount()-1
        UE_API int32 GetImageCount( int32 ComponentIndex, int32 LODIndex, int32 SurfaceIndex) const;

        //! Get an image resource id from a component
		//! \param ComponentIndex Index of the component, from 0 to GetComponentCount()-1
		//! \param LODIndex Index of the level of detail, from 0 to GetLODCount()-1
		//! \param SurfaceIndex Index of the surface, from 0 to GetSurfaceCount()-1
        //! \param ImageIndex Index of the image, from 0 to GetImageCount()-1
 		UE_API FResourceID GetImageId( int32 ComponentIndex, int32 LODIndex, int32 SurfaceIndex, int32 ImageIndex) const;

		//! Get the name of an image in a component
		//! \param ComponentIndex Index of the component, from 0 to GetComponentCount()-1
		//! \param LODIndex Index of the level of detail, from 0 to GetLODCount()-1
		//! \param SurfaceIndex Index of the surface, from 0 to GetSurfaceCount()-1
        //! \param ImageIndex Index of the image, from 0 to GetImageCount()-1
		UE_API FName GetImageName( int32 ComponentIndex, int32 LODIndex, int32 SurfaceIndex, int32 ImageIndex ) const;

		//! Get the number of vectors in a component
		//! \param ComponentIndex Index of the component, from 0 to GetComponentCount()-1
		//! \param LODIndex Index of the level of detail, from 0 to GetLODCount()-1
		//! \param SurfaceIndex Index of the surface, from 0 to GetSurfaceCount()-1
        UE_API int32 GetVectorCount( int32 ComponentIndex, int32 LODIndex, int32 SurfaceIndex) const;

		//! Get a vector from a component
		//! \param ComponentIndex Index of the component, from 0 to GetComponentCount()-1
		//! \param LODIndex Index of the level of detail, from 0 to GetLODCount()-1
		//! \param SurfaceIndex Index of the surface, from 0 to GetSurfaceCount()-1
        //! \param VectorIndex Index of the vector, from 0 to GetVectorCount()-1
        UE_API FVector4f GetVector( int32 ComponentIndex, int32 LODIndex, int32 SurfaceIndex, int32 VectorIndex) const;

		//! Get the name of a vector in a component
		//! \param ComponentIndex Index of the component, from 0 to GetComponentCount()-1
		//! \param LODIndex Index of the level of detail, from 0 to GetLODCount()-1
		//! \param SurfaceIndex Index of the surface, from 0 to GetSurfaceCount()-1
        //! \param VectorIndex Index of the vector, from 0 to GetVectorCount()-1
		UE_API FName GetVectorName( int32 ComponentIndex, int32 LODIndex, int32 SurfaceIndex, int32 VectorIndex ) const;

        //! Get the number of scalar values in a component
        //! \param ComponentIndex Index of the component, from 0 to GetComponentCount()-1
		//! \param LODIndex Index of the level of detail, from 0 to GetLODCount()-1
		//! \param SurfaceIndex Index of the surface, from 0 to GetSurfaceCount()-1
        UE_API int32 GetScalarCount( int32 ComponentIndex, int32 LODIndex, int32 SurfaceIndex) const;

        //! Get a scalar value from a component
        //! \param ComponentIndex Index of the component, from 0 to GetComponentCount()-1
		//! \param LODIndex Index of the level of detail, from 0 to GetLODCount()-1
		//! \param SurfaceIndex Index of the surface, from 0 to GetSurfaceCount()-1
        //! \param ScalarIndex Index of the scalar, from 0 to GetScalarCount()-1
        UE_API float GetScalar( int32 ComponentIndex, int32 LODIndex, int32 SurfaceIndex, int32 ScalarIndex) const;

        //! Get the name of a scalar from a component
        //! \param ComponentIndex Index of the component, from 0 to GetComponentCount()-1
		//! \param LODIndex Index of the level of detail, from 0 to GetLODCount()-1
		//! \param SurfaceIndex Index of the surface, from 0 to GetSurfaceCount()-1
        //! \param ScalarIndex Index of the scalar, from 0 to GetScalarCount()-1
		UE_API FName GetScalarName( int32 ComponentIndex, int32 LODIndex, int32 SurfaceIndex, int32 ScalarIndex ) const;

        //! Get the number of string values in a component
        //! \param ComponentIndex Index of the component, from 0 to GetComponentCount()-1
		//! \param LODIndex Index of the level of detail, from 0 to GetLODCount()-1
		//! \param SurfaceIndex Index of the surface, from 0 to GetSurfaceCount()-1
        UE_API int32 GetStringCount( int32 ComponentIndex, int32 LODIndex, int32 SurfaceIndex) const;

        //! Get a string value from a component
        //! \param ComponentIndex Index of the component, from 0 to GetComponentCount()-1
		//! \param LODIndex Index of the level of detail, from 0 to GetLODCount()-1
		//! \param SurfaceIndex Index of the surface, from 0 to GetSurfaceCount()-1
        //! \param StringIndex Index of the string, from 0 to GetStringCount()-1
        UE_API FString GetString( int32 ComponentIndex, int32 LODIndex, int32 SurfaceIndex, int32 StringIndex) const;

        //! Get the name of a string from a component
        //! \param ComponentIndex Index of the component, from 0 to GetComponentCount()-1
		//! \param LODIndex Index of the level of detail, from 0 to GetLODCount()-1
		//! \param SurfaceIndex Index of the surface, from 0 to GetSurfaceCount()-1
        //! \param StringIndex Index of the string, from 0 to GetStringCount()-1
        UE_API FName GetStringName( int32 ComponentIndex, int32 LODIndex, int32 SurfaceIndex, int32 StringIndex ) const;

		//! Get the index of the overlay material.
		//! \param ComponentIndex Index of the component, from 0 to GetComponentCount()-1
		UE_API int32 GetOverlayMaterial( int32 ComponentIndex ) const;

		//! Get the number of ExtensionData values in a component
		UE_API int32 GetExtensionDataCount() const;

		//! Get an ExtensionData value from a component
		//! \param Index Index of the ExtensionData to fetch
		//! \param OutExtensionData Receives the ExtensionData
		//! \param OutName Receives the name associated with the ExtensionData. Guaranteed to be a valid string of non-zero length.
		UE_API void GetExtensionData(int32 Index, TSharedPtr<const FExtensionData>& OutExtensionData, FName& OutName) const;

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
