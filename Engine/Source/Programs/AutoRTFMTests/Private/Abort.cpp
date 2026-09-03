// Copyright Epic Games, Inc. All Rights Reserved.

#include "AutoRTFMTesting.h"
#include "AutoRTFMTestUtils.h"
#include "Catch2Includes.h"

#include <map>
#include <vector>

TEST_CASE("Abort")
{
    int x = 42;
    std::vector<int> v;
    std::map<int, std::vector<int>> m;
    v.push_back(100);
    m[1].push_back(2);
    m[1].push_back(3);
    m[4].push_back(5);
    m[6].push_back(7);
    m[6].push_back(8);
    m[6].push_back(9);

	auto transaction = AutoRTFM::Transact([&]()
    {
		x = 5;
    	for (size_t n = 10; n--;)
    		v.push_back(2 * n);
    	m.clear();
    	m[10].push_back(11);
    	m[12].push_back(13);
    	m[12].push_back(14);
    	AutoRTFM::AbortTransaction();
	});

    REQUIRE(
        AutoRTFM::ETransactionResult::AbortedByRequest ==
		transaction);
    REQUIRE(x == 42);
    REQUIRE(v.size() == 1);
    REQUIRE(v[0] == 100);
    REQUIRE(m.size() == 3);
    REQUIRE(m[1].size() == 2);
    REQUIRE(m[1][0] == 2);
    REQUIRE(m[1][1] == 3);
    REQUIRE(m[4].size() == 1);
    REQUIRE(m[4][0] == 5);
    REQUIRE(m[6].size() == 3);
    REQUIRE(m[6][0] == 7);
    REQUIRE(m[6][1] == 8);
    REQUIRE(m[6][2] == 9);
}

TEST_CASE("Abort.NestedAbortOrder")
{
	unsigned Orderer = 0;

	AutoRTFM::Commit([&]
	{
		// If we are retrying transactions, need to reset the test state.
		AutoRTFM::OnAbort([&]
		{
			Orderer = 0;
		});

		AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
		{
			AutoRTFM::OnAbort([&]
				{
					REQUIRE(1 == Orderer);
					Orderer += 1;
				});

			AutoRTFM::OnAbort([&]
				{
					REQUIRE(0 == Orderer);
					Orderer += 1;
				});

			AutoRTFM::AbortTransaction();
		});
		REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == Result);
	});

	REQUIRE(2 == Orderer);
}

TEST_CASE("Abort.TransactionInOnCommit")
{
	AutoRTFM::Commit([&]
	{
		AutoRTFM::OnCommit([&]
		{
			bool bDidSomething = false;

			AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
			{
				bDidSomething = true;
			});

			REQUIRE(AutoRTFM::ETransactionResult::AbortedByTransactInOnCommit == Result);
			REQUIRE(false == bDidSomething);
		});
	});

}

TEST_CASE("Abort.TransactionInOnAbort")
{
	AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
	{
		AutoRTFM::OnAbort([&]
		{
			bool bDidSomething = false;

			AutoRTFM::ETransactionResult InnerResult = AutoRTFM::Transact([&]
			{
				bDidSomething = true;
			});

			REQUIRE(AutoRTFM::ETransactionResult::AbortedByTransactInOnAbort == InnerResult);
			REQUIRE(false == bDidSomething);
		});

		AutoRTFM::AbortTransaction();
	});

	REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == Result);
}

TEST_CASE("Abort.AbortInInnerTransaction")
{
	int Value = 1;

	AutoRTFM::Transact([&]
	{
		Value = 2;
		AutoRTFM::Transact([&]
		{
			Value = 3;
			AutoRTFM::AbortTransaction();  // Only cancels innermost nest.
		});
	});

	REQUIRE(Value == 2);
}

TEST_CASE("Abort.OnAbortInOuterTransaction_AbortInInnerTransaction")
{
	int Value = 1;

	AutoRTFM::Transact([&]
	{
		AutoRTFM::OnAbort([&]
		{
			if (AutoRTFM::ForTheRuntime::GetRetryTransaction() == AutoRTFM::ForTheRuntime::NoRetry)
			{
				REQUIRE(!"This should not be reached.");
				Value = 2;
			}
		});

		AutoRTFM::Transact([&]
		{
			REQUIRE(Value == 1);
			Value = 3;
			AutoRTFM::AbortTransaction();  // Only cancels innermost nest.
		});

		REQUIRE(Value == 1);
		Value = 4;
	});

	REQUIRE(Value == 4);
}

