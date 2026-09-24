// Copyright Epic Games, Inc. All Rights Reserved.

using System.Linq;
using System.Collections.Generic;

namespace UnrealBuildTool.Rules
{
    public class Chaos_Windows : Chaos
    {
        public Chaos_Windows(ReadOnlyTargetRules Target) : base(Target)
        {
			// Disabling the optimized VectorGather call because it is causing crashes on PC.
			PublicDefinitions.Add("USE_ISPC_OPTIMIZED_VECTORGATHER=0");
		}
    }
}
