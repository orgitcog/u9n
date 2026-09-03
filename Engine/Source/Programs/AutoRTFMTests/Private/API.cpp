// Copyright Epic Games, Inc. All Rights Reserved.

#include "API.h"

#include "AutoRTFMTesting.h"
#include "Catch2Includes.h"

#include <memory>
#include <thread>

TEST_CASE("API.autortfm_is_transactional")
{
    REQUIRE(false == autortfm_is_transactional());

    bool InTransaction = false;
    bool InOpenNest = false;

    AutoRTFM::Commit([&]
    {
        InTransaction = autortfm_is_transactional();

        AutoRTFM::Open([&]
        {
            InOpenNest = autortfm_is_transactional();
        });
    });

    REQUIRE(true == InTransaction);
    REQUIRE(true == InOpenNest);
}

TEST_CASE("API.autortfm_is_closed")
{
    REQUIRE(false == autortfm_is_closed());

    // Set to the opposite of what we expect at the end of function.
    bool InTransaction = false;
    bool InOpenNest = true;
    bool InClosedNestInOpenNest = false;

    AutoRTFM::Commit([&]
    {
        InTransaction = autortfm_is_closed();

        AutoRTFM::Open([&]
        {
            InOpenNest = autortfm_is_closed();

            REQUIRE(AutoRTFM::EContextStatus::OnTrack == AutoRTFM::Close([&]
            {
                InClosedNestInOpenNest = autortfm_is_closed();
            }));
        });
    });

    REQUIRE(true == InTransaction);
    REQUIRE(false == InOpenNest);
    REQUIRE(true == InClosedNestInOpenNest);
}

TEST_CASE("API.autortfm_abort_transaction")
{
    bool BeforeNest = false;
    bool InNest = false;
    bool AfterNest = false;
    
    AutoRTFM::Commit([&]
    {
        BeforeNest = true;

        AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
        {
            // Because we are aborting this won't actually occur!
            InNest = true;

            autortfm_abort_transaction();
        });

        REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == Result);

        AfterNest = true;
    });

    REQUIRE(true == BeforeNest);
    REQUIRE(false == InNest);
    REQUIRE(true == AfterNest);
}

TEST_CASE("API.autortfm_open")
{
    int Answer = 6 * 9;

    // An open call outside a transaction succeeds.
    autortfm_open([](void* const Arg)
    {
        *static_cast<int* const>(Arg) = 42;
    }, &Answer, nullptr);

    REQUIRE(42 == Answer);

    REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == AutoRTFM::Transact([&]
    {
        // An open call inside a transaction succeeds also.
        autortfm_open([](void* const Arg)
        {
            *static_cast<int* const>(Arg) *= 2;
        }, &Answer, nullptr);

        AutoRTFM::AbortTransaction();
    }));

    REQUIRE(84 == Answer);
}

TEST_CASE("API.autortfm_register_open_to_closed_functions")
{
    static autortfm_open_to_closed_mapping Mappings[] = 
    {
        {
            reinterpret_cast<void*>(NoAutoRTFM::DoSomethingC),
            reinterpret_cast<void*>(NoAutoRTFM::DoSomethingInTransactionC),
        },
        { nullptr, nullptr },
    };
    
    static autortfm_open_to_closed_table Table;
    Table.Mappings = Mappings;

    autortfm_register_open_to_closed_functions(&Table);

    int I = -42;

    AutoRTFM::Commit([&]
    {
        I = NoAutoRTFM::DoSomethingC(I);
    });

    REQUIRE(0 == I);
}

