// Copyright Epic Games, Inc. All Rights Reserved.
#include "Chaos/GraphColoring.h"
#include "Chaos/Array.h"
#include "ChaosLog.h"
#include "Chaos/Framework/Parallel.h"
#include "Containers/BitArray.h"
#include "Chaos/SoftsSolverParticlesRange.h"

template<typename DynamicParticlesType, int32 N, bool bAllDynamic>
static bool VerifyGraph(const TArray<TArray<int32>>& ColorGraph, const TArray<Chaos::TVector<int32, N>>& Graph, const DynamicParticlesType& InParticles)
{
	for (int32 Color = 0; Color < ColorGraph.Num(); ++Color)
	{
		TMap<int32, int32> ColorNodesToEdges;
		for (const int32 Edge : ColorGraph[Color])
		{
			for (int32 NIndex = 0; NIndex < N; ++NIndex)
			{
				const int32 Node = Graph[Edge][NIndex];
				const int32* const ExistingEdge = ColorNodesToEdges.Find(Node);
				if (ExistingEdge && *ExistingEdge != Edge)
				{
					UE_LOG(LogChaos, Error, TEXT("Color %d has duplicate Node %d. First added for Edge %d, and now found for Edge %d"), Color, Node, *ExistingEdge, Edge);
					return false;
				}
				if (bAllDynamic || InParticles.InvM(Node) != 0)
				{
					ColorNodesToEdges.Add(Node, Edge);
				}
			}
		}
	}
	return true;
}


//just verify if different element in each subcolor has intersecting nodes
template <typename T>
static bool VerifyGridBasedSubColoring(const TArray<TArray<int32>>& ElementsPerColor, const Chaos::TMPMGrid<T>& Grid, const TArray<TArray<int32>>& ConstraintsNodesSet, const TArray<TArray<TArray<int32>>>& ElementsPerSubColors)
{
	for (int32 i = 0; i < ElementsPerSubColors.Num(); i++)
	{
		for (int32 j = 0; j < ElementsPerSubColors[i].Num(); j++)
		{
			TSet<int32> CoveredGridNodes;
			//first gather all Grid nodes of an element:
			for (int32 k = 0; k < ElementsPerSubColors[i][j].Num(); k++)
			{
				int32 e = ElementsPerSubColors[i][j][k];
				for (int32 Node:ConstraintsNodesSet[e])
				{
					if (CoveredGridNodes.Contains(Node))
					{
						return false;
					}
					CoveredGridNodes.Emplace(Node);
				}
			}
		}
	}
	return true;

}

template <typename T>
static bool VerifyWeakConstraintsColoring(const TArray<TArray<int32>>& Indices, const TArray<TArray<int32>>& SecondIndices, const Chaos::TDynamicParticles<T, 3>& InParticles, const TArray<TArray<int32>>& ConstraintsPerColor)
{
	TArray<bool> ConstraintIsIncluded;
	ConstraintIsIncluded.Init(false, Indices.Num());
	for (int32 i = 0; i < ConstraintsPerColor.Num(); i++)
	{
		for (int32 j = 0; j < ConstraintsPerColor[i].Num(); j++)
		{
			ConstraintIsIncluded[ConstraintsPerColor[i][j]] = true;
		}
	}

	for (int32 kk = 0; kk < Indices.Num(); kk++)
	{
		if (!ConstraintIsIncluded[kk])
		{
			return false;
		}
	}

	for (int32 i = 0; i < ConstraintsPerColor.Num(); i++)
	{
		TSet<int32> CoveredParticles;
		for (int32 j = 0; j < ConstraintsPerColor[i].Num(); j++)
		{
			for (int32 Node : Indices[ConstraintsPerColor[i][j]])
			{
				if (CoveredParticles.Contains(Node))
				{
					return false;
				}
				CoveredParticles.Emplace(Node);
			}

			if (SecondIndices.Num() > 0)
			{
				for (int32 Node : SecondIndices[ConstraintsPerColor[i][j]])
				{
					if (CoveredParticles.Contains(Node))
					{
						return false;
					}
					CoveredParticles.Emplace(Node);
				}

			}
		}
	}

	return true;

}

template <typename T>
static bool VerifyNodalColoring(const TArray<Chaos::TVec4<int32>>& Graph, const Chaos::TDynamicParticles<T, 3>& InParticles, const int32 GraphParticlesStart, const int32 GraphParticlesEnd, const TArray<TArray<int32>>& IncidentElements, const TArray<TArray<int32>>& IncidentElementsLocalIndex, const TArray<TArray<int32>> ParticlesPerColor)
{
	TArray<bool> ParticleIsIncluded;
	ParticleIsIncluded.Init(false, InParticles.Size());
	for (int32 i = 0; i < ParticlesPerColor.Num(); i++) 
	{
		for (int32 j = 0; j < ParticlesPerColor[i].Num(); j++)
		{
			ParticleIsIncluded[ParticlesPerColor[i][j]] = true;
		}
	}

	for (int32 p = 0; p < GraphParticlesEnd - GraphParticlesStart; p++)
	{
		int32 ParticleIndex = p + GraphParticlesStart;
		if (InParticles.InvM(ParticleIndex) != (T)0.)
		{
			if (!ParticleIsIncluded[ParticleIndex])
			{
				return false;
			}
		}
	}

	TArray<int32> Particle2Incident;
	Particle2Incident.Init(-1, GraphParticlesEnd - GraphParticlesStart);
	for (int32 i = 0; i < IncidentElements.Num(); i++)
	{
		if (IncidentElements[i].Num() > 0)
		{
			Particle2Incident[Graph[IncidentElements[i][0]][IncidentElementsLocalIndex[i][0]]] = i;
		}
	}

	for (int32 i = 0; i < ParticlesPerColor.Num(); i++)
	{
		TSet<int32> IncidentParticles;
		for (int32 j = 0; j < ParticlesPerColor[i].Num(); j++)
		{
			int32 ParticleIndex = ParticlesPerColor[i][j];
			TSet<int32> LocalIncidentParticles;
			for (int32 k = 0; k < IncidentElements[Particle2Incident[ParticleIndex]].Num(); k++)
			{
				int32 ElementIndex = IncidentElements[Particle2Incident[ParticleIndex]][k];
				for (int32 ie = 0; ie < 4; ie++)
				{
					LocalIncidentParticles.Emplace(Graph[ElementIndex][ie]);
				}
			}
			if (IncidentParticles.Contains(ParticleIndex))
			{
				return false;
			}
			else
			{
				for (int32 LocalParticle : LocalIncidentParticles)
				{
					IncidentParticles.Emplace(LocalParticle);
				}
			}
		}
	}

	return true;

}

