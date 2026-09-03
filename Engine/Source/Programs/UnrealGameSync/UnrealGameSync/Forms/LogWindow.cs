// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealGameSync
{
	public partial class LogWindow : ThemedForm
	{
		public LogWindow(string text)
		{
			InitializeComponent();

			LogTextBox.Text = text;
			LogTextBox.Select(text.Length, 0);
		}
	}
}
