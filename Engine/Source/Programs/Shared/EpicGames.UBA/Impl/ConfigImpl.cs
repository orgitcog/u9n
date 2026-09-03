// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Runtime.InteropServices;

namespace EpicGames.UBA.Impl
{
	internal class ConfigImpl : IConfig
	{
		#region DllImport
		[DllImport("UbaHost", CharSet = CharSet.Auto)]
		static extern nint Config_Load(string configFile);
		[DllImport("UbaHost", CharSet = CharSet.Auto)]
		static extern nint Config_AddTable(nint config, string name);
		[DllImport("UbaHost", CharSet = CharSet.Auto)]
		static extern nint ConfigTable_AddValueString(nint table, string name, string value);
		#endregion

		readonly nint _handle = IntPtr.Zero;

		public ConfigImpl(string configFile)
		{
			_handle = Config_Load(configFile);
		}

		public void AddValue(string table,string name, string value)
		{
			nint tablePtr = Config_AddTable(_handle, table);
			ConfigTable_AddValueString(tablePtr, name, value);
		}
	}
}