// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using UnrealBuildTool.Configuration.CompileWarnings;

namespace UnrealBuildTool.Tests
{
	[TestClass]
	public class WarningLevelDefaultAttributeTests
	{
		[TestMethod]
		public void TestWindowsCompletelyDefined()
		{
			List<WarningLevelDefaultAttribute> attributes = new List<WarningLevelDefaultAttribute>
			{
				new WarningLevelDefaultAttribute( WarningLevel.Off, BuildSettingsVersion.V1, BuildSettingsVersion.V2),
				new WarningLevelDefaultAttribute(WarningLevel.Warning, BuildSettingsVersion.V3, BuildSettingsVersion.V4),
				new WarningLevelDefaultAttribute(WarningLevel.Error, BuildSettingsVersion.V5, BuildSettingsVersion.Latest)
			};

			List<WarningLevelDefaultAttribute> merged = WarningLevelDefaultAttribute.EnsureWarningLevelDefaultBounds(attributes);

			Assert.AreEqual(3, merged.Count);
			Assert.AreEqual(BuildSettingsVersion.V1, merged[0].MinVersion);
			Assert.AreEqual(BuildSettingsVersion.V2, merged[0].MaxVersion);
			Assert.AreEqual(WarningLevel.Off, merged[0].DefaultLevel);

			Assert.AreEqual(BuildSettingsVersion.V3, merged[1].MinVersion);
			Assert.AreEqual(BuildSettingsVersion.V4, merged[1].MaxVersion);
			Assert.AreEqual(WarningLevel.Warning, merged[1].DefaultLevel);

			Assert.AreEqual(BuildSettingsVersion.V5, merged[2].MinVersion);
			Assert.AreEqual(BuildSettingsVersion.Latest, merged[2].MaxVersion);
			Assert.AreEqual(WarningLevel.Error, merged[2].DefaultLevel);
		}

		[TestMethod]
		public void TestGapResolveToLowerBound()
		{
			List<WarningLevelDefaultAttribute> attributes = new List<WarningLevelDefaultAttribute>
			{
				new WarningLevelDefaultAttribute(WarningLevel.Warning, BuildSettingsVersion.V1, BuildSettingsVersion.V2),
				new WarningLevelDefaultAttribute(WarningLevel.Error, BuildSettingsVersion.V4, BuildSettingsVersion.V5),
			};

			List<WarningLevelDefaultAttribute> merged = WarningLevelDefaultAttribute.EnsureWarningLevelDefaultBounds(attributes);

			Assert.AreEqual(2, merged.Count);
			Assert.AreEqual(BuildSettingsVersion.V1, merged[0].MinVersion);
			Assert.AreEqual(BuildSettingsVersion.V3, merged[0].MaxVersion);
			Assert.AreEqual(WarningLevel.Warning, merged[0].DefaultLevel);

			Assert.AreEqual(BuildSettingsVersion.V4, merged[1].MinVersion);
			Assert.AreEqual(BuildSettingsVersion.V5, merged[1].MaxVersion);
			Assert.AreEqual(WarningLevel.Error, merged[1].DefaultLevel);
		}

		[TestMethod]
		public void TestLowerUpperOverlapResizeBottomUpward()
		{
			List<WarningLevelDefaultAttribute> attributes = new List<WarningLevelDefaultAttribute>
			{
				new WarningLevelDefaultAttribute(WarningLevel.Warning, BuildSettingsVersion.V1, BuildSettingsVersion.V3),
				new WarningLevelDefaultAttribute(WarningLevel.Error, BuildSettingsVersion.V2, BuildSettingsVersion.V4)
			};

			List<WarningLevelDefaultAttribute> merged = WarningLevelDefaultAttribute.EnsureWarningLevelDefaultBounds(attributes);

			Assert.AreEqual(2, merged.Count);
			Assert.AreEqual(BuildSettingsVersion.V1, merged[0].MinVersion);
			Assert.AreEqual(BuildSettingsVersion.V3, merged[0].MaxVersion);
			Assert.AreEqual(WarningLevel.Warning, merged[0].DefaultLevel);

			Assert.AreEqual(BuildSettingsVersion.V4, merged[1].MinVersion);
			Assert.AreEqual(BuildSettingsVersion.V4, merged[1].MaxVersion);
			Assert.AreEqual(WarningLevel.Error, merged[1].DefaultLevel);
		}

		[TestMethod]
		public void TestAllRangeWide()
		{
			List<WarningLevelDefaultAttribute> attributes = new List<WarningLevelDefaultAttribute>
			{
				new WarningLevelDefaultAttribute( WarningLevel.Warning, BuildSettingsVersion.V1, BuildSettingsVersion.V2),
				new WarningLevelDefaultAttribute(WarningLevel.Error, BuildSettingsVersion.V1, BuildSettingsVersion.V4)
			};

			List<WarningLevelDefaultAttribute> merged = WarningLevelDefaultAttribute.EnsureWarningLevelDefaultBounds(attributes);

			Assert.AreEqual(2, merged.Count);

			Assert.AreEqual(BuildSettingsVersion.V1, merged[0].MinVersion);
			Assert.AreEqual(BuildSettingsVersion.V2, merged[0].MaxVersion);
			Assert.AreEqual(WarningLevel.Warning, merged[0].DefaultLevel);

			Assert.AreEqual(BuildSettingsVersion.V3, merged[1].MinVersion);
			Assert.AreEqual(BuildSettingsVersion.V4, merged[1].MaxVersion);
			Assert.AreEqual(WarningLevel.Error, merged[1].DefaultLevel);
		}

		[TestMethod]
		public void TestResolveLowerBound()
		{
			List<WarningLevelDefaultAttribute> attributes = new List<WarningLevelDefaultAttribute>
			{
				new WarningLevelDefaultAttribute(WarningLevel.Warning, BuildSettingsVersion.V1, BuildSettingsVersion.V2),
				new WarningLevelDefaultAttribute(WarningLevel.Error, BuildSettingsVersion.V3, BuildSettingsVersion.V4)
			};

			Assert.AreEqual(WarningLevel.Warning, WarningLevelDefaultAttribute.ResolveWarningLevelDefault(attributes, BuildSettingsVersion.V1));
			Assert.AreEqual(WarningLevel.Warning, WarningLevelDefaultAttribute.ResolveWarningLevelDefault(attributes, BuildSettingsVersion.V2));

			Assert.AreEqual(WarningLevel.Error, WarningLevelDefaultAttribute.ResolveWarningLevelDefault(attributes, BuildSettingsVersion.V3));
			Assert.AreEqual(WarningLevel.Error, WarningLevelDefaultAttribute.ResolveWarningLevelDefault(attributes, BuildSettingsVersion.V4));

			Assert.AreEqual(WarningLevel.Default, WarningLevelDefaultAttribute.ResolveWarningLevelDefault(attributes, BuildSettingsVersion.V5));
		}
	}
}