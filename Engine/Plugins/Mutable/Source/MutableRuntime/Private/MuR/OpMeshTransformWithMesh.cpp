// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuR/OpMeshTransformWithMesh.h"

#include "OpMeshClipWithMesh.h"
#include "MuR/Mesh.h"
#include "MuR/MeshPrivate.h"
#include "MuR/MutableTrace.h"


namespace mu
{
void MeshTransformWithMesh(FMesh* Result, const FMesh* SourceMesh, const FMesh* BoundingMesh, const FMatrix44f& Transform, bool& bOutSuccess)
{
	MUTABLE_CPUPROFILER_SCOPE(MeshClipWithMesh);
	bOutSuccess = true;

	const uint32 VCount = SourceMesh->GetVertexBuffers().GetElementCount();
	if (!VCount)
	{
		bOutSuccess = false;
		return; // OutSuccess false indicates the SourceMesh can be reused in this case. 
	}

	Result->CopyFrom(*SourceMesh);

	// Classify which vertices in the SourceMesh are completely bounded by the BoundingMesh geometry.
	// If no BoundingMesh is provided, this defaults to act exactly like mu::MeshTransform
	TBitArray<> VertexInBoundaryMesh;  
	if (BoundingMesh)
	{
		MeshClipMeshClassifyVertices(VertexInBoundaryMesh, SourceMesh, BoundingMesh);
	}

	const FMatrix44f TransformInvT = Transform.Inverse().GetTransposed();

    const FMeshBufferSet& MBSPriv = Result->GetVertexBuffers();
    for ( int32 b=0; b<MBSPriv.Buffers.Num(); ++b )
    {
        for ( int32 c=0; c<MBSPriv.Buffers[b].Channels.Num(); ++c )
        {
            EMeshBufferSemantic sem = MBSPriv.Buffers[b].Channels[c].Semantic;
            int semIndex = MBSPriv.Buffers[b].Channels[c].SemanticIndex;

            UntypedMeshBufferIterator it( Result->GetVertexBuffers(), sem, semIndex );

            switch ( sem )
            {
            case EMeshBufferSemantic::Position:
                for ( uint32_t v=0; v<VCount; ++v )
                {
                	if (!BoundingMesh || VertexInBoundaryMesh[v])
                	{
                		FVector4f value( 0.0f, 0.0f, 0.0f, 1.0f );
                		for( int i=0; i<it.GetComponents(); ++i )
                		{
                			ConvertData( i, &value[0], EMeshBufferFormat::Float32, it.ptr(), it.GetFormat() );
                		}

                		value = Transform.TransformFVector4( value );

                		for( int i=0; i<it.GetComponents(); ++i )
                		{
                			ConvertData( i, it.ptr(), it.GetFormat(), &value[0], EMeshBufferFormat::Float32 );
                		}
                	}
                	++it;
                }
                break;

            case EMeshBufferSemantic::Normal:
            case EMeshBufferSemantic::Tangent:
            case EMeshBufferSemantic::Binormal:
	            {
            		const uint8 NumComponents = FMath::Min(it.GetComponents(), 3); // Due to quantization, the serialized component W may not be zero. Must be zero to avoid being affected by the transform position.

            		for ( uint32_t v=0; v<VCount; ++v )
            		{
            			if (!BoundingMesh || VertexInBoundaryMesh[v])
            			{
            				FVector4f value( 0.0f, 0.0f, 0.0f, 0.0f );
                		
            				for (uint8 i = 0; i < NumComponents; ++i )
            				{
            					ConvertData( i, &value[0],  EMeshBufferFormat::Float32, it.ptr(), it.GetFormat() );
            				}

            				value = TransformInvT.TransformFVector4(value);

            				// Notice that 4th component is not modified.
            				for (uint8 i = 0; i < NumComponents; ++i )
            				{
            					ConvertData( i, it.ptr(), it.GetFormat(), &value[0],  EMeshBufferFormat::Float32 );
            				}
            			}
            			++it;
            		}
	            }
                break;

            default:
                break;
            }
        }
    }
}

}