TEST_CASE("API.autortfm_on_commit")
{
    bool OuterTransaction = false;
    bool InnerTransaction = false;
    bool InnerTransactionWithAbort = false;
    bool InnerOpenNest = false;

    AutoRTFM::Commit([&]
    {
        autortfm_on_commit([](void* const Arg)
        {
            *static_cast<bool* const>(Arg) = true;
        }, &OuterTransaction);

        // This should only be modified on the commit!
        if (OuterTransaction)
        {
            AutoRTFM::AbortTransaction();
        }

        AutoRTFM::Commit([&]
        {
            autortfm_on_commit([](void* const Arg)
            {
                *static_cast<bool* const>(Arg) = true;
            }, &InnerTransaction);
        });

        // This should only be modified on the commit!
        if (InnerTransaction)
        {
            AutoRTFM::AbortTransaction();
        }

        AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
        {
            autortfm_on_commit([](void* const Arg)
            {
                *static_cast<bool* const>(Arg) = true;
            }, &InnerTransactionWithAbort);

            AutoRTFM::AbortTransaction();
        });

        REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == Result);

        // This should never be modified because its transaction aborted!
        if (InnerTransactionWithAbort)
        {
            AutoRTFM::AbortTransaction();
        }

        AutoRTFM::Open([&]
        {
            autortfm_on_commit([](void* const Arg)
            {
                *static_cast<bool* const>(Arg) = true;
            }, &InnerOpenNest);

            // This should be modified immediately!
            if (!InnerOpenNest)
            {
                AutoRTFM::AbortTransaction();
            }
        });
    });

    REQUIRE(true == OuterTransaction);
    REQUIRE(true == InnerTransaction);
    REQUIRE(false == InnerTransactionWithAbort);
    REQUIRE(true == InnerOpenNest);
}

TEST_CASE("API.autortfm_on_abort")
{
	// Too hard to get this test working when retrying nested transactions so bail!
	if (AutoRTFM::ForTheRuntime::ShouldRetryNestedTransactionsToo())
	{
		return;
	}

    bool OuterTransaction = false;
    bool InnerTransaction = false;
    bool InnerTransactionWithAbort = false;
    bool InnerOpenNest = false;

    REQUIRE(AutoRTFM::ETransactionResult::Committed == AutoRTFM::Transact([&]
    {
		// If we are retrying transactions, need to reset the test state.
		AutoRTFM::OnAbort([&]
		{
			OuterTransaction = false;
			InnerTransaction = false;
			InnerTransactionWithAbort = false;
			InnerOpenNest = false;
		});

        autortfm_on_abort([](void* const Arg)
        {
            *static_cast<bool* const>(Arg) = true;
        }, &OuterTransaction);

        // This should only be modified on the commit!
        if (OuterTransaction)
        {
            AutoRTFM::AbortTransaction();
        }

        AutoRTFM::Commit([&]
        {
            autortfm_on_abort([](void* const Arg)
            {
                *static_cast<bool* const>(Arg) = true;
            }, &InnerTransaction);
        });

        // This should only be modified on the commit!
        if (InnerTransaction)
        {
            AutoRTFM::AbortTransaction();
        }

        AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
        {
            autortfm_on_abort([](void* const Arg)
            {
                *static_cast<bool* const>(Arg) = true;
            }, &InnerTransactionWithAbort);

            AutoRTFM::AbortTransaction();
        });

        REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == Result);

        // OnAbort runs eagerly on inner abort
        if (!InnerTransactionWithAbort)
        {
            AutoRTFM::AbortTransaction();
        }

        AutoRTFM::Open([&]
        {
            autortfm_on_abort([](void* const Arg)
            {
                *static_cast<bool* const>(Arg) = true;
            }, &InnerOpenNest);
        });

        // This should only be modified on the commit!
        if (InnerOpenNest)
        {
            AutoRTFM::AbortTransaction();
        }
    }));

    REQUIRE(false == OuterTransaction);
    REQUIRE(false == InnerTransaction);
    REQUIRE(true == InnerTransactionWithAbort);
    REQUIRE(false == InnerOpenNest);
}

TEST_CASE("API.autortfm_did_allocate")
{
    constexpr unsigned Size = 1024;
    unsigned BumpAllocator[Size];
    unsigned NextBump = 0;

    AutoRTFM::Commit([&]
    {
		// If we are retrying transactions, need to reset the test state.
		AutoRTFM::OnAbort([&]
		{
			NextBump = 0;
		});

        for (unsigned I = 0; I < Size; I++)
        {
            unsigned* Data;
            AutoRTFM::Open([&]
            {
                Data = reinterpret_cast<unsigned*>(autortfm_did_allocate(
                    &BumpAllocator[NextBump++],
                    sizeof(unsigned)));
            });

            *Data = I;
        }
    });

    for (unsigned I = 0; I < Size; I++)
    {
        REQUIRE(I == BumpAllocator[I]);
    }
}

TEST_CASE("API.ETransactionResult")
{
    int Answer = 6 * 9;

    REQUIRE(AutoRTFM::ETransactionResult::Committed == AutoRTFM::Transact([&]
    {
        Answer = 42;
    }));

    REQUIRE(42 == Answer);

    REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == AutoRTFM::Transact([&]
    {
        Answer = 13;
        AutoRTFM::AbortTransaction();
    }));

    REQUIRE(42 == Answer);
}

