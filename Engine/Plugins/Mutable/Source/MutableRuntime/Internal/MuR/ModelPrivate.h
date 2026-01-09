// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Model.h"
#include "MuR/System.h"

#include "MuR/SerialisationPrivate.h"
#include "MuR/Operations.h"
#include "MuR/ExtensionData.h"
#include "MuR/Image.h"
#include "MuR/Mesh.h"
#include "MuR/ParametersPrivate.h"
#include "MuR/MutableRuntimeModule.h"

#define UE_API MUTABLERUNTIME_API

#define MUTABLE_MAX_RUNTIME_PARAMETERS_PER_STATE	65
#define MUTABLE_GROW_BORDER_VALUE					2

namespace mu
{

	/** Used to debug and log. */
	constexpr bool DebugRom = false;
	constexpr bool DebugRomAll = false;
	constexpr int32 DebugRomIndex = 44;
	constexpr int32 DebugImageIndex = 9;

	struct FConstantResourceIndex
	{
		uint32 Index : 31;
		/** This may mean that the resource needs to be looked up on a different array. */
		uint32 Streamable : 1;
	};
	static_assert(sizeof(FConstantResourceIndex) == 4);
	MUTABLE_DEFINE_POD_SERIALISABLE(FConstantResourceIndex);
	MUTABLE_DEFINE_POD_VECTOR_SERIALISABLE(FConstantResourceIndex);

	/** This is encoded with minimal bits. Make sure to review all uses if extended. */
	enum class ERomDataType : uint32
	{
		Image = 0,
		Mesh  = 1
	};

    /** Data stored for a rom even if it is not loaded.
	* This struct is size-sensitive since there may be many roms and it is always loaded in memory when a CO is.
	*/
    struct FRomDataRuntime
    {
		//! Size of the rom
		uint32 Size : 30;

		/** Index of the resource in its type-specific array. See ERomDataType. */
		uint32 ResourceType : 1;

		/** Properties of the rom data. */
		uint32 IsHighRes : 1;

		// TODO: Store the offset here and delete the FModelStreamableBlock map?
		//! Offset in file
		// uint32 Offset = 0;
    };

	/** Not critical to keep this size, but it is memory-usage sensitive. */
	static_assert(sizeof(FRomDataRuntime) == 4);
	MUTABLE_DEFINE_POD_SERIALISABLE(FRomDataRuntime);

	struct FRomDataCompile
	{
		/** ID used to identify the origin of this data and used for grouping. */
		uint32 SourceId;
	};
	MUTABLE_DEFINE_POD_SERIALISABLE(FRomDataCompile);

    //!
    template<typename DATA>
    inline void AppendCode(TArray<uint8>& Code, const DATA& Data )
    {
        int32 Pos = Code.Num();
        Code.SetNum( Pos+sizeof(DATA), EAllowShrinking::No);
		FMemory::Memcpy (&Code[Pos], &Data, sizeof(DATA));
    }

	//!
	struct FImageLODRange
	{
		int32 FirstIndex = 0;
		uint16 ImageSizeX = 0;
		uint16 ImageSizeY = 0;
		uint16 _Padding = 0;
		uint8 LODCount = 0;
		EImageFormat ImageFormat = EImageFormat::None;
	};
	MUTABLE_DEFINE_POD_SERIALISABLE(FImageLODRange);

	struct FMeshContentRange
	{
		static constexpr uint32 FirstIndexMaxBits   = 24;
		static constexpr uint32 ContentFlagsMaxBits = 32 - FirstIndexMaxBits;
		static constexpr uint32 FirstIndexBitMask   = (1 << FirstIndexMaxBits) - 1; 

		static_assert(FirstIndexMaxBits < 32);
		static_assert(ContentFlagsMaxBits >= sizeof(EMeshContentFlags)*8);

		// NOTE: Bitfields layout can be implementation defined, here we want consistency across all
		// compilers so that the struct can be POD serializable.
		uint32 FirstIndex_ContentFlags = 0; // Low bits are FirstIndex, high bits are ContentFlags.
		uint32 MeshIDPrefix = 0;

		FORCEINLINE EMeshContentFlags GetContentFlags() const
		{
			return static_cast<EMeshContentFlags>(
					(FirstIndex_ContentFlags >> FirstIndexMaxBits) &
					((1 << ContentFlagsMaxBits) - 1));
		}

		FORCEINLINE uint32 GetFirstIndex() const
		{
			return FirstIndex_ContentFlags & FirstIndexBitMask;
		}