TEST_CASE("Abort.Cascade")
{
	bool bTouched = false;

	const AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
	{
		bTouched = true;
		AutoRTFM::Transact([&]
		{
			AutoRTFM::CascadingAbortTransaction();
		});

		FAIL("Execution should never reach this point");
	});

	REQUIRE(AutoRTFM::ETransactionResult::AbortedByCascade == Result);
	REQUIRE(false == bTouched);
}

TEST_CASE("Abort.CascadeWithPostAbortCallback")
{
	bool bTouched = false;
	bool bPostAbortCallbackWasExecuted = false;

	const AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
	{
		bTouched = true;
		AutoRTFM::Transact([&]
		{
			AutoRTFM::CascadingAbortTransaction([&]
			{
				REQUIRE(!bTouched);  // Rollbacks must occur before the post-abort callback is invoked.
				bPostAbortCallbackWasExecuted = true;
			});
		});

		FAIL("Execution should never reach this point");
	});

	REQUIRE(AutoRTFM::ETransactionResult::AbortedByCascade == Result);
	REQUIRE(!bTouched);
	REQUIRE(bPostAbortCallbackWasExecuted);
}

TEST_CASE("Abort.CascadingAbortMustRunOnAbortsBeforePostAbort")
{
	bool bOnAbortWasCalled = false;
	bool bPostAbortCallbackWasExecuted = false;

	const AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
	{
		AutoRTFM::OnAbort([&]
		{
			bOnAbortWasCalled = true;
		});

		AutoRTFM::CascadingAbortTransaction([&]
		{
			REQUIRE(bOnAbortWasCalled);  // OnAborts must before the post-abort callback is invoked.
			bPostAbortCallbackWasExecuted = true;
		});
	});

	REQUIRE(AutoRTFM::ETransactionResult::AbortedByCascade == Result);
	REQUIRE(bOnAbortWasCalled);
	REQUIRE(bPostAbortCallbackWasExecuted);
}

TEST_CASE("Abort.CascadingRollbackInOpen")
{
	bool bTouched = false;
	bool bExecutedCodeAfterCascadingRollback = false;

	const AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
	{
		bTouched = true;

		AutoRTFM::Open([&]
		{
			const AutoRTFM::ETransactionResult InnerResult = AutoRTFM::ForTheRuntime::CascadingRollbackTransaction();

			REQUIRE(AutoRTFM::ETransactionResult::AbortedByCascade == InnerResult);
			bExecutedCodeAfterCascadingRollback = true;
		});
	});

	REQUIRE(AutoRTFM::ETransactionResult::AbortedByCascade == Result);
	REQUIRE(!bTouched);
	REQUIRE(bExecutedCodeAfterCascadingRollback);
}

TEST_CASE("Abort.CascadeThroughOpen")
{
	bool bTouched = false;

	const AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
	{
		bTouched = true;

		AutoRTFM::Open([&]
		{
			const AutoRTFM::EContextStatus Status = AutoRTFM::Close([&]
			{
				AutoRTFM::Transact([&]
				{
					AutoRTFM::CascadingAbortTransaction();
				});
			});

			REQUIRE(AutoRTFM::EContextStatus::AbortedByCascadingAbort == Status);
		});
	});

	REQUIRE(AutoRTFM::ETransactionResult::AbortedByCascade == Result);
	REQUIRE(false == bTouched);
}

TEST_CASE("Abort.CascadeWithCallbackThroughOpen")
{
	bool bTouched = false;
	bool bPostAbortCallbackWasExecuted = false;

	const AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
	{
		bTouched = true;

		AutoRTFM::Open([&]
		{
			const AutoRTFM::EContextStatus Status = AutoRTFM::Close([&]
			{
				AutoRTFM::Transact([&]
				{
					AutoRTFM::CascadingAbortTransaction([&]
					{
						REQUIRE(!bTouched);  // Rollbacks must occur before the post-abort callback is invoked.
						bPostAbortCallbackWasExecuted = true;
					});
				});
			});

			REQUIRE(AutoRTFM::EContextStatus::AbortedByCascadingAbort == Status);
		});
	});

	REQUIRE(AutoRTFM::ETransactionResult::AbortedByCascade == Result);
	REQUIRE(!bTouched);
	REQUIRE(bPostAbortCallbackWasExecuted);
}

