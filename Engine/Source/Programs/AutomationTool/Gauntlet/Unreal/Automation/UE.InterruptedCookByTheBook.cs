// Copyright Epic Games, Inc. All Rights Reserved.

using Gauntlet;

namespace UE
{
	public class InterruptedCookByTheBook : CookByTheBookEditors
	{
		public InterruptedCookByTheBook(UnrealTestContext InContext) : base(InContext)
		{
			// Do nothing
		}

		public override void TickTest()
		{
			base.TickTest();

			if (!IsEditorRestarted && Checker.HasValidated(CookingInProgressKey))
			{
				RestartEditorRole();
			}
		}
	}
}