		FORCEINLINE void SetContentFlags(EMeshContentFlags ContentFlags)
		{
			check(uint32(ContentFlags) < (1 << ContentFlagsMaxBits));

			FirstIndex_ContentFlags = 
					(FirstIndex_ContentFlags & FirstIndexBitMask) | 
					((uint32(ContentFlags) << FirstIndexMaxBits));
		}

		FORCEINLINE void SetFirstIndex(uint32 FirstIndex) 
		{
			check(FirstIndex < ((1 << FirstIndexMaxBits)));

			FirstIndex_ContentFlags = 
					(FirstIndex_ContentFlags & ~FirstIndexBitMask) | 
					(FirstIndex & FirstIndexBitMask);
		}
	};
	static_assert(sizeof(FMeshContentRange) == sizeof(uint32)*2);
	MUTABLE_DEFINE_POD_SERIALISABLE(FMeshContentRange);

	struct FExtensionDataConstant
	{
		// This should always be valid, but if the state is Unloaded it won't be usable.
		//
		// Avoid storing references to this Data in Memory while the state is Unloaded.
		TSharedPtr<const FExtensionData> Data;

		enum class ELoadState : uint8
		{
			Invalid,
			Unloaded,
			FailedToLoad,
			CurrentlyLoaded,
			AlwaysLoaded
		};

		void Serialise(FOutputArchive& Arch) const
		{
			Arch << Data;
		}
		
		void Unserialise(FInputArchive& Arch)
		{
			Arch >> Data;

			check(Data);
			check(Data->Origin == FExtensionData::EOrigin::ConstantAlwaysLoaded || Data->Origin == FExtensionData::EOrigin::ConstantStreamed);
		}
	};

    //!
    struct FProgram
    {
        FProgram()
        {
            // Add the null instruction at address 0.
            // TODO: Will do it in the linker
            AppendCode( ByteCode, EOpType::NONE );
            OpAddress.Add(0);
        }

        struct FState
        {
            //! Name of the state
            FString Name;

            //! First instruction of the full build of an instance in this state
            OP::ADDRESS Root = 0;

            //! List of parameters index (to FProgram::Parameters) of the runtime parameters of
            //! this state.
			TArray<int> m_runtimeParameters;

            //! List of instructions that need to be cached to efficiently update this state
			TArray<OP::ADDRESS> m_updateCache;

            //! List of root instructions for the dynamic resources that depend on the runtime
            //! parameters of this state, with a mask of relevant runtime parameters.
            //! The mask has a bit on for every runtime parameter in the m_runtimeParameters array.
			//! The uint64 is linked to MUTABLE_MAX_RUNTIME_PARAMETERS_PER_STATE
			TArray< TPair<OP::ADDRESS,uint64> > m_dynamicResources;

            //!
            inline void Serialise( FOutputArchive& arch ) const
            {
                arch << Name;
                arch << Root;
                arch << m_runtimeParameters;
                arch << m_updateCache;
                arch << m_dynamicResources;
            }

            //!
            inline void Unserialise( FInputArchive& arch )
            {
                arch >> Name;
                arch >> Root;
                arch >> m_runtimeParameters;
                arch >> m_updateCache;
                arch >> m_dynamicResources;
            }

            /** Returns the mask of parameters (from the runtime parameter list of this state) including the parameters that 
			* are relevant for the dynamic resource at the given address.
			*/
            uint64 IsDynamic( OP::ADDRESS at ) const
            {
                uint64 res = 0;

                for ( int32 i=0; i<m_dynamicResources.Num(); ++i )
                {
                    if ( m_dynamicResources[i].Key==at )
                    {
                        res = m_dynamicResources[i].Value;
						break;
                    }
                }

                return res;
            }

            //!
            bool IsUpdateCache( OP::ADDRESS at ) const
            {
                bool res = false;

                for (int32 i=0; !res && i<m_updateCache.Num(); ++i )
                {
                    if ( m_updateCache[i]==at )
                    {
                        res = true;
                    }
                }

                return res;
            }

            //!
            void AddUpdateCache( OP::ADDRESS at )
            {
                if ( !IsUpdateCache(at) )
                {
                    m_updateCache.Add( at );
                }
            }
        };

        //! Location in the ByteCode of the beginning of each operation
		TArray<uint32> OpAddress;

        //! Byte-coded representation of the program, using variable-sized op data.
		TArray<uint8> ByteCode;

        //!
		TArray<FState> States;

		/** Data for every rom required in-game. */
		TArray<FRomDataRuntime> Roms;

		/** Data for every rom required at compile-time. It is empty in cooked data. */
		TArray<FRomDataCompile> RomsCompileData;
	
		/** Loaded roms worth tracking (only images and meshes for now). Stores the rom's data type.*/
		TSparseArray<uint8> LoadedMemTrackedRoms;