TEST_CASE("Abort.CascadeThroughManualTransaction")
{
	bool bPostAbortCallbackWasExecuted = false;
	bool bTouched = false;

	const AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
	{
		bTouched = true;

		AutoRTFM::Open([&]
		{
			REQUIRE(true == AutoRTFM::ForTheRuntime::StartTransaction());

			const AutoRTFM::EContextStatus Status = AutoRTFM::Close([&]
			{
				AutoRTFM::CascadingAbortTransaction([&]
				{
					REQUIRE(!bTouched);  // Rollbacks must occur before the post-abort callback is invoked.
					bPostAbortCallbackWasExecuted = true;
				});
			});

			REQUIRE(AutoRTFM::EContextStatus::AbortedByCascadingAbort == Status);

			// We need to clear the status ourselves.
			AutoRTFM::ForTheRuntime::ClearTransactionStatus();

			// Before manually starting the cascade again.
			AutoRTFM::ForTheRuntime::CascadingRollbackTransaction();
		});
	});

	REQUIRE(AutoRTFM::ETransactionResult::AbortedByCascade == Result);
	REQUIRE(!bTouched);
	REQUIRE(bPostAbortCallbackWasExecuted);
}

static int SAbortKey;

TEST_CASE("Abort.PushOnAbortHandler_NoAbort")
{
	int Value = 55;

	AutoRTFM::Commit([&]
	{
		Value = 66;
		AutoRTFM::PushOnAbortHandler(&SAbortKey, [&Value](){ Value = 77; });
	});

	REQUIRE(Value == 66);
}

TEST_CASE("Abort.PushOnAbortHandler_WithAbort")
{
	int Value = 55;

	const AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
	{
		Value = 66;
		AutoRTFM::PushOnAbortHandler(&SAbortKey, [&Value](){ Value = 77; });

		AutoRTFM::AbortTransaction();
	});

	REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == Result);
	REQUIRE(Value == 77);
}

TEST_CASE("Abort.PushOnAbortHandler_MutableCapture")
{
	FString Message = "Hello";

	AutoRTFM::Transact([&]
	{
		AutoRTFM::PushOnAbortHandler(&SAbortKey, [MessageCopy = Message]() mutable
		{
			MessageCopy += " World!";
			REQUIRE(MessageCopy == "Hello World!");
		});

		AutoRTFM::AbortTransaction();
	});
}

TEST_CASE("Abort.PushOnAbortHandler_WithPop_NoAbort")
{
	int Value = 55;

	AutoRTFM::Commit([&]
	{
		Value = 66;
		AutoRTFM::PushOnAbortHandler(&SAbortKey, [&Value]() { Value = 77; });
		Value = 88;

		AutoRTFM::PopOnAbortHandler(&SAbortKey);
	});

	REQUIRE(Value == 88);
}

TEST_CASE("Abort.PushOnAbortHandler_WithPopAll_NoAbort")
{
	int Value = 55;

	AutoRTFM::Commit([&]
	{
		Value = 66;
		AutoRTFM::PushOnAbortHandler(&SAbortKey, [&Value](){ Value = 77; });
		Value = 88;

		AutoRTFM::PopAllOnAbortHandlers(&SAbortKey);
	});

	REQUIRE(Value == 88);
}

TEST_CASE("Abort.PushOnAbortHandler_WithPop_WithAbort")
{
	int Value = 55;

	const AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
	{
		Value = 66;
		AutoRTFM::PushOnAbortHandler(&SAbortKey, [&Value](){ Value = 77; });
		Value = 88;

		AutoRTFM::PopOnAbortHandler(&SAbortKey);

		AutoRTFM::AbortTransaction();
	});

	REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == Result);
	REQUIRE(Value == 55);
}

TEST_CASE("Abort.PushOnAbortHandler_WithPopAll_WithAbort")
{
	int Value = 55;

	const AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
	{
		Value = 66;
		AutoRTFM::PushOnAbortHandler(&SAbortKey, [&Value]() { Value = 77; });
		Value = 88;

		AutoRTFM::PopAllOnAbortHandlers(&SAbortKey);

		AutoRTFM::AbortTransaction();
	});

	REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == Result);
	REQUIRE(Value == 55);
}

