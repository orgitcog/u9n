// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Globalization;
using System.Text.Json;
using System.Threading.Tasks;
using HordeCommon;
using HordeServer.Server;
using HordeServer.Utilities;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using Microsoft.Extensions.Logging.Abstractions;

namespace HordeServer.Tests.Server
{
	[TestClass]
	public class DowntimeServiceTest
	{
		private static async Task<ScheduledDowntime> GetDowntimeFromJsonAsync(string json)
		{
			ScheduledDowntime scheduledDowntime = JsonSerializer.Deserialize<ScheduledDowntime>(json, JsonUtils.DefaultSerializerOptions)!;
			GlobalConfig config = new();
			config.Downtime.Add(scheduledDowntime);
			await config.PostLoadAsync(new ServerSettings(), [], []);
			return scheduledDowntime;
		}

		private static DateTimeOffset GetNow(int year, int month, int day, int hour, int minute)
		{
			DateTime clock = new(year, month, day, hour, minute, 0, DateTimeKind.Utc);
			DateTimeOffset now = TimeZoneInfo.ConvertTime(new DateTimeOffset(clock), TimeZoneInfo.Utc);
			return now;
		}

		private static DateTimeOffset DateTimeOffsetParse(string input)
		{
			return DateTimeOffset.Parse(input, CultureInfo.InvariantCulture);
		}

		[TestMethod]
		public async Task AdvanceToDowntimeIsActiveAsync()
		{
			FakeClock clock = new();
			GlobalConfig config = new();
			ScheduledDowntime downtime = new()
			{
				StartTime = clock.UtcNow + TimeSpan.FromSeconds(30),
				FinishTime = clock.UtcNow + TimeSpan.FromSeconds(90)
			};
			config.Downtime.Add(downtime);
			await config.PostLoadAsync(new ServerSettings(), [], []);
			TestOptionsMonitor<GlobalConfig> optionsMonitor = new(config);
			await using DowntimeService downtimeService = new(clock, optionsMonitor, NullLogger<DowntimeService>.Instance);
			Assert.IsFalse(downtimeService.IsDowntimeActive);
			await clock.AdvanceAsync(TimeSpan.FromSeconds(60));
			downtimeService.Tick();
			Assert.IsTrue(downtimeService.IsDowntimeActive);
		}

		[TestMethod]
		public async Task IsNotActiveAsync()
		{
			ScheduledDowntime downtime = await GetDowntimeFromJsonAsync("""
				{
					"startTime": "2020-09-06T03:00:00+00:00",
					"finishTime": "2020-09-06T03:45:00+00:00"
				}
			""");
			Assert.IsFalse(downtime.IsActive(GetNow(2024, 12, 8, 2, 45)));
		}

		[TestMethod]
		public async Task IsActiveAsync()
		{
			ScheduledDowntime downtime = await GetDowntimeFromJsonAsync("""
				{
					"startTime": "2020-09-06T03:00:00+00:00",
					"finishTime": "2020-09-06T03:45:00+00:00"
				}
			""");
			Assert.IsTrue(downtime.IsActive(GetNow(2020, 9, 6, 3, 15)));
		}

		[TestMethod]
		public async Task IsNotActiveDailyAsync()
		{
			ScheduledDowntime downtime = await GetDowntimeFromJsonAsync("""
				{
					"startTime": "2020-09-06T03:00:00+00:00",
					"finishTime": "2020-09-06T03:45:00+00:00",
					"frequency": "Daily"
				}
			""");
			Assert.IsFalse(downtime.IsActive(GetNow(2024, 12, 8, 2, 45)));
		}

		[TestMethod]
		public async Task IsActiveDailyAsync()
		{
			ScheduledDowntime downtime = await GetDowntimeFromJsonAsync("""
				{
					"startTime": "2020-09-06T03:00:00+00:00",
					"finishTime": "2020-09-06T03:45:00+00:00",
					"frequency": "Daily"
				}
			""");
			Assert.IsTrue(downtime.IsActive(GetNow(2024, 12, 8, 3, 15)));
		}

		[TestMethod]
		public async Task IsNotActiveWeeklyAsync()
		{
			ScheduledDowntime downtime = await GetDowntimeFromJsonAsync("""
				{
					"startTime": "2020-09-06T03:00:00+00:00",
					"finishTime": "2020-09-06T03:45:00+00:00",
					"frequency": "Weekly"
				}
			""");
			Assert.IsFalse(downtime.IsActive(GetNow(2024, 12, 8, 2, 45)));
		}