		/** Constant image mip data is split in 2 sets: ConstantImageLODsPermanent constains data that is always loaded. 
		* Index with FConstantResourceIndex::Index, when Streamable is 0.
		*/
		TArray<TSharedPtr<const FImage>> ConstantImageLODsPermanent;

		/** Constant image mip data is split in 2 sets: ConstantImageLODsStreamed constains data that is streamed in and out. 
		* Index with FConstantResourceIndex::Index, when Streamable is 1.
		* This part is empty for an unused Model, and shouldn't be serialised.
		*/
		TMap<uint32, TSharedPtr<const FImage>> ConstantImageLODsStreamed;

		//! Constant image mip chain indices: ranges in this array are defined in FImageLODRange and the indices here refer to ConstantImageLODs.
		TArray<FConstantResourceIndex> ConstantImageLODIndices;

		//! Constant image data.
		TArray<FImageLODRange> ConstantImages;

		/** Constant mesh content indices: ranges in this array are defined in FMeshContentRange and the indices here refer to ConstantMeshes. */
		TArray<FConstantResourceIndex> ConstantMeshContentIndices;
		
		/** Constant mesh data */
		TArray<FMeshContentRange> ConstantMeshes;

		/** Constant mesh data is split in 2 sets: ConstantMeshesPermanent constains data that is always loaded.
		* Index with FConstantResourceIndex::Index, when Streamable is 0.
		*/
		TArray<TSharedPtr<const FMesh>> ConstantMeshesPermanent;

		/** Constant mesh data is split in 2 sets: ConstantMeshesStreamed constains data that is streamed in and out.
		* Index with FConstantResourceIndex::Index, when Streamable is 1.
		* This part is empty for an unused Model, and shouldn't be serialised.
		*/
		TMap<uint32, TSharedPtr<const FMesh>> ConstantMeshesStreamed;

		//! Constant FExtensionData
		TArray<FExtensionDataConstant> ConstantExtensionData;

        //! Constant string data
		TArray<FString> ConstantStrings;

        //! Constant layout data
		TArray<TSharedPtr<const FLayout>> ConstantLayouts;

        //! Constant projectors
		TArray<FProjector> ConstantProjectors;

        //! Constant matrices, usually used for transforms
		TArray<FMatrix44f> ConstantMatrices;

		//! Constant shapes
		TArray<FShape> ConstantShapes;

        //! Constant curves
		TArray<FRichCurve> ConstantCurves;

        //! Constant skeletons
		TArray<TSharedPtr<const FSkeleton>> ConstantSkeletons;

		//! Constant Physics Bodies
		TArray<TSharedPtr<const FPhysicsBody>> ConstantPhysicsBodies;

        //! FParameters of the model.
        //! The value stored here is the default value.
		TArray<FParameterDesc> Parameters;

        //! Ranges for iteration of the model operations.
		TArray<FRangeDesc> Ranges;

        //! List of parameter lists. These are used in several places, like storing the
        //! pregenerated list of parameters influencing a resource.
        //! The parameter lists are sorted.
		TArray<TArray<uint16>> ParameterLists;

#if WITH_EDITOR
		//! State of the program. True unless the streamed resources were destroyed,
		//! which could happen in the editor after recompiling the CO.
		bool bIsValid = true;
#endif
        //!
        void Serialise( FOutputArchive& arch ) const
        {
            arch << OpAddress;
            arch << ByteCode;
            arch << States;
			arch << Roms;
			arch << RomsCompileData;
			arch << ConstantImageLODsPermanent;
			arch << ConstantImageLODIndices;
			arch << ConstantImages;
			arch << ConstantMeshesPermanent;
			arch << ConstantMeshContentIndices;
			arch << ConstantMeshes;
			arch << ConstantExtensionData;
			arch << ConstantStrings;
            arch << ConstantLayouts;
            arch << ConstantProjectors;
			arch << ConstantMatrices;
			arch << ConstantShapes;
            arch << ConstantCurves;
            arch << ConstantSkeletons;
			arch << ConstantPhysicsBodies;
            arch << Parameters;
            arch << Ranges;
            arch << ParameterLists;
        }

        //!
        void Unserialise( FInputArchive& arch )
        {
            arch >> OpAddress;
            arch >> ByteCode;
            arch >> States;
			arch >> Roms;
			arch >> RomsCompileData;
			arch >> ConstantImageLODsPermanent;
			arch >> ConstantImageLODIndices;
			arch >> ConstantImages;
			arch >> ConstantMeshesPermanent;
			arch >> ConstantMeshContentIndices;
			arch >> ConstantMeshes;
			arch >> ConstantExtensionData;
			arch >> ConstantStrings;
            arch >> ConstantLayouts;
            arch >> ConstantProjectors;
			arch >> ConstantMatrices;
			arch >> ConstantShapes;
            arch >> ConstantCurves;
            arch >> ConstantSkeletons;
			arch >> ConstantPhysicsBodies;
            arch >> Parameters;
            arch >> Ranges;
            arch >> ParameterLists;
        }