TEST_CASE("Abort.PushOnAbortHandler_Duplicates1")
{
	int Value = 55;

	const AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
	{
		Value = 66;
		AutoRTFM::PushOnAbortHandler(&SAbortKey, [&Value]() { Value = 77; });
		AutoRTFM::PushOnAbortHandler(&SAbortKey, [&Value]() { Value = 88; });
		Value = 99;

		AutoRTFM::PopOnAbortHandler(&SAbortKey);

		AutoRTFM::AbortTransaction();
	});

	REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == Result);

	// The first push on abort will still go through.
	REQUIRE(Value == 77);
}

TEST_CASE("Abort.PushOnAbortHandler_PopAll_Duplicates")
{
	int Value = 55;

	const AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
	{
		Value = 66;
		AutoRTFM::PushOnAbortHandler(&SAbortKey, [&Value]() { Value = 77; });
		AutoRTFM::PushOnAbortHandler(&SAbortKey, [&Value]() { Value = 88; });
		Value = 99;

		AutoRTFM::PopAllOnAbortHandlers(&SAbortKey);

		AutoRTFM::AbortTransaction();
	});

	REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == Result);

	// No abort handlers should execute.
	REQUIRE(Value == 55);
}

TEST_CASE("Abort.PushOnAbortHandler_Duplicates2")
{
	int Value = 55;

	const AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
	{
		Value = 66;
		AutoRTFM::PushOnAbortHandler(&SAbortKey, [&Value](){ Value += 12; });
		AutoRTFM::PushOnAbortHandler(&SAbortKey, [&Value](){ Value = 65; });
		Value = 99;

		AutoRTFM::AbortTransaction();
	});

	REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == Result);
	REQUIRE(Value == 77);
}

