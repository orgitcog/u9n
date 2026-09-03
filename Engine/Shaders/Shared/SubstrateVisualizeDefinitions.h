// Copyright Epic Games, Inc. All Rights Reserved.

/*================================================================================================
	SubstrateVisualise.h: used in ray tracing shaders and C++ code to define common constants
	!!! Changing this file requires recompilation of the engine !!!
=================================================================================================*/

#pragma once

#if defined(__cplusplus) || SUBSTRATE_ENABLED

#ifndef __cplusplus
// Change this to force recompilation of all Substrate dependent shaders (for instance https://guidgenerator.com/online-guid-generator.aspx)
#pragma message("UESHADERMETADATA_VERSION E9DF359B-BC55-46BC-B4DF-A5E94467C029")
#include "/Engine/Shared/SubstrateDefinitions.h"
#else
#include "SubstrateDefinitions.h"
#endif

struct FSubsterateDebugClosure
{
	int Type;

	int bHasWeightL;// = BSDF_GETHASTRANSABOVE(BSDF);
	int bHasGreyWeightV;// = BSDF_GETHASGREYWEIGHT_V(BSDF);
	int Address; // SubstrateAddressing.CurrentIndex

	int NormalID; //BSDF_GETSHAREDLOCALBASISID(BSDF)
	int BasisType; // SubstrateGetSharedLocalBasisType(HEADER_GETSHAREDLOCALBASESTYPE(Header.PackedHeader), BSDF_GETSHAREDLOCALBASISID(BSDF));

	//////////
	// Slabs
	int bIsTopLayer; // BSDF_GETISTOPLAYER(BSDF)
	int SSSType; // BSDF_GETSSSTYPE(BSDF) != SSS_TYPE_NONE
	int bIsThin; // BSDF_GETISTHIN(BSDF)
	float LuminanceWeightR, LuminanceWeightG, LuminanceWeightB; // bHasGreyWeightV BSDF.LuminanceWeightV.x or rgb
	float TransmittanceAboveAlongNR, TransmittanceAboveAlongNG, TransmittanceAboveAlongNB; // bHasGreyTopTrans BSDF.TransmittanceAboveAlongN.x or rgb
	float CoverageAboveAlongN;

	float DiffuseR,	DiffuseG,	DiffuseB;
	float F0R,		F0G,		F0B;
	float Roughness;

	int bHasF90;
	float F90R, F90G, F90B;

	int bHasAnisotropy;
	float Anisotropy;

	int bHasHaziness;
	float HazeRoughness;
	float HazeWeight;
	int HazeSimpleClearCoatMode;

	float  SSSOpacity;
	float SSSMFPR, SSSMFPG, SSSMFPB;
	float SSSRescaledMFPR, SSSRescaledMFPG, SSSRescaledMFPB;
	float  SSSPhase;
	float  SSSThickness;
	float  SSSProfileRadius;
	int SSSPRofileID;

	float FuzzAmount;
	float FuzzColorR, FuzzColorG, FuzzColorB;
	float FuzzRoughness;

	float GlintValue;
	float GlintUVDDXx;
	float GlintUVDDXy;
	float GlintUVDDYx;
	float GlintUVDDYy;

	int SpecProfileID;
	int SpecProfileParameterization;
};

struct FSubstratePixelDebugData
{
	int ClosureCount;
	int MaterialMode;
	int bIsComplexSpecialMaterial;
	int OptimisedLegacyMode;

	float MaterialAO;
	float IndirectIrradiance;
	float TopLayerRoughness;
	int HasPrecShadowMask;
	int HasZeroPrecShadowMask;
	int DoesCastContactShadow;
	int HasDynamicIndirectShadowCasterRepresentation;
	int HasSubsurface;
	int LocalBasesCount;

	FSubsterateDebugClosure Closures[SUBSTRATE_MAX_CLOSURE_COUNT];

	int MemoryDisplayMode;
	int MemorySlotA;
	int MemorySlotB;
	int MemorySlotC;
	int MemorySSSData;
	int MemoryTotal;

	int GPUFrameNumber;
};

#ifndef __cplusplus
uint SubstrateDebugDataSizeInUints;
RWStructuredBuffer<int> SubstrateDebugDataUAV;
struct FSubstrateDebugDataSerializer
{
	int WriteIndex;