        //! Debug method that sanity-checks the program with a variety of tests.
        void Check();

        //! Debug method that logs the top used instruction types.
        void LogHistogram() const;

		//! Return true if the given ROM is loaded.
		FORCEINLINE bool IsRomLoaded(int32 RomIndex) const
		{
			switch (Roms[RomIndex].ResourceType)
			{
				case uint32(ERomDataType::Image):
					return ConstantImageLODsStreamed.Contains(RomIndex);
				case uint32(ERomDataType::Mesh):
					return ConstantMeshesStreamed.Contains(RomIndex);
				default:
					check(false);
					break;
			}

			return false;
		}

		/** Unload a rom resource. 
		* If a pointer to a size is passed in OutDataSize, the value will be set with the size of the unloaded rom.
		*/
		FORCEINLINE void UnloadRom(int32 RomIndex, int32* OutDataSize=nullptr)
		{
			if (DebugRom && (DebugRomAll||RomIndex == DebugRomIndex))
				UE_LOG(LogMutableCore, Log, TEXT("Unloading rom %d."), RomIndex);

			if (LoadedMemTrackedRoms.IsValidIndex(RomIndex))
			{
				LoadedMemTrackedRoms.RemoveAt(RomIndex);
			}

			switch (Roms[RomIndex].ResourceType)
			{
			case uint32(ERomDataType::Image):
			{
				TSharedPtr<const FImage> Data;
				ConstantImageLODsStreamed.RemoveAndCopyValue(RomIndex, Data);
				if (OutDataSize && Data)
				{
					*OutDataSize = Data->GetDataSize();
				}
				break;
			}
			case uint32(ERomDataType::Mesh):
			{
				TSharedPtr<const FMesh> Data;
				ConstantMeshesStreamed.RemoveAndCopyValue(RomIndex, Data);
				if (OutDataSize && Data)
				{
					*OutDataSize = Data->GetDataSize();
				}
				break;
			}
			default:
				check(false);
				break;
			}
		}

		FORCEINLINE void SetMeshRomValue(uint32 RomIndex, const TSharedPtr<FMesh>& Value)
		{
			check(Roms[RomIndex].ResourceType == uint32(ERomDataType::Mesh));
			check(!ConstantMeshesStreamed.Contains(RomIndex));

			LoadedMemTrackedRoms.EmplaceAt(RomIndex, (uint8)Roms[RomIndex].ResourceType);
			ConstantMeshesStreamed.Add(RomIndex, Value);
		}

		FORCEINLINE void SetImageRomValue(uint32 RomIndex, const TSharedPtr<FImage>& Value)
		{
			check(Roms[RomIndex].ResourceType == uint32(ERomDataType::Image));
			check(!ConstantImageLODsStreamed.Contains(RomIndex));

			LoadedMemTrackedRoms.EmplaceAt(RomIndex, (uint8)Roms[RomIndex].ResourceType);
			ConstantImageLODsStreamed.Add(RomIndex, Value);
		}

		OP::ADDRESS AddConstant(TSharedPtr<const FExtensionData> Data)
		{
			// Ensure unique
			for (int32 Index = 0; Index < ConstantExtensionData.Num(); Index++)
			{
				const FExtensionData* Candidate = ConstantExtensionData[Index].Data.Get();
				if (*Candidate == *Data)
				{
					return Index;
				}
			}

			FExtensionDataConstant& NewConstant = ConstantExtensionData.AddDefaulted_GetRef();
			NewConstant.Data = Data;

			return ConstantExtensionData.Num() - 1;
		}

		OP::ADDRESS AddConstant( TSharedPtr<const FLayout> pLayout )
        {
            // Ensure unique
            for (int32 i=0; i<ConstantLayouts.Num(); ++i)
            {
                if (ConstantLayouts[i]==pLayout)
                {
                    return (OP::ADDRESS)i;
                }
            }

            OP::ADDRESS index = OP::ADDRESS( ConstantLayouts.Num() );
            ConstantLayouts.Add( pLayout );
            return index;
        }