template <typename T>
static bool VerifyNodalColoring(const TArray<TArray<int32>>& Graph, const Chaos::TDynamicParticles<T, 3>& InParticles, const int32 GraphParticlesStart, const int32 GraphParticlesEnd, const TArray<TArray<int32>>& IncidentElements, const TArray<TArray<int32>>& IncidentElementsLocalIndex, const TArray<TArray<int32>> ParticlesPerColor)
{
 	checkSlow(GraphParticlesStart <= GraphParticlesEnd);
	checkSlow(GraphParticlesEnd <= (int32)InParticles.Size());
	TArray<bool> ParticleIsIncluded;
	ParticleIsIncluded.Init(false, InParticles.Size());
	for (int32 i = 0; i < ParticlesPerColor.Num(); i++)
	{
		for (int32 j = 0; j < ParticlesPerColor[i].Num(); j++)
		{
			ParticleIsIncluded[ParticlesPerColor[i][j]] = true;
		}
	}

	for (int32 p = 0; p < GraphParticlesEnd - GraphParticlesStart; p++)
	{
		int32 ParticleIndex = p + GraphParticlesStart;
		if (InParticles.InvM(ParticleIndex) != (T)0. && IncidentElements[ParticleIndex].Num() > 0 )
		{
			if (!ParticleIsIncluded[ParticleIndex])
			{
				return false;
			}
		}
	}

	TArray<int32> Particle2Incident;
	Particle2Incident.Init(INDEX_NONE, GraphParticlesEnd - GraphParticlesStart);
	for (int32 i = 0; i < GraphParticlesEnd - GraphParticlesStart; i++)
	{
		Particle2Incident[i] = i + GraphParticlesStart;
	}

	for (int32 i = 0; i < ParticlesPerColor.Num(); i++)
	{
		TSet<int32> IncidentParticles;
		for (int32 j = 0; j < ParticlesPerColor[i].Num(); j++)
		{
			int32 ParticleIndex = ParticlesPerColor[i][j];
			TSet<int32> LocalIncidentParticles;
			for (int32 k = 0; k < IncidentElements[Particle2Incident[ParticleIndex]].Num(); k++)
			{
				int32 ElementIndex = IncidentElements[Particle2Incident[ParticleIndex]][k];
				for (int32 ie = 0; ie < Graph[ElementIndex].Num(); ie++)
				{
					LocalIncidentParticles.Add(Graph[ElementIndex][ie]);
				}
			}
			if (IncidentParticles.Contains(ParticleIndex))
			{
				return false;
			}
			else
			{
				for (int32 LocalParticle : LocalIncidentParticles)
				{
					IncidentParticles.Add(LocalParticle);
				}
			}
		}
	}

	return true;
}

template <typename T>
static bool VerifyExtraNodalColoring(const Chaos::TDynamicParticles<T, 3>& InParticles, const TArray<TArray<int32>>& Graph, const TArray<TArray<int32>>& ExtraGraph, const TArray<TArray<int32>>& IncidentElements, const TArray<TArray<int32>>& ExtraIncidentElements,const TArray<int32>& ParticleColors,const TArray<TArray<int32>>& ParticlesPerColor)
{
	TBitArray ParticleIsIncluded(false, InParticles.Size());
	for (int32 i = 0; i < ParticlesPerColor.Num(); i++)
	{
		for (int32 j = 0; j < ParticlesPerColor[i].Num(); j++)
		{
			ParticleIsIncluded[ParticlesPerColor[i][j]] = true;
		}
	}

	for (int32 ParticleIndex = 0; ParticleIndex < (int32)InParticles.Size(); ParticleIndex++)
	{
		if (InParticles.InvM(ParticleIndex) != (T)0. && (IncidentElements[ParticleIndex].Num() > 0 || ExtraIncidentElements[ParticleIndex].Num() > 0))
		{
			if (!ParticleIsIncluded[ParticleIndex])
			{
				return false;
			}
		}
	}


	for (int32 i = 0; i < ParticlesPerColor.Num(); i++)
	{
		TSet<int32> IncidentParticles;
		IncidentParticles.Reserve(ParticlesPerColor[i].Num());
		for (int32 j = 0; j < ParticlesPerColor[i].Num(); j++)
		{
			int32 ParticleIndex = ParticlesPerColor[i][j];
			TSet<int32> LocalIncidentParticles;
			for (int32 k = 0; k < IncidentElements[ParticleIndex].Num(); k++)
			{
				int32 ElementIndex = IncidentElements[ParticleIndex][k];
				for (int32 ie = 0; ie < Graph[ElementIndex].Num(); ie++)
				{
					LocalIncidentParticles.Add(Graph[ElementIndex][ie]);
				}
			}
			if (ParticleIndex < ExtraIncidentElements.Num())
			{
				for (int32 k = 0; k < ExtraIncidentElements[ParticleIndex].Num(); k++)
				{
					int32 ElementIndex = ExtraIncidentElements[ParticleIndex][k];
					for (int32 ie = 0; ie < ExtraGraph[ElementIndex].Num(); ie++)
					{
						LocalIncidentParticles.Add(ExtraGraph[ElementIndex][ie]);
					}
				}
			}
			if (IncidentParticles.Contains(ParticleIndex))
			{
				return false;
			}
			else
			{
				for (int32 LocalParticle : LocalIncidentParticles)
				{
					IncidentParticles.Add(LocalParticle);
				}
			}
		}
	}

	return true;
}

