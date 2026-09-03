// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using EpicGames.Core;
using EpicGames.UHT.Tables;
using EpicGames.UHT.Types;
using EpicGames.UHT.Utils;

namespace EpicGames.UHT.Parsers
{

	/// <summary>
	/// Collection of UENUM specifiers
	/// </summary>
	[UnrealHeaderTool]
	[SuppressMessage("CodeQuality", "IDE0051:Remove unused private members", Justification = "Attribute accessed method")]
	public static class UhtFieldSpecifiers
	{
		[UhtSpecifier(Extends = UhtTableNames.ScriptStruct, ValueType = UhtSpecifierValueType.OptionalEqualsKeyValuePairList)]
		[UhtSpecifier(Extends = UhtTableNames.Class, ValueType = UhtSpecifierValueType.OptionalEqualsKeyValuePairList)]
		[UhtSpecifier(Extends = UhtTableNames.Enum, ValueType = UhtSpecifierValueType.OptionalEqualsKeyValuePairList)]
		[UhtSpecifier(Extends = UhtTableNames.Interface, ValueType = UhtSpecifierValueType.OptionalEqualsKeyValuePairList)]
		[UhtSpecifier(Extends = UhtTableNames.NativeInterface, ValueType = UhtSpecifierValueType.OptionalEqualsKeyValuePairList)]
		private static void VerseSpecifier(UhtSpecifierContext specifierContext, List<KeyValuePair<StringView, StringView>> values)
		{
			UhtField fieldObj = (UhtField)specifierContext.Type;

			// Extract all the elements of the meta data
			foreach (KeyValuePair<StringView, StringView> kvp in values)
			{
				ReadOnlySpan<char> key = kvp.Key.Span;
				if (key.Equals("name", StringComparison.OrdinalIgnoreCase))
				{
					fieldObj.VerseName = kvp.Value.ToString();
				}
				else if (key.Equals("module", StringComparison.OrdinalIgnoreCase))
				{
					fieldObj.VerseModule = kvp.Value.ToString();
				}
				else if (key.Equals("noalias", StringComparison.OrdinalIgnoreCase))
				{
					fieldObj.FieldExportFlags |= UhtFieldExportFlags.NoVerseAlias;
				}
				else if (key.Equals("parametric", StringComparison.OrdinalIgnoreCase))
				{
					if (fieldObj is UhtScriptStruct scriptStruct)
					{
						scriptStruct.ScriptStructExportFlags |= UhtScriptStructExportFlags.IsVerseParametric;
					}
					else
					{
						fieldObj.LogError($"Verse specifier option '{key}' is only valid on USTRUCTs");
					}
				}
				else if (key.Equals("experimental", StringComparison.OrdinalIgnoreCase))
				{
					if (fieldObj is UhtClass classObj)
					{
						classObj.MetaData.Add(classObj.Session.Config!.ValkyrieDevelopmentStatusKey, classObj.Session.Config!.ValkyrieDevelopmentStatusValueExperimental);
					}
					else
					{
						fieldObj.LogError($"Verse specifier option '{key}' is only valid on UCLASSes");
					}
				}
				else if (key.Equals("deprecated", StringComparison.OrdinalIgnoreCase))
				{
					if (fieldObj is UhtClass classObj)
					{
						classObj.MetaData.Add(classObj.Session.Config!.ValkyrieDeprecationStatusKey, classObj.Session.Config!.ValkyrieDeprecationStatusValueDeprecated);
					}
					else
					{
						fieldObj.LogError($"Verse specifier option '{key}' is only valid on UCLASSes");
					}
				}
				else
				{
					fieldObj.LogError($"Verse specifier option '{key}' is unknown");
				}
			}

			// If no name was specified, generate one using the following algorithm FVerseXxYyZz -> xx_yy_zz
			if (String.IsNullOrEmpty(fieldObj.VerseName))
			{
				ReadOnlySpan<char> span = StripVersePrefix(fieldObj);
				if (!span.IsEmpty)
				{
					using BorrowStringBuilder borrowStringBuilder = new BorrowStringBuilder(StringBuilderCache.Small);
					bool first = true;
					foreach (char c in span)
					{
						if (c >= 'A' && c <= 'Z')
						{
							if (!first)
							{
								borrowStringBuilder.StringBuilder.Append('_');
							}
							borrowStringBuilder.StringBuilder.Append(Char.ToLower(c));
						}
						else
						{
							borrowStringBuilder.StringBuilder.Append(c);
						}
						first = false;
					}
					fieldObj.VerseName = borrowStringBuilder.StringBuilder.ToString();
				}
			}

			// Final validation
			if (!fieldObj.IsVerseField)
			{
				fieldObj.LogError($"Verse specifier must include the name or the source name prefixed with EVerse, UVerse, AVerse, or FVerse depending on the type");
			}
			else if (fieldObj.Outer is not UhtPackage)
			{
				fieldObj.LogError($"Verse specifier can only appear on top level classes and structures");
			}
			else
			{
				if (String.IsNullOrEmpty(fieldObj.VerseModule))
				{
					fieldObj.EngineName = fieldObj.VerseName!;
				}
				else
				{
					fieldObj.EngineName = $"{fieldObj.VerseModule}_{fieldObj.VerseName!}";
				}

				using BorrowStringBuilder borrowBuilder = new(StringBuilderCache.Small);
				borrowBuilder.StringBuilder.AppendVerseUEVNIPackageName(fieldObj);
				fieldObj.Outer = fieldObj.Module.CreatePackage(borrowBuilder.StringBuilder.ToString());
			}
		}

		private static ReadOnlySpan<char> StripVersePrefix(UhtField fieldObj)
		{
			// For enums, enforce 
			if (fieldObj is UhtEnum)
			{
				if (fieldObj.EngineName.StartsWith("EVerse", StringComparison.Ordinal))
				{
					return fieldObj.EngineName.AsSpan(6);
				}
			}
			else
			{
				if (fieldObj.EngineName.StartsWith("Verse", StringComparison.Ordinal))
				{
					return fieldObj.EngineName.AsSpan(5);
				}
			}
			return new();
		}
	}
}