        OP::ADDRESS AddConstant( TSharedPtr<const FSkeleton> Skeleton )
        {
            // Ensure unique
            for ( int32 i=0; i<ConstantSkeletons.Num(); ++i)
            {
                if ( ConstantSkeletons[i]== Skeleton
                     ||
                     *ConstantSkeletons[i]==*Skeleton
                     )
                {
                    return (OP::ADDRESS)i;
                }
            }

            OP::ADDRESS index = OP::ADDRESS( ConstantSkeletons.Num() );
            ConstantSkeletons.Add(Skeleton);
            return index;
        }

        OP::ADDRESS AddConstant(TSharedPtr<const FPhysicsBody> pPhysicsBody )
        {
            // Ensure unique
            for (int32 i=0; i<ConstantPhysicsBodies.Num(); ++i)
            {
                if ( ConstantPhysicsBodies[i]==pPhysicsBody
                     ||
                     *ConstantPhysicsBodies[i]==*pPhysicsBody
                     )
                {
                    return (OP::ADDRESS)i;
                }
            }

            OP::ADDRESS index = OP::ADDRESS( ConstantPhysicsBodies.Num() );
            ConstantPhysicsBodies.Add( pPhysicsBody );
            return index;
        }

        OP::ADDRESS AddConstant( const FString& str )
        {            
            // Ensure unique
            for (int32 i=0; i<ConstantStrings.Num(); ++i)
            {
                if (ConstantStrings[i]==str)
                {
                    return (OP::ADDRESS)i;
                }
            }

            OP::ADDRESS index = OP::ADDRESS( ConstantStrings.Num() );
            ConstantStrings.Add( str );
            return index;
        }

        OP::ADDRESS AddConstant( const FMatrix44f& m )
        {
            // Ensure unique
            for (int32 i=0; i<ConstantMatrices.Num(); ++i)
            {
                if (ConstantMatrices[i]==m)
                {
                    return (OP::ADDRESS)i;
                }
            }

            OP::ADDRESS index = OP::ADDRESS( ConstantMatrices.Num() );
            ConstantMatrices.Add( m );
            return index;
        }

        OP::ADDRESS AddConstant( const FShape& m )
        {
            // Ensure unique
            for (int32 i=0; i<ConstantShapes.Num(); ++i)
            {
                if (ConstantShapes[i]==m)
                {
                    return (OP::ADDRESS)i;
                }
            }

            OP::ADDRESS index = OP::ADDRESS( ConstantShapes.Num() );
            ConstantShapes.Add( m );
            return index;
        }

        OP::ADDRESS AddConstant( const FProjector& m )
        {
            // Ensure unique
            for (int32 i=0; i<ConstantProjectors.Num(); ++i)
            {
                if (ConstantProjectors[i]==m)
                {
                    return (OP::ADDRESS)i;
                }
            }

            OP::ADDRESS index = OP::ADDRESS( ConstantProjectors.Num() );
            ConstantProjectors.Add( m );
            return index;
        }

        OP::ADDRESS AddConstant( const FRichCurve& m )
        {
            OP::ADDRESS index = OP::ADDRESS( ConstantCurves.AddUnique( m ) );
            return index;
        }


		inline TSharedPtr<const FImage> GetImageLOD(FConstantResourceIndex Index) const
		{
			TSharedPtr<const FImage> Mip;
			if (!Index.Streamable)
			{
				if (ConstantImageLODsPermanent.IsValidIndex(Index.Index))
				{
					Mip = ConstantImageLODsPermanent[Index.Index];
				}
			}
			else
			{
				const TSharedPtr<const FImage>* Found = ConstantImageLODsStreamed.Find(Index.Index);
				if (Found)
				{
					Mip = *Found;
				}
			}

			return Mip;
		}


