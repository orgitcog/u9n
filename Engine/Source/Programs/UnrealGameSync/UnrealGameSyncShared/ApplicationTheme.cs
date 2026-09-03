// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Drawing;
using System.Reflection;

namespace UnrealGameSync
{
	[AttributeUsage(System.AttributeTargets.Property)]
	sealed class ThemeAttribute : System.Attribute
	{

	}

	public class ApplicationTheme
	{
		public const string LightThemeId = "Light";
		public const string DarkThemeId = "Dark";

		public static string DefaultThemeId { get; set; } = LightThemeId;

		static readonly ReadOnlyCollection<Func<ApplicationTheme>> s_defaultThemeConstructors = new ReadOnlyCollection<Func<ApplicationTheme>>(new List<Func<ApplicationTheme>>() { CreateLightTheme, CreateDarkTheme });

		public string ThemeId { get; init; }

		[Theme]
		public string ThemeName { get; init; } = "Unnamed";

		[Theme]
		public bool UseDarkMode { get; init; } = false;
		[Theme]
		public bool UseFlatControls { get; init; } = false;

		[Theme]
		public Color ForegroundColor { get; init; } = SystemColors.ControlDark;
		[Theme]
		public Color BackgroundColor { get; init; } = SystemColors.Control;

		[Theme]
		public Color ActiveCaption { get; init; } = SystemColors.ActiveCaption;
		[Theme]
		public Color ActiveCaptionText { get; init; } = SystemColors.ActiveCaptionText;
		[Theme]
		public Color InactiveCaption { get; init; } = SystemColors.InactiveCaption;
		[Theme]
		public Color InactiveCaptionText { get; init; } = SystemColors.InactiveCaptionText;

		[Theme]
		public Color MenuHighlightBGColor { get; init; } = Color.FromArgb(179, 215, 243);
		[Theme]
		public Color MenuHighlightBorderColor { get; init; } = SystemColors.Highlight;

		[Theme]
		public Color TextColor { get; init; } = SystemColors.ControlText;
		[Theme]
		public Color DimmedText { get; init; } = SystemColors.GrayText;
		[Theme]
		public Color LinkColor { get; init; } = SystemColors.HotTrack;
		[Theme]
		public Color LinkColorHover { get; init; } = Color.FromArgb(SystemColors.HotTrack.B, SystemColors.HotTrack.G, SystemColors.HotTrack.R);
		[Theme]
		public Color LinkColorActive { get; init; } = Color.FromArgb(SystemColors.HotTrack.B / 2, SystemColors.HotTrack.G / 2, SystemColors.HotTrack.R);
		[Theme]
		public Color SuccessColor { get; init; } = Color.FromArgb(28, 180, 64);
		[Theme]
		public Color WarningColor { get; init; } = Color.FromArgb(128, 128, 0);
		[Theme]
		public Color ErrorColor { get; init; } = Color.FromArgb(189, 54, 47);
		[Theme]
		public Color StandardNotificationBGTint { get; init; } = Color.FromArgb(220, 220, 240);
		[Theme]
		public Color SuccessNotificationBGTint { get; init; } = Color.FromArgb(248, 254, 246);
		[Theme]
		public Color WarningNotificationBGTint { get; init; } = Color.FromArgb(240, 240, 220);
		[Theme]
		public Color ErrorNotificationBGTint { get; init; } = Color.FromArgb(240, 220, 220);

		[Theme]
		public Color InputBackgroundColor { get; init; } = SystemColors.Window;
		[Theme]
		public Color InputBorderColor { get; init; } = Color.Empty;

		[Theme]
		public Color CodeBadgeColor { get; init; } = Color.FromArgb(116, 185, 255);
		[Theme]
		public Color ContentBadgeColor { get; init; } = Color.FromArgb(162, 155, 255);

		[Theme]
		public Color SyncBadgeColor { get; init; } = Color.FromArgb(140, 180, 230);
		[Theme]
		public Color SyncHoverBadgeColor { get; init; } = Color.FromArgb(112, 146, 190);

		[Theme]
		public bool ShouldInvertColorBrightness { get; init; } = false;

		public static string GetThemeConfigSectionName(string themeId) => "Theme." + themeId;
		static ApplicationTheme CreateLightTheme()
		{
			return new ApplicationTheme(LightThemeId)
			{
				ThemeName = (DefaultThemeId == LightThemeId) ? "Light (Default)" : "Light",
				// Inline property defaults are expected to be for Light Theme
			};
		}