TEST_CASE("API.IsTransactional")
{
    REQUIRE(false == AutoRTFM::IsTransactional());

    bool InTransaction = false;
    bool InOpenNest = false;
	bool InAbort = true;
	bool InCommit = true;

    AutoRTFM::Commit([&]
    {
        InTransaction = AutoRTFM::IsTransactional();

        AutoRTFM::Open([&]
        {
            InOpenNest = AutoRTFM::IsTransactional();
        });

		AutoRTFM::Transact([&]
			{
				AutoRTFM::OnAbort([&]
					{
						InAbort = AutoRTFM::IsTransactional();
					});

				AutoRTFM::AbortTransaction();
			});

		AutoRTFM::OnCommit([&]
			{
				InCommit = AutoRTFM::IsTransactional();
			});
    });

    REQUIRE(true == InTransaction);
    REQUIRE(true == InOpenNest);
	REQUIRE(false == InAbort);
	REQUIRE(false == InCommit);
}

TEST_CASE("API.IsClosed")
{
    REQUIRE(false == AutoRTFM::IsClosed());

    // Set to the opposite of what we expect at the end of function.
    bool InTransaction = false;
    bool InOpenNest = true;
    bool InClosedNestInOpenNest = false;
	bool InAbort = true;
	bool InCommit = true;

    AutoRTFM::Commit([&]
    {
        InTransaction = AutoRTFM::IsClosed();

		AutoRTFM::Transact([&]
			{
				AutoRTFM::OnAbort([&]
					{
						InAbort = AutoRTFM::IsClosed();
					});

				AutoRTFM::AbortTransaction();
			});

		AutoRTFM::OnCommit([&]
			{
				InCommit = AutoRTFM::IsClosed();
			});

        AutoRTFM::Open([&]
        {
            InOpenNest = AutoRTFM::IsClosed();

            REQUIRE(AutoRTFM::EContextStatus::OnTrack == AutoRTFM::Close([&]
            {
                InClosedNestInOpenNest = AutoRTFM::IsClosed();
            }));
        });
    });

    REQUIRE(true == InTransaction);
    REQUIRE(false == InOpenNest);
    REQUIRE(true == InClosedNestInOpenNest);
	REQUIRE(false == InAbort);
	REQUIRE(false == InCommit);
}

TEST_CASE("API.IsCommittingOrAborting")
{
	REQUIRE(false == AutoRTFM::IsCommittingOrAborting());

	// Set to the opposite of what we expect at the end of function.
	bool InTransaction = true;
	bool InOpenNest = true;
	bool InClosedNestInOpenNest = true;
	bool InAbort = false;
	bool InCommit = false;

	AutoRTFM::Commit([&]
		{
			InTransaction = AutoRTFM::IsCommittingOrAborting();

			AutoRTFM::Transact([&]
				{
					AutoRTFM::OnAbort([&]
						{
							InAbort = AutoRTFM::IsCommittingOrAborting();
						});

					AutoRTFM::AbortTransaction();
				});

			AutoRTFM::OnCommit([&]
				{
					InCommit = AutoRTFM::IsCommittingOrAborting();
				});

			AutoRTFM::Open([&]
				{
					InOpenNest = AutoRTFM::IsCommittingOrAborting();

					REQUIRE(AutoRTFM::EContextStatus::OnTrack == AutoRTFM::Close([&]
						{
							InClosedNestInOpenNest = AutoRTFM::IsCommittingOrAborting();
						}));
				});
		});

	REQUIRE(false == InTransaction);
	REQUIRE(false == InOpenNest);
	REQUIRE(false == InClosedNestInOpenNest);
	REQUIRE(true == InAbort);
	REQUIRE(true == InCommit);
}

TEST_CASE("API.Transact")
{
	int Answer = 6 * 9;

	REQUIRE(AutoRTFM::ETransactionResult::Committed == AutoRTFM::Transact([&]
	{
		Answer = 42;
	}));

	REQUIRE(42 == Answer);
}

TEST_CASE("API.TransactMacro_NoAbort")
{
	int Answer = 6 * 9;

	// Allowing the transaction to commit should work.
	UE_AUTORTFM_TRANSACT
	{
		Answer = 42;
	};

	REQUIRE(42 == Answer);
}

