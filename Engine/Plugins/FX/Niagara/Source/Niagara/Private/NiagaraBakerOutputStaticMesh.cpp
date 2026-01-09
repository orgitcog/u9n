// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraBakerOutputStaticMesh.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NiagaraBakerOutputStaticMesh)

bool UNiagaraBakerOutputStaticMesh::Equals(const UNiagaraBakerOutput& OtherBase) const
{
	const UNiagaraBakerOutputStaticMesh& Other = *CastChecked<UNiagaraBakerOutputStaticMesh>(&OtherBase);
	return
		Super::Equals(Other);
}

#if WITH_EDITOR
FString UNiagaraBakerOutputStaticMesh::MakeOutputName() const
{
	return FString::Printf(TEXT("StaticMesh_%d"), GetFName().GetNumber());
}
#endif

#if WITH_EDITORONLY_DATA
void UNiagaraBakerOutputStaticMesh::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

}
#endif

