// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuR/Model.h"

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "HAL/LowLevelMemTracker.h"
#include "HAL/PlatformMath.h"
#include "Misc/AssertionMacros.h"
#include "MuR/Image.h"
#include "MuR/Mesh.h"
#include "MuR/ModelPrivate.h"
#include "MuR/Operations.h"
#include "MuR/ParametersPrivate.h"
#include "MuR/Serialisation.h"
#include "MuR/SerialisationPrivate.h"
#include "MuR/System.h"
#include "MuR/Types.h"
#include "Templates/Tuple.h"


namespace mu
{
	MUTABLE_IMPLEMENT_POD_SERIALISABLE(FRomDataRuntime);
	MUTABLE_IMPLEMENT_POD_SERIALISABLE(FRomDataCompile);
	MUTABLE_IMPLEMENT_POD_SERIALISABLE(FImageLODRange);
	MUTABLE_IMPLEMENT_POD_SERIALISABLE(FMeshContentRange);
	MUTABLE_IMPLEMENT_POD_SERIALISABLE(FConstantResourceIndex);
	MUTABLE_IMPLEMENT_POD_VECTOR_SERIALISABLE(FConstantResourceIndex);

	
    //---------------------------------------------------------------------------------------------
    void FProgram::Check()
    {
    #ifdef MUTABLE_DEBUG
		// Insert debug checks here.
    #endif
    }


    //---------------------------------------------------------------------------------------------
    void FProgram::LogHistogram() const
    {
#if 0
        uint64 countPerType[(int32)EOpType::COUNT];
        mutable_memset(countPerType,0,sizeof(countPerType));

        for ( const uint32& o: OpAddress )
        {
            EOpType type = GetOpType(o);
            countPerType[(int32)type]++;
        }

		TArray< TPair<uint64,EOpType> > sorted((int32)EOpType::COUNT);
        for (int32 i=0; i<(int32)EOpType::COUNT; ++i)
        {
            sorted[i].second = (EOpType)i;
            sorted[i].first = countPerType[i];
        }

        std::sort(sorted.begin(),sorted.end(), []( const pair<uint64,EOpType>& a, const pair<uint64,EOpType>& b )
        {
            return a.first>b.first;
        });

        UE_LOG(LogMutableCore,Log, TEXT("Op histogram (%llu ops):"), OpAddress.Num());
        for(int32 i=0; i<8; ++i)
        {
            float p = sorted[i].first/float(OpAddress.Num())*100.0f;
            UE_LOG(LogMutableCore,Log, TEXT("  %3.2f%% : %d"), p, (int32)sorted[i].second );
        }
#endif
    }

	
	//---------------------------------------------------------------------------------------------
    void FModel::Private::UnloadRoms()
    {
	    for (int32 RomIndex = 0; RomIndex < Program.Roms.Num(); ++RomIndex)
	    {
		    Program.UnloadRom(RomIndex);
	    }
    }
	

    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    FModel::FModel()
    {
        m_pD = new Private();
    }


    //---------------------------------------------------------------------------------------------
    FModel::~FModel()
    {
		MUTABLE_CPUPROFILER_SCOPE(ModelDestructor);

        check( m_pD );
        delete m_pD;
        m_pD = 0;
    }