	void Serialize(int Data)
	{
		if (WriteIndex < SubstrateDebugDataSizeInUints)
		{
			SubstrateDebugDataUAV[WriteIndex++] = Data;
		}
	}

	void Serialize(float Data)
	{
		if (WriteIndex < SubstrateDebugDataSizeInUints)
		{
			SubstrateDebugDataUAV[WriteIndex++] = asuint(Data);
		}
	}
};
#else
struct FSubstrateDebugDataSerializer
{
	int ReadIndex = 0;
	int* SubstratePixelDebugData = nullptr;

	void Serialize(int& Data)
	{
		Data = SubstratePixelDebugData[ReadIndex++];
	}

	void Serialize(float& Data)
	{
		Data = ((float*)SubstratePixelDebugData)[ReadIndex++];
	}
};
#endif

#ifndef __cplusplus
void SerializeSubstratePixelDebugData(in FSubstrateDebugDataSerializer S, in FSubstratePixelDebugData D)
#else
void SerializeSubstratePixelDebugData(FSubstrateDebugDataSerializer& S, FSubstratePixelDebugData& D)
#endif
{
	S.Serialize(D.ClosureCount);
#ifdef __cplusplus
	// Safe guard to avoid crash in case of readback problem.
	D.ClosureCount = FMath::Min(D.ClosureCount, int(SUBSTRATE_MAX_CLOSURE_COUNT));
#endif

	S.Serialize(D.MaterialMode);
	S.Serialize(D.bIsComplexSpecialMaterial);
	S.Serialize(D.OptimisedLegacyMode);

	S.Serialize(D.MaterialAO);
	S.Serialize(D.IndirectIrradiance);
	S.Serialize(D.TopLayerRoughness);
	S.Serialize(D.HasPrecShadowMask);
	S.Serialize(D.HasZeroPrecShadowMask);
	S.Serialize(D.DoesCastContactShadow);
	S.Serialize(D.HasDynamicIndirectShadowCasterRepresentation);
	S.Serialize(D.HasSubsurface);
	S.Serialize(D.LocalBasesCount);

	for (int i = 0; i < D.ClosureCount; ++i)
	{
		S.Serialize(D.Closures[i].Type);

		S.Serialize(D.Closures[i].bHasWeightL);
		S.Serialize(D.Closures[i].bHasGreyWeightV);
		S.Serialize(D.Closures[i].Address);

		S.Serialize(D.Closures[i].NormalID);
		S.Serialize(D.Closures[i].BasisType);

		S.Serialize(D.Closures[i].bIsTopLayer);
		S.Serialize(D.Closures[i].SSSType);
		S.Serialize(D.Closures[i].bIsThin);
		S.Serialize(D.Closures[i].LuminanceWeightR);
		S.Serialize(D.Closures[i].LuminanceWeightG);
		S.Serialize(D.Closures[i].LuminanceWeightB);
		S.Serialize(D.Closures[i].TransmittanceAboveAlongNR);
		S.Serialize(D.Closures[i].TransmittanceAboveAlongNG);
		S.Serialize(D.Closures[i].TransmittanceAboveAlongNB);
		S.Serialize(D.Closures[i].CoverageAboveAlongN);

		S.Serialize(D.Closures[i].DiffuseR);
		S.Serialize(D.Closures[i].DiffuseG);
		S.Serialize(D.Closures[i].DiffuseB);
		S.Serialize(D.Closures[i].F0R);
		S.Serialize(D.Closures[i].F0G);
		S.Serialize(D.Closures[i].F0B);
		S.Serialize(D.Closures[i].Roughness);

		S.Serialize(D.Closures[i].bHasF90);
		S.Serialize(D.Closures[i].F90R);
		S.Serialize(D.Closures[i].F90G);
		S.Serialize(D.Closures[i].F90B);

		S.Serialize(D.Closures[i].bHasAnisotropy);
		S.Serialize(D.Closures[i].Anisotropy);

		S.Serialize(D.Closures[i].bHasHaziness);
		S.Serialize(D.Closures[i].HazeRoughness);
		S.Serialize(D.Closures[i].HazeWeight);
		S.Serialize(D.Closures[i].HazeSimpleClearCoatMode);

		S.Serialize(D.Closures[i].SSSOpacity);
		S.Serialize(D.Closures[i].SSSMFPR);
		S.Serialize(D.Closures[i].SSSMFPG);
		S.Serialize(D.Closures[i].SSSMFPB);
		S.Serialize(D.Closures[i].SSSRescaledMFPR);
		S.Serialize(D.Closures[i].SSSRescaledMFPG);
		S.Serialize(D.Closures[i].SSSRescaledMFPB);
		S.Serialize(D.Closures[i].SSSPhase);
		S.Serialize(D.Closures[i].SSSThickness);
		S.Serialize(D.Closures[i].SSSProfileRadius);
		S.Serialize(D.Closures[i].SSSPRofileID);

		S.Serialize(D.Closures[i].FuzzAmount);
		S.Serialize(D.Closures[i].FuzzColorR);
		S.Serialize(D.Closures[i].FuzzColorG);
		S.Serialize(D.Closures[i].FuzzColorB);
		S.Serialize(D.Closures[i].FuzzRoughness);

		S.Serialize(D.Closures[i].GlintValue);
		S.Serialize(D.Closures[i].GlintUVDDXx);
		S.Serialize(D.Closures[i].GlintUVDDXy);
		S.Serialize(D.Closures[i].GlintUVDDYx);
		S.Serialize(D.Closures[i].GlintUVDDYy);

		S.Serialize(D.Closures[i].SpecProfileID);
		S.Serialize(D.Closures[i].SpecProfileParameterization);
	}

	S.Serialize(D.MemoryDisplayMode);
	S.Serialize(D.MemorySlotA);
	S.Serialize(D.MemorySlotB);
	S.Serialize(D.MemorySlotC);
	S.Serialize(D.MemorySSSData);
	S.Serialize(D.MemoryTotal);

	S.Serialize(D.GPUFrameNumber);
}

