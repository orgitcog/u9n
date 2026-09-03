// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Text.Json;
using EpicGames.Core;
using Microsoft.Extensions.Logging;

namespace EpicGames.Horde.Issues.Handlers
{
	/// <summary>
	/// Instance of a localization error
	/// </summary>
	[IssueHandler]
	public class LocalizationIssueHandler : IssueHandler
	{
		private class LocalizationData
		{
			public string _localizationTarget = "";
			public string _localizationID = "";
		}

		readonly List<IssueEventGroup> _issues = new List<IssueEventGroup>();

		/// <inheritdoc/>
		public override int Priority => 10;

		/// <summary>
		/// Determines if the given event id matches
		/// </summary>
		/// <param name="eventId">The event id to compare</param>
		/// <returns>True if the given event id matches</returns>
		public static bool IsMatchingEventId(EventId? eventId)
		{
			return eventId == KnownLogEvents.Engine_Localization;
		}

		/// <summary>
		/// Determines if an event should be masked by this 
		/// </summary>
		/// <param name="eventId"></param>
		/// <returns></returns>
		static bool IsMaskedEventId(EventId eventId)
		{
			return eventId == KnownLogEvents.ExitCode;
		}

		/// <summary>
		/// Extracts the localization properties of an IssueEvent to generate all the IssueKeys
		/// </summary>
		/// <param name="keys">Set of keys</param>
		/// <param name="issueEvent">The event data</param>
		/// <param name="data">The localization data</param>
		private static void AddLocalizationIssueKeys(HashSet<IssueKey> keys, IssueEvent issueEvent, LocalizationData data)
		{
			foreach (JsonLogEvent line in issueEvent.Lines)
			{
				JsonDocument document = JsonDocument.Parse(line.Data);

				JsonElement properties;
				if (document.RootElement.TryGetProperty("properties", out properties) && properties.ValueKind == JsonValueKind.Object)
				{
					foreach (JsonProperty property in properties.EnumerateObject())
					{
						if (property.NameEquals("conflict") && property.Value.ValueKind == JsonValueKind.String)
						{
							keys.AddSourceFile(property.Value.GetString()!, IssueKeyType.File, data._localizationID);
						}
						else if((property.NameEquals("localizationID") || property.NameEquals("locID")) && property.Value.ValueKind == JsonValueKind.String)
						{
							keys.Add(new IssueKey(property.Value.GetString()!, IssueKeyType.None));
						}
					}
				}
			}
		}

		/// <summary>
		/// Extracts the localization data from the properties of an IssueEvent
		/// </summary>
		/// <param name="issueEvent">The event data</param>
		/// <param name="data">The localization data</param>
		private static void GetLocalizationData(IssueEvent issueEvent, LocalizationData data)
		{
			foreach (JsonLogEvent line in issueEvent.Lines)
			{
				JsonDocument document = JsonDocument.Parse(line.Data);

				JsonElement properties;
				if (document.RootElement.TryGetProperty("properties", out properties) && properties.ValueKind == JsonValueKind.Object)
				{
					foreach (JsonProperty property in properties.EnumerateObject())
					{
						if (property.NameEquals("localizationTarget") && property.Value.ValueKind == JsonValueKind.String)
						{
							data._localizationTarget = property.Value.GetString()!;
						}
						else if ((property.NameEquals("localizationID") || property.NameEquals("locID")) && property.Value.ValueKind == JsonValueKind.String)
						{
							data._localizationID = property.Value.GetString()!;
						}
					}
				}
			}
		}

		/// <inheritdoc/>
		public override bool HandleEvent(IssueEvent issueEvent)
		{
			if (issueEvent.EventId != null)
			{
				EventId eventId = issueEvent.EventId.Value;
				if (IsMatchingEventId(eventId))
				{
					LocalizationData data = new LocalizationData();
					GetLocalizationData(issueEvent, data);

					string localizationIssueGroup = (System.String.IsNullOrEmpty(data._localizationTarget) ? "Localization" : "Localization Target " + data._localizationTarget);
					IssueEventGroup issue = new IssueEventGroup(localizationIssueGroup, "Localization {Severity} in {Files}", IssueChangeFilter.All);
					
					issue.Events.Add(issueEvent);

					issue.Keys.AddSourceFiles(issueEvent, data._localizationID);
					AddLocalizationIssueKeys(issue.Keys, issueEvent, data);

					if (issue.Keys.Count > 0)
					{
						_issues.Add(issue);
					}

					return true;
				}
				else if (_issues.Count > 0 && IsMaskedEventId(eventId))
				{
					return true;
				}
			}
			return false;
		}

		/// <inheritdoc/>
		public override IEnumerable<IssueEventGroup> GetIssues() => _issues;
	}
}
