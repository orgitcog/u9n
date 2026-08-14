// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Windows.Forms;

namespace UnrealGameSync
{
	public class ThemedForm : Form
	{
		protected override void OnHandleCreated(EventArgs e)
		{
			base.OnHandleCreated(e);

			ThemeManager?.ApplyThemeRecursively(this);
		}
		
		protected static ApplicationThemeManager? ThemeManager => ProgramApplicationContext.GetThemeManager();
		protected static ApplicationTheme Theme => ProgramApplicationContext.GetTheme();
	}
}