TEST_CASE("Abort.PushOnAbortHandler_Order")
{
	SECTION("HandlerSandwich")
	{
		SECTION("WithoutPop")
		{
			int Value = 37;

			const AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
			{
				AutoRTFM::OnAbort([&Value] { REQUIRE(42 == Value); Value += 1; });
				AutoRTFM::PushOnAbortHandler(&SAbortKey, [&Value]() { REQUIRE(40 == Value); Value += 2; });
				AutoRTFM::OnAbort([&Value] { REQUIRE(37 == Value); Value += 3; });

				Value = 99;

				AutoRTFM::AbortTransaction();
			});

			REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == Result);
			REQUIRE(Value == 43);
		}

		SECTION("WithPop")
		{
			int Value = 37;

			const AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
			{
				AutoRTFM::OnAbort([&Value] { REQUIRE(40 == Value); Value += 1; });
				AutoRTFM::PushOnAbortHandler(&SAbortKey, [&Value]() { REQUIRE(false); });
				AutoRTFM::OnAbort([&Value] { REQUIRE(37 == Value); Value += 3; });

				AutoRTFM::PopOnAbortHandler(&SAbortKey);

				Value = 99;

				AutoRTFM::AbortTransaction();
			});

			REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == Result);
			REQUIRE(Value == 41);
		}
	}

	SECTION("HandlerInChild")
	{
		SECTION("WithoutPop")
		{
			int Value = 37;

			const AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
			{
				AutoRTFM::OnAbort([&Value]
				{
					REQUIRE(42 == Value);
					Value += 1;
				});

				// Make a child transaction.
				AutoRTFM::Commit([&]
				{
					AutoRTFM::PushOnAbortHandler(&SAbortKey, [&Value]()
					{
						// If we are retrying nested transactions too, we can't check that
						// the value was something specific before hand!
						if (!AutoRTFM::ForTheRuntime::ShouldRetryNestedTransactionsToo())
						{
							REQUIRE(40 == Value);
							Value += 2;
						}
						else
						{
							Value += 1;
						}
					});
				});

				AutoRTFM::OnAbort([&Value]
				{
					// If we are retrying nested transactions too, we've ran the on-abort in the
					// child transaction once, so our value will be larger.
					if (!AutoRTFM::ForTheRuntime::ShouldRetryNestedTransactionsToo())
					{
						REQUIRE(37 == Value);
					}
					else
					{
						REQUIRE(38 == Value);
					}

					Value += 3;
				});

				Value = 99;

				AutoRTFM::AbortTransaction();
			});

			REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == Result);
			REQUIRE(Value == 43);
		}

		SECTION("WithPop")
		{
			int Value = 37;

			const AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
			{
				AutoRTFM::OnAbort([&Value] { REQUIRE(40 == Value); Value += 1; });

				// Make a child transaction.
				AutoRTFM::Commit([&]
				{
					AutoRTFM::PushOnAbortHandler(&SAbortKey, [&Value]()
					{
						// Only if we are retrying on 
						REQUIRE(AutoRTFM::ForTheRuntime::ShouldRetryNestedTransactionsToo());
					});
				});

				AutoRTFM::OnAbort([&Value] { REQUIRE(37 == Value); Value += 3; });

				// Bit funky, but we can pop the child's push here!
				AutoRTFM::PopOnAbortHandler(&SAbortKey);

				Value = 99;

				AutoRTFM::AbortTransaction();
			});

			REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == Result);
			REQUIRE(Value == 41);
		}

		SECTION("AbortInChild")
		{
			int Value = 99;

			AutoRTFM::ETransactionResult Result = AutoRTFM::ETransactionResult::Committed;
			AutoRTFM::Commit([&]
			{
				AutoRTFM::OnCommit([&Value] { REQUIRE(37 == Value); Value += 1; });

				// Make a child transaction.
				Result = AutoRTFM::Transact([&]
				{
					AutoRTFM::PushOnAbortHandler(&SAbortKey, [&Value]() { REQUIRE(99 == Value); Value += 2; });
					AutoRTFM::AbortTransaction();
				});

				AutoRTFM::Open([&]
				{
					REQUIRE(Value == 101);
				});

				AutoRTFM::OnCommit([&Value] { REQUIRE(38 == Value); Value += 3; });

				Value = 37;

				AutoRTFM::OnAbort([&Value] { Value = 99; });
			});

			REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == Result);
			REQUIRE(Value == 41);
		}

		SECTION("PopInChild")
		{
			int Value = 99;
			AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
			{
				AutoRTFM::PushOnAbortHandler(&SAbortKey, [&Value] { Value = 42; });
				AutoRTFM::Commit([&] { AutoRTFM::PopOnAbortHandler(&SAbortKey); });
				AutoRTFM::AbortTransaction();
			});

			REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == Result);
			REQUIRE(99 == Value);
		}

		SECTION("PopInChildAndAbort")
		{
			int Value = 99;
			AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
			{
				AutoRTFM::PushOnAbortHandler(&SAbortKey, [&Value] { Value = 42; });

				AutoRTFM::Transact([&]
				{
					AutoRTFM::PopOnAbortHandler(&SAbortKey);

					// This abort means the pop on abort handler *should not* propagate
					// to the parent scope (meaning the parents push on abort should
					// run as normal).
					AutoRTFM::AbortTransaction();
				});

				AutoRTFM::AbortTransaction();
			});

			REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == Result);
			REQUIRE(42 == Value);
		}

		SECTION("PopInChildsChild")
		{
			int Value = 99;
			AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
			{
				AutoRTFM::PushOnAbortHandler(&SAbortKey, [&Value] { Value = 42; });
				AutoRTFM::Commit([&] { AutoRTFM::Commit([&] { AutoRTFM::PopOnAbortHandler(&SAbortKey); }); });
				AutoRTFM::AbortTransaction();
			});

			REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == Result);
			REQUIRE(99 == Value);
		}

		SECTION("PopAllInChild")
		{
			int Value = 99;
			AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
			{
				AutoRTFM::PushOnAbortHandler(&SAbortKey, [&Value] { Value = 42; });
				AutoRTFM::Commit([&]
				{
					AutoRTFM::PushOnAbortHandler(&SAbortKey, [&Value] { Value = 13; });
					AutoRTFM::PopAllOnAbortHandlers(&SAbortKey);
				});
				AutoRTFM::AbortTransaction();
			});

			REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == Result);
			REQUIRE(99 == Value);
		}

		SECTION("PopAllInChildAbort")
		{
			int Value = 99;
			AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
			{
				AutoRTFM::PushOnAbortHandler(&SAbortKey, [&Value] { Value = 42; });
				AutoRTFM::Transact([&]
				{
					AutoRTFM::PushOnAbortHandler(&SAbortKey, [&Value] { Value = 13; });
					AutoRTFM::PopAllOnAbortHandlers(&SAbortKey);

					// This abort will ensure that the pop-all cannot affect the outer transactions
					// push on abort!
					AutoRTFM::AbortTransaction();
				});
				AutoRTFM::AbortTransaction();
			});

			REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == Result);
			REQUIRE(42 == Value);
		}
	}
}