template <typename T>
static bool VerifyExtraNodalColoring(const Chaos::TDynamicParticles<T, 3>& InParticles, const TArray<TArray<int32>>& StaticGraph, const TArray<TArray<int32>>& DynamicGraph, const TArray<TArray<int32>>& ExtraGraph, const TArray<TArray<int32>>& StaticIncidentElements, const TArray<TArray<int32>>& DynamicIncidentElements, const TArray<TArray<int32>>& ExtraIncidentElements,const TArray<int32>& ParticleColors,const TArray<TArray<int32>>& ParticlesPerColor)
{
	TBitArray ParticleIsIncluded(false, InParticles.Size());
	for (int32 i = 0; i < ParticlesPerColor.Num(); i++)
	{
		for (int32 j = 0; j < ParticlesPerColor[i].Num(); j++)
		{
			ParticleIsIncluded[ParticlesPerColor[i][j]] = true;
		}
	}

	for (int32 ParticleIndex = 0; ParticleIndex < (int32)InParticles.Size(); ParticleIndex++)
	{
		if (InParticles.InvM(ParticleIndex) != (T)0. && (StaticIncidentElements[ParticleIndex].Num() > 0 || ExtraIncidentElements[ParticleIndex].Num() > 0 || DynamicIncidentElements[ParticleIndex].Num() > 0))
		{
			if (!ParticleIsIncluded[ParticleIndex])
			{
				return false;
			}
		}
	}


	for (int32 i = 0; i < ParticlesPerColor.Num(); i++)
	{
		TSet<int32> IncidentParticles;
		IncidentParticles.Reserve(ParticlesPerColor[i].Num());
		for (int32 j = 0; j < ParticlesPerColor[i].Num(); j++)
		{
			int32 ParticleIndex = ParticlesPerColor[i][j];
			TSet<int32> LocalIncidentParticles;
			for (int32 k = 0; k < StaticIncidentElements[ParticleIndex].Num(); k++)
			{
				int32 ElementIndex = StaticIncidentElements[ParticleIndex][k];
				for (int32 ie = 0; ie < StaticGraph[ElementIndex].Num(); ie++)
				{
					LocalIncidentParticles.Add(StaticGraph[ElementIndex][ie]);
				}
			}
			for (int32 k = 0; k < DynamicIncidentElements[ParticleIndex].Num(); k++)
			{
				int32 ElementIndex = DynamicIncidentElements[ParticleIndex][k];
				for (int32 ie = 0; ie < DynamicGraph[ElementIndex].Num(); ie++)
				{
					LocalIncidentParticles.Add(DynamicGraph[ElementIndex][ie]);
				}
			}
			if (ParticleIndex < ExtraIncidentElements.Num())
			{
				for (int32 k = 0; k < ExtraIncidentElements[ParticleIndex].Num(); k++)
				{
					int32 ElementIndex = ExtraIncidentElements[ParticleIndex][k];
					for (int32 ie = 0; ie < ExtraGraph[ElementIndex].Num(); ie++)
					{
						LocalIncidentParticles.Add(ExtraGraph[ElementIndex][ie]);
					}
				}
			}
			if (IncidentParticles.Contains(ParticleIndex))
			{
				return false;
			}
			else
			{
				for (int32 LocalParticle : LocalIncidentParticles)
				{
					IncidentParticles.Add(LocalParticle);
				}
			}
		}
	}

	return true;
}

template<typename DynamicParticlesType, int32 N, bool bAllDynamic>
TArray<TArray<int32>> Chaos::FGraphColoring::ComputeGraphColoringParticlesOrRange(const TArray<TVector<int32, N>>& Graph, const DynamicParticlesType& InParticles, const int32 GraphParticlesStart, const int32 GraphParticlesEnd)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FGraphColoring_ComputeGraphColoringN);
	checkSlow(GraphParticlesStart <= GraphParticlesEnd);
	checkSlow(GraphParticlesEnd <= (int32)InParticles.Size());

	TArray<TArray<int32>> ColorGraph;

	int32 MaxColor = -1;
	TArray<FColorSet> NodeUsedColorsSubArray;
	NodeUsedColorsSubArray.SetNum(GraphParticlesEnd - GraphParticlesStart);
	TArrayView<FColorSet> NodeUsedColors(NodeUsedColorsSubArray.GetData() - GraphParticlesStart, GraphParticlesEnd); // Only nodes starting with GraphParticlesStart are valid to access
	for (int32 EdgeIndex = 0; EdgeIndex < Graph.Num(); ++EdgeIndex)
	{
		// Find color that hasn't already been assigned to a node on this edge.
		int32 FirstFreeColor = 0;
		while (true)
		{
			bool bColorFound = false;
			for (int32 NIndex = 0; NIndex < N; ++NIndex)
			{
				const int32 NodeIndex = Graph[EdgeIndex][NIndex];
				if constexpr (!bAllDynamic)
				{
					const bool bIsParticleDynamic = InParticles.InvM(NodeIndex) != (decltype(InParticles.InvM(NodeIndex)))0.;
					if (!bIsParticleDynamic)
					{
						continue;
					}
				}
				if (NodeUsedColors[NodeIndex].Contains(FirstFreeColor))
				{
					bColorFound = true;
					break;
				}
			}
			if (!bColorFound)
			{
				break;
			}
			++FirstFreeColor;
		}
		MaxColor = FMath::Max(MaxColor, FirstFreeColor);
		if (ColorGraph.Num() <= MaxColor)
		{
			ColorGraph.SetNum(MaxColor + 1);
		}
		for (int32 NIndex = 0; NIndex < N; ++NIndex)
		{
			const int32 NodeIndex = Graph[EdgeIndex][NIndex];
			NodeUsedColors[NodeIndex].Add(FirstFreeColor);
		}
		ColorGraph[FirstFreeColor].Add(EdgeIndex);
	}
#if DO_GUARD_SLOW
	const bool bVerifyGraphResult = VerifyGraph<DynamicParticlesType, N, bAllDynamic>(ColorGraph, Graph, InParticles);
	checkSlow(bVerifyGraphResult);
#endif
	return ColorGraph;
}