TEST_CASE("API.TransactMacro_WithAbort")
{
	int Answer = 42;

	// Aborting the transaction should also work.
	UE_AUTORTFM_TRANSACT
	{
		Answer = 6 * 9;
		AutoRTFM::AbortTransaction();
	};

	REQUIRE(42 == Answer);
}

TEST_CASE("API.Commit")
{
    int Answer = 6 * 9;

    AutoRTFM::Commit([&]
    {
        Answer = 42;
    });

    REQUIRE(42 == Answer);
}

TEST_CASE("API.Abort")
{
    bool BeforeNest = false;
    bool InNest = false;
    bool AfterNest = false;

    AutoRTFM::Commit([&]
    {
        BeforeNest = true;

        AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
        {
            // Because we are aborting this won't actually occur!
            InNest = true;

            AutoRTFM::AbortTransaction();
        });

        REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == Result);

        AfterNest = true;
    });

    REQUIRE(true == BeforeNest);
    REQUIRE(false == InNest);
    REQUIRE(true == AfterNest);
}

TEST_CASE("API.Open")
{
    int Answer = 6 * 9;

    // An open call outside a transaction succeeds.
    AutoRTFM::Open([&]
    {
        Answer = 42;
    });

    REQUIRE(42 == Answer);

    REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == AutoRTFM::Transact([&]
    {
        // An open call inside a transaction succeeds also.
        AutoRTFM::Open([&]
        {
            Answer *= 2;
        });

        AutoRTFM::AbortTransaction();
    }));

    REQUIRE(84 == Answer);
}

TEST_CASE("API.OpenMacro_NoAbort")
{
    int Answer = 6 * 9;

    // An open call outside a transaction succeeds.
    UE_AUTORTFM_OPEN
    {
        Answer = 42;
    };

    REQUIRE(42 == Answer);
}

TEST_CASE("API.OpenMacro_WithAbort")
{
	int Answer = 21;

	REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == AutoRTFM::Transact([&]
    {
        // An open call inside a transaction succeeds.
        UE_AUTORTFM_OPEN
        {
            Answer *= 2;
        };

        AutoRTFM::AbortTransaction();
    }));

    REQUIRE(42 == Answer);
}

TEST_CASE("API.Close")
{
    bool InClosedNest = false;
    bool InOpenNest = false;
    bool InClosedNestInOpenNest = false;

    AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
    {
        // A closed call inside a transaction does not abort.
        AutoRTFM::EContextStatus CloseStatusA = AutoRTFM::Close([&] { InClosedNest = true; });
        REQUIRE(AutoRTFM::EContextStatus::OnTrack == CloseStatusA);

        AutoRTFM::Open([&]
        {
            AutoRTFM::EContextStatus CloseStatusB = AutoRTFM::Close([&]
            {
                InClosedNestInOpenNest = true;
            });
            REQUIRE(AutoRTFM::EContextStatus::OnTrack == CloseStatusB);

            InOpenNest = true;
        });

        AutoRTFM::AbortTransaction();
    });

    REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == Result);

    REQUIRE(false == InClosedNest);
    REQUIRE(true == InOpenNest);
    REQUIRE(false == InClosedNestInOpenNest);
}

TEST_CASE("API.OnCommit")
{
    bool OuterTransaction = false;
    bool InnerTransaction = false;
    bool InnerTransactionWithAbort = false;
    bool InnerOpenNest = false;

    REQUIRE(AutoRTFM::ETransactionResult::Committed == AutoRTFM::Transact([&]
    {
        AutoRTFM::OnCommit([&]
        {
            OuterTransaction = true;
        });

        // This should only be modified on the commit!
        if (OuterTransaction)
        {
            AutoRTFM::AbortTransaction();
        }

        AutoRTFM::Commit([&]
        {
            AutoRTFM::OnCommit([&]
            {
                InnerTransaction = true;
            });
        });

        // This should only be modified on the commit!
        if (InnerTransaction)
        {
			AutoRTFM::AbortTransaction();
        }


        AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
        {
            AutoRTFM::OnCommit([&]
            {
                InnerTransactionWithAbort = true;
            });

            AutoRTFM::AbortTransaction();
        });

        REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == Result);

        // This should never be modified because its transaction aborted!
        if (InnerTransactionWithAbort)
        {
			AutoRTFM::AbortTransaction();
        }

        AutoRTFM::Open([&]
        {
            AutoRTFM::OnCommit([&]
            {
                InnerOpenNest = true;
            });

            // This should be modified immediately!
            if (!InnerOpenNest)
            {
				AutoRTFM::AbortTransaction();
            }
        });
    }));

    REQUIRE(true == OuterTransaction);
    REQUIRE(true == InnerTransaction);
    REQUIRE(false == InnerTransactionWithAbort);
    REQUIRE(true == InnerOpenNest);
}