#ifndef __cplusplus

#if SUBSTRATE_MATERIALCONTAINER_IS_VIEWRESOURCE
FSubstratePixelDebugData ConvertToSerializableSubstratePixelDebugData(uint2 InCoord, uint GPUFrameNumber)
#else
FSubstratePixelDebugData ConvertToSerializableSubstratePixelDebugData(uint2 InCoord, FSubstrateRaytracingPayload Payload, uint GPUFrameNumber)
#endif
{
	FSubstratePixelDebugData Data = (FSubstratePixelDebugData)0;

	FSubstrateAddressing SubstrateAddressing = GetSubstratePixelDataByteOffset(InCoord, uint2(View.BufferSizeAndInvSize.xy), Substrate.MaxBytesPerPixel);
	const uint FootPrint_Start = SubstrateAddressing.ReadBytes;

#if SUBSTRATE_MATERIALCONTAINER_IS_VIEWRESOURCE
	FSubstratePixelHeader Header = UnpackSubstrateHeaderIn(Substrate.MaterialTextureArray, SubstrateAddressing, Substrate.TopLayerTexture);
#else
	FSubstratePixelHeader Header = UnpackSubstrateHeaderIn(Payload, SubstrateAddressing, Payload);
#endif
	const uint FootPrint_PostHeader = SubstrateAddressing.ReadBytes;

#if SUBSTRATE_MATERIALCONTAINER_IS_VIEWRESOURCE
	FSubstrateSubsurfaceHeader SSSHeader = SubstrateLoadSubsurfaceHeader(Substrate.MaterialTextureArray, Substrate.FirstSliceStoringSubstrateSSSData, SubstrateAddressing.PixelCoords);
	FSubstrateTopLayerData TopLayerData = SubstrateUnpackTopLayerData(Substrate.TopLayerTexture.Load(uint3(InCoord, 0)));
#else
	FSubstrateSubsurfaceHeader SSSHeader = (FSubstrateSubsurfaceHeader)0;
	FSubstrateTopLayerData TopLayerData = SubstrateUnpackTopLayerData(Payload.PackedTopLayerData);
#endif
	Data.TopLayerRoughness = TopLayerData.Roughness;

	const bool bSubstrateMaterial = Header.ClosureCount > 0;
	const bool bIsSimpleMaterial = Header.IsSimpleMaterial() || Header.ClosureCount == 0;
	const bool bIsSingleMaterial = !Header.IsSimpleMaterial() && Header.IsSingleMaterial();

	FSubstrateIrradianceAndOcclusion IrradianceAndOcclusion = SubstrateGetIrradianceAndAO(Header);

	Data.ClosureCount = Header.ClosureCount;

	Data.MaterialMode = Header.GetMaterialMode();
	Data.OptimisedLegacyMode = HEADER_MATERIALMODE_NONE;
	if (bIsSingleMaterial)
	{
#if SUBSTRATE_DEFERRED_SHADING
		Data.OptimisedLegacyMode = (Header.PackedHeader >> (HEADER_SINGLEENCODING_BIT_COUNT)) & HEADER_SINGLE_OPTLEGACYMODE_BIT_MASK;
#endif
	}
	Data.bIsComplexSpecialMaterial = Header.IsComplexSpecialMaterial();

	Data.MaterialAO = IrradianceAndOcclusion.MaterialAO;
	Data.IndirectIrradiance = IrradianceAndOcclusion.IndirectIrradiance;
	Data.HasPrecShadowMask = Header.HasPrecShadowMask() ? 1 : 0;
	Data.HasZeroPrecShadowMask = Header.HasZeroPrecShadowMask() ? 1 : 0;
	Data.DoesCastContactShadow = Header.DoesCastContactShadow() ? 1 : 0;
	Data.HasDynamicIndirectShadowCasterRepresentation = Header.HasDynamicIndirectShadowCasterRepresentation() ? 1 : 0;
	Data.HasSubsurface = Header.HasSubsurface() ? 1 : 0;
#if SUBSTRATE_DEFERRED_SHADING
	Data.LocalBasesCount = Header.GetLocalBasesCount();
#endif

	LOOP
	for(uint i = 0; i < Header.ClosureCount; ++i)
	{
		// Unpack BSDF data
	#if SUBSTRATE_MATERIALCONTAINER_IS_VIEWRESOURCE
		FSubstrateBSDF BSDF = UnpackSubstrateBSDF(Substrate.MaterialTextureArray, SubstrateAddressing, Header);
	#else
		FSubstrateBSDF BSDF = UnpackSubstrateBSDFIn(Payload, SubstrateAddressing, Header);
	#endif

		Data.Closures[i].Address = SubstrateAddressing.CurrentIndex;
		Data.Closures[i].Type = BSDF_GETTYPE(BSDF);

#if SUBSTRATE_DEFERRED_SHADING
		Data.Closures[i].NormalID = BSDF_GETSHAREDLOCALBASISID(BSDF);
		Data.Closures[i].BasisType = SubstrateGetSharedLocalBasisType(HEADER_GETSHAREDLOCALBASESTYPE(Header.PackedHeader), BSDF_GETSHAREDLOCALBASISID(BSDF));
#endif

		Data.Closures[i].bIsTopLayer = BSDF_GETISTOPLAYER(BSDF);
		Data.Closures[i].bHasWeightL = BSDF_GETHASTRANSABOVE(BSDF);
		Data.Closures[i].bHasGreyWeightV = BSDF_GETHASGREYWEIGHT_V(BSDF);

		Data.Closures[i].LuminanceWeightR = BSDF.LuminanceWeightV.r;
		Data.Closures[i].LuminanceWeightG = BSDF.LuminanceWeightV.g;
		Data.Closures[i].LuminanceWeightB = BSDF.LuminanceWeightV.b;
		Data.Closures[i].TransmittanceAboveAlongNR = BSDF.TransmittanceAboveAlongN.r;
		Data.Closures[i].TransmittanceAboveAlongNG = BSDF.TransmittanceAboveAlongN.g;
		Data.Closures[i].TransmittanceAboveAlongNB = BSDF.TransmittanceAboveAlongN.b;
		Data.Closures[i].CoverageAboveAlongN = BSDF.CoverageAboveAlongN;

		Data.Closures[i].SSSType = BSDF_GETSSSTYPE(BSDF);
		Data.Closures[i].bIsThin = BSDF_GETISTHIN(BSDF);

		switch (Data.Closures[i].Type)
		{
		case SUBSTRATE_BSDF_TYPE_SLAB:
		{
			Data.Closures[i].DiffuseR = SLAB_DIFFUSEALBEDO(BSDF).r;
			Data.Closures[i].DiffuseG = SLAB_DIFFUSEALBEDO(BSDF).g;
			Data.Closures[i].DiffuseB = SLAB_DIFFUSEALBEDO(BSDF).b;
			Data.Closures[i].F0R = SLAB_F0(BSDF).r;
			Data.Closures[i].F0G = SLAB_F0(BSDF).g;
			Data.Closures[i].F0B = SLAB_F0(BSDF).b;
			Data.Closures[i].Roughness = SLAB_ROUGHNESS(BSDF);

			Data.Closures[i].bHasF90 = BSDF_GETHASF90(BSDF);
			if (Data.Closures[i].bHasF90)
			{
				Data.Closures[i].F90R = SLAB_F90(BSDF).r;
				Data.Closures[i].F90G = SLAB_F90(BSDF).g;
				Data.Closures[i].F90B = SLAB_F90(BSDF).b;
			}
			else
			{
				Data.Closures[i].F90R = 1.0f;
				Data.Closures[i].F90G = 1.0f;
				Data.Closures[i].F90B = 1.0f;
			}

			Data.Closures[i].bHasAnisotropy = BSDF_GETHASANISOTROPY(BSDF);
			if (Data.Closures[i].bHasAnisotropy)
			{
				Data.Closures[i].Anisotropy = SLAB_ANISOTROPY(BSDF);
			}

			Data.Closures[i].bHasHaziness = BSDF_GETHASHAZINESS(BSDF);
			if (Data.Closures[i].bHasHaziness)
			{
				FHaziness Haziness = UnpackHaziness(SLAB_HAZINESS(BSDF));
				Data.Closures[i].HazeRoughness = Haziness.Roughness;
				Data.Closures[i].HazeWeight = Haziness.Weight;
				Data.Closures[i].HazeSimpleClearCoatMode = Haziness.bSimpleClearCoat ? 1u : 0u;
			}

			if (Data.Closures[i].SSSType != SSS_TYPE_NONE || Data.Closures[i].bIsThin)
			{
				if (Data.Closures[i].SSSType == SSS_TYPE_WRAP || Data.Closures[i].SSSType == SSS_TYPE_TWO_SIDED_WRAP)
				{
				#if SUBSTRATE_MATERIALCONTAINER_IS_VIEWRESOURCE
					Data.Closures[i].SSSOpacity = SubstrateSubSurfaceHeaderGetWrapOpacity(SSSHeader);
				#endif
					Data.Closures[i].SSSMFPR = SLAB_SSSMFP(BSDF).r;
					Data.Closures[i].SSSMFPG = SLAB_SSSMFP(BSDF).g;
					Data.Closures[i].SSSMFPB = SLAB_SSSMFP(BSDF).b;
					Data.Closures[i].SSSPhase = SLAB_SSSPHASEANISOTROPY(BSDF);
					Data.Closures[i].SSSThickness = BSDF_GETTHICKNESSCM(BSDF);
				}
				else if (Data.Closures[i].SSSType == SSS_TYPE_SIMPLEVOLUME)
				{
				#if SUBSTRATE_MATERIALCONTAINER_IS_VIEWRESOURCE
					Data.Closures[i].SSSOpacity = SubstrateSubSurfaceHeaderGetWrapOpacity(SSSHeader);
				#endif
					Data.Closures[i].SSSMFPR = SLAB_SSSMFP(BSDF).r;
					Data.Closures[i].SSSMFPG = SLAB_SSSMFP(BSDF).g;
					Data.Closures[i].SSSMFPB = SLAB_SSSMFP(BSDF).b;
					Data.Closures[i].SSSPhase = SLAB_SSSPHASEANISOTROPY(BSDF);
					Data.Closures[i].SSSThickness = BSDF_GETTHICKNESSCM(BSDF);
				}
				else if (Data.Closures[i].SSSType == SSS_TYPE_DIFFUSION_PROFILE)
				{
				#if SUBSTRATE_MATERIALCONTAINER_IS_VIEWRESOURCE
					const uint ProfileId = SubstrateSubSurfaceHeaderGetProfileId(SSSHeader);
					const float RadiusScale = SubstrateSubSurfaceHeaderGetProfileRadiusScale(SSSHeader);
					const float3 DiffuseMFP = GetSubsurfaceProfileMFPInCm(ProfileId).xyz * RadiusScale;
					const float Phase = GetTransmissionProfileParams(ProfileId).ScatteringDistribution;

					Data.Closures[i].SSSPRofileID = ProfileId;
					Data.Closures[i].SSSProfileRadius = RadiusScale;
					Data.Closures[i].SSSMFPR = DiffuseMFP.r;
					Data.Closures[i].SSSMFPG = DiffuseMFP.g;
					Data.Closures[i].SSSMFPB = DiffuseMFP.b;
					Data.Closures[i].SSSPhase = Phase;
				#endif
					if (Data.Closures[i].bIsThin)
					{
						Data.Closures[i].SSSThickness = SLAB_SSSPROFILETHICKNESSCM(BSDF);
					}
				}
				else if (Data.Closures[i].SSSType == SSS_TYPE_DIFFUSION)
				{
				#if SUBSTRATE_MATERIALCONTAINER_IS_VIEWRESOURCE
					const float3 OriginalMFP = SubstrateSubSurfaceHeaderGetMFP(SSSHeader);
					const float3 RescaledMFP = SLAB_SSSMFP(BSDF);


					Data.Closures[i].SSSMFPR = OriginalMFP.r;
					Data.Closures[i].SSSMFPG = OriginalMFP.g;
					Data.Closures[i].SSSMFPB = OriginalMFP.b;

					if (Data.Closures[i].bIsThin)
					{
						Data.Closures[i].SSSRescaledMFPR = RescaledMFP.r;
						Data.Closures[i].SSSRescaledMFPG = RescaledMFP.g;
						Data.Closures[i].SSSRescaledMFPB = RescaledMFP.b;
						Data.Closures[i].SSSThickness = BSDF_GETTHICKNESSCM(BSDF);
					}
				#endif

					Data.Closures[i].SSSPhase = SLAB_SSSPHASEANISOTROPY(BSDF);
				}
			}

			if (BSDF_GETHASFUZZ(BSDF))
			{
				Data.Closures[i].FuzzAmount = SLAB_FUZZ_AMOUNT(BSDF);
				Data.Closures[i].FuzzColorR = SLAB_FUZZ_COLOR(BSDF).r;
				Data.Closures[i].FuzzColorG = SLAB_FUZZ_COLOR(BSDF).g;
				Data.Closures[i].FuzzColorB = SLAB_FUZZ_COLOR(BSDF).b;
				Data.Closures[i].FuzzRoughness = SLAB_FUZZ_ROUGHNESS(BSDF);
			}

			if (BSDF_GETHASGLINT(BSDF))
			{
				Data.Closures[i].GlintValue = SLAB_GLINT_VALUE(BSDF);
				Data.Closures[i].GlintUVDDXx = SLAB_GLINT_UVDDX(BSDF).x;
				Data.Closures[i].GlintUVDDXy = SLAB_GLINT_UVDDX(BSDF).y;
				Data.Closures[i].GlintUVDDYx = SLAB_GLINT_UVDDY(BSDF).x;
				Data.Closures[i].GlintUVDDYy = SLAB_GLINT_UVDDY(BSDF).y;
			}
			else
			{
				Data.Closures[i].GlintValue = 1.0;	// No glint
			}

			Data.Closures[i].SpecProfileID = -1.0f;
			if (BSDF_GETHASSPECPROFILE(BSDF))
			{
				Data.Closures[i].SpecProfileID = GetSpecularProfileId(SLAB_SPECPROFILEID(BSDF));
				Data.Closures[i].SpecProfileParameterization = GetSpecularProfileParameterization(SLAB_SPECPROFILEID(BSDF));
			}
		}
		break;

		case SUBSTRATE_BSDF_TYPE_HAIR:
		{
			// Reusing the slab data to avoid using too many registers in case the compiler has trouble optimising
			Data.Closures[i].DiffuseR	= HAIR_BASECOLOR(BSDF).r;
			Data.Closures[i].DiffuseG	= HAIR_BASECOLOR(BSDF).g;
			Data.Closures[i].DiffuseB	= HAIR_BASECOLOR(BSDF).b;
			Data.Closures[i].F0R		= HAIR_SPECULAR(BSDF);
			Data.Closures[i].Roughness	= HAIR_ROUGHNESS(BSDF);
			Data.Closures[i].F90R		= HAIR_SCATTER(BSDF);
			Data.Closures[i].F90G		= HAIR_BACKLIT(BSDF);
			Data.Closures[i].F90B		= HAIR_COMPLEXTRANSMITTANCE(BSDF);
		}
		break;

		case SUBSTRATE_BSDF_TYPE_EYE:
		{
			// Reusing the slab data to avoid using too many registers in case the compiler has trouble optimising
			Data.Closures[i].DiffuseR	= EYE_DIFFUSEALBEDO(BSDF).r;
			Data.Closures[i].DiffuseG	= EYE_DIFFUSEALBEDO(BSDF).g;
			Data.Closures[i].DiffuseB	= EYE_DIFFUSEALBEDO(BSDF).b;
			Data.Closures[i].F0R		= EYE_F0(BSDF);
			Data.Closures[i].Roughness	= EYE_ROUGHNESS(BSDF);
			Data.Closures[i].F90R		= EYE_IRISMASK(BSDF);
			Data.Closures[i].F90G		= EYE_IRISDISTANCE(BSDF);

			Data.Closures[i].SSSMFPR	= EYE_IRISNORMAL(BSDF).x;
			Data.Closures[i].SSSMFPG	= EYE_IRISNORMAL(BSDF).y;
			Data.Closures[i].SSSMFPB	= EYE_IRISNORMAL(BSDF).z;
			Data.Closures[i].SSSRescaledMFPR = EYE_IRISPLANENORMAL(BSDF).x;
			Data.Closures[i].SSSRescaledMFPG = EYE_IRISPLANENORMAL(BSDF).y;
			Data.Closures[i].SSSRescaledMFPB = EYE_IRISPLANENORMAL(BSDF).z;
			
		#if SUBSTRATE_MATERIALCONTAINER_IS_VIEWRESOURCE
			const bool bHasSSS = Header.HasSubsurface();
			const bool bIsValid = SubstrateSubSurfaceHeaderGetIsValid(SSSHeader);
			const bool bIsProfile = SubstrateSubSurfaceHeaderGetIsProfile(SSSHeader);
			Data.Closures[i].SSSPRofileID = -1;
			if (bHasSSS && bIsValid && bIsProfile)
			{
				Data.Closures[i].SSSPRofileID = SubstrateSubSurfaceHeaderGetProfileId(SSSHeader);
			}
		#endif
		}
		break;

		case SUBSTRATE_BSDF_TYPE_SINGLELAYERWATER:
		{
			// Reusing the slab data to avoid using too many registers in case the compiler has trouble optimising
			Data.Closures[i].DiffuseR	= SLW_BASECOLOR(BSDF).r;
			Data.Closures[i].DiffuseG	= SLW_BASECOLOR(BSDF).g;
			Data.Closures[i].DiffuseB	= SLW_BASECOLOR(BSDF).b;
			Data.Closures[i].F0R		= SLW_SPECULAR(BSDF).r;
			Data.Closures[i].F0G		= SLW_METALLIC(BSDF).r;
			Data.Closures[i].Roughness	= SLW_ROUGHNESS(BSDF);
			Data.Closures[i].SSSOpacity = SLW_TOPMATERIALOPACITY(BSDF);
		}
		break;

		default:
		{
			// Error
		}
		break;
		}
	}

	const uint FootPrint_PostBSDFs = SubstrateAddressing.ReadBytes;

	// Output memory reads
	Data.MemoryDisplayMode = 0;
	{
		const bool bHasSSSData = SubstrateSubSurfaceHeaderGetIsValid(SSSHeader);
		const uint TopLayerDataBytes = 4;
		const uint SSSDataBytes = 8;

		const bool bSubstrateMaterial = Header.ClosureCount > 0;
		const bool bIsSimpleMaterial  = Header.IsSimpleMaterial() || Header.ClosureCount == 0;
		const bool bIsSingleMaterial  = !Header.IsSimpleMaterial() && Header.IsSingleMaterial();

		const uint HeaderSize = FootPrint_PostHeader - FootPrint_Start;
		const uint BSDFsSize  = FootPrint_PostBSDFs  - FootPrint_PostHeader;
		const uint TotalSize  = (FootPrint_PostBSDFs - FootPrint_Start) + (bHasSSSData ? SSSDataBytes : 0);

		if (Header.ClosureCount > 0)
		{
			if (bIsSimpleMaterial || Header.IsSingleLayerWater())
			{
				Data.MemoryDisplayMode = 1;
				Data.MemorySlotA = HeaderSize + BSDFsSize - TopLayerDataBytes;	// Header+BSDF
				Data.MemorySlotB = TopLayerDataBytes;							// TopNormalTex
			}
			else if (bIsSingleMaterial || Header.IsEye() || Header.IsHair())
			{
				Data.MemoryDisplayMode = 2;
				Data.MemorySlotA = HeaderSize - TopLayerDataBytes;				// Header
				Data.MemorySlotB = TopLayerDataBytes;							// TopNormalTex
				Data.MemorySlotC = BSDFsSize;									// BSDF
			}
			else
			{
				Data.MemoryDisplayMode = 3;
				Data.MemorySlotA = HeaderSize;									// Header+Norm
				Data.MemorySlotB = BSDFsSize;									// BSDFs
			}

			if (bHasSSSData)
			{
				Data.MemorySSSData = SSSDataBytes;								// SSS Data
			}
		}

		Data.MemoryTotal = TotalSize;
	}

	Data.GPUFrameNumber = asint(GPUFrameNumber);

	return Data;
}

