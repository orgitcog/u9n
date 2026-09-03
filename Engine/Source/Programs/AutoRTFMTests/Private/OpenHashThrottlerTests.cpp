// Copyright Epic Games, Inc. All Rights Reserved.

#include "WriteLog.h"
#include "Catch2Includes.h"
#include "OpenHashThrottler.h"

TEST_CASE("OpenHashThrottler")
{
	using FSeconds = AutoRTFM::FOpenHashThrottler::FSeconds;
	
	AutoRTFM::FOpenHashThrottler Throttler{
		/* LogInterval            */ 1e10, // Not testing logging.
		/* AdjustThrottleInterval */ 0.1,  // Adjust probabilities every 100ms.
		/* TargetFractionHashing  */ 0.1   // At most we want to spent 10% of the time hashing.
	};

	const void* OpenReturnAddressA = reinterpret_cast<const void*>(static_cast<uintptr_t>(0x10000));
	const void* OpenReturnAddressB = reinterpret_cast<const void*>(static_cast<uintptr_t>(0x20000));
	const void* OpenReturnAddressC = reinterpret_cast<const void*>(static_cast<uintptr_t>(0x30000));
	AutoRTFM::FWriteLog WriteLog;

	// The current time for the tests.
	FSeconds Time = 0;

	auto Hash = [&](FSeconds DurationNotHashing, FSeconds DurationHashing, const void* OpenReturnAddress)
	{
		Time += DurationNotHashing;
		Throttler.OnHash(Time, Time + DurationHashing, OpenReturnAddress, WriteLog);
		Time += DurationHashing;
	};
	
	FSeconds LastUpdate = 0;
	auto Update = [&](FSeconds Duration = 0)
	{
		Time += Duration;
		Throttler.Update(Time - LastUpdate);
		LastUpdate = Time;
	};

	SECTION("Initial state")
	{
		REQUIRE(Throttler.HashProbabilityFor(OpenReturnAddressA) == 1.0);
		REQUIRE(Throttler.HashProbabilityFor(OpenReturnAddressB) == 1.0);
		REQUIRE(Throttler.HashProbabilityFor(OpenReturnAddressC) == 1.0);
	}

	SECTION("Before adjustment")
	{
		Hash(/* DurationNotHashing */ 0, /* DurationHashing */ 0.01, OpenReturnAddressA);
		Hash(/* DurationNotHashing */ 0, /* DurationHashing */ 0.01, OpenReturnAddressB);
		Hash(/* DurationNotHashing */ 0, /* DurationHashing */ 0.01, OpenReturnAddressC);
		Update();
		REQUIRE(Throttler.HashProbabilityFor(OpenReturnAddressA) == 1.0);
		REQUIRE(Throttler.HashProbabilityFor(OpenReturnAddressB) == 1.0);
		REQUIRE(Throttler.HashProbabilityFor(OpenReturnAddressC) == 1.0);
	}

	SECTION("Within TargetFractionHashing limits")
	{
		for (int I = 0; I < 100; I++)
		{
			// 1% of the time spent hashing (less than the 10% threshold to throttle)
			Hash(/* DurationNotHashing */ 0.099, /* DurationHashing */ 0.001, OpenReturnAddressA);
			Hash(/* DurationNotHashing */ 0.099, /* DurationHashing */ 0.001, OpenReturnAddressB);
			Hash(/* DurationNotHashing */ 0.099, /* DurationHashing */ 0.001, OpenReturnAddressC);
			Update();
		}
		REQUIRE(Throttler.HashProbabilityFor(OpenReturnAddressA) == 1.0);
		REQUIRE(Throttler.HashProbabilityFor(OpenReturnAddressB) == 1.0);
		REQUIRE(Throttler.HashProbabilityFor(OpenReturnAddressC) == 1.0);
	}

	SECTION("Exceeds TargetFractionHashing limits")
	{
		SECTION("Small Margin") // limit exceeded by typical margin (1x - 2x)
		{
			for (int I = 0; I < 100; I++)
			{
				// 15% of the time spent hashing (greater than the 10% threshold to throttle)
				Hash(/* DurationNotHashing */ 0.85, /* DurationHashing */ 0.15, OpenReturnAddressA);
				Hash(/* DurationNotHashing */ 0.85, /* DurationHashing */ 0.15, OpenReturnAddressB);
				Hash(/* DurationNotHashing */ 0.85, /* DurationHashing */ 0.15, OpenReturnAddressC);
				Update();
			}
			REQUIRE(Throttler.HashProbabilityFor(OpenReturnAddressA) < 1.0);
			REQUIRE(Throttler.HashProbabilityFor(OpenReturnAddressA) > 0.0);
			REQUIRE(Throttler.HashProbabilityFor(OpenReturnAddressB) < 1.0);
			REQUIRE(Throttler.HashProbabilityFor(OpenReturnAddressB) > 0.0);
			REQUIRE(Throttler.HashProbabilityFor(OpenReturnAddressC) < 1.0);
			REQUIRE(Throttler.HashProbabilityFor(OpenReturnAddressC) > 0.0);
		}
		
		SECTION("Excessive") // limit exceeded by large margin (2x+)
		{
			const double OriginalProbabilityC = Throttler.HashProbabilityFor(OpenReturnAddressC);
			for (int I = 0; I < 100; I++)
			{
				// 1% of the time hashing (below throttling level)
				Hash(/* DurationNotHashing */ 0.99, /* DurationHashing */ 0.01, OpenReturnAddressC);
				Update();
			}
			// Below throttling threshold, should not have been updated.
			REQUIRE(OriginalProbabilityC == Throttler.HashProbabilityFor(OpenReturnAddressC));
			
			for (int I = 0; I < 100; I++)
			{
				// 30% of the time spent hashing (greater than the 10% threshold to throttle and
				// more then 2x the budget)
				Hash(/* DurationNotHashing */ 0.60, /* DurationHashing */ 0.40, OpenReturnAddressA);
				Hash(/* DurationNotHashing */ 0.70, /* DurationHashing */ 0.30, OpenReturnAddressB);
				Update();
			}

			// When excessively exceeding the budget, all open probabilities are
			// uniformly reduced, including inactive opens.
			REQUIRE(Throttler.HashProbabilityFor(OpenReturnAddressA) < 1.0);
			REQUIRE(Throttler.HashProbabilityFor(OpenReturnAddressA) > 0.0);
			REQUIRE(Throttler.HashProbabilityFor(OpenReturnAddressB) == Throttler.HashProbabilityFor(OpenReturnAddressA));
			REQUIRE(Throttler.HashProbabilityFor(OpenReturnAddressC) < OriginalProbabilityC);
			REQUIRE(Throttler.HashProbabilityFor(OpenReturnAddressC) > 0.0);
		}
	}

	SECTION("Normalize by duration")
	{
		for (int I = 0; I < 100; I++)
		{
			// Time spent hashing ratios:
			// OpenReturnAddressA:OpenReturnAddressB:OpenReturnAddressC - 5:3:1 
			Hash(/* DurationNotHashing */ 0.25, /* DurationHashing */ 0.05, OpenReturnAddressA);
			Hash(/* DurationNotHashing */ 0.25, /* DurationHashing */ 0.03, OpenReturnAddressB);
			Hash(/* DurationNotHashing */ 0.25, /* DurationHashing */ 0.01, OpenReturnAddressC);
			Update();
		}
		REQUIRE(Throttler.HashProbabilityFor(OpenReturnAddressA) < 1.0);
		REQUIRE(Throttler.HashProbabilityFor(OpenReturnAddressA) > 0.0);
		REQUIRE(Throttler.HashProbabilityFor(OpenReturnAddressB) < 1.0);
		REQUIRE(Throttler.HashProbabilityFor(OpenReturnAddressB) > 0.0);
		REQUIRE(Throttler.HashProbabilityFor(OpenReturnAddressC) <= 1.0);
		REQUIRE(Throttler.HashProbabilityFor(OpenReturnAddressC) >  0.0);
		REQUIRE(Throttler.HashProbabilityFor(OpenReturnAddressA) < Throttler.HashProbabilityFor(OpenReturnAddressB));
		REQUIRE(Throttler.HashProbabilityFor(OpenReturnAddressB) < Throttler.HashProbabilityFor(OpenReturnAddressC));
	}

	SECTION("Normalize by frequency")
	{
		for (int I = 0; I < 100; I++)
		{
			// Hash count ratios:
			// OpenReturnAddressA:OpenReturnAddressB:OpenReturnAddressC - 5:3:1 
			Hash(/* DurationNotHashing */ 0.25, /* DurationHashing */ 0.05, OpenReturnAddressA);
			Hash(/* DurationNotHashing */ 0.25, /* DurationHashing */ 0.05, OpenReturnAddressB);
			Hash(/* DurationNotHashing */ 0.25, /* DurationHashing */ 0.05, OpenReturnAddressA);
			Hash(/* DurationNotHashing */ 0.25, /* DurationHashing */ 0.05, OpenReturnAddressC);
			Hash(/* DurationNotHashing */ 0.25, /* DurationHashing */ 0.05, OpenReturnAddressA);
			Hash(/* DurationNotHashing */ 0.25, /* DurationHashing */ 0.05, OpenReturnAddressB);
			Hash(/* DurationNotHashing */ 0.25, /* DurationHashing */ 0.05, OpenReturnAddressA);
			Hash(/* DurationNotHashing */ 0.25, /* DurationHashing */ 0.05, OpenReturnAddressB);
			Hash(/* DurationNotHashing */ 0.25, /* DurationHashing */ 0.05, OpenReturnAddressA);
			Update();
		}
		REQUIRE(Throttler.HashProbabilityFor(OpenReturnAddressA) < 1.0);
		REQUIRE(Throttler.HashProbabilityFor(OpenReturnAddressA) > 0.0);
		REQUIRE(Throttler.HashProbabilityFor(OpenReturnAddressB) < 1.0);
		REQUIRE(Throttler.HashProbabilityFor(OpenReturnAddressB) > 0.0);
		REQUIRE(Throttler.HashProbabilityFor(OpenReturnAddressC) <= 1.0);
		REQUIRE(Throttler.HashProbabilityFor(OpenReturnAddressC) >  0.0);
		REQUIRE(Throttler.HashProbabilityFor(OpenReturnAddressA) < Throttler.HashProbabilityFor(OpenReturnAddressB));
		REQUIRE(Throttler.HashProbabilityFor(OpenReturnAddressB) < Throttler.HashProbabilityFor(OpenReturnAddressC));
	}

	SECTION("New opens inherit lowest probability")
	{
		for (int I = 0; I < 100; I++)
		{
			Hash(/* DurationNotHashing */ 0.25, /* DurationHashing */ 0.05, OpenReturnAddressA);
			Hash(/* DurationNotHashing */ 0.25, /* DurationHashing */ 0.03, OpenReturnAddressB);
			Update();
		}

		Hash(/* DurationNotHashing */ 0.20, /* DurationHashing */ 0.01, OpenReturnAddressC);
		
		REQUIRE(Throttler.HashProbabilityFor(OpenReturnAddressA) < Throttler.HashProbabilityFor(OpenReturnAddressB));
		REQUIRE(Throttler.HashProbabilityFor(OpenReturnAddressC) == Throttler.HashProbabilityFor(OpenReturnAddressA));
	}

	SECTION("Preserve probabilities for old open return addresses")
	{
		// Throttle OpenReturnAddressA
		for (int I = 0; I < 10; I++)
		{
			Hash(/* DurationNotHashing */ 0.05, /* DurationHashing */ 0.02, OpenReturnAddressA);
			Update();
		}

		Update();
		const double ProbabilityA = Throttler.HashProbabilityFor(OpenReturnAddressA);
		REQUIRE(ProbabilityA < 1.0);

		// Now throttle OpenReturnAddressB and OpenReturnAddressC
		for (int I = 0; I < 10; I++)
		{
			Hash(/* DurationNotHashing */ 0.05, /* DurationHashing */ 0.01, OpenReturnAddressB);
			Hash(/* DurationNotHashing */ 0.05, /* DurationHashing */ 0.01, OpenReturnAddressC);
			Update();
		}
		
		REQUIRE(Throttler.HashProbabilityFor(OpenReturnAddressB) < 1.0);
		REQUIRE(Throttler.HashProbabilityFor(OpenReturnAddressC) < 1.0);
		// Even though OpenReturnAddressA wasn't used in the last 100 hashes, its probability remains untouched.
		REQUIRE(Throttler.HashProbabilityFor(OpenReturnAddressA) == ProbabilityA);
	}

	SECTION("Raise probabilities by reduced duration hashing")
	{
		for (int I = 0; I < 100; I++)
		{
			Hash(/* DurationNotHashing */ 0.5, /* DurationHashing */ 0.2, OpenReturnAddressA);
			Hash(/* DurationNotHashing */ 0.5, /* DurationHashing */ 0.2, OpenReturnAddressB);
			Hash(/* DurationNotHashing */ 0.5, /* DurationHashing */ 0.2, OpenReturnAddressC);
			Update(1.0);
		}

		const double ProbabilityA = Throttler.HashProbabilityFor(OpenReturnAddressA);
		const double ProbabilityB = Throttler.HashProbabilityFor(OpenReturnAddressB);
		const double ProbabilityC = Throttler.HashProbabilityFor(OpenReturnAddressC);

		for (int I = 0; I < 100; I++)
		{
			Hash(/* DurationNotHashing */ 0.5, /* DurationHashing */ 0.1, OpenReturnAddressA);
			Hash(/* DurationNotHashing */ 0.5, /* DurationHashing */ 0.1, OpenReturnAddressB);
			Hash(/* DurationNotHashing */ 0.5, /* DurationHashing */ 0.1, OpenReturnAddressC);
			Update(1.0);
		}

		REQUIRE(Throttler.HashProbabilityFor(OpenReturnAddressA) > ProbabilityA);
		REQUIRE(Throttler.HashProbabilityFor(OpenReturnAddressB) > ProbabilityB);
		REQUIRE(Throttler.HashProbabilityFor(OpenReturnAddressC) > ProbabilityC);
	}
	
	SECTION("Raise probabilities by reduced frequency")
	{
		for (int I = 0; I < 100; I++)
		{
			Hash(/* DurationNotHashing */ 0.25, /* DurationHashing */ 0.10, OpenReturnAddressA);
			Hash(/* DurationNotHashing */ 0.25, /* DurationHashing */ 0.10, OpenReturnAddressB);
			Hash(/* DurationNotHashing */ 0.25, /* DurationHashing */ 0.10, OpenReturnAddressC);
			Hash(/* DurationNotHashing */ 0.25, /* DurationHashing */ 0.10, OpenReturnAddressA);
			Hash(/* DurationNotHashing */ 0.25, /* DurationHashing */ 0.10, OpenReturnAddressB);
			Hash(/* DurationNotHashing */ 0.25, /* DurationHashing */ 0.10, OpenReturnAddressC);
			Update(1.0);
		}

		const double ProbabilityA = Throttler.HashProbabilityFor(OpenReturnAddressA);
		const double ProbabilityB = Throttler.HashProbabilityFor(OpenReturnAddressB);
		const double ProbabilityC = Throttler.HashProbabilityFor(OpenReturnAddressC);

		for (int I = 0; I < 100; I++)
		{
			Hash(/* DurationNotHashing */ 0.35, /* DurationHashing */ 0.10, OpenReturnAddressA);
			Hash(/* DurationNotHashing */ 0.35, /* DurationHashing */ 0.10, OpenReturnAddressB);
			Hash(/* DurationNotHashing */ 0.35, /* DurationHashing */ 0.10, OpenReturnAddressC);
			Update(1.0);
		}

		REQUIRE(Throttler.HashProbabilityFor(OpenReturnAddressA) > ProbabilityA);
		REQUIRE(Throttler.HashProbabilityFor(OpenReturnAddressB) > ProbabilityB);
		REQUIRE(Throttler.HashProbabilityFor(OpenReturnAddressC) > ProbabilityC);
	}
}