		static ApplicationTheme CreateDarkTheme()
		{
			return new ApplicationTheme(DarkThemeId)
			{
				ThemeName = (DefaultThemeId == DarkThemeId) ? "Dark (Default)" : "Dark",

				UseDarkMode = true,
				UseFlatControls = true,

				ForegroundColor = SystemColors.Control,
				BackgroundColor = Color.FromArgb(30, 30, 30),
				
				ActiveCaption = Color.FromArgb(46, 73, 102),
				ActiveCaptionText = Color.FromArgb(255, 255, 255),
				InactiveCaption = Color.FromArgb(37, 51, 65),
				InactiveCaptionText = Color.FromArgb(255, 255, 255),
				
				MenuHighlightBGColor = Color.FromArgb(12, 48, 75),
				MenuHighlightBorderColor = Color.FromArgb(41, 159, 255),

				TextColor = SystemColors.ControlLight,
				DimmedText = SystemColors.ControlDarkDark,
				LinkColor = Color.FromArgb(84, 170, 255),
				LinkColorHover = Color.FromArgb(255, 170, 84),
				LinkColorActive = Color.FromArgb(127, 85, 84),
				SuccessColor = Color.FromArgb(74, 227, 110),
				WarningColor = Color.FromArgb(255, 255, 128),
				ErrorColor = Color.FromArgb(208, 74, 67),
				
				StandardNotificationBGTint = Color.FromArgb(15, 15, 36),
				SuccessNotificationBGTint = Color.FromArgb(6, 18, 2),
				WarningNotificationBGTint = Color.FromArgb(36, 36, 15),
				ErrorNotificationBGTint = Color.FromArgb(36, 15, 15),

				InputBackgroundColor = Color.FromArgb(45, 45, 45),
				InputBorderColor = Color.Black,

				CodeBadgeColor = Color.FromArgb(0, 69, 138),
				ContentBadgeColor = Color.FromArgb(83, 54, 150),
				
				SyncBadgeColor = Color.FromArgb(25, 65, 113),
				SyncHoverBadgeColor = Color.FromArgb(65, 99, 144),
				
				ShouldInvertColorBrightness = true
			};
		}

		public static Dictionary<string, string> GetThemeList(ConfigFile configFile)
		{
			Dictionary<string, string> themeList = new Dictionary<string, string>();

			ConfigSection? themesSection = configFile.FindSection("Themes");
			string[] themeIds = themesSection?.GetValues("Themes", Array.Empty<string>()) ?? Array.Empty<string>();

			foreach (string themeId in themeIds)
			{
				ConfigSection? themeSection = configFile.FindSection(GetThemeConfigSectionName(themeId));
				if (themeSection != null)
				{
					themeList.Add(themeId, themeSection.GetValue("ThemeName", "Unnamed"));
				}
			}

			return themeList;
		}

		public static void AddDefaultThemesToConfig(ConfigFile configFile)
		{
			ConfigSection themesSection = configFile.FindOrAddSection("Themes");
			string[] oldThemeIds = themesSection.GetValues("Themes", Array.Empty<string>());
			List<string> themeIds = new List<string>(oldThemeIds);

			foreach (Func<ApplicationTheme> func in s_defaultThemeConstructors)
			{
				ApplicationTheme theme = func();
				// Default themes are not editable, we add them to the ini as reference for creating new themes
				theme.AddThemeToConfig(configFile);

				if (!themeIds.Contains(theme.ThemeId))
				{
					themeIds.Add(theme.ThemeId);
				}
			}

			themesSection.SetValues("Themes", themeIds.ToArray());
		}

		public static bool IsColorLight(Color color)
		{
			return ((0.2126 * color.R) + (0.7152 * color.G) + (0.0722 * color.B)) > 127;
		}

		public static Color InvertColorBrightness(Color color)
		{
			float hue = color.GetHue();
			float saturation = color.GetSaturation();
			float brightness = 1.0f - color.GetBrightness();
			
			float c = (1 - Math.Abs((2 * brightness) - 1)) * saturation;
			float x = c * (1 - Math.Abs(((hue / 60) % 2) - 1));
			float m = brightness - (c / 2);

			switch ((int)hue / 60)
			{
				case 0:
					return Color.FromArgb(color.A, (int)((c + m) * 255), (int)((x + m) * 255), (int)(m * 255));
				case 1:
					return Color.FromArgb(color.A, (int)((x + m) * 255), (int)((c + m) * 255), (int)(m * 255));
				case 2:
					return Color.FromArgb(color.A, (int)(m * 255), (int)((c + m) * 255), (int)((x + m) * 255));
				case 3:
					return Color.FromArgb(color.A, (int)(m * 255), (int)((x + m) * 255), (int)((c + m) * 255));
				case 4:
					return Color.FromArgb(color.A, (int)((x + m) * 255), (int)(m * 255), (int)((c + m) * 255));
				case 5:
					return Color.FromArgb(color.A, (int)((c + m) * 255), (int)(m * 255), (int)((x + m) * 255));
			}
			
			return color;
		}