		[TestMethod]
		public async Task IsActiveWeeklyAsync()
		{
			ScheduledDowntime downtime = await GetDowntimeFromJsonAsync("""
				{
					"startTime": "2020-09-06T03:00:00+00:00",
					"finishTime": "2020-09-06T03:45:00+00:00",
					"frequency": "Weekly"
				}
			""");
			Assert.IsTrue(downtime.IsActive(GetNow(2024, 12, 8, 3, 15)));
		}

		[TestMethod]
		public async Task FinishTimeFromDurationAsync()
		{
			ScheduledDowntime downtime = await GetDowntimeFromJsonAsync("""
				{
					"startTime": "2020-09-06T03:00:00+00:00",
					"duration": "0:45"
				}
			""");
			Assert.AreEqual(DateTimeOffsetParse("2020-09-06T03:45+0"), downtime.FinishTime);
		}

		[TestMethod]
		public async Task DurationFromFinishTimeAsync()
		{
			ScheduledDowntime downtime = await GetDowntimeFromJsonAsync("""
				{
					"startTime": "2020-09-06T03:00:00+00:00",
					"finishTime": "2020-09-06T03:30:00+00:00",
				}
			""");
			Assert.AreEqual(TimeSpan.FromMinutes(30), downtime.Duration);
		}

		[TestMethod]
		public async Task DurationHigherPrecedenceAsync()
		{
			ScheduledDowntime downtime = await GetDowntimeFromJsonAsync("""
				{
					"startTime": "2020-09-06T03:00:00+00:00",
					"finishTime": "2020-09-06T03:30:00+00:00",
					"duration": "0:45"
				}
			""");
			Assert.AreEqual(DateTimeOffsetParse("2020-09-06T03:45+0"), downtime.FinishTime);
		}

		[TestMethod]
		public async Task StartTimeAndDurationMinuteAbbreviationAsync()
		{
			ScheduledDowntime downtime = await GetDowntimeFromJsonAsync("""
				{
					"startTime": "Sun Sep 6, 2020, 3AM",
					"duration": "0:45"
				}
			""");
			Assert.AreEqual(DateTimeOffsetParse("2020-09-06T03:00"), downtime.StartTime);
			Assert.AreEqual(DateTimeOffsetParse("2020-09-06T03:45"), downtime.FinishTime);
		}
		
		[TestMethod]
		public async Task WrongDayOfWeekNameErrorAsync()
		{
			await Assert.ThrowsExceptionAsync<FormatException>(async () =>
			{
				await GetDowntimeFromJsonAsync("""
					{
						"startTime": "Mon Sep 6, 2020, 3AM",
						"finishTime": "Sun Sep 6, 2020, 3:30AM",
					}
				""");
			});
		}

		[TestMethod]
		public async Task StartTimeAndDurationHourAbbreviationAsync()
		{
			ScheduledDowntime downtime = await GetDowntimeFromJsonAsync("""
				{
					"startTime": "Sun Sep 6, 2020, 3AM",
					"duration": "1:30"
				}
			""");
			Assert.AreEqual(DateTimeOffsetParse("2020-09-06T03:00"), downtime.StartTime);
			Assert.AreEqual(DateTimeOffsetParse("2020-09-06T04:30"), downtime.FinishTime);
		}

		[TestMethod]
		public async Task StartTimeAndFinishTimeAbbreviationAsync()
		{
			ScheduledDowntime downtime = await GetDowntimeFromJsonAsync("""
				{
					"startTime": "Dec 1, 2024, 3AM",
					"finishTime": "Sunday December 1 2024, 3:15AM"
				}
			""");
			Assert.AreEqual(DateTimeOffsetParse("2024-12-01T03:00"), downtime.StartTime);
			Assert.AreEqual(DateTimeOffsetParse("2024-12-01T03:15"), downtime.FinishTime);
		}

		[TestMethod]
		public async Task WithTimeZoneAsync()
		{
			ScheduledDowntime downtime = await GetDowntimeFromJsonAsync("""
				{
					"startTime": "Jan 1, 2020, 3AM",
					"duration": "1:30",
					"timezone": "Eastern Standard Time"
				}
			""");
			Assert.AreEqual(DateTimeOffsetParse("2020-01-01T03:00-05:00"), downtime.StartTime);
			Assert.AreEqual(DateTimeOffsetParse("2020-01-01T04:30-05:00"), downtime.FinishTime);
		}