template<typename T>
void Chaos::ComputeGridBasedGraphSubColoringPointer(const TArray<TArray<int32>>& ElementsPerColor, const TMPMGrid<T>& Grid, const int32 GridSize, TUniquePtr<TArray<TArray<int32>>>& PreviousColoring, const TArray<TArray<int32>>& ConstraintsNodesSet, TArray<TArray<TArray<int32>>>& ElementsPerSubColors) 
{
	typedef TArray<int32, TInlineAllocator<8>> FColorSet;

	bool bHaveInitialGuess = true;
	if (!PreviousColoring) 
	{
		bHaveInitialGuess = false;
		PreviousColoring = MakeUnique<TArray<TArray<int32>>>();
		PreviousColoring->SetNum(ElementsPerColor.Num());
	}
	ElementsPerSubColors.SetNum(ElementsPerColor.Num());

	//for (int32 i = 0; i < ElementsPerColor.Num(); i++) {
	PhysicsParallelFor(ElementsPerColor.Num(), [bHaveInitialGuess, &ElementsPerColor, &Grid, GridSize, &PreviousColoring, &ConstraintsNodesSet, &ElementsPerSubColors](const int32 Color)
		{
			const TArray<int32>& ColorElements = ElementsPerColor[Color];
			TArray<int32>& PreviousColoringElements = PreviousColoring->operator[](Color);
			if (!bHaveInitialGuess)
			{
				PreviousColoringElements.Init(-1, ColorElements.Num());
			}
			int32 NumNodes = GridSize;
			TArray<int32> ElementSubColors;
			ElementSubColors.Init(-1, ColorElements.Num());
			TArray<FColorSet> UsedColors;
			UsedColors.SetNum(NumNodes);
			int32 MaxColor = -1;
			for (int32 ElementIndex = 0; ElementIndex < ColorElements.Num(); ElementIndex++) 
			{
				int32 ColorToUse = 0;
				int32 Element = ColorElements[ElementIndex];
				// check initial guess:
				if (bHaveInitialGuess)
				{
					ColorToUse = PreviousColoringElements[ElementIndex];
					bool bColorFound = false;
					for (const int32 Node : ConstraintsNodesSet[Element]) 
					{
						if (UsedColors[Node].Contains(ColorToUse)) 
						{
							bColorFound = true;
							break;
						}
					}
					if (bColorFound) 
					{
						ColorToUse = 0;
					}
					else 
					{
						for (const int32 Node : ConstraintsNodesSet[Element])
						{
							UsedColors[Node].Emplace(ColorToUse);
						}
						ElementSubColors[ElementIndex] = ColorToUse;
					}
				}
				if (ElementSubColors[ElementIndex] == -1)
				{
					while (true) 
					{
						bool bColorFound = false;
						for (const int32 Node : ConstraintsNodesSet[Element]) 
						{
							if (UsedColors[Node].Contains(ColorToUse)) 
							{
								bColorFound = true;
								break;
							}
						}
						if (!bColorFound) {
							break;
						}
						ColorToUse++;
					}
					ElementSubColors[ElementIndex] = ColorToUse;
					for (const int32 Node : ConstraintsNodesSet[Element])
					{
						UsedColors[Node].Emplace(ColorToUse);
					}
				}

				// assign colors to previous guess for next timestep:
				MaxColor = FMath::Max(MaxColor, ColorToUse);
				PreviousColoringElements[ElementIndex] = ColorToUse;
			}

			ElementsPerSubColors[Color].Reset();
			ElementsPerSubColors[Color].SetNum(MaxColor + 1);

			for (int32 ElementIndex = 0; ElementIndex < ColorElements.Num(); ElementIndex++)
			{
				ElementsPerSubColors[Color][ElementSubColors[ElementIndex]].Emplace(ColorElements[ElementIndex]);
			}
		}, ElementsPerColor.Num() < 20);
	
	checkSlow(VerifyGridBasedSubColoring<T>(ElementsPerColor, Grid, ConstraintsNodesSet, ElementsPerSubColors));
}


template<typename T>
void Chaos::ComputeWeakConstraintsColoring(const TArray<TArray<int32>>& Indices, const TArray<TArray<int32>>& SecondIndices, const Chaos::TDynamicParticles<T, 3>& InParticles, TArray<TArray<int32>>& ConstraintsPerColor)
{
	TArray<TSet<int32>*> UsedColors;
	UsedColors.Init(nullptr, InParticles.Size());

	ensure(Indices.Num() == SecondIndices.Num() || SecondIndices.Num() == 0);

	TArray<int32> ConstraintColors;
	ConstraintColors.Init(-1, Indices.Num());

	if (SecondIndices.Num() == 0)
	{
		for (int32 ConstraintIndex = 0; ConstraintIndex < Indices.Num(); ConstraintIndex++)
		{
			for (int32 i = 0; i < Indices[ConstraintIndex].Num(); i++)
			{
				if (!UsedColors[Indices[ConstraintIndex][i]]) {
					UsedColors[Indices[ConstraintIndex][i]] = new TSet<int32>();
				}
				
			}
			int32 ColorToUse = 0;
			while (true)
			{
				bool ColorFound = false;
				for (auto node : Indices[ConstraintIndex]) {
					if (!UsedColors[node]) {
						UsedColors[node] = new TSet<int32>();
					}
					if (UsedColors[node]->Contains(ColorToUse)) {
						ColorFound = true;
						break;
					}
				}
				if (!ColorFound) {
					break;
				}
				ColorToUse++;
			}
			ConstraintColors[ConstraintIndex] = ColorToUse;
		}
	}
	else 
	{
		for (int32 ConstraintIndex = 0; ConstraintIndex < Indices.Num(); ConstraintIndex++)
		{
			for (int32 i = 0; i < Indices[ConstraintIndex].Num(); i++)
			{
				if (!UsedColors[Indices[ConstraintIndex][i]]) {
					UsedColors[Indices[ConstraintIndex][i]] = new TSet<int32>();
				}
			}
			for (int32 j = 0; j < SecondIndices[ConstraintIndex].Num(); j++)
			{
				if (!UsedColors[SecondIndices[ConstraintIndex][j]]) {
					UsedColors[SecondIndices[ConstraintIndex][j]] = new TSet<int32>();
				}
			}
			int32 ColorToUse = 0;
			while (true)
			{
				bool ColorFound = false;
				for (auto node : Indices[ConstraintIndex]) {
					if (!UsedColors[node]) {
						UsedColors[node] = new TSet<int32>();
					}
					if (UsedColors[node]->Contains(ColorToUse)) {
						ColorFound = true;
						break;
					}
				}
				for (auto node : SecondIndices[ConstraintIndex]) {
					if (!UsedColors[node]) {
						UsedColors[node] = new TSet<int32>();
					}
					if (UsedColors[node]->Contains(ColorToUse)) {
						ColorFound = true;
						break;
					}
				}
				if (!ColorFound) {
					break;
				}
				ColorToUse++;
			}
			ConstraintColors[ConstraintIndex] = ColorToUse;
			for (auto node : Indices[ConstraintIndex]) {
				UsedColors[node]->Emplace(ColorToUse);
			}
			for (auto node : SecondIndices[ConstraintIndex]) {
				UsedColors[node]->Emplace(ColorToUse);
			}
		}
	}

	for (int ii = 0; ii < UsedColors.Num(); ii++) {
		delete (UsedColors[ii]);
	}

	int32 NumColors = FMath::Max<int32>(ConstraintColors);

	//int32 num_colors = *std::max_element(ElementSubColors.begin(), ElementSubColors.end());
	ConstraintsPerColor.Empty();
	ConstraintsPerColor.SetNum(NumColors + 1);

	for (int32 j = 0; j < Indices.Num(); j++) 
	{
		ConstraintsPerColor[ConstraintColors[j]].Emplace(j);
	}

	checkSlow(VerifyWeakConstraintsColoring<T>(Indices, SecondIndices, InParticles, ConstraintsPerColor));
}


