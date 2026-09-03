// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Math/UnrealMath.h"
#include "TextureResource.h"
#include "VirtualTexturing.h"

class FTextureResource;

struct UMaterialCacheVirtualTextureDescriptor
{
	operator FUintVector2() const
	{
		FUintVector2 Out;
		FMemory::Memcpy(&Out, this, sizeof(FUintVector2));
		return Out;
	}
	
	// DWord0
	uint32_t PageX : 12;
	uint32_t PageY : 12;
	uint32_t PageTableMipBias : 4;
	uint32_t SpaceID : 4;

	// DWord1
	uint32_t WidthInPages : 12;
	uint32_t HeightInPages : 12;
	uint32_t MaxLevel : 6;
	uint32_t OptionalCoordinateIndex : 2;
};

inline UMaterialCacheVirtualTextureDescriptor PackMaterialCacheTextureDescriptor(FTextureResource* Resource) 
{
	UMaterialCacheVirtualTextureDescriptor Descriptor{};
	if (!Resource)
	{
		return Descriptor;
	}

	FVirtualTexture2DResource* VirtualResource = Resource->GetVirtualTexture2DResource();
	if (!ensure(VirtualResource))
	{
		return Descriptor;
	}

	IAllocatedVirtualTexture* Allocation = VirtualResource->GetAllocatedVT();
	if (!ensure(Allocation))
	{
		return Descriptor;
	}
	
	Descriptor.PageX                   = Allocation->GetVirtualPageX();
	Descriptor.PageY                   = Allocation->GetVirtualPageY();
	Descriptor.WidthInPages            = Allocation->GetWidthInTiles();
	Descriptor.HeightInPages           = Allocation->GetHeightInTiles();
	Descriptor.PageTableMipBias        = FMath::FloorLog2(Allocation->GetVirtualTileSize());
	Descriptor.SpaceID                 = Allocation->GetSpaceID();
	Descriptor.MaxLevel                = Allocation->GetMaxLevel();
	Descriptor.OptionalCoordinateIndex = 0;
	return Descriptor;
}

static_assert(sizeof(UMaterialCacheVirtualTextureDescriptor) == sizeof(FUintVector2), "Unexpected descriptor size");
