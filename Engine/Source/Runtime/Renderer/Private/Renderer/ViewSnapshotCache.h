// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

class FViewInfo;

namespace ViewSnapshotCache
{
	// Creates and returns a snapshot from the input view.
	extern FViewInfo* Create(const FViewInfo* View);

	// Called on the render thread after rendering to prepare snapshots for destruction.
	extern void Deallocate();

	// Called on a worker thread to destroy snapshots.
	extern void Destroy();
}