    //---------------------------------------------------------------------------------------------
    void FModel::Serialise( const FModel* p, FOutputArchive& arch )
    {
		LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));

        arch << *p->m_pD;
    }

    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    class FOutputModelStream : public FOutputStream
    {
    public:

        // Life cycle
        FOutputModelStream(FModelWriter* InStreamer )
            : Streamer( InStreamer )
        {
        }

        // FInputStream interface
        void Write( const void* Data, uint64 Size ) override
        {
            Streamer->Write( Data, Size );
        }

    private:

        FModelWriter* Streamer;
    };


    //---------------------------------------------------------------------------------------------
    void FModel::Serialise( FModel* p, FModelWriter& streamer, bool bDropData )
    {
		LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));

		mu::FProgram& Program = p->m_pD->Program;

		FOutputMemoryStream MemStream(16 * 1024 * 1024);

		// Save images and unload from memory
		for (TPair<uint32, TSharedPtr<const FImage>>& Entry: Program.ConstantImageLODsStreamed )
		{
			int32 RomIndex = Entry.Key;
			const FRomDataRuntime& RomData = Program.Roms[RomIndex];
			check(RomData.ResourceType == uint32(ERomDataType::Image));

			// Serialize to memory, to find out final size of this rom
			MemStream.Reset();
			FOutputArchive MemoryArch(&MemStream);
			FImage::Serialise(Entry.Value.Get(), MemoryArch);
			check(RomData.Size == MemStream.GetBufferSize());

			streamer.OpenWriteFile(RomIndex, true);
			streamer.Write(MemStream.GetBuffer(), MemStream.GetBufferSize());
			streamer.CloseWriteFile();

			// Do this progressively to avoid duplicating all data in memory.
			if (bDropData)
			{
				Entry.Value.Reset();
			}
		}

		if (bDropData)
		{
			Program.ConstantImageLODsStreamed.Empty(0);
		}

		// Save meshes and unload from memory
		for (TPair<uint32, TSharedPtr<const FMesh>>& ResData : Program.ConstantMeshesStreamed)
		{
			int32 RomIndex = ResData.Key;
			const FRomDataRuntime& RomData = Program.Roms[RomIndex];
			check(RomData.ResourceType == uint32(ERomDataType::Mesh));

			// Serialize to memory, to find out final size of this rom
			MemStream.Reset();
			FOutputArchive MemoryArch(&MemStream);
			FMesh::Serialise(ResData.Value.Get(), MemoryArch);
			check(RomData.Size == MemStream.GetBufferSize());

			streamer.OpenWriteFile(RomIndex, true);
			streamer.Write(MemStream.GetBuffer(), MemStream.GetBufferSize());
			streamer.CloseWriteFile();

			// Do this progressively to avoid duplicating all data in memory.
			if (bDropData)
			{
				ResData.Value.Reset();
			}
		}

		if (bDropData)
		{
			Program.ConstantMeshesStreamed.Empty(0);
		}

		// Store the main data of the model
		{
			streamer.OpenWriteFile(0, false);
			FOutputModelStream stream(&streamer);
			FOutputArchive arch(&stream);

			arch << *p->m_pD;

			streamer.CloseWriteFile();
		}
	}


	bool FModel::HasExternalData() const
    {
		return m_pD->Program.Roms.Num() > 0;
    }

#if WITH_EDITOR
	bool FModel::IsValid() const
	{
		return m_pD->Program.bIsValid;
	}


	void FModel::Invalidate()
    {
		m_pD->Program.bIsValid = false;
    }