		public static ApplicationTheme? LoadThemeFromConfig(string themeId, ConfigFile configFile)
		{
			ConfigSection? themeSection = configFile.FindSection(GetThemeConfigSectionName(themeId));
			if (themeSection != null)
			{
				return new ApplicationTheme(themeId, themeSection);
			}

			return null;
		}
		

		public static ApplicationTheme ConstructDefaultTheme()
		{
			return (DefaultThemeId == LightThemeId) ? CreateLightTheme() : CreateDarkTheme();
		}

		ApplicationTheme(string inThemeId, ConfigSection? themeSection = null)
		{
			ThemeId = inThemeId;

			if (themeSection != null)
			{
				foreach (PropertyInfo propertyInfo in GetType().GetProperties())
				{
					if (propertyInfo.GetCustomAttribute<ThemeAttribute>() != null)
					{
						if (!themeSection.Pairs.ContainsKey(propertyInfo.Name))
						{
							continue;
						}

						if (propertyInfo.PropertyType == typeof(int))
						{
							int? value = themeSection.GetOptionalIntValue(propertyInfo.Name, null);
							if (value != null)
							{
								propertyInfo.SetValue(this, value);
							}
						}
						else if (propertyInfo.PropertyType == typeof(long) && propertyInfo.GetValue(this) is long defaultLongValue)
						{
							long value = themeSection.GetValue(propertyInfo.Name, defaultLongValue);
							propertyInfo.SetValue(this, value);
						}
						else if (propertyInfo.PropertyType == typeof(bool) && propertyInfo.GetValue(this) is bool defaultBoolValue)
						{
							bool value = themeSection.GetValue(propertyInfo.Name, defaultBoolValue);
							propertyInfo.SetValue(this, value);
						}
						else if (propertyInfo.PropertyType == typeof(string))
						{
							string? value = themeSection.GetValue(propertyInfo.Name);
							if (value != null)
							{
								propertyInfo.SetValue(this, value);
							}
						}
						else if (propertyInfo.PropertyType == typeof(Color))
						{
							string? value = themeSection.GetValue(propertyInfo.Name);
							if (value != null)
							{
								string[] channels = value.Split(',');

								Color color = Color.FromArgb(
									Int32.Parse(channels[3].Substring(2)), // alpha is stored last
									Int32.Parse(channels[0].Substring(2)),
									Int32.Parse(channels[1].Substring(2)),
									Int32.Parse(channels[2].Substring(2))
								);
								
								propertyInfo.SetValue(this, color);
							}
						}
						else
						{
							// unhandled
						}
					}
				}
			}
		}

		public void AddThemeToConfig(ConfigFile configFile)
		{
			ConfigSection themeSection = configFile.FindOrAddSection(GetThemeConfigSectionName(ThemeId));
			themeSection.Clear();

			foreach (PropertyInfo propertyInfo in GetType().GetProperties())
			{
				if (propertyInfo.GetCustomAttribute<ThemeAttribute>() != null)
				{
					object? propertyValue = propertyInfo.GetValue(this);
					if (propertyValue is int valueInt)
					{
						themeSection.SetValue(propertyInfo.Name, valueInt);
					}
					else if (propertyValue is long valueLong)
					{
						themeSection.SetValue(propertyInfo.Name, valueLong);
					}
					else if (propertyValue is bool valueBool)
					{
						themeSection.SetValue(propertyInfo.Name, valueBool);
					}
					else if (propertyValue is string valueString)
					{
						themeSection.SetValue(propertyInfo.Name, valueString);
					}
					else if (propertyValue is Color valueColor)
					{
						themeSection.SetValue(propertyInfo.Name, $"R={valueColor.R},G={valueColor.G},B={valueColor.B},A={valueColor.A}");
					}
					else
					{
						// unhandled
					}
				}
			}
		}

		public Color? CheckInvertColor(Color? color)
		{
			if (color != null && ShouldInvertColorBrightness)
			{
				return InvertColorBrightness(color.Value);
			}

			return color;
		}
	}
}