        //! Get a constant image, assuming at least some mips are loaded. The image constant will be composed with lodaded mips if necessary.
		template <typename CreateImageFunc>
        void GetConstant( uint32 ConstantIndex, TSharedPtr<const FImage>& res, int32 MipsToSkip, const CreateImageFunc& CreateImage) const
        {
			int32 ReallySkippedLODs = FMath::Min(ConstantImages[ConstantIndex].LODCount - 1, MipsToSkip);
			int32 FirstLODIndexIndex = ConstantImages[ConstantIndex].FirstIndex;

			// Get the first mip
			int32 ResultLODIndexIndex = FirstLODIndexIndex + ReallySkippedLODs;
			FConstantResourceIndex ResultLODIndex = ConstantImageLODIndices[ResultLODIndexIndex];
			TSharedPtr<const FImage> CurrentMip = GetImageLOD(ResultLODIndex);

			// We may need to skip more LODs if they are not loaded
			while (!CurrentMip)
			{
				++ReallySkippedLODs;
				
				if (ReallySkippedLODs >= ConstantImages[ConstantIndex].LODCount)
				{
					// We don't have a single mip loaded for the image that was requested
					ensure(false);
					break;
				}

				++ResultLODIndexIndex;
				ResultLODIndex = ConstantImageLODIndices[ResultLODIndexIndex];
				CurrentMip = GetImageLOD(ResultLODIndex);
			}

			int32 FinalLODs = ConstantImages[ConstantIndex].LODCount - ReallySkippedLODs;
			check(FinalLODs > 0);

			// Shortcut if we only want one mip
			if (FinalLODs == 1)
			{
				res = CurrentMip;
				return;
			}

			// Compose the result image
			{
				MUTABLE_CPUPROFILER_SCOPE(ComposeConstantImage);

				TSharedPtr<FImage> Result = CreateImage(CurrentMip->GetSizeX(), CurrentMip->GetSizeY(), FinalLODs, CurrentMip->GetFormat(), EInitializationType::NotInitialized);
				Result->Flags = CurrentMip->Flags;

				// Some non-block pixel formats require separate memory size calculation
				if (Result->DataStorage.IsEmpty())
				{
					for (int32 LOD = 0; LOD < FinalLODs; ++LOD)
					{
						FConstantResourceIndex LODIndex = ConstantImageLODIndices[ResultLODIndexIndex + LOD];

						TSharedPtr<const FImage> Image = GetImageLOD(LODIndex);

						// This could happen in the case of missing data files
						if (!Image)
						{
							break;
						}

						int32 MipSizeBytes = Image->GetLODDataSize(0);
						Result->DataStorage.ResizeLOD(LOD, MipSizeBytes);
					}
				}

				for (int32 LOD = 0; LOD < FinalLODs; ++LOD)
				{
					check(CurrentMip->GetLODCount() == 1);
					check(CurrentMip->GetFormat() == Result->GetFormat());

					TArrayView<uint8> ResultLODView = Result->DataStorage.GetLOD(LOD);
					TArrayView<const uint8> CurrentMipView = CurrentMip->DataStorage.GetLOD(0);
					
					check(CurrentMipView.Num() == ResultLODView.Num());

					FMemory::Memcpy(ResultLODView.GetData(), CurrentMipView.GetData(), ResultLODView.Num());

					if (LOD + 1 < FinalLODs)
					{
						ResultLODIndex = ConstantImageLODIndices[ResultLODIndexIndex + LOD + 1];
						CurrentMip = GetImageLOD(ResultLODIndex);

						// This could only happen if missing or corrupted data.
						if (!CurrentMip)
						{
							break;
						}
					}
				}

				res = Result;
			}
		}
		