template<typename T>
TArray<TArray<int32>> Chaos::ComputeNodalColoring(const TArray<TVec4<int32>>& Graph, const Chaos::TDynamicParticles<T, 3>& InParticles, const int32 GraphParticlesStart, const int32 GraphParticlesEnd, const TArray<TArray<int32>>& IncidentElements, const TArray<TArray<int32>>& IncidentElementsLocalIndex)
{
	using namespace Chaos;

	checkSlow(GraphParticlesStart <= GraphParticlesEnd);
	checkSlow(GraphParticlesEnd <= (int32)InParticles.Size());
	TArray<TArray<int32>> ParticlesPerColor;

	TArray<int32> Particle2Incident;
	Particle2Incident.Init(-1, GraphParticlesEnd - GraphParticlesStart);
	//Assuming that offset of Graph is GraphParticlesStart
	for (int32 i = 0; i < IncidentElements.Num(); i++) 
	{
		if (IncidentElements[i].Num() > 0)
		{
			Particle2Incident[Graph[IncidentElements[i][0]][IncidentElementsLocalIndex[i][0]] - GraphParticlesStart] = i;
		}
	}

	TArray<TSet<int32>*> ElementColorsSet;
	ElementColorsSet.Init(nullptr, Graph.Num());
	TArray<int32> ParticleColors;
	ParticleColors.Init(-1, GraphParticlesEnd - GraphParticlesStart);

	for (int32 p = 0; p < GraphParticlesEnd - GraphParticlesStart; p++) 
	{
		int32 ParticleIndex = p + GraphParticlesStart;
		if (InParticles.InvM(ParticleIndex) != (T)0. && Particle2Incident[ParticleIndex] != -1) 
		{
			for (int32 j = 0; j < IncidentElements[Particle2Incident[ParticleIndex]].Num(); j++) 
			{
				int32 LocalIndex = IncidentElementsLocalIndex[Particle2Incident[ParticleIndex]][j];
				int32 e = IncidentElements[Particle2Incident[ParticleIndex]][j];
				if (!ElementColorsSet[e]) 
				{
					ElementColorsSet[e] = new TSet<int32>();
				}
			}
			int32 color_to_use = 0;
			while (true) 
			{
				bool ColorFound = false;
				for (int32 j = 0; j < IncidentElements[Particle2Incident[ParticleIndex]].Num(); j++) 
				{
					int32 e = IncidentElements[Particle2Incident[ParticleIndex]][j];
					if (ElementColorsSet[e]->Contains(color_to_use)) 
					{
						ColorFound = true;
						break;
					}
				}
				if (!ColorFound) 
				{
					ParticleColors[p] = color_to_use;
					for (int32 j = 0; j < IncidentElements[Particle2Incident[ParticleIndex]].Num(); j++) 
					{
						int32 e = IncidentElements[Particle2Incident[ParticleIndex]][j];
						ElementColorsSet[e]->Emplace(color_to_use);
						if (ElementColorsSet[e]->Num() == 4) 
						{
							delete ElementColorsSet[e];
							ElementColorsSet[e] = nullptr;
						}
					}
					break;
				}
				color_to_use++;
			}
		}
	}

	for (int32 ii = 0; ii < ElementColorsSet.Num(); ii++) 
	{
		if (ElementColorsSet[ii]) 
		{
			delete (ElementColorsSet[ii]);
		}
	}

	int32 SizeColors = FMath::Max<int32>(ParticleColors);

	ParticlesPerColor.SetNum(0);
	ParticlesPerColor.Init(TArray<int32>(), SizeColors + 1);

	for (int32 j = 0; j < ParticleColors.Num(); j++) 
	{
		if (ParticleColors[j] != -1) 
		{
			ParticlesPerColor[ParticleColors[j]].Emplace(j + GraphParticlesStart);
		}
	}

	checkSlow(VerifyNodalColoring<T>(Graph, InParticles, GraphParticlesStart, GraphParticlesEnd, IncidentElements, IncidentElementsLocalIndex, ParticlesPerColor));

	return ParticlesPerColor;
}


