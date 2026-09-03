// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using EpicGames.Core;
using Microsoft.Extensions.Logging;

namespace UnrealBuildTool.Configuration.CompileWarnings
{
	/// <summary>
	/// Flag used to communicate which context the <see cref="WarningLevelDefaultAttribute"/> should be applied.
	/// </summary>
	internal enum InitializationContext
	{
		/// <summary>
		/// Apply the <see cref="WarningLevelDefaultAttribute"/> in all contexts.
		/// </summary>
		Any,
		/// <summary>
		/// Apply the <see cref="WarningLevelDefaultAttribute"/> exclusively in the <see cref="CppCompileWarnings.ApplyTargetDefaults(CppCompileWarnings, Boolean)"/> context.
		/// </summary>
		Target,
		/// <summary>
		/// Apply the <see cref="WarningLevelDefaultAttribute"/> exclusively in the <see cref="CppCompileWarnings.ApplyDefaults"/> context.
		/// </summary>
		Constructor
	}

	/// <summary>
	/// Attribute used to set the default values of a <see cref="WarningLevel"/> under the context of <see cref="CppCompileWarnings.ApplyDefaults"/> context and <see cref="CppCompileWarnings.ApplyTargetDefaults(CppCompileWarnings, Boolean)"/> context.
	/// </summary>
	[AttributeUsage(AttributeTargets.Field | AttributeTargets.Property, AllowMultiple = true)]
	internal sealed class WarningLevelDefaultAttribute : Attribute
	{
		private static readonly ILogger? s_logger = Log.Logger;

		/// <summary>
		/// The lower bound (inclusive) <see cref="BuildSettingsVersion"/> of which to apply this <see cref="DefaultLevel"/> to.
		/// </summary>
		public BuildSettingsVersion MinVersion { get; }

		/// <summary>
		/// The upper bound (inclusive) <see cref="BuildSettingsVersion"/> of which to apply this <see cref="DefaultLevel"/> to.
		/// </summary>
		public BuildSettingsVersion MaxVersion { get; }

		/// <summary>
		/// The default warning level.
		/// </summary>
		public WarningLevel DefaultLevel { get; }

		/// <summary>
		/// The context in which this attribute should be applied.
		/// </summary>
		public InitializationContext Context { get; }

		/// <summary>
		/// Constructs a ApplyWarningsDefaultValueAttribute.
		/// </summary>
		/// <param name="minVersion">Minimum version of the bound to apply this setting to. Inclusive.</param>
		/// <param name="maxVersion">Maximum version of the bound to apply this setting to. Inclusive.</param>
		/// <param name="defaultLevel">The default warning level to apply for this bound.</param>
		/// <param name="context">The context in which to apply this default.</param>
		public WarningLevelDefaultAttribute(WarningLevel defaultLevel = WarningLevel.Default, BuildSettingsVersion minVersion = BuildSettingsVersion.V1, BuildSettingsVersion maxVersion = BuildSettingsVersion.Latest, InitializationContext context = InitializationContext.Any)
		{
			MinVersion = minVersion;
			MaxVersion = maxVersion;
			DefaultLevel = defaultLevel;
			Context = context;
		}

		/// <summary>
		/// Resolves the set of <see cref="WarningLevelDefaultAttribute"/> to a <see cref="WarningLevel"/> provided the current <see cref="BuildSettingsVersion"/>.
		/// </summary>
		/// <param name="unsortedDefaultValueAttributes">The set of attributes to consider.</param>
		/// <param name="activeVersion">The current build settings version.</param>
		/// <returns>The corresponding <see cref="WarningLevel"/> of which to apply to the property with the default value attributes.</returns>
		/// <remarks>Will invoke <see cref="EnsureWarningLevelDefaultBounds(IList{WarningLevelDefaultAttribute})"/> on the input <see cref="WarningLevelDefaultAttribute"/>.</remarks>
		internal static WarningLevel ResolveWarningLevelDefault(IList<WarningLevelDefaultAttribute> unsortedDefaultValueAttributes, BuildSettingsVersion activeVersion)
		{
			List<WarningLevelDefaultAttribute> sortedAndMerged = EnsureWarningLevelDefaultBounds(unsortedDefaultValueAttributes);

			WarningLevel returnWarningLevel = WarningLevel.Default;

			foreach (WarningLevelDefaultAttribute attr in sortedAndMerged)
			{
				// If we find our appropriate range, we early out.
				if (attr.MinVersion <= activeVersion && activeVersion <= attr.MaxVersion)
				{
					returnWarningLevel = attr.DefaultLevel;
					break;
				}
			}

			return returnWarningLevel;
		}

		/// <summary>
		/// Ensures the attributes collection represents a contiguous, non-overlapping range.
		/// </summary>
		/// <param name="attributes">The list of attributes to verify.</param>
		/// <returns>A list of attributes that has no overlaps, and is contiguous.</returns>
		internal static List<WarningLevelDefaultAttribute> EnsureWarningLevelDefaultBounds(IList<WarningLevelDefaultAttribute> attributes)
		{
			List<WarningLevelDefaultAttribute> sorted = attributes.OrderBy(a => a.MinVersion).ToList();
			List<WarningLevelDefaultAttribute> merged = new List<WarningLevelDefaultAttribute>(sorted.Count);

			for (int i = 0; i < sorted.Count - 1; i++)
			{
				WarningLevelDefaultAttribute current = sorted[i];
				WarningLevelDefaultAttribute next = sorted[i + 1];

				if (current.MaxVersion < next.MinVersion - 1)
				{
					s_logger?.LogWarning("Malformed ApplyWarningsDefaultValueAttribute collection; taking corrective action to address gap in range (CurrentMax: {CurrentMax} NextMin: {NextMin}-1).", next.MinVersion, current.MaxVersion);

					// Extend current range to cover the gap up to the next MinVersion,using the old build settings version standard.
					current = new WarningLevelDefaultAttribute(
						current.DefaultLevel,
						current.MinVersion,
						next.MinVersion - 1
					);
				}
				else if (next.MinVersion <= current.MaxVersion)
				{
					s_logger?.LogWarning("Malformed ApplyWarningsDefaultValueAttribute collection; taking corrective action to address overlap in range (NextMin: {NextMin} <= CurrentMax: {CurrentMax}).", next.MinVersion, current.MaxVersion);

					// Reduce next range to be more constrained, leaving the larger current range at the old build settings version standard.
					next = new WarningLevelDefaultAttribute(
						next.DefaultLevel,
						current.MaxVersion + 1,
						next.MaxVersion
					);

					// Flatten the old value with the updated one.
					sorted[i + 1] = next;
				}

				merged.Add(current);
			}

			merged.Add(sorted.Last());

			return merged;
		}
	}
}