		template<class CreateMeshFunc>
        void GetConstant(
				uint32 MeshConstantIndex, 
				int32 SkeletonConstantIndex, 
				int32 PhysicsConstantIndex, 
				TSharedPtr<const FMesh>& OutMesh, 
				EMeshContentFlags FilterContentFlags, 
				CreateMeshFunc&& CreateMesh) const
        {
			MUTABLE_CPUPROFILER_SCOPE(GetConstant_Mesh)

			FMeshContentRange MeshContentRange = ConstantMeshes[MeshConstantIndex];

			TSharedPtr<const FMesh> EmptyMesh = nullptr;
			auto GetMeshAtResourceIndex = [&](FConstantResourceIndex ResourceIndex) -> TSharedPtr<const FMesh>
			{
				MUTABLE_CPUPROFILER_SCOPE(GetConstant_Mesh_GetMesh)

				if (!ResourceIndex.Streamable)
				{
					if (ConstantMeshesPermanent.IsValidIndex(ResourceIndex.Index))
					{
						return ConstantMeshesPermanent[ResourceIndex.Index];
					}

					if (!EmptyMesh)
					{
						EmptyMesh = MakeShared<FMesh>();
					}
					
					return EmptyMesh;
				}
				else
				{
					const TSharedPtr<const FMesh>* Found = ConstantMeshesStreamed.Find(ResourceIndex.Index);
					if (Found)
					{
						return *Found;
					}

					if (!EmptyMesh)
					{
						EmptyMesh = MakeShared<FMesh>();
					}

					return EmptyMesh;
				}
			};

			TSharedPtr<const FMesh> GeometryMesh = nullptr;
			TSharedPtr<const FMesh> PoseMesh     = nullptr;
			TSharedPtr<const FMesh> PhysicsMesh  = nullptr;
			TSharedPtr<const FMesh> MetaDataMesh = nullptr;

			int32 MeshRomCurrentIndex = MeshContentRange.GetFirstIndex();
			if (EnumHasAnyFlags(FilterContentFlags, EMeshContentFlags::GeometryData) &&
				EnumHasAnyFlags(MeshContentRange.GetContentFlags(), EMeshContentFlags::GeometryData))
			{
				const FConstantResourceIndex ResourceIndex = ConstantMeshContentIndices[MeshRomCurrentIndex];
				GeometryMesh = GetMeshAtResourceIndex(ResourceIndex);
			}
			MeshRomCurrentIndex += (int32)EnumHasAnyFlags(MeshContentRange.GetContentFlags(), EMeshContentFlags::GeometryData);

			if (EnumHasAnyFlags(FilterContentFlags, EMeshContentFlags::PoseData) &&
				EnumHasAnyFlags(MeshContentRange.GetContentFlags(), EMeshContentFlags::PoseData))
			{
				const FConstantResourceIndex ResourceIndex = ConstantMeshContentIndices[MeshRomCurrentIndex];
				PoseMesh = GetMeshAtResourceIndex(ResourceIndex);
			}
			MeshRomCurrentIndex += (int32)EnumHasAnyFlags(MeshContentRange.GetContentFlags(), EMeshContentFlags::PoseData);

			if (EnumHasAnyFlags(FilterContentFlags, EMeshContentFlags::PhysicsData) &&
				EnumHasAnyFlags(MeshContentRange.GetContentFlags(), EMeshContentFlags::PhysicsData))
			{
				const FConstantResourceIndex ResourceIndex = ConstantMeshContentIndices[MeshRomCurrentIndex];
				PhysicsMesh = GetMeshAtResourceIndex(ResourceIndex);
			}
			MeshRomCurrentIndex += (int32)EnumHasAnyFlags(MeshContentRange.GetContentFlags(), EMeshContentFlags::PhysicsData);

			if (EnumHasAnyFlags(FilterContentFlags, EMeshContentFlags::MetaData) &&
				EnumHasAnyFlags(MeshContentRange.GetContentFlags(), EMeshContentFlags::MetaData))
			{
				const FConstantResourceIndex ResourceIndex = ConstantMeshContentIndices[MeshRomCurrentIndex];
				MetaDataMesh = GetMeshAtResourceIndex(ResourceIndex);
			}
			MeshRomCurrentIndex += (int32)EnumHasAnyFlags(MeshContentRange.GetContentFlags(), EMeshContentFlags::MetaData);

			check(MeshRomCurrentIndex - MeshContentRange.GetFirstIndex() == FMath::CountBits((uint64)MeshContentRange.GetContentFlags()));

			uint32 MeshBudgetReserve = 0;
			MeshBudgetReserve += GeometryMesh ? GeometryMesh->GetDataSize() : 0;
			MeshBudgetReserve += PoseMesh     ? PoseMesh->GetDataSize()     : 0;
			MeshBudgetReserve += PhysicsMesh  ? PhysicsMesh->GetDataSize()  : 0;
			MeshBudgetReserve += MetaDataMesh ? MetaDataMesh->GetDataSize() : 0;

			TSharedPtr<FMesh> Result = nullptr;
			if (GeometryMesh)
			{
				MUTABLE_CPUPROFILER_SCOPE(GetConstant_Mesh_Geoemtry)

				Result = CreateMesh(MeshBudgetReserve);
				Result->CopyFrom(*GeometryMesh);
			}

			if (PoseMesh)
			{
				MUTABLE_CPUPROFILER_SCOPE(GetConstant_Mesh_Pose)

				if (!Result)
				{
					Result = CreateMesh(MeshBudgetReserve);
					Result->CopyFrom(*PoseMesh);
				}
				else
				{
					Result->BonePoses = PoseMesh->BonePoses; 
					Result->BoneMap   = PoseMesh->BoneMap;

					for (const TPair<EMeshBufferType, FMeshBufferSet>& AdditionalBuffer : PoseMesh->AdditionalBuffers)
					{
						const bool bIsPoseBufferType = 	
								AdditionalBuffer.Key == EMeshBufferType::SkeletonDeformBinding;

						if (bIsPoseBufferType)
						{
							Result->AdditionalBuffers.Emplace(AdditionalBuffer);
						}
					}	
				}
			}

			if (PhysicsMesh)
			{
				MUTABLE_CPUPROFILER_SCOPE(GetConstant_Mesh_Physics)
				
				if (!Result)
				{
					Result = CreateMesh(MeshBudgetReserve);
					Result->CopyFrom(*PhysicsMesh);
				}
				else
				{
					for (const TPair<EMeshBufferType, FMeshBufferSet>& AdditionalBuffer : PhysicsMesh->AdditionalBuffers)
					{
						const bool bIsPhysicsBufferType = 
								AdditionalBuffer.Key == EMeshBufferType::PhysicsBodyDeformBinding   ||
								AdditionalBuffer.Key == EMeshBufferType::PhysicsBodyDeformSelection ||
								AdditionalBuffer.Key == EMeshBufferType::PhysicsBodyDeformOffsets;

						if (bIsPhysicsBufferType)
						{
							Result->AdditionalBuffers.Emplace(AdditionalBuffer);
						}
					}
				}
			}

			if (MetaDataMesh)
			{
				MUTABLE_CPUPROFILER_SCOPE(GetConstant_Mesh_Metadata)

				if (!Result)
				{
					Result = CreateMesh(MeshBudgetReserve);
					Result->CopyFrom(*MetaDataMesh);
				}
				else
				{
					// Only in case the geoemtry has been filtered, add the meta data descriptors.
					if (EnumHasAnyFlags(MeshContentRange.GetContentFlags(), EMeshContentFlags::GeometryData) &&
						!EnumHasAnyFlags(FilterContentFlags, EMeshContentFlags::GeometryData))
					{
						check(MetaDataMesh->VertexBuffers.IsDescriptor());
						check(MetaDataMesh->IndexBuffers.IsDescriptor());

						Result->VertexBuffers = MetaDataMesh->VertexBuffers;
						Result->IndexBuffers = MetaDataMesh->IndexBuffers;
						Result->Surfaces = MetaDataMesh->Surfaces;
					}

					Result->Tags = MetaDataMesh->Tags; 
					Result->SkeletonIDs = MetaDataMesh->SkeletonIDs; 
					Result->StreamedResources = MetaDataMesh->StreamedResources; 
				}
			}

			Result->MeshIDPrefix = MeshContentRange.MeshIDPrefix;

			check(ConstantSkeletons.Num() > SkeletonConstantIndex);
			if (ConstantSkeletons.IsValidIndex(SkeletonConstantIndex))
			{
				Result->Skeleton = ConstantSkeletons[SkeletonConstantIndex];
			}

			check(ConstantPhysicsBodies.Num() > PhysicsConstantIndex);
			if (ConstantPhysicsBodies.IsValidIndex(PhysicsConstantIndex))
			{
				Result->PhysicsBody = ConstantPhysicsBodies[PhysicsConstantIndex];
			}

			OutMesh = Result;
		}