TEST_CASE("API.OnCommit_MutableCapture")
{
	FString Message = "Hello";

	AutoRTFM::Transact([&]
	{
		AutoRTFM::OnCommit([MessageCopy = Message]() mutable
		{
			MessageCopy += " World!";
			REQUIRE(MessageCopy == "Hello World!");
		});
	});
}

TEST_CASE("API.OnCommitMacro_NoAbort")
{
	int Value = 123;

	UE_AUTORTFM_TRANSACT
	{
		UE_AUTORTFM_ONCOMMIT(&Value)
		{
			Value = 456;
		};

		Value = 789;
	};

	REQUIRE(Value == 456);
}

TEST_CASE("API.OnCommitMacro_WithAbort")
{
	int Value = 123;

	UE_AUTORTFM_TRANSACT
	{
		UE_AUTORTFM_ONCOMMIT(&Value)
		{
			Value = 456;
		};

		Value = 789;

		AutoRTFM::AbortTransaction();
	};

	REQUIRE(Value == 123);
}

TEST_CASE("API.OnAbort")
{
	// Too hard to get this test working when retrying nested transactions so bail!
	if (AutoRTFM::ForTheRuntime::ShouldRetryNestedTransactionsToo())
	{
		return;
	}

    bool OuterTransaction = false;
    bool InnerTransaction = false;
    bool InnerTransactionWithAbort = false;
    bool InnerOpenNest = false;

    REQUIRE(AutoRTFM::ETransactionResult::Committed == AutoRTFM::Transact([&]
    {
		// If we are retrying transactions, need to reset the test state.
		AutoRTFM::OnAbort([&]
		{
			OuterTransaction = false;
			InnerTransaction = false;
			InnerTransactionWithAbort = false;
			InnerOpenNest = false;
		});

        AutoRTFM::OnAbort([&]
        {
            OuterTransaction = true;
        });

        // This should only be modified on the commit!
        if (OuterTransaction)
        {
			AutoRTFM::AbortTransaction();
        }

        AutoRTFM::Commit([&]
        {
            AutoRTFM::OnAbort([&]
            {
                InnerTransaction = true;
            });
        });

        // This should only be modified on the commit!
        if (InnerTransaction)
        {
			AutoRTFM::AbortTransaction();
        }

        AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
        {
            AutoRTFM::OnAbort([&]
            {
                InnerTransactionWithAbort = true;
            });

			AutoRTFM::AbortTransaction();
        });

        REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == Result);

        // Inner OnAbort runs eagerly
        if (!InnerTransactionWithAbort)
        {
			AutoRTFM::AbortTransaction();
        }

        AutoRTFM::Open([&]
        {
            AutoRTFM::OnAbort([&]
            {
                InnerOpenNest = true;
            });
        });

        // This should only be modified on the commit!
        if (InnerOpenNest)
        {
			AutoRTFM::AbortTransaction();
        }
    }));

    REQUIRE(false == OuterTransaction);
    REQUIRE(false == InnerTransaction);
    REQUIRE(true == InnerTransactionWithAbort);
    REQUIRE(false == InnerOpenNest);
}

TEST_CASE("API.OnAbort_MutableCapture")
{
	FString Message = "Hello";

	AutoRTFM::Transact([&]
	{
		AutoRTFM::OnAbort([MessageCopy = Message]() mutable
		{
			MessageCopy += " World!";
			REQUIRE(MessageCopy == "Hello World!");
		});

		AutoRTFM::AbortTransaction();
	});
}

TEST_CASE("API.OnAbortMacro_NoAbort")
{
	int Value = 123;

	UE_AUTORTFM_TRANSACT
	{
		Value = 456;

		UE_AUTORTFM_ONABORT(&Value)
		{
			Value = 123;
		};
	};

	REQUIRE(Value == 456);
}

TEST_CASE("API.OnAbortMacro_WithAbort")
{
	int Value = 123;

	UE_AUTORTFM_TRANSACT
	{
		Value = 234;

		UE_AUTORTFM_ONABORT(&Value)
		{
			Value = 123;
		};

		AutoRTFM::AbortTransaction();
	};

	REQUIRE(Value == 123);
}