		[TestMethod]
		public async Task WithTimeZoneAndDaylightSavingAsync()
		{
			ScheduledDowntime downtime = await GetDowntimeFromJsonAsync("""
				{
					"startTime": "Sun Sep 6, 2020, 3AM",
					"duration": "1:30",
					"timezone": "Eastern Standard Time"
				}
			""");
			Assert.AreEqual(DateTimeOffsetParse("2020-09-06T03:00-04:00"), downtime.StartTime);
			Assert.AreEqual(DateTimeOffsetParse("2020-09-06T04:30-04:00"), downtime.FinishTime);
		}

		[TestMethod]
		public async Task WithTimeZoneIanaAsync()
		{
			ScheduledDowntime downtime = await GetDowntimeFromJsonAsync("""
				{
					"startTime": "Jan 1, 2020, 3AM",
					"duration": "1:30",
					"timezone": "America/New_York"
				}
			""");
			Assert.AreEqual(DateTimeOffsetParse("2020-01-01T03:00-05:00"), downtime.StartTime);
			Assert.AreEqual(DateTimeOffsetParse("2020-01-01T04:30-05:00"), downtime.FinishTime);
		}

		[TestMethod]
		public async Task WithTimeZoneIanaAndDaylightSavingAsync()
		{
			ScheduledDowntime downtime = await GetDowntimeFromJsonAsync("""
				{
					"startTime": "Sun Sep 6, 2020, 3AM",
					"duration": "1:30",
					"timezone": "America/New_York"
				}
			""");
			Assert.AreEqual(DateTimeOffsetParse("2020-09-06T03:00-04:00"), downtime.StartTime);
			Assert.AreEqual(DateTimeOffsetParse("2020-09-06T04:30-04:00"), downtime.FinishTime);
		}

		[TestMethod]
		public async Task WrongTimeZoneNameAsync()
		{
			await Assert.ThrowsExceptionAsync<TimeZoneNotFoundException>(async () =>
			{
				await GetDowntimeFromJsonAsync("""
					{
						"startTime": "Jan 1, 2020, 3AM",
						"timezone": "ET"
					}
				""");
			});
		}

		[TestMethod]
		public async Task DowntimeToJsonAsync()
		{
			// -4 hours is Eastern Daylight Time
			string json = JsonSerializer.Serialize(await GetDowntimeFromJsonAsync("""
					{
						"startTime": "Sun Sep 6, 2020, 3AM -4",
						"duration": "0:30"
					}
				"""),
				JsonUtils.DefaultSerializerOptions);
			ScheduledDowntime downtime = await GetDowntimeFromJsonAsync(json);
			Assert.AreEqual(DateTimeOffsetParse("2020-09-06T03:00:00-04:00"), downtime.StartTime);
			Assert.AreEqual(DateTimeOffsetParse("2020-09-06T03:30:00-04:00"), downtime.FinishTime);
			Assert.AreEqual(TimeSpan.FromMinutes(30), downtime.Duration);
			Assert.AreEqual(ScheduledDowntimeFrequency.Once, downtime.Frequency);
			Assert.IsNull(downtime.TimeZone);
		}

		[TestMethod]
		public async Task DowntimeWithTimeZoneToJsonAsync()
		{
			string json = JsonSerializer.Serialize(await GetDowntimeFromJsonAsync("""
					{
						"startTime": "Jan 1, 2025, 3AM",
						"duration": "0:30",
						"timezone": "Eastern Standard Time"
					}
				"""),
				JsonUtils.DefaultSerializerOptions);
			ScheduledDowntime downtime = await GetDowntimeFromJsonAsync(json);
			Assert.AreEqual(DateTimeOffsetParse("2025-01-01T03:00:00-05:00"), downtime.StartTime);
			Assert.AreEqual(DateTimeOffsetParse("2025-01-01T03:30:00-05:00"), downtime.FinishTime);
			Assert.AreEqual(TimeSpan.FromMinutes(30), downtime.Duration);
			Assert.AreEqual(ScheduledDowntimeFrequency.Once, downtime.Frequency);
			Assert.AreEqual(downtime.TimeZone, "Eastern Standard Time");
		}
	}
}
