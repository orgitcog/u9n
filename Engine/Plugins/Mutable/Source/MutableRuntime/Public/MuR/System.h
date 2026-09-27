// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Image.h"
#include "MuR/Mesh.h"
#include "MuR/Instance.h"
#include "MuR/RefCounted.h"
#include "MuR/Settings.h"
#include "MuR/Types.h"
#include "Tasks/Task.h"
#include "Templates/SharedPointer.h"
#include "Templates/Tuple.h"
#include "HAL/Platform.h"

#include "System.generated.h"

#define UE_API MUTABLERUNTIME_API


/** If set to 1, this enables some expensive Unreal Insights traces, but can lead to 5x slower mutable operation.
* Other cheaper traces are enabled at all times.
*/
#define UE_MUTABLE_ENABLE_SLOW_TRACES	0


/** Despite being an UEnum, this is not always version-serialized (in MutableTools).
 * Beware of changing the enum options or order.
 */
UENUM()
enum class ETextureCompressionStrategy : uint8
{
	/** Don't change the generated format. */
	None,

	/** If a texture depends on run-time parameters for an object state, don't compress. */
	DontCompressRuntime,

	/** Never compress the textures for this state. */
	NeverCompress
};

MUTABLE_DEFINE_ENUM_SERIALISABLE(ETextureCompressionStrategy);


namespace mu
{
	// Forward references
	class FModel;
	class FModelReader;
	class FParameters;
    class FMesh;
	class FExtensionDataStreamer;


	/** */
	enum class EExecutionStrategy : uint8
	{
		/** Undefined. */
		None = 0,

		/** Always try to run operations that reduce working memory first. */
		MinimizeMemory,

		/** Always try to run operations that unlock more operations first. */
		MaximizeConcurrency,

		/** Utility value with the number of error types. */
		Count
	};


    /** Interface to request external images used as parameters. */
    class FExternalResourceProvider
    {
    public:

        //! Ensure virtual destruction
        virtual ~FExternalResourceProvider() = default;

        /** Returns the completion event and a cleanup function that must be called once event is completed. */
		virtual TTuple<UE::Tasks::FTask, TFunction<void()>> GetImageAsync(FName Id, uint8 MipmapsToSkip, TFunction<void(TSharedPtr<FImage>)>& ResultCallback) = 0;
		virtual TTuple<UE::Tasks::FTask, TFunction<void()>> GetReferencedImageAsync(const void* ModelPtr, int32 Id, uint8 MipmapsToSkip, TFunction<void(TSharedPtr<FImage>)>& ResultCallback) { check(false); return {}; }

		virtual mu::FExtendedImageDesc GetImageDesc(FName Id) = 0;

		/** Returns the completion event and a cleanup function that must be called once event is completed. */
		virtual TTuple<UE::Tasks::FTask, TFunction<void()>> GetMeshAsync(FName Id, int32 InLODIndex, int32 InSectionIndex, TFunction<void(TSharedPtr<FMesh>)>& ResultCallback) = 0;
    };


    /** Main system class to load models and build instances. */
	class FSystem
	{
    public:

        //! This constant can be used in place of the lodMask in methods like BeginUpdate
        static constexpr uint32 AllLODs = 0xffffffff;

    public:

		//! Constructor of a system object to build data.
        //! \param Settings Optional class with the settings to use in this system. The default
        //! value configures a production-ready system.
		UE_API FSystem( const FSettings& Settings = FSettings());

        //! Set a new provider for model data. 
        UE_API void SetStreamingInterface(const TSharedPtr<FModelReader>& );

        /** Set the working memory limit, overrding any set in the settings when the system was created.
         * Refer to Settings::SetWorkingMemoryBudget for more information.
		 */
        UE_API void SetWorkingMemoryBytes( uint64 Bytes );

        /** Removes all the possible working memory regardless of the budget set. This may make following 
		* operations take longer.
		*/
		UE_API void ClearWorkingMemory();
		
		/** Set the amount of generated resources keys that will be stored for resource reusal. */
		UE_API void SetGeneratedCacheSize(uint32 InCount);

        /** Set a new provider for external image data. This is only necessary if image parameters are used in the models. */
        UE_API void SetExternalResourceProvider(const TSharedPtr<FExternalResourceProvider>&);

		/** Set a function that will be used to convert image pixel formats instead of the internal conversion. 
		* \warning The provided function can be called from any thread, and also concurrently.
		* If this function fails (returns false in the first parameter) the internal function is attempted next.
		* This is useful to provide higher-quality external compressors in editor or when cooking.
		*/
		UE_API void SetImagePixelConversionOverride(const FImageOperator::FImagePixelFormatFunc&);