#if SUBSTRATE_MATERIALCONTAINER_IS_VIEWRESOURCE
void SerializeSubstratePixelDebugDataEntry(uint2 InCoord, uint GPUFrameNumber)
#else
void SerializeSubstratePixelDebugDataEntry(uint2 InCoord, FSubstrateRaytracingPayload Payload, uint GPUFrameNumber)
#endif
{
	// Convert Substrate GBuffer into the CPU/GPU common data to serialize.
#if SUBSTRATE_MATERIALCONTAINER_IS_VIEWRESOURCE
	FSubstratePixelDebugData Data = ConvertToSerializableSubstratePixelDebugData(InCoord, GPUFrameNumber);
#else
	FSubstratePixelDebugData Data = ConvertToSerializableSubstratePixelDebugData(InCoord, Payload, GPUFrameNumber);
#endif
	
	// And serialise it.
	FSubstrateDebugDataSerializer S = (FSubstrateDebugDataSerializer)0;
	SerializeSubstratePixelDebugData(S, Data);
}

void SerializeNullPixelDebugDataEntry()
{
	FSubstratePixelDebugData Data = (FSubstratePixelDebugData)0;
	FSubstrateDebugDataSerializer S = (FSubstrateDebugDataSerializer)0;
	SerializeSubstratePixelDebugData(S, Data);
}