template<typename T, typename ParticleType>
TArray<TArray<int32>> Chaos::ComputeNodalColoring(const TArray<TArray<int32>>& Graph, const Chaos::TDynamicParticles<T, 3>& InParticles, const int32 GraphParticlesStart, const int32 GraphParticlesEnd, const TArray<TArray<int32>>& IncidentElements, const TArray<TArray<int32>>& IncidentElementsLocalIndex, const TPBDActiveView<ParticleType>* InParticleActiveView, TArray<int32>* ParticleColorsOut)
{
	using namespace Chaos;

	checkSlow(GraphParticlesStart <= GraphParticlesEnd);
	checkSlow(GraphParticlesEnd <= (int32)InParticles.Size());
	checkSlow(InParticles.Size() == IncidentElements.Num());
	TArray<TArray<int32>> ParticlesPerColor;

	TArray<int32> Particle2Incident;
	Particle2Incident.Init(INDEX_NONE, GraphParticlesEnd - GraphParticlesStart);
	//Assuming that offset of Graph is GraphParticlesStart
	for (int32 i = 0; i < GraphParticlesEnd-GraphParticlesStart; i++)
	{
		Particle2Incident[i] = i+GraphParticlesStart;
	}

	TArray<TSet<int32>*> ElementColorsSet;
	ElementColorsSet.Init(nullptr, Graph.Num());
	TArray<int32> ParticleColors;
	ParticleColors.Init(INDEX_NONE, GraphParticlesEnd - GraphParticlesStart);

	if (InParticleActiveView)
	{
		InParticleActiveView->SequentialFor([&ElementColorsSet, &Graph, &GraphParticlesStart, &IncidentElements, &IncidentElementsLocalIndex, &Particle2Incident, &ParticleColors]
		(const Chaos::TDynamicParticles<T, 3>& InParticles, int32 ParticleIndex)
			{
				const int32 p = ParticleIndex - GraphParticlesStart;
				if (InParticles.InvM(ParticleIndex) != (T)0. && Particle2Incident[ParticleIndex] != INDEX_NONE) 
				{
					for (int32 j = 0; j < IncidentElements[Particle2Incident[ParticleIndex]].Num(); j++) 
					{
						int32 LocalIndex = IncidentElementsLocalIndex[Particle2Incident[ParticleIndex]][j];
						int32 e = IncidentElements[Particle2Incident[ParticleIndex]][j];
						if (!ElementColorsSet[e]) 
						{
							ElementColorsSet[e] = new TSet<int32>();
						}
					}
					int32 color_to_use = 0;
					while (true) 
					{
						bool ColorFound = false;
						for (int32 j = 0; j < IncidentElements[Particle2Incident[ParticleIndex]].Num(); j++) 
						{
							int32 e = IncidentElements[Particle2Incident[ParticleIndex]][j];
							if (ElementColorsSet[e]->Contains(color_to_use)) 
							{
								ColorFound = true;
								break;
							}
						}
						if (!ColorFound) 
						{
							ParticleColors[p] = color_to_use;
							for (int32 j = 0; j < IncidentElements[Particle2Incident[ParticleIndex]].Num(); j++) 
							{
								int32 e = IncidentElements[Particle2Incident[ParticleIndex]][j];
								ElementColorsSet[e]->Emplace(color_to_use);
								if (ElementColorsSet[e]->Num() == Graph[e].Num())
								{
									delete ElementColorsSet[e];
									ElementColorsSet[e] = nullptr;
								}
							}
							break;
						}
						color_to_use++;
					}
				}
			});
	}
	else
	{
		for (int32 p = 0; p < GraphParticlesEnd - GraphParticlesStart; p++) 
		{
			int32 ParticleIndex = p + GraphParticlesStart;
			if (InParticles.InvM(ParticleIndex) != (T)0. && Particle2Incident[ParticleIndex] != INDEX_NONE) 
			{
				for (int32 j = 0; j < IncidentElements[Particle2Incident[ParticleIndex]].Num(); j++) 
				{
					int32 LocalIndex = IncidentElementsLocalIndex[Particle2Incident[ParticleIndex]][j];
					int32 e = IncidentElements[Particle2Incident[ParticleIndex]][j];
					if (!ElementColorsSet[e]) 
					{
						ElementColorsSet[e] = new TSet<int32>();
					}
				}
				int32 color_to_use = 0;
				while (true) 
				{
					bool ColorFound = false;
					for (int32 j = 0; j < IncidentElements[Particle2Incident[ParticleIndex]].Num(); j++) 
					{
						int32 e = IncidentElements[Particle2Incident[ParticleIndex]][j];
						if (ElementColorsSet[e]->Contains(color_to_use)) 
						{
							ColorFound = true;
							break;
						}
					}
					if (!ColorFound) 
					{
						ParticleColors[p] = color_to_use;
						for (int32 j = 0; j < IncidentElements[Particle2Incident[ParticleIndex]].Num(); j++) 
						{
							int32 e = IncidentElements[Particle2Incident[ParticleIndex]][j];
							ElementColorsSet[e]->Emplace(color_to_use);
							if (ElementColorsSet[e]->Num() == Graph[e].Num())
							{
								delete ElementColorsSet[e];
								ElementColorsSet[e] = nullptr;
							}
						}
						break;
					}
					color_to_use++;
				}
			}
		}
	}

	for (int32 ii = 0; ii < ElementColorsSet.Num(); ii++) 
	{
		if (ElementColorsSet[ii]) 
		{
			delete (ElementColorsSet[ii]);
		}
	}

	int32 SizeColors = FMath::Max<int32>(ParticleColors);

	ParticlesPerColor.SetNum(0);
	ParticlesPerColor.Init(TArray<int32>(), SizeColors + 1);

	for (int32 j = 0; j < ParticleColors.Num(); j++) 
	{
		if (ParticleColors[j] != INDEX_NONE) 
		{
			ParticlesPerColor[ParticleColors[j]].Emplace(j + GraphParticlesStart);
		}
	}

	if (ParticleColorsOut)
	{
		*ParticleColorsOut = ParticleColors;
	}

	checkSlow(VerifyNodalColoring<T>(Graph, InParticles, GraphParticlesStart, GraphParticlesEnd, IncidentElements, IncidentElementsLocalIndex, ParticlesPerColor));

	return ParticlesPerColor;
}


template<typename T>
void Chaos::ComputeExtraNodalColoring(const TArray<TArray<int32>>& Graph, const TArray<TArray<int32>>& ExtraGraph, const Chaos::TDynamicParticles<T, 3>& InParticles, const TArray<TArray<int32>>& IncidentElements, const TArray<TArray<int32>>& ExtraIncidentElements, TArray<int32>& ParticleColors, TArray<TArray<int32>>& ParticlesPerColor)
{
	TBitArray ParticleIsAffected(false, IncidentElements.Num());
	for (int32 i = 0; i < ExtraIncidentElements.Num(); i++)
	{
		if (ExtraIncidentElements[i].Num() > 0)
		{
			ParticleIsAffected[i] = true;
		}
	}

	TSet<int32> UsedColors;
	for (int32 i = 0; i < ExtraIncidentElements.Num(); i++)
	{
		if (ParticleIsAffected[i])
		{
			int32 OriginalColor = ParticleColors[i];
			ParticleColors[i] = INDEX_NONE;
			if (OriginalColor != INDEX_NONE)
			{
				UsedColors.Reset();
				UsedColors.Reserve(ExtraIncidentElements[i].Num() + IncidentElements[i].Num());
				for (int32 j = 0; j < IncidentElements[i].Num(); j++)
				{
					int32 ConstraintIndex = IncidentElements[i][j];
					for (int32 ie = 0; ie < Graph[ConstraintIndex].Num(); ie++)
					{
						UsedColors.Add(ParticleColors[Graph[ConstraintIndex][ie]]);
					}
				}
				for (int32 j = 0; j < ExtraIncidentElements[i].Num(); j++)
				{
					int32 ConstraintIndex = ExtraIncidentElements[i][j];
					for (int32 ie = 0; ie < ExtraGraph[ConstraintIndex].Num(); ie++)
					{
						UsedColors.Add(ParticleColors[ExtraGraph[ConstraintIndex][ie]]);
					}
				}
				if (UsedColors.Contains(OriginalColor))
				{
					OriginalColor = 0;
					while (UsedColors.Contains(OriginalColor))
					{
						OriginalColor++;
					}
					ParticleColors[i] = OriginalColor;
				}
				else
				{
					ParticleColors[i] = OriginalColor;
					ParticleIsAffected[i] = false;
				}
			}
		}
	}


	PhysicsParallelFor(ParticlesPerColor.Num(), [&](const int32 i)
		{
			int32 CurrentIndex = 0;
			for (int32 j = 0; j < ParticlesPerColor[i].Num(); j++)
			{
				if (!ParticleIsAffected[ParticlesPerColor[i][j]])
				{
					if (CurrentIndex != j)
					{
						ParticlesPerColor[i][CurrentIndex] = ParticlesPerColor[i][j];
					}
					CurrentIndex += 1;
				}
			}
			ParticlesPerColor[i].SetNum(CurrentIndex);
		}, ParticlesPerColor.Num() > 0 && ParticlesPerColor[0].Num() < 1000);

	ParticlesPerColor.SetNum(FMath::Max<int32>(ParticleColors) + 1);
	for (int32 i = 0; i < ExtraIncidentElements.Num(); i++)
	{
		if (ParticleIsAffected[i] && ParticleColors[i] != INDEX_NONE)
		{
			ParticlesPerColor[ParticleColors[i]].Emplace(i);
		}
	}

	checkSlow(VerifyExtraNodalColoring<T>(InParticles, Graph, ExtraGraph, IncidentElements, ExtraIncidentElements, ParticleColors, ParticlesPerColor));
}

