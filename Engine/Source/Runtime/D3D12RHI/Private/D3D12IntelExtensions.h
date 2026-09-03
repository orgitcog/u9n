// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#ifndef INTEL_EXTENSIONS
#	define INTEL_EXTENSIONS 0
#endif

#if INTEL_EXTENSIONS

	#include "D3D12ThirdParty.h"

	#define INTC_IGDEXT_D3D12 1

	#include "Microsoft/AllowMicrosoftPlatformTypes.h"
THIRD_PARTY_INCLUDES_START
	#include <igdext.h>
THIRD_PARTY_INCLUDES_END
	#include "Microsoft/HideMicrosoftPlatformTypes.h"
	
	#include "RHICoreIntelBreadcrumbs.h"

	extern bool GDX12INTCAtomicUInt64Emulation;

	struct INTCExtensionContext;
	struct INTCExtensionInfo;

	struct INTCSupportedVersion
	{
		INTCExtensionVersion				Version;			// Required version
		IConsoleVariable*					CVar;				// Console variable that controls this feature
	};
	extern INTCExtensionVersion IntelExtensionsVersion;

	// Offsets for the version structure
	#define INTEL_EXTENSION_VERSION_GENERIC			0		// Generic version for all UE5 targets
	#define INTEL_EXTENSION_VERSION_BREADCRUMBS		1		// Intel Breadcrumbs support

	bool MatchIntelExtensionVersion(const INTCExtensionVersion& Version);
	void SetIntelExtensionsVersion( const INTCExtensionVersion& ExtensionsVersion );

	INTCExtensionContext* CreateIntelExtensionsContext(ID3D12Device* Device, INTCExtensionInfo& INTCExtensionInfo, uint32 DeviceId = 0);
	void DestroyIntelExtensionsContext(INTCExtensionContext* IntelExtensionContext);

	bool EnableIntelAtomic64Support(INTCExtensionContext* IntelExtensionContext, INTCExtensionInfo& INTCExtensionInfo);
	void EnableIntelAppDiscovery(uint32 DeviceId);

#if INTEL_GPU_CRASH_DUMPS
	namespace UE::RHICore::Intel::GPUCrashDumps::D3D12
	{
		uint64_t RegisterCommandList( ID3D12CommandList* pCommandList );
		//void UnregisterCommandList( FCommandList CommandList );

#if WITH_RHI_BREADCRUMBS
		void BeginBreadcrumb( ID3D12GraphicsCommandList* pCommandList, FRHIBreadcrumbNode* Breadcrumb );
		void EndBreadcrumb( ID3D12GraphicsCommandList* pCommandList, FRHIBreadcrumbNode* Breadcrumb );
#endif
	}
#endif

#endif //INTEL_EXTENSIONS
