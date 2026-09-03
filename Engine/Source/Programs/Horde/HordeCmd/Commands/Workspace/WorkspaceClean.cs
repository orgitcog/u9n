// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Horde.Storage;

namespace Horde.Commands.Workspace
{
	using Workspace = EpicGames.Horde.Storage.Workspace;

	[Command("workspace", "clean", "Removes all untracked files from the workspace")]
	class WorkspaceClean : WorkspaceStatusBase
	{
		protected override Task ExecuteInternalAsync(Workspace workspace, FileFilter? filter, WorkspaceStatusFlags flags, CancellationToken cancellationToken)
			=> workspace.CleanAsync(filter, flags, cancellationToken: cancellationToken);
	}
}
