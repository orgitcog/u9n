// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraBakerFunctionLibrary.h"

#include "NiagaraBakerRendererOutputStaticMesh.h"

void UNiagaraBakerFunctionLibrary::CaptureNiagaraToStaticMesh(UNiagaraComponent* ComponentToCapture, UStaticMesh* StaticMeshOutput, FNiagaraRendererReadbackParameters ReadbackParameters)
{
#if WITH_NIAGARA_RENDERER_READBACK
	NiagaraRendererReadback::EnqueueReadback(
		ComponentToCapture,
		[ComponentToCapture, StaticMeshOutput](const FNiagaraRendererReadbackResult& ReadbackResult)
		{
			// Failed or no data
			if (ReadbackResult.NumVertices == 0)
			{
				return;
			}

			FNiagaraBakerRendererOutputStaticMesh::ConvertReadbackResultsToStaticMesh(ReadbackResult, StaticMeshOutput);
		},
		ReadbackParameters
	);
#endif
}