        //! Create a new instance from the given model. The instance can then be configured through
        //! calls to BeginUpdate/EndUpdate.
        //! A call to NewInstance must be paired to a call to ReleasesInstance when the instance is
        //! no longer needed.
        //! \param pModel Model to build an instance of
        //! \return An identifier that is always bigger than 0.
        UE_API FInstance::FID NewInstance(const TSharedPtr<const FModel>& Model);

        //! \brief Update an instance with a new parameter set and/or state.
        //!
        //! \warning a call to BeginUpdate must be paired with a call to EndUpdate once the returned
        //! data has been processed.
        //! \param InstanceID The id of the instance to update, as created by a NewInstance call.
        //! \param Params The parameters that customise this instance.
        //! \param StateIndex The index of the state this instance will be set to. The states range
        //! from 0 to Model::GetStateCount-1
        //! \param LodMask Bitmask selecting the levels of detail to build (i-th bit selects i-th lod).
        //! \return the instance data with all the LOD, components, and ids to generate the meshes 
        //! and images. The returned Instance is only valid until the next call to EndUpdate with 
        //! the same instanceID parameter.
		UE_API TSharedPtr<const FInstance> BeginUpdate(FInstance::FID InstanceID, const TSharedPtr<const FParameters>& Params, int32 StateIndex, uint32 LodMask);

		//! Only valid between BeginUpdate and EndUpdate
		//! Calculate the description of an image, without generating it.
		UE_API UE::Tasks::TTask<FExtendedImageDesc> GetImageDesc(FInstance::FID InstanceID, FResourceID ImageId);

		//! Only valid between BeginUpdate and EndUpdate
		//! \param MipsToSkip Number of mips to skip compared from the full image.
		//! If 0, all mip levels will be generated. If more levels than possible to discard are specified, 
		//! the image will still contain a minimum number of mips specified at model compile time.
		UE_API UE::Tasks::TTask<TSharedPtr<const FImage>> GetImage(FInstance::FID InstanceID, FResourceID ImageId, int32 MipsToSkip = 0, int32 LOD = 0);

        //! Only valid between BeginUpdate and EndUpdate
		UE_API UE::Tasks::TTask<TSharedPtr<const FMesh>> GetMesh(FInstance::FID InstanceID, FResourceID MeshId, EMeshContentFlags MeshContentFilter = EMeshContentFlags::AllFlags);

		//! Only valid between BeginUpdate and EndUpdate
		//! Calculate the description of an image, without generating it.
		UE_API FExtendedImageDesc GetImageDescInline(FInstance::FID InstanceID, FResourceID ImageId);

		//! Only valid between BeginUpdate and EndUpdate
		//! \param MipsToSkip Number of mips to skip compared from the full image.
		//! If 0, all mip levels will be generated. If more levels than possible to discard are specified, 
		//! the image will still contain a minimum number of mips specified at model compile time.
		UE_API TSharedPtr<const FImage> GetImageInline(FInstance::FID InstanceID, FResourceID ImageId, int32 MipsToSkip = 0, int32 LOD = 0);

        //! Only valid between BeginUpdate and EndUpdate
		UE_API TSharedPtr<const FMesh> GetMeshInline(FInstance::FID InstanceID, FResourceID MeshId, EMeshContentFlags MeshContentFilter = EMeshContentFlags::AllFlags);

        //! Invalidate and free the last Instance data returned by a call to BeginUpdate with
        //! the same instance index. After a call to this method, that Instance cannot be used any
        //! more and its content is undefined.
        //! \param instance The index of the instance whose last data will be invalidated.
        UE_API void EndUpdate(FInstance::FID InstanceID);

        //! Completely destroy an instance. After a call to this method the given instance cannot be
        //! updated any more, and its resources may have been freed.
        //! \param instance The id of the instance to destroy.
        UE_API void ReleaseInstance(FInstance::FID InstanceID);

		//! Calculate the relevancy of every parameter. Some parameters may be unused depending on
		//! the values of other parameters. This method will set to true the flags for parameters
		//! that are relevant, and to false otherwise. This is useful to hide irrelevant parameters
		//! in dynamic user interfaces.
        //! \param pModel The model used to create the FParameters instance.
        //! \param pParameters Parameter set that we want to find the relevancy of.
        //! \param pFlags is a pointer to a preallocated array of booleans that contains at least
		//! pParameters->GetCount() elements.
        UE_API void GetParameterRelevancy( FInstance::FID InstanceID,
									const TSharedPtr<const FParameters>& FParameters,
									bool* Flags );

		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;

		UE_API Private* GetPrivate() const;

    public:

        // Prevent copy, move and assignment.
        FSystem( const FSystem& ) = delete;
		FSystem& operator=( const FSystem& ) = delete;
        FSystem( FSystem&& ) = delete;
        FSystem& operator=( FSystem&& ) = delete;

		UE_API ~FSystem();

	private:

		Private* m_pD;

	};


}

#undef UE_API