template<typename T>
void Chaos::ComputeExtraNodalColoring(const TArray<TArray<int32>>& StaticGraph, const TArray<TArray<int32>>& DynamicGraph, const TArray<TArray<int32>>& ExtraGraph, const Chaos::TDynamicParticles<T, 3>& InParticles, const TArray<TArray<int32>>& StaticIncidentElements, const TArray<TArray<int32>>& DynamicIncidentElements, const TArray<TArray<int32>>& ExtraIncidentElements, TArray<int32>& ParticleColors, TArray<TArray<int32>>& ParticlesPerColor)
{
	TBitArray ParticleIsAffected(false, InParticles.Size());
	for (int32 i = 0; i < ExtraIncidentElements.Num(); i++)
	{
		if (ExtraIncidentElements[i].Num() > 0)
		{
			ParticleIsAffected[i] = true;
		}
	}

	TSet<int32> UsedColors;
	for (int32 i = 0; i < ExtraIncidentElements.Num(); i++)
	{
		if (ParticleIsAffected[i])
		{
			int32 OriginalColor = ParticleColors[i];
			ParticleColors[i] = INDEX_NONE;
			if (OriginalColor != INDEX_NONE)
			{
				UsedColors.Reset();
				UsedColors.Reserve(ExtraIncidentElements[i].Num() + StaticIncidentElements[i].Num() + DynamicIncidentElements[i].Num());
				for (int32 j = 0; j < StaticIncidentElements[i].Num(); j++)
				{
					int32 ConstraintIndex = StaticIncidentElements[i][j];
					for (int32 ie = 0; ie < StaticGraph[ConstraintIndex].Num(); ie++)
					{
						UsedColors.Add(ParticleColors[StaticGraph[ConstraintIndex][ie]]);
					}
				}
				for (int32 j = 0; j < DynamicIncidentElements[i].Num(); j++)
				{
					int32 ConstraintIndex = DynamicIncidentElements[i][j];
					for (int32 ie = 0; ie < DynamicGraph[ConstraintIndex].Num(); ie++)
					{
						UsedColors.Add(ParticleColors[DynamicGraph[ConstraintIndex][ie]]);
					}
				}
				for (int32 j = 0; j < ExtraIncidentElements[i].Num(); j++)
				{
					int32 ConstraintIndex = ExtraIncidentElements[i][j];
					for (int32 ie = 0; ie < ExtraGraph[ConstraintIndex].Num(); ie++)
					{
						UsedColors.Add(ParticleColors[ExtraGraph[ConstraintIndex][ie]]);
					}
				}
				if (UsedColors.Contains(OriginalColor))
				{
					OriginalColor = 0;
					while (UsedColors.Contains(OriginalColor))
					{
						OriginalColor++;
					}
					ParticleColors[i] = OriginalColor;
				}
				else
				{
					ParticleColors[i] = OriginalColor;
					ParticleIsAffected[i] = false;
				}
			}
		}
	}


	PhysicsParallelFor(ParticlesPerColor.Num(), [&](const int32 i)
		{
			int32 CurrentIndex = 0;
			for (int32 j = 0; j < ParticlesPerColor[i].Num(); j++)
			{
				if (!ParticleIsAffected[ParticlesPerColor[i][j]])
				{
					if (CurrentIndex != j)
					{
						ParticlesPerColor[i][CurrentIndex] = ParticlesPerColor[i][j];
					}
					CurrentIndex += 1;
				}
			}
			ParticlesPerColor[i].SetNum(CurrentIndex);
		}, ParticlesPerColor.Num() > 0 && ParticlesPerColor[0].Num() < 1000);

	ParticlesPerColor.SetNum(FMath::Max<int32>(ParticleColors) + 1);
	for (int32 i = 0; i < ExtraIncidentElements.Num(); i++)
	{
		if (ParticleIsAffected[i] && ParticleColors[i] != INDEX_NONE)
		{
			ParticlesPerColor[ParticleColors[i]].Emplace(i);
		}
	}

	checkSlow(VerifyExtraNodalColoring<T>(InParticles, StaticGraph, DynamicGraph, ExtraGraph, StaticIncidentElements, DynamicIncidentElements, ExtraIncidentElements, ParticleColors, ParticlesPerColor));
}

#define UE_COMPUTE_GRAPH_COLORING_PARTICLES_OR_RANGE_N_HELPER(N, bAllDynamic) \
template CHAOS_API TArray<TArray<int32>> Chaos::FGraphColoring::ComputeGraphColoringParticlesOrRange<Chaos::TDynamicParticles<Chaos::FRealSingle, 3>, N, bAllDynamic>(const TArray<Chaos::TVector<int32, N>>&, const Chaos::TDynamicParticles<Chaos::FRealSingle, 3>&, const int32 GraphParticlesStart, const int32 GraphParticlesEnd); \
template CHAOS_API TArray<TArray<int32>> Chaos::FGraphColoring::ComputeGraphColoringParticlesOrRange<Chaos::TDynamicParticles<Chaos::FRealDouble, 3>, N, bAllDynamic>(const TArray<Chaos::TVector<int32, N>>&, const Chaos::TDynamicParticles<Chaos::FRealDouble, 3>&, const int32 GraphParticlesStart, const int32 GraphParticlesEnd); \
template CHAOS_API TArray<TArray<int32>> Chaos::FGraphColoring::ComputeGraphColoringParticlesOrRange<Chaos::Softs::FSolverParticles, N, bAllDynamic>(const TArray<Chaos::TVector<int32, N>>&, const Chaos::Softs::FSolverParticles&, const int32 GraphParticlesStart, const int32 GraphParticlesEnd); \
template CHAOS_API TArray<TArray<int32>> Chaos::FGraphColoring::ComputeGraphColoringParticlesOrRange<Chaos::Softs::FSolverParticlesRange, N, bAllDynamic>(const TArray<Chaos::TVector<int32, N>>&, const Chaos::Softs::FSolverParticlesRange&, const int32 GraphParticlesStart, const int32 GraphParticlesEnd);