TEST_CASE("API.DidAllocate")
{
    constexpr unsigned Size = 1024;
    unsigned BumpAllocator[Size];
    unsigned NextBump = 0;

    AutoRTFM::Commit([&]
    {
		// If we are retrying transactions, need to reset the test state.
		AutoRTFM::OnAbort([&]
		{
			NextBump = 0;
		});

        for (unsigned I = 0; I < Size; I++)
        {
            unsigned* Data;
            AutoRTFM::Open([&]
            {
                Data = reinterpret_cast<unsigned*>(AutoRTFM::DidAllocate(
                    &BumpAllocator[NextBump++],
                    sizeof(unsigned)));
            });

            *Data = I;
        }
    });

    for (unsigned I = 0; I < Size; I++)
    {
        REQUIRE(I == BumpAllocator[I]);
    }
}

TEST_CASE("API.IsOnCurrentTransactionStack")
{
	{
		int OnStackNotInTransaction = 1;
		REQUIRE(!AutoRTFM::IsOnCurrentTransactionStack(&OnStackNotInTransaction));

		int* OnHeapNotInTransaction = new int{2};
		REQUIRE(!AutoRTFM::IsOnCurrentTransactionStack(OnHeapNotInTransaction));
		delete OnHeapNotInTransaction;
	}

	AutoRTFM::Commit([&]
	{
		int OnStackInTransaction = 3;
		REQUIRE(AutoRTFM::IsOnCurrentTransactionStack(&OnStackInTransaction));

		int* OnHeapInTransaction = new int{4};
		REQUIRE(!AutoRTFM::IsOnCurrentTransactionStack(OnHeapInTransaction));
		delete OnHeapInTransaction;

		AutoRTFM::Commit([&]
		{
			// `OnStackInTransaction` is no longer in the innermost scope.
			REQUIRE(!AutoRTFM::IsOnCurrentTransactionStack(&OnStackInTransaction));

			int OnInnermostStackInTransaction = 5;
			REQUIRE(AutoRTFM::IsOnCurrentTransactionStack(&OnInnermostStackInTransaction));
		});
	});
}

TEST_CASE("API.CascadingRetryTransaction")
{
	SECTION("Callback not called outside transaction")
	{
		AutoRTFM::CascadingRetryTransaction([&] { FAIL("Unreachable"); });
	}

	SECTION("Non-nested committed transaction")
	{
		bool bFirst = true;
		AutoRTFM::Testing::Commit([&]
		{
			if (bFirst)
			{
				bFirst = false;
				AutoRTFM::CascadingRetryTransaction([&]
				{
					// The write above would have been undone.
					REQUIRE(bFirst);
					bFirst = false;
				});
			}
		});
		REQUIRE(!bFirst);
	}

	SECTION("Non-nested aborted transaction")
	{
		bool bFirst = true;
		AutoRTFM::Testing::Abort([&]
		{
			if (bFirst)
			{
				bFirst = false;
				AutoRTFM::CascadingRetryTransaction([&]
				{
					// The write above would have been undone.
					REQUIRE(bFirst);
					bFirst = false;
				});
			}

			AutoRTFM::AbortTransaction();
		});
		REQUIRE(!bFirst);
	}

	SECTION("Nested committed transaction")
	{
		bool bFirst = true;
		AutoRTFM::Testing::Commit([&]
		{
			AutoRTFM::Testing::Commit([&]
			{
				if (bFirst)
				{
					bFirst = false;
					AutoRTFM::CascadingRetryTransaction([&]
					{
						// The write above would have been undone.
						REQUIRE(bFirst);
						bFirst = false;
					});
				}
			});
		});
		REQUIRE(!bFirst);
	}

	SECTION("Nested aborted transaction")
	{
		bool bFirst = true;
		AutoRTFM::Testing::Commit([&]
		{
			AutoRTFM::Testing::Abort([&]
			{
				if (bFirst)
				{
					bFirst = false;
					AutoRTFM::CascadingRetryTransaction([&]
					{
						// The write above would have been undone.
						REQUIRE(bFirst);
						bFirst = false;
					});
				}

				AutoRTFM::AbortTransaction();
			});
		});
		REQUIRE(!bFirst);
	}

	SECTION("IsTransactional is false during the retry")
	{
		bool bFirst = true;
		AutoRTFM::Testing::Commit([&]
		{
			if (bFirst)
			{
				AutoRTFM::CascadingRetryTransaction([&]
				{
					REQUIRE(!AutoRTFM::IsTransactional());
					bFirst = false;
				});
			}
		});
		REQUIRE(!bFirst);
	}

	SECTION("IsClosed is false during the retry")
	{
		bool bFirst = true;
		AutoRTFM::Testing::Commit([&]
		{
			if (bFirst)
			{
				AutoRTFM::CascadingRetryTransaction([&]
				{
					REQUIRE(!AutoRTFM::IsClosed());
					bFirst = false;
				});
			}
		});
		REQUIRE(!bFirst);
	}
}