#endif


	void FModel::UnloadExternalData()
    {
		LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));

		m_pD->Program.ConstantImageLODsStreamed.Empty(0);
		m_pD->Program.ConstantMeshesStreamed.Empty(0);
	}


	TSharedPtr<FModel> FModel::StaticUnserialise( FInputArchive& arch )
    {
		LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));

		TSharedPtr<FModel> pResult = MakeShared<FModel>();
        arch >> *pResult->m_pD;
        return pResult;
    }


	FModel::Private* FModel::GetPrivate() const
    {
        return m_pD;
    }


    bool FModel::GetBoolDefaultValue(int32 Index) const
    {
    	check(m_pD->Program.Parameters.IsValidIndex(Index));
		check(m_pD->Program.Parameters[Index].Type == EParameterType::Bool);

        // Early out in case of invalid parameters
        if (!m_pD->Program.Parameters.IsValidIndex(Index) ||
            m_pD->Program.Parameters[Index].Type != EParameterType::Bool)
        {
            return false;
        }
		
        return m_pD->Program.Parameters[Index].DefaultValue.Get<FParamBoolType>();
    }


	int32 FModel::GetIntDefaultValue(int32 Index) const
	{
		check(m_pD->Program.Parameters.IsValidIndex(Index));
		check(m_pD->Program.Parameters[Index].Type == EParameterType::Int);

        // Early out in case of invalid parameters
        if (!m_pD->Program.Parameters.IsValidIndex(Index) ||
            m_pD->Program.Parameters[Index].Type != EParameterType::Int)
        {
            return 0;
        }
		
        return m_pD->Program.Parameters[Index].DefaultValue.Get<FParamIntType>();
	}


	float FModel::GetFloatDefaultValue(int32 Index) const
	{
    	check(m_pD->Program.Parameters.IsValidIndex(Index));
		check(m_pD->Program.Parameters[Index].Type == EParameterType::Float);

        // Early out in case of invalid parameters
        if (!m_pD->Program.Parameters.IsValidIndex(Index) ||
            m_pD->Program.Parameters[Index].Type != EParameterType::Float)
        {
            return 0.0f;
        }
		
        return m_pD->Program.Parameters[Index].DefaultValue.Get<FParamFloatType>();
	}


	void FModel::GetColourDefaultValue(int32 Index, FVector4f& OutValue) const
    {
    	check(m_pD->Program.Parameters.IsValidIndex(Index));
		check(m_pD->Program.Parameters[Index].Type == EParameterType::Color);

        // Early out in case of invalid parameters
        if (!m_pD->Program.Parameters.IsValidIndex(Index) ||
            m_pD->Program.Parameters[Index].Type != EParameterType::Color)
        {
            return;
        }

        FParamColorType& Color = m_pD->Program.Parameters[Index].DefaultValue.Get<FParamColorType>();
		
		OutValue = Color;
    }


	FMatrix44f FModel::GetMatrixDefaultValue(int32 Index) const
	{
    	check(m_pD->Program.Parameters.IsValidIndex(Index));
    	check(m_pD->Program.Parameters[Index].Type == EParameterType::Matrix);

    	// Early out in case of invalid parameters
    	if (!m_pD->Program.Parameters.IsValidIndex(Index) ||
			m_pD->Program.Parameters[Index].Type != EParameterType::Matrix)
    	{
    		return FMatrix44f::Identity;
    	}

    	return m_pD->Program.Parameters[Index].DefaultValue.Get<FParamMatrixType>();
	}


	void FModel::GetProjectorDefaultValue(int32 Index, EProjectorType* OutProjectionType, FVector3f* OutPos,
	                                     FVector3f* OutDir, FVector3f* OutUp, FVector3f* OutScale, float* OutProjectionAngle) const
	{
    	check(m_pD->Program.Parameters.IsValidIndex(Index));
		check(m_pD->Program.Parameters[Index].Type == EParameterType::Projector);

        // Early out in case of invalid parameters
        if (!m_pD->Program.Parameters.IsValidIndex(Index) ||
            m_pD->Program.Parameters[Index].Type != EParameterType::Projector)
        {
            return;
        }

        const FProjector& Projector = m_pD->Program.Parameters[Index].DefaultValue.Get<FParamProjectorType>();
        if (OutProjectionType) *OutProjectionType = Projector.type;
    	if (OutPos) *OutPos = Projector.position;
		if (OutDir) *OutDir = Projector.direction;
    	if (OutUp) *OutUp = Projector.up;
    	if (OutScale) *OutScale = Projector.scale;
    	if (OutProjectionAngle) *OutProjectionAngle = Projector.projectionAngle;
	}


	FName FModel::GetImageDefaultValue(int32 Index) const
    {
	    check(m_pD->Program.Parameters.IsValidIndex(Index));
		check(m_pD->Program.Parameters[Index].Type == EParameterType::Image);

        // Early out in case of invalid parameters
        if (!m_pD->Program.Parameters.IsValidIndex(Index) ||
            m_pD->Program.Parameters[Index].Type != EParameterType::Image)
        {
            return {};
        }
		
        return m_pD->Program.Parameters[Index].DefaultValue.Get<FParamAssetType>();
    }


    int32 FModel::GetRomCount() const
    {
    	return m_pD->Program.Roms.Num();
    }

#if WITH_EDITOR
	uint32 FModel::GetRomSourceId(int32 Index) const
	{
		return m_pD->Program.RomsCompileData[Index].SourceId;
	}
