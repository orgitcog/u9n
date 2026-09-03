// Copyright Epic Games, Inc. All Rights Reserved.

using System.Linq;
using Gauntlet;

namespace UE
{
	/// <summary>
	/// Implements the functionality of testing the cooking process with the possibility of restarting the cooking in a new editor
	/// </summary>
	public class CookByTheBookEditors : CookByTheBookEditor
	{
		protected bool IsEditorRestarted;

		public CookByTheBookEditors(UnrealTestContext InContext) : base(InContext)
		{
			// Do nothing
		}

		public override void CleanupTest()
		{
			IsEditorRestarted = false;
			CleanContentDir();
		}

		protected override void SetEditorRole(UnrealTestConfiguration Config)
		{
			UnrealTestRole[] EditorRoles = Config.RequireRoles(UnrealTargetRole.Editor, 2).ToArray();

			string TargetPlatformName = GetTargetPlatformName();
			string CommandLine = $@" {BaseEditorCommandLine} -targetplatform={TargetPlatformName}";

			EditorRoles[0].CommandLine += CommandLine;
			EditorRoles[1].CommandLine += CommandLine;
			EditorRoles[1].DeferredLaunch = true;
		}

		protected void RestartEditorRole()
		{
			Log.Info("Restart the editor");

			StopRunningEditor();
			MarkTestStarted(); // to prevent the test from being marked as completed and continue to run

			if (!CookHelpers.TryLaunchDeferredRole(UnrealApp.SessionInstance, UnrealTargetRole.Editor))
			{
				CompleteTest(TestResult.Failed);
				return;
			}

			InitTest();
			IsEditorRestarted = true;
		}

		private void StopRunningEditor()
		{
			if (GetRunningEditor() is not { } EditorRole)
			{
				Log.Error("Couldn't stop the Editor");
				CompleteTest(TestResult.Failed);
				return;
			}

			EditorRole.Kill();
		}
	}
}