TEST_CASE("API.IsAutoRTFMRuntimeEnabled")
{
	AutoRTFM::Testing::FEnabledStateResetterScoped _(AutoRTFM::ForTheRuntime::AutoRTFM_EnabledByDefault);

	// On entry to the test we'll be enabled-by-default, so first test we can set back to disabled by default.
	REQUIRE(AutoRTFM::ForTheRuntime::IsAutoRTFMRuntimeEnabled());
	REQUIRE(AutoRTFM::ForTheRuntime::SetAutoRTFMRuntime(AutoRTFM::ForTheRuntime::AutoRTFM_DisabledByDefault));
	REQUIRE(!AutoRTFM::ForTheRuntime::IsAutoRTFMRuntimeEnabled());

	// Now move up a priority level to enabled, and check that we cannot set back to enabled-by-default.
	REQUIRE(AutoRTFM::ForTheRuntime::SetAutoRTFMRuntime(AutoRTFM::ForTheRuntime::AutoRTFM_Enabled));
	REQUIRE(AutoRTFM::ForTheRuntime::IsAutoRTFMRuntimeEnabled());
	REQUIRE(!AutoRTFM::ForTheRuntime::SetAutoRTFMRuntime(AutoRTFM::ForTheRuntime::AutoRTFM_DisabledByDefault));
	REQUIRE(AutoRTFM::ForTheRuntime::IsAutoRTFMRuntimeEnabled());
	REQUIRE(AutoRTFM::ForTheRuntime::SetAutoRTFMRuntime(AutoRTFM::ForTheRuntime::AutoRTFM_Disabled));
	REQUIRE(!AutoRTFM::ForTheRuntime::IsAutoRTFMRuntimeEnabled());
	REQUIRE(!AutoRTFM::ForTheRuntime::SetAutoRTFMRuntime(AutoRTFM::ForTheRuntime::AutoRTFM_EnabledByDefault));
	REQUIRE(!AutoRTFM::ForTheRuntime::IsAutoRTFMRuntimeEnabled());

	// Now move up a priority level to overridden-enabled, and check we cannot set back to enabled or enabled-by-default.
	REQUIRE(AutoRTFM::ForTheRuntime::SetAutoRTFMRuntime(AutoRTFM::ForTheRuntime::AutoRTFM_OverriddenEnabled));
	REQUIRE(AutoRTFM::ForTheRuntime::IsAutoRTFMRuntimeEnabled());
	REQUIRE(!AutoRTFM::ForTheRuntime::SetAutoRTFMRuntime(AutoRTFM::ForTheRuntime::AutoRTFM_DisabledByDefault));
	REQUIRE(AutoRTFM::ForTheRuntime::IsAutoRTFMRuntimeEnabled());
	REQUIRE(!AutoRTFM::ForTheRuntime::SetAutoRTFMRuntime(AutoRTFM::ForTheRuntime::AutoRTFM_Disabled));
	REQUIRE(AutoRTFM::ForTheRuntime::IsAutoRTFMRuntimeEnabled());
	REQUIRE(AutoRTFM::ForTheRuntime::SetAutoRTFMRuntime(AutoRTFM::ForTheRuntime::AutoRTFM_OverriddenDisabled));
	REQUIRE(!AutoRTFM::ForTheRuntime::IsAutoRTFMRuntimeEnabled());
	REQUIRE(!AutoRTFM::ForTheRuntime::SetAutoRTFMRuntime(AutoRTFM::ForTheRuntime::AutoRTFM_EnabledByDefault));
	REQUIRE(!AutoRTFM::ForTheRuntime::IsAutoRTFMRuntimeEnabled());
	REQUIRE(!AutoRTFM::ForTheRuntime::SetAutoRTFMRuntime(AutoRTFM::ForTheRuntime::AutoRTFM_Enabled));
	REQUIRE(!AutoRTFM::ForTheRuntime::IsAutoRTFMRuntimeEnabled());

	// And lastly set force-enabled, and check nothing else can change.
	REQUIRE(AutoRTFM::ForTheRuntime::SetAutoRTFMRuntime(AutoRTFM::ForTheRuntime::AutoRTFM_ForcedEnabled));
	REQUIRE(AutoRTFM::ForTheRuntime::IsAutoRTFMRuntimeEnabled());
	REQUIRE(!AutoRTFM::ForTheRuntime::SetAutoRTFMRuntime(AutoRTFM::ForTheRuntime::AutoRTFM_ForcedDisabled));
	REQUIRE(AutoRTFM::ForTheRuntime::IsAutoRTFMRuntimeEnabled());
	REQUIRE(!AutoRTFM::ForTheRuntime::SetAutoRTFMRuntime(AutoRTFM::ForTheRuntime::AutoRTFM_OverriddenEnabled));
	REQUIRE(AutoRTFM::ForTheRuntime::IsAutoRTFMRuntimeEnabled());
	REQUIRE(!AutoRTFM::ForTheRuntime::SetAutoRTFMRuntime(AutoRTFM::ForTheRuntime::AutoRTFM_OverriddenDisabled));
	REQUIRE(AutoRTFM::ForTheRuntime::IsAutoRTFMRuntimeEnabled());
	REQUIRE(!AutoRTFM::ForTheRuntime::SetAutoRTFMRuntime(AutoRTFM::ForTheRuntime::AutoRTFM_Enabled));
	REQUIRE(AutoRTFM::ForTheRuntime::IsAutoRTFMRuntimeEnabled());
	REQUIRE(!AutoRTFM::ForTheRuntime::SetAutoRTFMRuntime(AutoRTFM::ForTheRuntime::AutoRTFM_Disabled));
	REQUIRE(AutoRTFM::ForTheRuntime::IsAutoRTFMRuntimeEnabled());
	REQUIRE(!AutoRTFM::ForTheRuntime::SetAutoRTFMRuntime(AutoRTFM::ForTheRuntime::AutoRTFM_EnabledByDefault));
	REQUIRE(AutoRTFM::ForTheRuntime::IsAutoRTFMRuntimeEnabled());
	REQUIRE(!AutoRTFM::ForTheRuntime::SetAutoRTFMRuntime(AutoRTFM::ForTheRuntime::AutoRTFM_DisabledByDefault));
	REQUIRE(AutoRTFM::ForTheRuntime::IsAutoRTFMRuntimeEnabled());
}

