// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Globalization;
using System.Text;
using System.Text.Json.Serialization;
using EpicGames.UHT.Utils;

namespace EpicGames.UHT.Types
{

	/// <summary>
	/// Series of flags not part of the engine's field flags that affect code generation or verification
	/// </summary>
	[Flags]
	public enum UhtFieldExportFlags : int
	{

		/// <summary>
		/// No export flags
		/// </summary>
		None = 0,

		/// <summary>
		/// Do not generate an alias in the verse namespace
		/// </summary>
		NoVerseAlias = 1 << 0,
	}

	/// <summary>
	/// Helper methods for testing flags.  These methods perform better than the generic HasFlag which hits
	/// the GC and stalls.
	/// </summary>
	public static class UhtFieldExportFlagsExtensions
	{

		/// <summary>
		/// Test to see if any of the specified flags are set
		/// </summary>
		/// <param name="inFlags">Current flags</param>
		/// <param name="testFlags">Flags to test for</param>
		/// <returns>True if any of the flags are set</returns>
		public static bool HasAnyFlags(this UhtFieldExportFlags inFlags, UhtFieldExportFlags testFlags)
		{
			return (inFlags & testFlags) != 0;
		}

		/// <summary>
		/// Test to see if all of the specified flags are set
		/// </summary>
		/// <param name="inFlags">Current flags</param>
		/// <param name="testFlags">Flags to test for</param>
		/// <returns>True if all the flags are set</returns>
		public static bool HasAllFlags(this UhtFieldExportFlags inFlags, UhtFieldExportFlags testFlags)
		{
			return (inFlags & testFlags) == testFlags;
		}

		/// <summary>
		/// Test to see if a specific set of flags have a specific value.
		/// </summary>
		/// <param name="inFlags">Current flags</param>
		/// <param name="testFlags">Flags to test for</param>
		/// <param name="matchFlags">Expected value of the tested flags</param>
		/// <returns>True if the given flags have a specific value.</returns>
		public static bool HasExactFlags(this UhtFieldExportFlags inFlags, UhtFieldExportFlags testFlags, UhtFieldExportFlags matchFlags)
		{
			return (inFlags & testFlags) == matchFlags;
		}
	}

	/// <summary>
	/// Controls how the verse name is appended to the builder
	/// </summary>
	public enum UhtVerseFullNameMode
	{
		/// <summary>
		/// Default mode
		/// </summary>
		Default,

		/// <summary>
		/// Fully qualified mode
		/// </summary>
		Qualified,
	}

	/// <summary>
	/// Represents a UField
	/// </summary>
	public abstract class UhtField : UhtObject
	{
		/// <inheritdoc/>
		public override string EngineClassName => "Field";

		/// <summary>
		/// UHT only field flags
		/// </summary>
		[JsonConverter(typeof(JsonStringEnumConverter))]
		public UhtFieldExportFlags FieldExportFlags { get; set; } = UhtFieldExportFlags.None;

		/// <summary>
		/// Name of the module containing the type
		/// </summary>
		public string? VerseModule { get; set; } = null;

		/// <summary>
		/// Cased name of the verse field
		/// </summary>
		public string? VerseName { get; set; } = null;

		/// <summary>
		/// Returns true if field is a verse element
		/// </summary>
		public bool IsVerseField => VerseName != null;

		/// <summary>
		/// Return the complete verse name
		/// </summary>
		public string VersePath => GetVerseFullName(UhtVerseFullNameMode.Default);

		/// <summary>
		/// Return the function name encoded for BPVM
		/// </summary>
		public string EncodedVersePath
		{
			get
			{
				StringBuilder builder = new();
				builder.AppendEncodedVerseName(VersePath);
				return builder.ToString();
			}
		}

		/// <summary>
		/// Construct a new field
		/// </summary>
		/// <param name="headerFile">Header file being parsed</param>
		/// <param name="outer">Outer object</param>
		/// <param name="lineNumber">Line number of declaration</param>
		protected UhtField(UhtHeaderFile headerFile, UhtType outer, int lineNumber) : base(headerFile, outer, lineNumber)
		{
		}

		/// <summary>
		/// Return the full name of the verse type
		/// </summary>
		/// <param name="mode">Controls how the name is generated</param>
		/// <returns></returns>
		public string GetVerseFullName(UhtVerseFullNameMode mode)
		{
			StringBuilder builder = new();
			AppendVerseFullName(builder, mode);
			return builder.ToString();
		}

		/// <summary>
		/// Append the verse path to the given builder
		/// </summary>
		/// <param name="builder">Destination builder</param>
		/// <param name="mode">Controls how the name is generated</param>
		public virtual void AppendVerseFullName(StringBuilder builder, UhtVerseFullNameMode mode)
		{
			AppendVersePathPart(builder, mode);
			AppendVerseName(builder, mode);
		}

		/// <summary>
		/// Append the verse path to the given builder
		/// </summary>
		/// <param name="builder">Destination builder</param>
		/// <param name="mode">Controls how the name is generated</param>
		/// <param name="isTopLevel">If true, this is the type that initiated the verse path</param>
		/// <exception cref="UhtException"></exception>
		private void AppendVersePathInternal(StringBuilder builder, UhtVerseFullNameMode mode, bool isTopLevel)
		{
			if (!IsVerseField)
			{
				throw new UhtException(this, "Attempt to write the Verse name on a field that isn't part of Verse");
			}
			if (Outer is UhtField outerField)
			{
				outerField.AppendVersePathInternal(builder, mode, false);
			}
			else
			{
				builder.Append(Module.Module.VersePath);
				if (!String.IsNullOrEmpty(VerseModule))
				{
					builder.Append('/').Append(VerseModule);
				}
			}
			AppendVersePath(builder, mode, isTopLevel);
		}

