// Copyright Epic Games, Inc. All Rights Reserved.

using static Gauntlet.UnrealSessionInstance;
using System.Linq;

namespace Gauntlet
{
	public static class CookHelpers
	{
		public static bool TryLaunchDeferredRole(UnrealSessionInstance TestInstance, UnrealTargetRole RoleType)
		{
			RoleInstance DeferredRole = TestInstance
				?.DeferredRoles
				?.FirstOrDefault(R => R.Role.RoleType == RoleType);

			if (DeferredRole is null || !TestInstance.LaunchDeferredRole(DeferredRole.Role))
			{
				Log.Error($"Couldn't launch the deferred role {RoleType}");
				return false;
			}

			return true;
		}
	}
}