TEST_CASE("API.CoinTossDisable")
{
	SECTION("With default enablement")
	{
		AutoRTFM::Testing::FEnabledStateResetterScoped _(AutoRTFM::ForTheRuntime::AutoRTFM_EnabledByDefault);

		// Set the chance of disabling to 100.0, effectively disabling the coin toss.
		AutoRTFM::ForTheRuntime::SetAutoRTFMEnabledProbability(100.0f);
		REQUIRE(!AutoRTFM::ForTheRuntime::CoinTossDisable());

		// Set the chance of disabling to 0.0, always disabling by coin toss.
		AutoRTFM::ForTheRuntime::SetAutoRTFMEnabledProbability(0.0f);
		REQUIRE(AutoRTFM::ForTheRuntime::CoinTossDisable());
	}
	
	SECTION("With force enablement")
	{
		AutoRTFM::Testing::FEnabledStateResetterScoped _(AutoRTFM::ForTheRuntime::AutoRTFM_ForcedEnabled);

		// Set the chance of enabling to 0.0, always disabling by coin toss - but this gets ignored because
		// we are set to force enable.
		AutoRTFM::ForTheRuntime::SetAutoRTFMEnabledProbability(0.0f);
		REQUIRE(!AutoRTFM::ForTheRuntime::CoinTossDisable());
	}

	SECTION("Already disabled")
	{
		AutoRTFM::Testing::FEnabledStateResetterScoped _(AutoRTFM::ForTheRuntime::AutoRTFM_DisabledByDefault);

		// Set the chance of disabling to 0.0, always disabling by coin toss.
		AutoRTFM::ForTheRuntime::SetAutoRTFMEnabledProbability(0.0f);
		REQUIRE(!AutoRTFM::ForTheRuntime::CoinTossDisable());
	}
}