		void GetExtensionDataConstant(int32 ConstantIndex, TSharedPtr<const FExtensionData>& Result) const
		{
			const FExtensionDataConstant& Constant = ConstantExtensionData[ConstantIndex];

			check(Constant.Data);

			Result = Constant.Data;
		}

        inline EOpType GetOpType( OP::ADDRESS at ) const
        {
            if (at>=OP::ADDRESS(OpAddress.Num())) return EOpType::NONE;

            EOpType result;
            uint64 byteCodeAddress = OpAddress[at];
			FMemory::Memcpy( &result, &ByteCode[byteCodeAddress], sizeof(EOpType) );
            check( result<EOpType::COUNT);
            return result;
        }

		template<typename ARGS>
		inline const ARGS GetOpArgs(OP::ADDRESS at) const
		{
			ARGS result;
			uint64 byteCodeAddress = OpAddress[at];
			byteCodeAddress += sizeof(EOpType);
			FMemory::Memcpy(&result, &ByteCode[byteCodeAddress], sizeof(ARGS));
			return result;
		}

		template<typename ARGS>
		inline void SetOpArgs(OP::ADDRESS at, const ARGS& Args)
		{
			uint64 byteCodeAddress = OpAddress[at];
			byteCodeAddress += sizeof(EOpType);
			FMemory::Memcpy(&ByteCode[byteCodeAddress], &Args, sizeof(ARGS));
		}

        inline const uint8* GetOpArgsPointer( OP::ADDRESS at ) const
        {
            uint64 byteCodeAddress = OpAddress[at];
            byteCodeAddress += sizeof(EOpType);
            const uint8* pData = (const uint8*)&ByteCode[byteCodeAddress];
            return pData;
        }

        inline uint8* GetOpArgsPointer( OP::ADDRESS at )
        {
            uint64 byteCodeAddress = OpAddress[at];
            byteCodeAddress += sizeof(EOpType);
            uint8* pData = (uint8*)&ByteCode[byteCodeAddress];
            return pData;
        }
    };


    //!
    class FModel::Private
    {
    public:

		mu::FProgram Program;

    	UE_API void UnloadRoms();

        void Serialise( FOutputArchive& arch ) const
        {
             arch << Program;
        }

        void Unserialise( FInputArchive& arch )
        {
            arch >> Program;
        }

    };

}

#undef UE_API
