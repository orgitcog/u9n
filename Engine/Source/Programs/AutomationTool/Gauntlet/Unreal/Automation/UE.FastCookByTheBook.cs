// Copyright Epic Games, Inc. All Rights Reserved.

using Gauntlet;

namespace UE
{
	public class FastCookByTheBook : CookByTheBook
	{
		public FastCookByTheBook(UnrealTestContext InContext) : base(InContext)
		{
			BaseEditorCommandLine += " -fastcook";
		}

		protected override ContentFolder GetExpectedCookedContent()
		{
			ContentFolder CookedContent = base.GetExpectedCookedContent();
			CookedContent.Files.Add("Fastcook.txt");

			return CookedContent;
		}
	}
}