#endif


////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////


struct FSubstrateSystemInfoData
{
	int TileCount[SUBSTRATE_TILE_TYPE_COUNT];
};

#ifndef __cplusplus
void SerializeSubstrateSystemInfoDebugData(in FSubstrateDebugDataSerializer S, in FSubstrateSystemInfoData D)
#else
void SerializeSubstrateSystemInfoDebugData(FSubstrateDebugDataSerializer& S, FSubstrateSystemInfoData& D)
#endif
{
	for (int i = 0; i < SUBSTRATE_TILE_TYPE_COUNT; ++i)
	{
		S.Serialize(D.TileCount[i]);
	}
}

#ifndef __cplusplus

uint GetTileCount(uint InType);

void ConvertToSerializableSubstratePixelDebugData()
{
	FSubstrateSystemInfoData Data = (FSubstrateSystemInfoData)0;

	// Setup the system info to send to the CPU for debug print
	for (int i = 0; i < SUBSTRATE_TILE_TYPE_COUNT; ++i)
	{
		Data.TileCount[i] = GetTileCount(i);
	}

	// And serialise it.
	FSubstrateDebugDataSerializer S = (FSubstrateDebugDataSerializer)0;
	SerializeSubstrateSystemInfoDebugData(S, Data);
}

#endif

#endif // defined(__cplusplus) || SUBSTRATE_ENABLED