TEST_CASE("Abort.OnAbortTiming")
{
	bool bOnAbortRan = false;
	int Memory = 666;
	AutoRTFM::Commit([&]
	{
		// If we are retrying transactions, need to reset the test state.
		AutoRTFM::OnAbort([&]
		{
			REQUIRE(bOnAbortRan);
			REQUIRE(Memory == 666);
			bOnAbortRan = false;
		});

		REQUIRE(bOnAbortRan == false);
		REQUIRE(Memory == 666);

		AutoRTFM::Transact([&]
		{
			Memory = 1234;
			REQUIRE(Memory == 1234);

			AutoRTFM::OnAbort([&]
			{
				REQUIRE(Memory == 666);
				bOnAbortRan = true;
			});

			AutoRTFM::AbortTransaction();
		});
	});
	REQUIRE(Memory == 666);
	REQUIRE(bOnAbortRan == true);
}

static void FnHasNoClosed()
{
	(void)fopen("fopen() is not supported in a closed transaction", "rb");
}

TEST_CASE("Abort.Language")
{
	AutoRTFMTestUtils::FScopedInternalAbortAction Scoped1(AutoRTFM::ForTheRuntime::EAutoRTFMInternalAbortActionState::Abort);
	AutoRTFMTestUtils::FScopedEnsureOnInternalAbort Scoped2(false);

	bool bTouched = false;

	const AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
	{
		bTouched = true;
		FnHasNoClosed();
	});

	REQUIRE(AutoRTFM::ETransactionResult::AbortedByLanguage == Result);
	REQUIRE(false == bTouched);
}

TEST_CASE("Abort.LanguageThroughOpen")
{
	AutoRTFMTestUtils::FScopedInternalAbortAction Scoped1(AutoRTFM::ForTheRuntime::EAutoRTFMInternalAbortActionState::Abort);
	AutoRTFMTestUtils::FScopedEnsureOnInternalAbort Scoped2(false);

	bool bTouched = false;

	const AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
	{
		bTouched = true;

		AutoRTFM::Open([&]
		{
			const AutoRTFM::EContextStatus Status = AutoRTFM::Close([&]
			{
				FnHasNoClosed();
			});

			REQUIRE(AutoRTFM::EContextStatus::AbortedByLanguage == Status);
		});
	});

	REQUIRE(AutoRTFM::ETransactionResult::AbortedByLanguage == Result);
	REQUIRE(false == bTouched);
}

// Test for SOL-5804
TEST_CASE("Abort.StackWriteToOuterOpen")
{
	bool WritesUndone = true;
	bool Success = false;

	const AutoRTFM::ETransactionResult TransactionResult = AutoRTFM::Transact([&]
	{
		AutoRTFM::Open([&]
		{
			std::array<int, 64> Values{};

			AutoRTFM::EContextStatus Status = AutoRTFM::Close([&]
			{
				// On stack outside transaction.
				// Should be reverted as part of the abort.
				WritesUndone = false;

				// On stack inside transaction.
				// Writes should not be reverted as part of the abort.
				for (size_t I = 0; I < Values.size(); I++)
				{
					Values[I] = static_cast<int>(I * 10);
				}
			});

			REQUIRE(AutoRTFM::EContextStatus::OnTrack == Status);
		});

		// If any of the variables on the stack within the Open() get written to
		// on abort, then it should change the values of this array.
		std::array<int, 64> StackGuard{};

		// The OnAbort handler should be called *after* the memory is reverted.
		AutoRTFM::OnAbort([&]
		{
			if (!WritesUndone)
			{
				FAIL("OnAbort was called without first reverting memory");
			}
			else if (StackGuard != std::array<int, 64>{})
			{
				FAIL("StackGuard was corrupted");
			}
			else
			{
				Success = true;
			}
		});

		// Do the abort!
		AutoRTFM::AbortTransaction();
	});

	REQUIRE(Success);
	REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == TransactionResult);
}

