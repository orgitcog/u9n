// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UNiagaraSimCache;

namespace FNiagaraEditorSimCacheUtils
{
	extern void ExportToDisk(TArrayView<UNiagaraSimCache*> CachesToExport);
	extern void ExportToDisk(const UNiagaraSimCache* CacheToExport);
}