#endif

    uint32 FModel::GetRomSize(int32 Index) const
    {
    	return m_pD->Program.Roms[Index].Size;
    }

	bool FModel::IsMeshData(int32 Index) const
	{
		return m_pD->Program.Roms[Index].ResourceType==uint32(ERomDataType::Mesh);
	}

	bool FModel::IsRomHighRes(int32 Index) const
	{
		return m_pD->Program.Roms[Index].IsHighRes==1;
	}

    int32 FModel::GetConstantImageRomId(int32 ConstantImageIndex, int32 LODIndex) const
    {
        const FProgram& Program = m_pD->Program;
        check(Program.ConstantImages.IsValidIndex(ConstantImageIndex));
        
        FImageLODRange LODRange = Program.ConstantImages[ConstantImageIndex];

        if (LODIndex >= LODRange.LODCount) 
        {
            return -1; 
        }

        FConstantResourceIndex ResourceIndex = Program.ConstantImageLODIndices[LODRange.FirstIndex + LODIndex];
        if (!ResourceIndex.Streamable)
        {
            return -1;
        }

        return ResourceIndex.Index;
    }


    TSharedPtr<FParameters> FModel::NewParameters(TSharedPtr<const FModel> Model, const FParameters* OldParameters )
    {
		LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));

        TSharedPtr<FParameters> pRes = MakeShared<FParameters>();

        pRes->GetPrivate()->Model = Model;

		const FProgram& Program = Model->GetPrivate()->Program;
        pRes->GetPrivate()->Values.SetNum(Program.Parameters.Num());
        for ( int32 p=0; p< Program.Parameters.Num(); ++p )
        {
            pRes->GetPrivate()->Values[p] = Program.Parameters[p].DefaultValue;
        }

        // Copy old values
        if ( OldParameters )
        {
            for ( int32 p=0; p<OldParameters->GetCount(); ++p )
            {
                int32 thisP = pRes->GetPrivate()->Find( OldParameters->GetName(p) );

                if ( thisP>=0 )
                {
                    if ( OldParameters->GetType(p)==pRes->GetType(thisP) )
                    {
                        switch ( pRes->GetType(thisP) )
                        {
						case EParameterType::Bool:
                            pRes->SetBoolValue( thisP, OldParameters->GetBoolValue(p) );
                            break;

                        case EParameterType::Int:
                            pRes->SetIntValue( thisP, OldParameters->GetIntValue(p) );
                            break;

                        case EParameterType::Float:
                            pRes->SetFloatValue( thisP, OldParameters->GetFloatValue(p) );
                            break;

                        case EParameterType::Color:
                        {
                            FVector4f V;
                            OldParameters->GetColourValue( p, V );
                            pRes->SetColourValue( thisP, V );
                            break;
                        }

                        case EParameterType::Projector:
                        {
//							float m[16];
//							OldParameters->GetProjectorValue( p, m );
                            pRes->GetPrivate()->Values[thisP].Set<FParamProjectorType>(OldParameters->GetPrivate()->Values[p].Get<FParamProjectorType>());
                            break;
                        }

                        case EParameterType::Image:
                            pRes->SetImageValue( thisP, OldParameters->GetImageValue(p) );
                            break;

                        default:
                            check(false);
                            break;
                        }
                    }
                }
            }
        }

        return pRes;
    }


    //---------------------------------------------------------------------------------------------
    bool FModel::IsParameterMultidimensional(const int32 ParamIndex) const
    {
		if (m_pD->Program.Parameters.IsValidIndex(ParamIndex))
		{
			return m_pD->Program.Parameters[ParamIndex].Ranges.Num() > 0;
		}

		return false;
    }
	

    //---------------------------------------------------------------------------------------------
    int32 FModel::GetStateCount() const
    {
        return (int32)m_pD->Program.States.Num();
    }


    //---------------------------------------------------------------------------------------------
    const FString& FModel::GetStateName( int32 index ) const
    {
        const char* strRes = 0;

        if ( index>=0 && index<(int32)m_pD->Program.States.Num() )
        {
            return m_pD->Program.States[index].Name;
        }

		static FString None;
        return None;
    }


    //---------------------------------------------------------------------------------------------
    int32 FModel::FindState( const FString& Name ) const
    {
        int32 res = -1;

        for ( int32 i=0; res<0 && i<(int32)m_pD->Program.States.Num(); ++i )
        {
            if ( m_pD->Program.States[i].Name == Name )
            {
                res = i;
            }
        }

        return res;
    }


    //---------------------------------------------------------------------------------------------
    int32 FModel::GetStateParameterCount( int32 stateIndex ) const
    {
        int32 res = -1;

        if ( stateIndex>=0 && stateIndex<(int32)m_pD->Program.States.Num() )
        {
            res = (int32)m_pD->Program.States[stateIndex].m_runtimeParameters.Num();
        }

        return res;
    }


    //---------------------------------------------------------------------------------------------
    int32 FModel::GetStateParameterIndex( int32 stateIndex, int32 paramIndex ) const
    {
        int32 res = -1;

        if ( stateIndex>=0 && stateIndex<(int32)m_pD->Program.States.Num() )
        {
            const FProgram::FState& state = m_pD->Program.States[stateIndex];
            if ( paramIndex>=0 && paramIndex<(int32)state.m_runtimeParameters.Num() )
            {
                res = (int32)state.m_runtimeParameters[paramIndex];
            }
        }

        return res;
    }

}