		/// <summary>
		/// Append the verse path part to the given builder
		/// </summary>
		/// <param name="builder">Destination builder</param>
		/// <param name="mode">Controls how the name is generated</param>
		protected virtual void AppendVersePathPart(StringBuilder builder, UhtVerseFullNameMode mode)
		{
			builder.Append('(');
			AppendVersePathInternal(builder, mode, true);
			builder.Append(":)");
		}

		/// <summary>
		/// Helper method that appends only this path contribution to the verse path.
		/// </summary>
		/// <param name="builder"></param>
		/// <param name="mode">Controls how the name is generated</param>
		/// <param name="isTopLevel">If true, this is the type that initiated the verse path</param>
		protected virtual void AppendVersePath(StringBuilder builder, UhtVerseFullNameMode mode, bool isTopLevel)
		{
			if (!isTopLevel)
			{
				builder.Append('/').Append(VerseName);
			}
		}

		/// <summary>
		/// Helper method that appends only this instance contribution to the verse path.
		/// </summary>
		/// <param name="builder"></param>
		/// <param name="mode">Controls how the name is generated</param>
		protected virtual void AppendVerseName(StringBuilder builder, UhtVerseFullNameMode mode)
		{
			builder.Append(VerseName);
		}
	}

	/// <summary>
	/// Helper extension methods for fields
	/// </summary>
	public static class UhtFieldStringBuilderExtensions
	{

		/// <summary>
		/// Append the Verse UE VNI package name
		/// </summary>
		/// <param name="builder">Destination builder</param>
		/// <param name="fieldObj">Field to serialize</param>
		/// <returns>Builder</returns>
		public static StringBuilder AppendVerseUEVNIPackageName(this StringBuilder builder, UhtField fieldObj)
		{
			if (!fieldObj.IsVerseField)
			{
				throw new UhtException(fieldObj, "Attempt to write the Verse VNI package name on a field that isn't part of Verse");
			}
			UhtModule module = fieldObj.Module;
			return builder.Append('/').Append(module.Module.VerseMountPoint).Append("/_Verse/VNI/").Append(module.Module.Name);
		}

		/// <summary>
		/// Append the Verse UE package name (without a leading forward slash)
		/// </summary>
		/// <param name="builder">Destination builder</param>
		/// <param name="fieldObj">Field to serialize</param>
		/// <returns>Builder</returns>
		public static StringBuilder AppendVerseUEPackageName(this StringBuilder builder, UhtField fieldObj)
		{
			if (!fieldObj.IsVerseField)
			{
				throw new UhtException(fieldObj, "Attempt to write the Verse package name on a field that isn't part of Verse");
			}
			UhtModule module = fieldObj.Module;
			return builder.Append(module.Module.VerseMountPoint).Append('/').Append(module.Module.Name);
		}

		/// <summary>
		/// Append the full verse name of the field
		/// </summary>
		/// <param name="builder">Destination builder</param>
		/// <param name="fieldObj">Field to serialize</param>
		/// <param name="mode">Controls the type of string generated</param>
		/// <returns>Builder</returns>
		public static StringBuilder AppendVersePath(this StringBuilder builder, UhtField fieldObj, UhtVerseFullNameMode mode)
		{
			fieldObj.AppendVerseFullName(builder, mode);
			return builder;
		}

		/// <summary>
		/// Given a verse name, encode it so it can be represented as an FName
		/// </summary>
		/// <param name="builder">Destination builder</param>
		/// <param name="name">Name to encode</param>
		/// <returns>Builder</returns>
		public static StringBuilder AppendEncodedVerseName(this StringBuilder builder, ReadOnlySpan<char> name)
		{
			bool isFirstChar = true;
			while (!name.IsEmpty)
			{
				char c = name[0];
				name = name[1..];

				if ((c >= 'a' && c <= 'z')
					|| (c >= 'A' && c <= 'Z')
					|| (c >= '0' && c <= '9' && !isFirstChar))
				{
					builder.Append(c);
				}
				else if (c == '[' && !name.IsEmpty && name[0] == ']')
				{
					name = name[1..];
					builder.Append("_K");
				}
				else if (c == '-' && !name.IsEmpty && name[0] == '>')
				{
					name = name[1..];
					builder.Append("_T");
				}
				else if (c == '_')
				{
					builder.Append("__");
				}
				else if (c == '(')
				{
					builder.Append("_L");
				}
				else if (c == ',')
				{
					builder.Append("_M");
				}
				else if (c == ':')
				{
					builder.Append("_N");
				}
				else if (c == '^')
				{
					builder.Append("_P");
				}
				else if (c == '?')
				{
					builder.Append("_Q");
				}
				else if (c == ')')
				{
					builder.Append("_R");
				}
				else if (c == '\'')
				{
					builder.Append("_U");
				}
				else
				{
					builder.Append('_').AppendFormat(CultureInfo.InvariantCulture, "{0:x2}", (uint)c);
				}

				isFirstChar = false;
			}
			return builder;
		}
	}
}
