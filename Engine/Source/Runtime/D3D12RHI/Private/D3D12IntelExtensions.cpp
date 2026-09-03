// Copyright Epic Games, Inc. All Rights Reserved.

#include "D3D12IntelExtensions.h"
#include "D3D12RHICommon.h"
#include "D3D12RHIPrivate.h"

#if INTEL_EXTENSIONS

#if INTEL_GPU_CRASH_DUMPS

namespace UE::RHICore::Intel::GPUCrashDumps
{
	extern RHICORE_API TAutoConsoleVariable<int32> CVarIntelCrashDumps;
}

#endif // INTEL_GPU_CRASH_DUMPS

const INTCSupportedVersion SupportedExtensionsVersion[] = {
	{ { 4, 8, 0 }, nullptr },														// Emulated Typed 64bit Atomics - this is required to run Nanite on ACM (DG2)
#if INTEL_GPU_CRASH_DUMPS
	{ { 4, 14, 0 }, &( *UE::RHICore::Intel::GPUCrashDumps::CVarIntelCrashDumps) }	// Intel GPU crash dumps
#endif // INTEL_GPU_CRASH_DUMPS
};

bool MatchIntelExtensionVersion(const INTCExtensionVersion& ExtensionsVersion)
{
	uint32_t count = sizeof( SupportedExtensionsVersion ) / sizeof( SupportedExtensionsVersion[ 0 ] );

	// We try to match the highest version first (most features)
	for( int i = count - 1; i >= 0; i-- )
	{
		const INTCSupportedVersion& SupportedVersion = SupportedExtensionsVersion[ i ];

		if( SupportedVersion.CVar && SupportedVersion.CVar->GetInt() == 0 )
		{
			continue;			// skip disabled feature
		}

		if( ( ExtensionsVersion.HWFeatureLevel == SupportedVersion.Version.HWFeatureLevel ) &&
			( ExtensionsVersion.APIVersion == SupportedVersion.Version.APIVersion ) &&
			( ExtensionsVersion.Revision == SupportedVersion.Version.Revision ) )
		{
			// First match wins
			return true;
		}
	}
	
	// No matches
	return false;
}

#if INTEL_GPU_CRASH_DUMPS
namespace UE::RHICore::Intel::GPUCrashDumps::D3D12
{
	uint64_t RegisterCommandList( ID3D12CommandList* pCommandList )
	{
		if( !IsEnabled() )
		{
			return 0;
		}

		return INTC_D3D12_GetCommandListHandle( FD3D12DynamicRHI::GetD3DRHI()->GetIntelExtensionContext(), pCommandList );
	}

#if WITH_RHI_BREADCRUMBS
	void BeginBreadcrumb(ID3D12GraphicsCommandList* pCommandList, FRHIBreadcrumbNode* Breadcrumb)
	{
		if( !IsEnabled() )
		{
			return;
		}
		
		FMarker Marker(Breadcrumb);
		if (Marker)
		{
#if INTEL_BREADCRUMBS_USE_BREADCRUMB_PTRS
			INTC_D3D12_SetEventMarker( FD3D12DynamicRHI::GetD3DRHI()->GetIntelExtensionContext(), pCommandList, INTC_EVENT_MARKER_BEGIN | INTC_EVENT_MARKER_PTR, Marker.GetPtr(), Marker.GetSize() );
#else
			INTC_D3D12_SetEventMarker( FD3D12DynamicRHI::GetD3DRHI()->GetIntelExtensionContext(), pCommandList, INTC_EVENT_MARKER_BEGIN | INTC_EVENT_MARKER_WSTRING, Marker.GetPtr(), Marker.GetSize() );
#endif
		}
	}

	void EndBreadcrumb(ID3D12GraphicsCommandList* pCommandList, FRHIBreadcrumbNode* Breadcrumb)
	{
		if( !IsEnabled() )
		{
			return;
		}

		FMarker Marker(Breadcrumb);
		if (Marker)
		{
#if INTEL_BREADCRUMBS_USE_BREADCRUMB_PTRS
			INTC_D3D12_SetEventMarker( FD3D12DynamicRHI::GetD3DRHI()->GetIntelExtensionContext(), pCommandList, INTC_EVENT_MARKER_END | INTC_EVENT_MARKER_PTR, Marker.GetPtr(), Marker.GetSize() );
#else
			INTC_D3D12_SetEventMarker( FD3D12DynamicRHI::GetD3DRHI()->GetIntelExtensionContext(), pCommandList, INTC_EVENT_MARKER_END | INTC_EVENT_MARKER_WSTRING, Marker.GetPtr(), Marker.GetSize() );
#endif
		}
	}
#endif
}
#endif // INTEL_GPU_CRASH_DUMPS

#endif // INTEL_EXTENSIONS
