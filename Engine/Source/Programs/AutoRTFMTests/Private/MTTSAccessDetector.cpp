// Copyright Epic Games, Inc. All Rights Reserved.

#include "AutoRTFMTesting.h"
#include "Catch2Includes.h"
#include "Misc/MTTransactionallySafeAccessDetector.h"

#include <atomic>
#include <thread>

#if ENABLE_MT_DETECTOR

TEST_CASE("MTTransactionallySafeAccessDetector")
{
	SECTION("Transact(UE_MT_SCOPED_READ_ACCESS, Scope(UE_MT_SCOPED_READ_ACCESS), Abort)")
	{
		FRWTransactionallySafeAccessDetector Detector;
		AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
			{
				UE_MT_SCOPED_READ_ACCESS(Detector);
				{
					UE_MT_SCOPED_READ_ACCESS(Detector);
				}
				AutoRTFM::AbortTransaction();
			});
		REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == Result);
	}

	SECTION("UE_MT_SCOPED_READ_ACCESS, Transact(UE_MT_SCOPED_READ_ACCESS)")
	{
		FRWTransactionallySafeAccessDetector Detector;
		UE_MT_SCOPED_READ_ACCESS(Detector);
		AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
			{
				UE_MT_SCOPED_READ_ACCESS(Detector);
			});
		REQUIRE(AutoRTFM::ETransactionResult::Committed == Result);
	}

	SECTION("UE_MT_SCOPED_READ_ACCESS, Transact(UE_MT_SCOPED_READ_ACCESS, Abort)")
	{
		FRWTransactionallySafeAccessDetector Detector;
		UE_MT_SCOPED_READ_ACCESS(Detector);
		AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
			{
				UE_MT_SCOPED_READ_ACCESS(Detector);
				AutoRTFM::AbortTransaction();
			});
		REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == Result);
	}

	SECTION("Transact(UE_MT_SCOPED_WRITE_ACCESS)")
	{
		FRWTransactionallySafeAccessDetector Detector;
		AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
			{
				UE_MT_SCOPED_WRITE_ACCESS(Detector);
			});
		REQUIRE(AutoRTFM::ETransactionResult::Committed == Result);
	}

	SECTION("Transact(UE_MT_SCOPED_WRITE_ACCESS, Abort)")
	{
		FRWTransactionallySafeAccessDetector Detector;
		AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
			{
				UE_MT_SCOPED_WRITE_ACCESS(Detector);
				AutoRTFM::AbortTransaction();
			});
		REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == Result);
	}

	SECTION("Transact(Scope(UE_MT_SCOPED_WRITE_ACCESS), Abort)")
	{
		FRWTransactionallySafeAccessDetector Detector;
		AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
			{
				{
					UE_MT_SCOPED_WRITE_ACCESS(Detector);
				}
				AutoRTFM::AbortTransaction();
			});
		REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == Result);
	}

	SECTION("Transact(Scope(UE_MT_SCOPED_READ_ACCESS), Scope(UE_MT_SCOPED_READ_ACCESS))")
	{
		FRWTransactionallySafeAccessDetector Detector;
		AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
			{
				{
					UE_MT_SCOPED_READ_ACCESS(Detector);
				}
				{
					UE_MT_SCOPED_READ_ACCESS(Detector);
				}
			});
		REQUIRE(AutoRTFM::ETransactionResult::Committed == Result);
	}

	SECTION("Transact(Scope(UE_MT_SCOPED_READ_ACCESS), Scope(UE_MT_SCOPED_WRITE_ACCESS))")
	{
		FRWTransactionallySafeAccessDetector Detector;
		AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
			{
				{
					UE_MT_SCOPED_READ_ACCESS(Detector);
				}
				{
					UE_MT_SCOPED_WRITE_ACCESS(Detector);
				}
			});
		REQUIRE(AutoRTFM::ETransactionResult::Committed == Result);
	}

	SECTION("Transact(Scope(UE_MT_SCOPED_WRITE_ACCESS), Scope(UE_MT_SCOPED_READ_ACCESS))")
	{
		FRWTransactionallySafeAccessDetector Detector;
		AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
			{
				{
					UE_MT_SCOPED_WRITE_ACCESS(Detector);
				}
				{
					UE_MT_SCOPED_READ_ACCESS(Detector);
				}
			});
		REQUIRE(AutoRTFM::ETransactionResult::Committed == Result);
	}

	SECTION("Transact(Scope(UE_MT_SCOPED_WRITE_ACCESS), Scope(UE_MT_SCOPED_WRITE_ACCESS))")
	{
		FRWTransactionallySafeAccessDetector Detector;
		AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
			{
				{
					UE_MT_SCOPED_WRITE_ACCESS(Detector);
				}
				{
					UE_MT_SCOPED_WRITE_ACCESS(Detector);
				}
			});
		REQUIRE(AutoRTFM::ETransactionResult::Committed == Result);
	}

	SECTION("Transact(Scope(UE_MT_SCOPED_READ_ACCESS), Scope(UE_MT_SCOPED_READ_ACCESS), Abort)")
	{
		FRWTransactionallySafeAccessDetector Detector;
		AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
			{
				{
					UE_MT_SCOPED_READ_ACCESS(Detector);
				}
				{
					UE_MT_SCOPED_READ_ACCESS(Detector);
				}
				AutoRTFM::AbortTransaction();
			});
		REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == Result);
	}

	SECTION("Transact(Scope(UE_MT_SCOPED_READ_ACCESS), Scope(UE_MT_SCOPED_WRITE_ACCESS), Abort)")
	{
		FRWTransactionallySafeAccessDetector Detector;
		AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
			{
				{
					UE_MT_SCOPED_READ_ACCESS(Detector);
				}
				{
					UE_MT_SCOPED_WRITE_ACCESS(Detector);
				}
				AutoRTFM::AbortTransaction();
			});
		REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == Result);
	}

	SECTION("Transact(Scope(UE_MT_SCOPED_WRITE_ACCESS), Scope(UE_MT_SCOPED_READ_ACCESS), Abort)")
	{
		FRWTransactionallySafeAccessDetector Detector;
		AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
			{
				{
					UE_MT_SCOPED_WRITE_ACCESS(Detector);
				}
				{
					UE_MT_SCOPED_READ_ACCESS(Detector);
				}
				AutoRTFM::AbortTransaction();
			});
		REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == Result);
	}

	SECTION("Transact(Scope(UE_MT_SCOPED_WRITE_ACCESS), Scope(UE_MT_SCOPED_WRITE_ACCESS), Abort)")
	{
		FRWTransactionallySafeAccessDetector Detector;
		AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
			{
				{
					UE_MT_SCOPED_WRITE_ACCESS(Detector);
				}
				{
					UE_MT_SCOPED_WRITE_ACCESS(Detector);
				}
				AutoRTFM::AbortTransaction();
			});
		REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == Result);
	}

	SECTION("Transact(Scope(UE_MT_SCOPED_READ_ACCESS), Scope(UE_MT_SCOPED_READ_ACCESS, Abort))")
	{
		FRWTransactionallySafeAccessDetector Detector;
		AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
			{
				{
					UE_MT_SCOPED_READ_ACCESS(Detector);
				}
				{
					UE_MT_SCOPED_READ_ACCESS(Detector);
					AutoRTFM::AbortTransaction();
				}
			});
		REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == Result);
	}

	SECTION("Transact(Scope(UE_MT_SCOPED_READ_ACCESS), Scope(UE_MT_SCOPED_WRITE_ACCESS, Abort))")
	{
		FRWTransactionallySafeAccessDetector Detector;
		AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
			{
				{
					UE_MT_SCOPED_READ_ACCESS(Detector);
				}
				{
					UE_MT_SCOPED_WRITE_ACCESS(Detector);
					AutoRTFM::AbortTransaction();
				}
			});
		REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == Result);
	}

	SECTION("Transact(Scope(UE_MT_SCOPED_WRITE_ACCESS), Scope(UE_MT_SCOPED_READ_ACCESS, Abort))")
	{
		FRWTransactionallySafeAccessDetector Detector;
		AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
			{
				{
					UE_MT_SCOPED_WRITE_ACCESS(Detector);
				}
				{
					UE_MT_SCOPED_READ_ACCESS(Detector);
					AutoRTFM::AbortTransaction();
				}
			});
		REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == Result);
	}

	SECTION("Transact(Scope(UE_MT_SCOPED_WRITE_ACCESS), Scope(UE_MT_SCOPED_WRITE_ACCESS, Abort))")
	{
		FRWTransactionallySafeAccessDetector Detector;
		AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
			{
				{
					UE_MT_SCOPED_WRITE_ACCESS(Detector);
				}
				{
					UE_MT_SCOPED_WRITE_ACCESS(Detector);
					AutoRTFM::AbortTransaction();
				}
			});
		REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == Result);
	}

	SECTION("Transact(UE_MT_SCOPED_READ_ACCESS, Open(UE_MT_SCOPED_READ_ACCESS))")
	{
		FRWTransactionallySafeAccessDetector Detector;
		AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
			{
				UE_MT_SCOPED_READ_ACCESS(Detector);
				AutoRTFM::Open([&]
					{
						UE_MT_SCOPED_READ_ACCESS(Detector);
					});
			});
		REQUIRE(AutoRTFM::ETransactionResult::Committed == Result);
	}

	SECTION("Transact(UE_MT_SCOPED_READ_ACCESS, Open(UE_MT_SCOPED_READ_ACCESS), Abort)")
	{
		FRWTransactionallySafeAccessDetector Detector;
		AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
			{
				UE_MT_SCOPED_READ_ACCESS(Detector);
				AutoRTFM::Open([&]
					{
						UE_MT_SCOPED_READ_ACCESS(Detector);
					});
				AutoRTFM::AbortTransaction();
			});
		REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == Result);
	}

	SECTION("Transact(Scope(UE_MT_SCOPED_READ_ACCESS, Open(UE_MT_SCOPED_READ_ACCESS)), Abort)")
	{
		FRWTransactionallySafeAccessDetector Detector;
		AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
			{
				{
					UE_MT_SCOPED_READ_ACCESS(Detector);
					AutoRTFM::Open([&]
						{
							UE_MT_SCOPED_READ_ACCESS(Detector);
						});
				}
				AutoRTFM::AbortTransaction();
			});
		REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == Result);
	}

	SECTION("Transact(Scope(UE_MT_SCOPED_READ_ACCESS), Open(UE_MT_SCOPED_READ_ACCESS))")
	{
		FRWTransactionallySafeAccessDetector Detector;
		AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
			{
				{
					UE_MT_SCOPED_READ_ACCESS(Detector);
				}
				AutoRTFM::Open([&]
					{
						UE_MT_SCOPED_READ_ACCESS(Detector);
					});
			});
		REQUIRE(AutoRTFM::ETransactionResult::Committed == Result);
	}

	SECTION("Transact(Scope(UE_MT_SCOPED_READ_ACCESS), Open(UE_MT_SCOPED_WRITE_ACCESS))")
	{
		FRWTransactionallySafeAccessDetector Detector;
		AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
			{
				{
					UE_MT_SCOPED_READ_ACCESS(Detector);
				}
				AutoRTFM::Open([&]
					{
						UE_MT_SCOPED_WRITE_ACCESS(Detector);
					});
			});
		REQUIRE(AutoRTFM::ETransactionResult::Committed == Result);
	}

	SECTION("Transact(Scope(UE_MT_SCOPED_WRITE_ACCESS), Open(UE_MT_SCOPED_READ_ACCESS))")
	{
		FRWTransactionallySafeAccessDetector Detector;
		AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
			{
				{
					UE_MT_SCOPED_WRITE_ACCESS(Detector);
				}
				AutoRTFM::Open([&]
					{
						UE_MT_SCOPED_READ_ACCESS(Detector);
					});
			});
		REQUIRE(AutoRTFM::ETransactionResult::Committed == Result);
	}

	SECTION("Transact(Scope(UE_MT_SCOPED_WRITE_ACCESS), Open(UE_MT_SCOPED_WRITE_ACCESS))")
	{
		FRWTransactionallySafeAccessDetector Detector;
		AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
			{
				{
					UE_MT_SCOPED_WRITE_ACCESS(Detector);
				}
				AutoRTFM::Open([&]
					{
						UE_MT_SCOPED_WRITE_ACCESS(Detector);
					});
			});
		REQUIRE(AutoRTFM::ETransactionResult::Committed == Result);
	}

	SECTION("Transact(Open(UE_MT_SCOPED_READ_ACCESS), Scope(UE_MT_SCOPED_READ_ACCESS))")
	{
		FRWTransactionallySafeAccessDetector Detector;
		AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
			{
				AutoRTFM::Open([&]
					{
						UE_MT_SCOPED_READ_ACCESS(Detector);
					});
				UE_MT_SCOPED_READ_ACCESS(Detector);
			});
		REQUIRE(AutoRTFM::ETransactionResult::Committed == Result);
	}

	SECTION("Transact(Open(UE_MT_SCOPED_READ_ACCESS), Scope(UE_MT_SCOPED_WRITE_ACCESS))")
	{
		FRWTransactionallySafeAccessDetector Detector;
		AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
			{
				AutoRTFM::Open([&]
					{
						UE_MT_SCOPED_READ_ACCESS(Detector);
					});
				UE_MT_SCOPED_WRITE_ACCESS(Detector);
			});
		REQUIRE(AutoRTFM::ETransactionResult::Committed == Result);
	}

	SECTION("Transact(Open(UE_MT_SCOPED_WRITE_ACCESS), Scope(UE_MT_SCOPED_READ_ACCESS))")
	{
		FRWTransactionallySafeAccessDetector Detector;
		AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
			{
				AutoRTFM::Open([&]
					{
						UE_MT_SCOPED_WRITE_ACCESS(Detector);
					});
				UE_MT_SCOPED_READ_ACCESS(Detector);
			});
		REQUIRE(AutoRTFM::ETransactionResult::Committed == Result);
	}

	SECTION("Transact(Open(UE_MT_SCOPED_WRITE_ACCESS), UE_MT_SCOPED_WRITE_ACCESS)")
	{
		FRWTransactionallySafeAccessDetector Detector;
		AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
			{
				AutoRTFM::Open([&]
					{
						UE_MT_SCOPED_WRITE_ACCESS(Detector);
					});
				UE_MT_SCOPED_WRITE_ACCESS(Detector);
			});
		REQUIRE(AutoRTFM::ETransactionResult::Committed == Result);
	}

	SECTION("Transact(Scope(UE_MT_SCOPED_READ_ACCESS, Spawn(UE_MT_SCOPED_READ_ACCESS)))")
	{
		FRWTransactionallySafeAccessDetector Detector;
		std::atomic_uint Handshake = 0; // Used to order this thread and the spawnee.
		std::thread OtherThread;
		AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
			{
				// Because we can retry transactions reset handshake to zero.
				AutoRTFM::Open([&]
					{
						Handshake = 0;
					});

				{
					UE_MT_SCOPED_READ_ACCESS(Detector);

					AutoRTFM::Open([&]
						{
							OtherThread = std::thread([&]
								{
									// Let the main thread progress.
									Handshake += 1;

									{
										UE_MT_SCOPED_READ_ACCESS(Detector);

										// Wait for the main thread.
										while (2 != Handshake) {}
									}
								});
						});
				}

				AutoRTFM::Open([&]
					{
						// Wait for the spawnee.
						while (1 != Handshake) {}

						// Let the spawnee progress.
						Handshake += 1;

						// Wait for the spawnee to finish before unwinding the
						// stack and invalidating the Handshake and Detector
						// references used by the thread.
						OtherThread.join();
					});
			});
		REQUIRE(AutoRTFM::ETransactionResult::Committed == Result);
	}

	SECTION("Transact(UE_MT_SCOPED_READ_ACCESS, Transact(UE_MT_SCOPED_READ_ACCESS))")
	{
		FRWTransactionallySafeAccessDetector Detector;
		AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
			{
				UE_MT_SCOPED_READ_ACCESS(Detector);

				AutoRTFM::Commit([&]
					{
						UE_MT_SCOPED_READ_ACCESS(Detector);
					});
			});
		REQUIRE(AutoRTFM::ETransactionResult::Committed == Result);
	}

	SECTION("Transact(Scope(UE_MT_SCOPED_READ_ACCESS), Transact(UE_MT_SCOPED_READ_ACCESS))")
	{
		FRWTransactionallySafeAccessDetector Detector;
		AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
			{
				{
					UE_MT_SCOPED_READ_ACCESS(Detector);
				}

				AutoRTFM::Commit([&]
					{
						UE_MT_SCOPED_READ_ACCESS(Detector);
					});
			});
		REQUIRE(AutoRTFM::ETransactionResult::Committed == Result);
	}

	SECTION("Transact(Scope(UE_MT_SCOPED_READ_ACCESS), Transact(UE_MT_SCOPED_WRITE_ACCESS))")
	{
		FRWTransactionallySafeAccessDetector Detector;
		AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
			{
				{
					UE_MT_SCOPED_READ_ACCESS(Detector);
				}

				AutoRTFM::Commit([&]
					{
						UE_MT_SCOPED_WRITE_ACCESS(Detector);
					});
			});
		REQUIRE(AutoRTFM::ETransactionResult::Committed == Result);
	}

	SECTION("Transact(Scope(UE_MT_SCOPED_WRITE_ACCESS), Transact(UE_MT_SCOPED_READ_ACCESS))")
	{
		FRWTransactionallySafeAccessDetector Detector;
		AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
			{
				{
					UE_MT_SCOPED_WRITE_ACCESS(Detector);
				}

				AutoRTFM::Commit([&]
					{
						UE_MT_SCOPED_READ_ACCESS(Detector);
					});
			});
		REQUIRE(AutoRTFM::ETransactionResult::Committed == Result);
	}

	SECTION("Transact(Scope(UE_MT_SCOPED_WRITE_ACCESS), Transact(UE_MT_SCOPED_WRITE_ACCESS))")
	{
		FRWTransactionallySafeAccessDetector Detector;
		AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
			{
				{
					UE_MT_SCOPED_WRITE_ACCESS(Detector);
				}

				AutoRTFM::Commit([&]
					{
						UE_MT_SCOPED_WRITE_ACCESS(Detector);
					});
			});
		REQUIRE(AutoRTFM::ETransactionResult::Committed == Result);
	}

	SECTION("Transact(Scope(UE_MT_SCOPED_READ_ACCESS), Transact(UE_MT_SCOPED_READ_ACCESS), Abort)")
	{
		FRWTransactionallySafeAccessDetector Detector;
		AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
			{
				{
					UE_MT_SCOPED_READ_ACCESS(Detector);
				}

				AutoRTFM::Commit([&]
					{
						UE_MT_SCOPED_READ_ACCESS(Detector);
					});

				AutoRTFM::AbortTransaction();
			});
		REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == Result);
	}

	SECTION("Transact(Scope(UE_MT_SCOPED_READ_ACCESS), Transact(UE_MT_SCOPED_WRITE_ACCESS), Abort)")
	{
		FRWTransactionallySafeAccessDetector Detector;
		AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
			{
				{
					UE_MT_SCOPED_READ_ACCESS(Detector);
				}

				AutoRTFM::Commit([&]
					{
						UE_MT_SCOPED_WRITE_ACCESS(Detector);
					});

				AutoRTFM::AbortTransaction();
			});
		REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == Result);
	}

	SECTION("Transact(Scope(UE_MT_SCOPED_WRITE_ACCESS), Transact(UE_MT_SCOPED_READ_ACCESS), Abort)")
	{
		FRWTransactionallySafeAccessDetector Detector;
		AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
			{
				{
					UE_MT_SCOPED_WRITE_ACCESS(Detector);
				}

				AutoRTFM::Commit([&]
					{
						UE_MT_SCOPED_READ_ACCESS(Detector);
					});

				AutoRTFM::AbortTransaction();
			});
		REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == Result);
	}

	SECTION("Transact(Scope(UE_MT_SCOPED_WRITE_ACCESS), Transact(UE_MT_SCOPED_WRITE_ACCESS), Abort)")
	{
		FRWTransactionallySafeAccessDetector Detector;
		AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
			{
				{
					UE_MT_SCOPED_WRITE_ACCESS(Detector);
				}

				AutoRTFM::Commit([&]
					{
						UE_MT_SCOPED_WRITE_ACCESS(Detector);
					});

				AutoRTFM::AbortTransaction();
			});
		REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == Result);
	}

	SECTION("Transact(Scope(UE_MT_SCOPED_READ_ACCESS), Transact(UE_MT_SCOPED_READ_ACCESS), Abort)")
	{
		FRWTransactionallySafeAccessDetector Detector;
		AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
			{
				{
					UE_MT_SCOPED_READ_ACCESS(Detector);
				}

				AutoRTFM::ETransactionResult Result2 = AutoRTFM::Transact([&]
					{
						UE_MT_SCOPED_READ_ACCESS(Detector);
						AutoRTFM::AbortTransaction();
					});
				REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == Result2);
			});
		REQUIRE(AutoRTFM::ETransactionResult::Committed == Result);
	}

	SECTION("Transact(Scope(UE_MT_SCOPED_READ_ACCESS), Transact(UE_MT_SCOPED_WRITE_ACCESS), Abort)")
	{
		FRWTransactionallySafeAccessDetector Detector;
		AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
			{
				{
					UE_MT_SCOPED_READ_ACCESS(Detector);
				}

				AutoRTFM::ETransactionResult Result2 = AutoRTFM::Transact([&]
					{
						UE_MT_SCOPED_WRITE_ACCESS(Detector);
						AutoRTFM::AbortTransaction();
					});
				REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == Result2);
			});
		REQUIRE(AutoRTFM::ETransactionResult::Committed == Result);
	}

	SECTION("Transact(Scope(UE_MT_SCOPED_WRITE_ACCESS), Transact(UE_MT_SCOPED_READ_ACCESS), Abort)")
	{
		FRWTransactionallySafeAccessDetector Detector;
		AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
			{
				{
					UE_MT_SCOPED_WRITE_ACCESS(Detector);
				}

				AutoRTFM::ETransactionResult Result2 = AutoRTFM::Transact([&]
					{
						UE_MT_SCOPED_READ_ACCESS(Detector);
						AutoRTFM::AbortTransaction();
					});
				REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == Result2);
			});
		REQUIRE(AutoRTFM::ETransactionResult::Committed == Result);
	}

	SECTION("Transact(Scope(UE_MT_SCOPED_WRITE_ACCESS), Transact(UE_MT_SCOPED_WRITE_ACCESS), Abort)")
	{
		FRWTransactionallySafeAccessDetector Detector;
		AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
			{
				{
					UE_MT_SCOPED_WRITE_ACCESS(Detector);
				}

				AutoRTFM::ETransactionResult Result2 = AutoRTFM::Transact([&]
					{
						UE_MT_SCOPED_WRITE_ACCESS(Detector);
						AutoRTFM::AbortTransaction();
					});
				REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == Result2);
			});
		REQUIRE(AutoRTFM::ETransactionResult::Committed == Result);
	}

	SECTION("Transact(Scope(UE_MT_SCOPED_READ_ACCESS), OnCommit(UE_MT_SCOPED_READ_ACCESS))")
	{
		FRWTransactionallySafeAccessDetector Detector;
		AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
			{
				{
					UE_MT_SCOPED_READ_ACCESS(Detector);
				}

				AutoRTFM::OnCommit([&]
					{
						UE_MT_SCOPED_READ_ACCESS(Detector);
					});
			});
		REQUIRE(AutoRTFM::ETransactionResult::Committed == Result);
	}

	SECTION("Transact(Scope(UE_MT_SCOPED_READ_ACCESS), OnCommit(UE_MT_SCOPED_WRITE_ACCESS))")
	{
		FRWTransactionallySafeAccessDetector Detector;
		AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
			{
				{
					UE_MT_SCOPED_READ_ACCESS(Detector);
				}

				AutoRTFM::OnCommit([&]
					{
						UE_MT_SCOPED_WRITE_ACCESS(Detector);
					});
			});
		REQUIRE(AutoRTFM::ETransactionResult::Committed == Result);
	}

	SECTION("Transact(Scope(UE_MT_SCOPED_WRITE_ACCESS), OnCommit(UE_MT_SCOPED_READ_ACCESS))")
	{
		FRWTransactionallySafeAccessDetector Detector;
		AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
			{
				{
					UE_MT_SCOPED_WRITE_ACCESS(Detector);
				}

				AutoRTFM::OnCommit([&]
					{
						UE_MT_SCOPED_READ_ACCESS(Detector);
					});
			});
		REQUIRE(AutoRTFM::ETransactionResult::Committed == Result);
	}

	SECTION("Transact(Scope(UE_MT_SCOPED_WRITE_ACCESS), OnCommit(UE_MT_SCOPED_WRITE_ACCESS))")
	{
		FRWTransactionallySafeAccessDetector Detector;
		AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
			{
				{
					UE_MT_SCOPED_WRITE_ACCESS(Detector);
				}

				AutoRTFM::OnCommit([&]
					{
						UE_MT_SCOPED_WRITE_ACCESS(Detector);
					});
			});
		REQUIRE(AutoRTFM::ETransactionResult::Committed == Result);
	}

	SECTION("Transact(Scope(UE_MT_SCOPED_READ_ACCESS), OnAbort(UE_MT_SCOPED_READ_ACCESS))")
	{
		FRWTransactionallySafeAccessDetector Detector;
		AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
			{
				{
					UE_MT_SCOPED_READ_ACCESS(Detector);
				}

				AutoRTFM::OnAbort([&]
					{
						UE_MT_SCOPED_READ_ACCESS(Detector);
					});

				AutoRTFM::AbortTransaction();
			});
		REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == Result);
	}

	SECTION("Transact(Scope(UE_MT_SCOPED_READ_ACCESS), OnAbort(UE_MT_SCOPED_WRITE_ACCESS))")
	{
		FRWTransactionallySafeAccessDetector Detector;
		AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
			{
				{
					UE_MT_SCOPED_READ_ACCESS(Detector);
				}

				AutoRTFM::OnAbort([&]
					{
						UE_MT_SCOPED_WRITE_ACCESS(Detector);
					});

				AutoRTFM::AbortTransaction();
			});
		REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == Result);
	}

	SECTION("Transact(Scope(UE_MT_SCOPED_WRITE_ACCESS), OnAbort(UE_MT_SCOPED_READ_ACCESS))")
	{
		FRWTransactionallySafeAccessDetector Detector;
		AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
			{
				{
					UE_MT_SCOPED_WRITE_ACCESS(Detector);
				}

				AutoRTFM::OnAbort([&]
					{
						UE_MT_SCOPED_READ_ACCESS(Detector);
					});

				AutoRTFM::AbortTransaction();
			});
		REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == Result);
	}

	SECTION("Transact(Scope(UE_MT_SCOPED_WRITE_ACCESS), OnAbort(UE_MT_SCOPED_WRITE_ACCESS), Abort)")
	{
		FRWTransactionallySafeAccessDetector Detector;
		AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
			{
				{
					UE_MT_SCOPED_WRITE_ACCESS(Detector);
				}

				AutoRTFM::OnAbort([&]
					{
						UE_MT_SCOPED_WRITE_ACCESS(Detector);
					});

				AutoRTFM::AbortTransaction();
			});
		REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == Result);
	}

	SECTION("Transact(Create, UE_MT_SCOPED_READ_ACCESS)")
	{
		AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
			{
				FRWTransactionallySafeAccessDetector Detector;
				UE_MT_SCOPED_READ_ACCESS(Detector);
			});
		REQUIRE(AutoRTFM::ETransactionResult::Committed == Result);
	}

	SECTION("Transact(Create, UE_MT_SCOPED_READ_ACCESS, Abort)")
	{
		AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
			{
				FRWTransactionallySafeAccessDetector Detector;
				UE_MT_SCOPED_READ_ACCESS(Detector);
				AutoRTFM::AbortTransaction();
			});
		REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == Result);
	}

	SECTION("Transact(Create, UE_MT_SCOPED_WRITE_ACCESS)")
	{
		AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
			{
				FRWTransactionallySafeAccessDetector Detector;
				UE_MT_SCOPED_WRITE_ACCESS(Detector);
			});
		REQUIRE(AutoRTFM::ETransactionResult::Committed == Result);
	}

	SECTION("Transact(Create, UE_MT_SCOPED_WRITE_ACCESS, Abort)")
	{
		AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
			{
				FRWTransactionallySafeAccessDetector Detector;
				UE_MT_SCOPED_WRITE_ACCESS(Detector);
				AutoRTFM::AbortTransaction();
			});
		REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == Result);
	}

	SECTION("UE_MT_SCOPED_READ_ACCESS, destruct, memzero, reconstruct")
	{
		FRWTransactionallySafeAccessDetector Detector;
		SECTION("Commit")
		{
			AutoRTFM::Testing::Commit([&]
			{
				{
					// Read-lock and then unlock
					UE_MT_SCOPED_READ_ACCESS(Detector);
				}
				Detector.~FRWTransactionallySafeAccessDetector();
				memset(&Detector, 0, sizeof(Detector));
				new (&Detector) FRWTransactionallySafeAccessDetector();
			});
		}
		SECTION("Abort")
		{
			AutoRTFM::Testing::Abort([&]
			{
				{
					// Read-lock and then unlock
					UE_MT_SCOPED_READ_ACCESS(Detector);
				}
				Detector.~FRWTransactionallySafeAccessDetector();
				memset(&Detector, 0, sizeof(Detector));
				new (&Detector) FRWTransactionallySafeAccessDetector();
				AutoRTFM::AbortTransaction();
			});
		}
	}

	SECTION("UE_MT_SCOPED_WRITE_ACCESS, destruct, memzero, reconstruct")
	{
		FRWTransactionallySafeAccessDetector Detector;
		SECTION("Commit")
		{
			AutoRTFM::Testing::Commit([&]
			{
				{
					// Write-lock and then unlock
					UE_MT_SCOPED_WRITE_ACCESS(Detector);
				}
				Detector.~FRWTransactionallySafeAccessDetector();
				memset(&Detector, 0, sizeof(Detector));
				new (&Detector) FRWTransactionallySafeAccessDetector();
			});
		}
		SECTION("Abort")
		{
			AutoRTFM::Testing::Abort([&]
			{
				{
					// Write-lock and then unlock
					UE_MT_SCOPED_WRITE_ACCESS(Detector);
				}
				Detector.~FRWTransactionallySafeAccessDetector();
				memset(&Detector, 0, sizeof(Detector));
				new (&Detector) FRWTransactionallySafeAccessDetector();
				AutoRTFM::AbortTransaction();
			});
		}
	}
}

#endif // ENABLE_MT_DETECTOR