UE_COMPUTE_GRAPH_COLORING_PARTICLES_OR_RANGE_N_HELPER(2, false)
UE_COMPUTE_GRAPH_COLORING_PARTICLES_OR_RANGE_N_HELPER(3, false)
UE_COMPUTE_GRAPH_COLORING_PARTICLES_OR_RANGE_N_HELPER(4, false)
UE_COMPUTE_GRAPH_COLORING_PARTICLES_OR_RANGE_N_HELPER(6, false)
UE_COMPUTE_GRAPH_COLORING_PARTICLES_OR_RANGE_N_HELPER(4, true)

template CHAOS_API void Chaos::ComputeGridBasedGraphSubColoringPointer(const TArray<TArray<int32>>& ElementsPerColor, const TMPMGrid<Chaos::FRealSingle>& Grid, const int32 GridSize, TUniquePtr<TArray<TArray<int32>>>& PreviousColoring, const TArray<TArray<int32>>& ConstraintsNodesSet, TArray<TArray<TArray<int32>>>& ElementsPerSubColors);
template CHAOS_API void Chaos::ComputeGridBasedGraphSubColoringPointer(const TArray<TArray<int32>>& ElementsPerColor, const TMPMGrid<Chaos::FRealDouble>& Grid, const int32 GridSize, TUniquePtr<TArray<TArray<int32>>>& PreviousColoring, const TArray<TArray<int32>>& ConstraintsNodesSet, TArray<TArray<TArray<int32>>>& ElementsPerSubColors);

template CHAOS_API void Chaos::ComputeWeakConstraintsColoring<Chaos::FRealSingle>(const TArray<TArray<int32>>& Indices, const TArray<TArray<int32>>& SecondIndices, const Chaos::TDynamicParticles<Chaos::FRealSingle, 3>& InParticles, TArray<TArray<int32>>& ConstraintsPerColor);
template CHAOS_API void Chaos::ComputeWeakConstraintsColoring<Chaos::FRealDouble>(const TArray<TArray<int32>>& Indices, const TArray<TArray<int32>>& SecondIndices, const Chaos::TDynamicParticles<Chaos::FRealDouble, 3>& InParticles, TArray<TArray<int32>>& ConstraintsPerColor);

template CHAOS_API TArray<TArray<int32>> Chaos::ComputeNodalColoring<Chaos::FRealSingle>(const TArray<TVec4<int32>>& Graph, const Chaos::TDynamicParticles<Chaos::FRealSingle, 3>& InParticles, const int32 GraphParticlesStart, const int32 GraphParticlesEnd, const TArray<TArray<int32>>& IncidentElements, const TArray<TArray<int32>>& IncidentElementsLocalIndex);
template CHAOS_API TArray<TArray<int32>> Chaos::ComputeNodalColoring<Chaos::FRealDouble>(const TArray<TVec4<int32>>& Graph, const Chaos::TDynamicParticles<Chaos::FRealDouble, 3>& InParticles, const int32 GraphParticlesStart, const int32 GraphParticlesEnd, const TArray<TArray<int32>>& IncidentElements, const TArray<TArray<int32>>& IncidentElementsLocalIndex);

template CHAOS_API TArray<TArray<int32>> Chaos::ComputeNodalColoring<Chaos::FRealSingle>(const TArray<TArray<int32>>& Graph, const Chaos::TDynamicParticles<Chaos::FRealSingle, 3>& InParticles, const int32 GraphParticlesStart, const int32 GraphParticlesEnd, const TArray<TArray<int32>>& IncidentElements, const TArray<TArray<int32>>& IncidentElementsLocalIndex, const TPBDActiveView<Chaos::Softs::FSolverParticles>* InParticleActiveView, TArray<int32>* ParticleColorsOut);
template CHAOS_API TArray<TArray<int32>> Chaos::ComputeNodalColoring<Chaos::FRealDouble>(const TArray<TArray<int32>>& Graph, const Chaos::TDynamicParticles<Chaos::FRealDouble, 3>& InParticles, const int32 GraphParticlesStart, const int32 GraphParticlesEnd, const TArray<TArray<int32>>& IncidentElements, const TArray<TArray<int32>>& IncidentElementsLocalIndex, const TPBDActiveView<Chaos::TDynamicParticles<Chaos::FRealDouble, 3>>* InParticleActiveView, TArray<int32>* ParticleColorsOut);

template CHAOS_API void Chaos::ComputeExtraNodalColoring<Chaos::FRealSingle>(const TArray<TArray<int32>>& Graph, const TArray<TArray<int32>>& ExtraGraph, const Chaos::TDynamicParticles<Chaos::FRealSingle, 3>& InParticles, const TArray<TArray<int32>>& IncidentElements, const TArray<TArray<int32>>& ExtraIncidentElements, TArray<int32>& ParticleColors, TArray<TArray<int32>>& ParticlesPerColor);
template CHAOS_API void Chaos::ComputeExtraNodalColoring<Chaos::FRealDouble>(const TArray<TArray<int32>>& Graph, const TArray<TArray<int32>>& ExtraGraph, const Chaos::TDynamicParticles<Chaos::FRealDouble, 3>& InParticles, const TArray<TArray<int32>>& IncidentElements, const TArray<TArray<int32>>& ExtraIncidentElements, TArray<int32>& ParticleColors, TArray<TArray<int32>>& ParticlesPerColor);

template CHAOS_API void Chaos::ComputeExtraNodalColoring<Chaos::FRealSingle>(const TArray<TArray<int32>>& StaticGraph, const TArray<TArray<int32>>& DynamicGraph, const TArray<TArray<int32>>& ExtraGraph, const Chaos::TDynamicParticles<Chaos::FRealSingle, 3>& InParticles, const TArray<TArray<int32>>& StaticIncidentElements, const TArray<TArray<int32>>& DynamicIncidentElements, const TArray<TArray<int32>>& ExtraIncidentElements, TArray<int32>& ParticleColors, TArray<TArray<int32>>& ParticlesPerColor);
template CHAOS_API void Chaos::ComputeExtraNodalColoring<Chaos::FRealDouble>(const TArray<TArray<int32>>& StaticGraph, const TArray<TArray<int32>>& DynamicGraph, const TArray<TArray<int32>>& ExtraGraph, const Chaos::TDynamicParticles<Chaos::FRealDouble, 3>& InParticles, const TArray<TArray<int32>>& StaticIncidentElements, const TArray<TArray<int32>>& DynamicIncidentElements, const TArray<TArray<int32>>& ExtraIncidentElements, TArray<int32>& ParticleColors, TArray<TArray<int32>>& ParticlesPerColor);