// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using Microsoft.Extensions.Logging;
using System.Text.RegularExpressions;

#nullable enable

namespace AutomationUtils.Matchers
{
	/// <summary>
	/// Matcher log output from zen cli in plain progress
	/// </summary>
	class ZenCliEventMatcher : ILogEventMatcher
	{
		static readonly Regex s_pattern = new Regex(
			@"\[.*\]\s?\[(?<severity>warning|error|info)\]\s?(\[(?<file>\S+)\:(?<line>\d+)\])?(?<message>.*)");

		public LogEventMatch? Match(ILogCursor input)
		{
			if (input.TryMatch(s_pattern, out Match? match))
			{
				LogEventBuilder builder = new LogEventBuilder(input);
				builder.Annotate(match.Groups["severity"], LogEventMarkup.Severity);
				string file = match.Groups["file"].Value;
				string line = match.Groups["line"].Value;
				if (!string.IsNullOrEmpty(file) && !string.IsNullOrEmpty(line))
				{
					builder.AnnotateSourceFile(match.Groups["file"], "");
					builder.Annotate(match.Groups["line"], LogEventMarkup.LineNumber);
				}

				LogLevel level = match.Groups["severity"].Value switch
				{
					"error" => LogLevel.Error,
					"warning" => LogLevel.Information, // we do not want to annotate warnings from zencli as warnings as that causes too much noise - handled retries are warnings but failed retries are already errors
					_ => LogLevel.Information,
				};

				return builder.ToMatch(LogEventPriority.Low, level, KnownLogEvents.ZenCli_Event);
				
			}
			return null;
		}
	}
}