TEST_CASE("Abort.PushOnAbortOrdering")
{
	SECTION("InParent")
	{
		unsigned Orderer = 0;
		AutoRTFM::Testing::Abort([&]
		{
			AutoRTFM::OnAbort([&] { REQUIRE(6 == Orderer++); });
			AutoRTFM::PushOnAbortHandler(&Orderer, [&] { REQUIRE(5 == Orderer++); });
			AutoRTFM::OnAbort([&] { REQUIRE(4 == Orderer++); });
			AutoRTFM::PushOnAbortHandler(&Orderer, [&] { REQUIRE(false); });
			AutoRTFM::OnAbort([&] { REQUIRE(3 == Orderer++); });
			AutoRTFM::PopOnAbortHandler(&Orderer);
			AutoRTFM::OnAbort([&] { REQUIRE(2 == Orderer++); });
			AutoRTFM::PushOnAbortHandler(&Orderer, [&] { REQUIRE(1 == Orderer++); });
			AutoRTFM::OnAbort([&] { REQUIRE(0 == Orderer++); });
			AutoRTFM::AbortTransaction();
		});
	}

	SECTION("InChild")
	{
		// The inner transaction cannot fail in this example so lets skip the test if we are testing that.
		if (AutoRTFM::ForTheRuntime::ShouldRetryNestedTransactionsToo())
		{
			return;
		}

		unsigned Orderer = 0;
		AutoRTFM::Testing::Abort([&]
		{
			AutoRTFM::OnAbort([&] { REQUIRE(20 == Orderer++); });
			AutoRTFM::PushOnAbortHandler(&Orderer, [&] { REQUIRE(19 == Orderer++); });
			AutoRTFM::OnAbort([&] { REQUIRE(18 == Orderer++); });
			AutoRTFM::PushOnAbortHandler(&Orderer, [&] { REQUIRE(false); });
			AutoRTFM::OnAbort([&] { REQUIRE(17 == Orderer++); });
			AutoRTFM::PopOnAbortHandler(&Orderer);
			AutoRTFM::OnAbort([&] { REQUIRE(16 == Orderer++); });
			AutoRTFM::PushOnAbortHandler(&Orderer, [&] { REQUIRE(15 == Orderer++); });
			AutoRTFM::OnAbort([&] { REQUIRE(14 == Orderer++); });

			// This commits which will add the on abort handlers to the parent scope.
			AutoRTFM::Commit([&]
			{
				AutoRTFM::OnAbort([&] { REQUIRE(13 == Orderer++); });
				AutoRTFM::PushOnAbortHandler(&Orderer, [&] { REQUIRE(12 == Orderer++); });
				AutoRTFM::OnAbort([&] { REQUIRE(11 == Orderer++); });
				AutoRTFM::PushOnAbortHandler(&Orderer, [&] { REQUIRE(false); });
				AutoRTFM::OnAbort([&] { REQUIRE(10 == Orderer++); });
				AutoRTFM::PopOnAbortHandler(&Orderer);
				AutoRTFM::OnAbort([&] { REQUIRE(9 == Orderer++); });
				AutoRTFM::PushOnAbortHandler(&Orderer, [&] { REQUIRE(8 == Orderer++); });
				AutoRTFM::OnAbort([&] { REQUIRE(7 == Orderer++); });
			});

			AutoRTFM::OnAbort([&] { REQUIRE(6 == Orderer++); });
			AutoRTFM::PushOnAbortHandler(&Orderer, [&] { REQUIRE(5 == Orderer++); });
			AutoRTFM::OnAbort([&] { REQUIRE(4 == Orderer++); });
			AutoRTFM::PushOnAbortHandler(&Orderer, [&] { REQUIRE(false); });
			AutoRTFM::OnAbort([&] { REQUIRE(3 == Orderer++); });
			AutoRTFM::PopOnAbortHandler(&Orderer);
			AutoRTFM::OnAbort([&] { REQUIRE(2 == Orderer++); });
			AutoRTFM::PushOnAbortHandler(&Orderer, [&] { REQUIRE(1 == Orderer++); });
			AutoRTFM::OnAbort([&] { REQUIRE(0 == Orderer++); });

			AutoRTFM::AbortTransaction();
		});
	}
}
