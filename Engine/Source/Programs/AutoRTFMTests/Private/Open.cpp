// Copyright Epic Games, Inc. All Rights Reserved.

#include "AutoRTFM.h"
#include "AutoRTFMTestUtils.h"
#include "Catch2Includes.h"

#include <atomic>
#include <catch_amalgamated.hpp>
#include <map>
#include <vector>

namespace
{

UE_AUTORTFM_ALWAYS_OPEN static void AssignIntPointer(int* Pointer, int Value)
{
    *Pointer = Value;
}

using FnPtr = void (*)(int* Pointer);

UE_AUTORTFM_ALWAYS_OPEN static FnPtr GetFunctionPointer()
{
	auto FnPtr = +[](int* Pointer)
	{
		*Pointer = 42;
	};
	return FnPtr;
}

}

TEST_CASE("Open")
{
    bool bDidRun = false;
    REQUIRE(
        AutoRTFM::ETransactionResult::Committed ==
        AutoRTFM::Transact([&]
        {
            AutoRTFM::Open([&] { bDidRun = true; });
            AutoRTFM::Open([&] { REQUIRE(bDidRun); });
        }));
    REQUIRE(bDidRun);
}

TEST_CASE("Open.Large")
{
    int x = 42;
    std::vector<int> v;
    std::map<int, std::vector<int>> m;
    bool bRanOpen = false;
    v.push_back(100);
    m[1].push_back(2);
    m[1].push_back(3);
    m[4].push_back(5);
    m[6].push_back(7);
    m[6].push_back(8);
    m[6].push_back(9);
    REQUIRE(
        AutoRTFM::ETransactionResult::Committed ==
        AutoRTFM::Transact([&] {
            x = 5;

            for (size_t n = 10; n--;)
            {
                v.push_back(2 * n);
            }

            m.clear();
            m[10].push_back(11);
            m[12].push_back(13);
            m[12].push_back(14);

            AutoRTFM::Open([&] {
#if 0
                // These checks are UB, because the open is interacting with transactional data!
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
#endif
                bRanOpen = true;
            });
        }));
    
    REQUIRE(bRanOpen);
    REQUIRE(x == 5);
    REQUIRE(v.size() == 11);
    REQUIRE(v[0] == 100);
    REQUIRE(v[1] == 18);
    REQUIRE(v[2] == 16);
    REQUIRE(v[3] == 14);
    REQUIRE(v[4] == 12);
    REQUIRE(v[5] == 10);
    REQUIRE(v[6] == 8);
    REQUIRE(v[7] == 6);
    REQUIRE(v[8] == 4);
    REQUIRE(v[9] == 2);
    REQUIRE(v[10] == 0);
    REQUIRE(m.size() == 2);
    REQUIRE(m[10].size() == 1);
    REQUIRE(m[10][0] == 11);
    REQUIRE(m[12].size() == 2);
    REQUIRE(m[12][0] == 13);
    REQUIRE(m[12][1] == 14);
}

TEST_CASE("Open.Atomics")
{
    std::atomic<bool> bDidRun = false;
    REQUIRE(
        AutoRTFM::ETransactionResult::Committed ==
        AutoRTFM::Transact([&]
        {
            AutoRTFM::Open([&] { bDidRun = true; });
            AutoRTFM::Open([&] { REQUIRE(bDidRun); });
        }));
    REQUIRE(bDidRun);
}

TEST_CASE("Open.FunctionPtrFromAlwaysOpenFunction")
{
	FnPtr Func = GetFunctionPointer();

	int Value = 0;

	SECTION("Commit")
	{
		AutoRTFM::Commit([&]
		{
			Func(&Value);
			REQUIRE(Value == 42);
		});
		REQUIRE(Value == 42);
	}

	SECTION("Abort")
	{
		AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
		{
			Func(&Value);
			REQUIRE(Value == 42);
			AutoRTFM::AbortTransaction();
		});
		REQUIRE(Result == AutoRTFM::ETransactionResult::AbortedByRequest);
		REQUIRE(Value == 0);
	}

	SECTION("Open")
	{
		AutoRTFM::Open([&]
		{
			Func(&Value);
			REQUIRE(Value == 42);
		});
		REQUIRE(Value == 42);
	}
}

TEST_CASE("Open.ReturnValue")
{
    static_assert(AutoRTFM::IsSafeToReturnFromOpen<int>);
    static_assert(AutoRTFM::IsSafeToReturnFromOpen<float>);
    static_assert(AutoRTFM::IsSafeToReturnFromOpen<int*>);
    static_assert(AutoRTFM::IsSafeToReturnFromOpen<void>);
    static_assert(AutoRTFM::IsSafeToReturnFromOpen<std::tuple<int, float>>);
    static_assert(!AutoRTFM::IsSafeToReturnFromOpen<std::string>);
    static_assert(!AutoRTFM::IsSafeToReturnFromOpen<std::tuple<int, std::string>>);

    SECTION("int")
    {
        int Value = 10;
        AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
            {
                int Got = AutoRTFM::Open([] { return 42; });
                AutoRTFM::Open([&] { Value = Got; });
            });
        REQUIRE(AutoRTFM::ETransactionResult::Committed == Result);
        REQUIRE(42 == Value);
    }

#if !UE_BUILD_DEBUG
	// jira SOL-7541: std::string only under debug+linux hits a stdlib
	// missing function in the closed!
    SECTION("char*")
    {
        std::string Value = "<unassigned>";
        AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
            {
                // Note: AutoRTFM::Open() is returning a const char*
                std::string Got = AutoRTFM::Open([] { return "meow"; });
                AutoRTFM::Open([&] { Value = Got; });
            });
        REQUIRE(AutoRTFM::ETransactionResult::Committed == Result);
        REQUIRE("meow" == Value);
    }
#endif //!UE_BUILD_DEBUG
    
    SECTION("tuple")
    {
        int Int = 0;
        std::string String = "<unassigned>";
        AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
            {
                auto [I, S] = AutoRTFM::Open([] 
                    { 
                        return std::make_tuple(42, "woof"); 
                    });
                AutoRTFM::Open([&]
                    { 
                        Int = I; 
                        String = S; 
                    });
            });
        REQUIRE(AutoRTFM::ETransactionResult::Committed == Result);
        REQUIRE(42 == Int);
        REQUIRE("woof" == String);
    }

    SECTION("Custom type")
    {
        SECTION("AutoRTFMAssignFromOpenToClosed() by value")
        {
            struct FMyStruct
            {
                int Value = 0;
                static void AutoRTFMAssignFromOpenToClosed(FMyStruct& Closed, FMyStruct Open)
                {
                    Closed.Value = Open.Value;
                }
            };
            static_assert(AutoRTFM::IsSafeToReturnFromOpen<FMyStruct>);

            AutoRTFM::ETransactionResult TransactionResult = AutoRTFM::Transact([&]
            {
                FMyStruct Closed = AutoRTFM::Open([] { return FMyStruct{42}; });
                REQUIRE(42 == Closed.Value);
            });
            REQUIRE(AutoRTFM::ETransactionResult::Committed == TransactionResult);
        }
        SECTION("AutoRTFMAssignFromOpenToClosed() by const-ref")
        {
            struct FMyStruct
            {
                int Value = 0;
                static void AutoRTFMAssignFromOpenToClosed(FMyStruct& Closed, const FMyStruct& Open)
                {
                    Closed.Value = Open.Value;
                }
            };
            static_assert(AutoRTFM::IsSafeToReturnFromOpen<FMyStruct>);

            AutoRTFM::ETransactionResult TransactionResult = AutoRTFM::Transact([&]
            {
                FMyStruct Closed = AutoRTFM::Open([] { return FMyStruct{42}; });
                REQUIRE(42 == Closed.Value);
            });
            REQUIRE(AutoRTFM::ETransactionResult::Committed == TransactionResult);
        }
        SECTION("AutoRTFMAssignFromOpenToClosed() by rvalue-ref")
        {
            struct FMyStruct
            {
                int Value = 0;
                bool* WasMoved = nullptr;
                static void AutoRTFMAssignFromOpenToClosed(FMyStruct& Closed, FMyStruct&& Open)
                {
                    Closed.Value = Open.Value;
                    *Open.WasMoved = true;
                }
            };
            static_assert(AutoRTFM::IsSafeToReturnFromOpen<FMyStruct>);

            AutoRTFM::ETransactionResult TransactionResult = AutoRTFM::Transact([&]
            {
                bool WasMoved = false;
                FMyStruct Closed = AutoRTFM::Open([&] { return FMyStruct{42, &WasMoved}; });
                REQUIRE(true == WasMoved);
                REQUIRE(42 == Closed.Value);
            });
            REQUIRE(AutoRTFM::ETransactionResult::Committed == TransactionResult);
        }
    }
}

TEST_CASE("Open.Collision")
{
    AUTORTFM_SCOPED_ENABLE_MEMORY_VALIDATION_AS_WARNING();

    struct FLargeStruct
    {
        int V[256];
    };

    AutoRTFMTestUtils::FCaptureWarningContext WarningContext;
    int I = 0;
    int J = 0;

    SECTION("NoCollision")
    {
        SECTION("Different Memory Locations")
        {
            AutoRTFM::Transact([&]
            {
                I = 42;
                AutoRTFM::Open([&] { J = 24; });
            });

            REQUIRE(I == 42);
            REQUIRE(J == 24);
            REQUIRE(WarningContext.GetWarnings().empty());
        }

        SECTION("Transact(Open(RecordOpenWriteNoMemoryValidation()), Open(Assign))")
        {
            AutoRTFM::Transact([&]
            {
                AutoRTFM::Open([&]
                {
                    AutoRTFM::RecordOpenWriteNoMemoryValidation(&I);
                });
                AutoRTFM::Open([&]
                {
                    I = 42;
                });
            });

            REQUIRE(I == 42);
            REQUIRE(WarningContext.GetWarnings().empty());
        }

        SECTION("Transact(Transact(Open(RecordOpenWriteNoMemoryValidation())), Open(Assign))")
        {
            AutoRTFM::Transact([&]
            {
                AutoRTFM::Transact([&]
                {
                    AutoRTFM::Open([&]
                    {
                        AutoRTFM::RecordOpenWriteNoMemoryValidation(&I);
                    });
                });
                
                AutoRTFM::Open([&]
                {
                    I = 42;
                });
            });

            REQUIRE(I == 42);
            REQUIRE(WarningContext.GetWarnings().empty());
        }

        SECTION("Transact(Open(RecordOpenWriteNoMemoryValidation()), Open(Assign)) <large>")
        {
            FLargeStruct LargeStruct{};
            AutoRTFM::Transact([&]
            {
                AutoRTFM::Open([&]
                {
                    AutoRTFM::RecordOpenWriteNoMemoryValidation(&LargeStruct);
                });
                AutoRTFM::Open([&]
                {
                    for (int& V : LargeStruct.V)
                    {
                        V = 42;
                    }
                });
            });

            for (int V : LargeStruct.V)
            {
                REQUIRE(V == 42);
            }
            REQUIRE(WarningContext.GetWarnings().empty());
        }

        SECTION("Transact(Open(Assign), Assign)")
        {
            AutoRTFM::Transact([&]
            {
                AutoRTFM::Open([&] { I = 24; });
                I = 42;
            });

            REQUIRE(I == 42);
            REQUIRE(WarningContext.GetWarnings().empty());
        }

        SECTION("Transact(Assign, Open(Transact(Assign)))")
        {
            AutoRTFM::Transact([&]
            {
                I = 24;
                AutoRTFM::Open([&]
                {
                    AutoRTFM::Transact([&]
                    {
                        I = 42;
                    });
                });
            });

            REQUIRE(I == 42);
            REQUIRE(WarningContext.GetWarnings().empty());
        }

        SECTION("Transact(Open(Transact(Assign)), Assign)")
        {
            AutoRTFM::Transact([&]
            {
                AutoRTFM::Open([&]
                {
                    AutoRTFM::Transact([&]
                    {
                        I = 24;
                    });
                });
                I = 42;
            });

            REQUIRE(I == 42);
            REQUIRE(WarningContext.GetWarnings().empty());
        }
    }

    SECTION("Transact(Assign, Open(Assign))")
    {
        AutoRTFM::Transact([&]
        {
            I = 42;
            AutoRTFM::Open([&] { I = 24; });
        });

        REQUIRE_THAT(WarningContext.GetWarnings(),
            Catch::Matchers::VectorContains(FString(AutoRTFMTestUtils::kMemoryModifiedWarning)));
    }

    SECTION("Transact(Assign, Transact(Open(Assign)))")
    {
        AutoRTFM::Transact([&]
        {
            I = 42;
            AutoRTFM::Transact([&]
            {
                AutoRTFM::Open([&] { I = 24; });
            });
        });

        REQUIRE_THAT(WarningContext.GetWarnings(),
            Catch::Matchers::VectorContains(FString(AutoRTFMTestUtils::kMemoryModifiedWarning)));
    }

    SECTION("Transact(Assign, Transact(Transact(Open(Assign))))")
    {
        AutoRTFM::Transact([&]
        {
            I = 42;
            AutoRTFM::Transact([&]
            {
                AutoRTFM::Transact([&]
                {
                    AutoRTFM::Open([&]
                    {
                        I = 24;
                    });
                });
            });
        });

        REQUIRE_THAT(WarningContext.GetWarnings(),
            Catch::Matchers::VectorContains(FString(AutoRTFMTestUtils::kMemoryModifiedWarning)));
    }

    SECTION("Transact(Transact(Assign, Transact(Open(Assign))))")
    {
        AutoRTFM::Transact([&]
        {
            AutoRTFM::Transact([&]
            {
                I = 42;
                AutoRTFM::Transact([&]
                {
                    AutoRTFM::Open([&]
                    {
                        I = 24;
                    });
                });
            });
        });

        REQUIRE_THAT(WarningContext.GetWarnings(),
            Catch::Matchers::VectorContains(FString(AutoRTFMTestUtils::kMemoryModifiedWarning)));
    }

    SECTION("Transact(Transact(Assign, Open(Assign, Transact())))")
    {
        AutoRTFM::Transact([&]
        {
            I = 42;
            AutoRTFM::Open([&]
            {
                I = 24;
                AutoRTFM::Transact([&] {});
            });
        });

        REQUIRE_THAT(WarningContext.GetWarnings(),
            Catch::Matchers::VectorContains(FString(AutoRTFMTestUtils::kMemoryModifiedWarning)));
    }

    SECTION("Transact(Transact(Assign, Open(Transact(), Assign)))")
    {
        AutoRTFM::Transact([&]
        {
            I = 42;
            AutoRTFM::Open([&]
            {
                AutoRTFM::Transact([&] {});
                I = 24;
            });
        });

        REQUIRE_THAT(WarningContext.GetWarnings(),
            Catch::Matchers::VectorContains(FString(AutoRTFMTestUtils::kMemoryModifiedWarning)));
    }

    SECTION("Transact(Assign, CallOpen(Assign))")
    {
        AutoRTFM::Transact([&]
        {
            I = 42;
            AssignIntPointer(&I, 24);
        });

        REQUIRE_THAT(WarningContext.GetWarnings(),
            Catch::Matchers::VectorContains(FString(AutoRTFMTestUtils::kMemoryModifiedWarning)));
    }

    SECTION("Transact(Assign, Transact(CallOpen(Assign)))")
    {
        AutoRTFM::Transact([&]
        {
            I = 42;
            AutoRTFM::Transact([&]
            {
                AssignIntPointer(&I, 24);
            });
        });

        REQUIRE_THAT(WarningContext.GetWarnings(),
            Catch::Matchers::VectorContains(FString(AutoRTFMTestUtils::kMemoryModifiedWarning)));
    }

    SECTION("Transact(Assign, OpenNoValidation(Assign))")
    {
        AutoRTFM::Transact([&]
        {
            I = 42;
            AutoRTFM::Open<AutoRTFM::EMemoryValidationLevel::Disabled>([&]
            {
                I = 10;
            });
        });

        REQUIRE(WarningContext.GetWarnings().empty());
    }

    SECTION("Transact(Assign, AlwaysOpenNoValidation(Assign))")
    {
        struct S
        {
            UE_AUTORTFM_ALWAYS_OPEN_NO_MEMORY_VALIDATION
            static void AssignToInt(int& I)
            {
                I = 10;
            }
        };

        AutoRTFM::Transact([&]
        {
            I = 42;
            S::AssignToInt(I);
        });

        REQUIRE(WarningContext.GetWarnings().empty());
    }

    SECTION("Transact(Assign, Open(Assign, OpenNoValidation()))")
    {
        AutoRTFM::Transact([&]
        {
            I = 42;
            AutoRTFM::Open([&]
            {
                I = 10;
                AutoRTFM::Open<AutoRTFM::EMemoryValidationLevel::Disabled>([&]{});
            });
        });

        REQUIRE_THAT(WarningContext.GetWarnings(),
            Catch::Matchers::VectorContains(FString(AutoRTFMTestUtils::kMemoryModifiedWarning)));
    }
    
    SECTION("Transact(Assign, Open(Assign, Close(OpenNoValidation())))")
    {
        AutoRTFM::Transact([&]
        {
            I = 42;
            AutoRTFM::Open([&]
            {
                I = 10;
                std::ignore = AutoRTFM::Close([&]
                {
                    AutoRTFM::Open<AutoRTFM::EMemoryValidationLevel::Disabled>([&]{});
                });
            });
        });

        REQUIRE_THAT(WarningContext.GetWarnings(),
            Catch::Matchers::VectorContains(FString(AutoRTFMTestUtils::kMemoryModifiedWarning)));
    }

    SECTION("Transact(Open(RecordOpenWrite()), Open(Assign))")
    {
        AutoRTFM::Transact([&]
        {
            AutoRTFM::Open([&]
            {
                AutoRTFM::RecordOpenWrite(&I);
            });
            AutoRTFM::Open([&]
            {
                I = 42;
            });
        });

        REQUIRE(I == 42);
        REQUIRE_THAT(WarningContext.GetWarnings(),
            Catch::Matchers::VectorContains(FString(AutoRTFMTestUtils::kMemoryModifiedWarning)));
    }

    SECTION("Transact(Open(RecordOpenWriteNoMemoryValidation()), Open(RecordOpenWrite()), Open(Assign))")
    {
        AutoRTFM::Transact([&]
        {
            AutoRTFM::Open([&]
            {
                AutoRTFM::RecordOpenWriteNoMemoryValidation(&I);
            });
            AutoRTFM::Open([&]
            {
                AutoRTFM::RecordOpenWrite(&I);
            });
            AutoRTFM::Open([&]
            {
                I = 42;
            });
        });

        REQUIRE_THAT(WarningContext.GetWarnings(),
            Catch::Matchers::VectorContains(FString(AutoRTFMTestUtils::kMemoryModifiedWarning)));
    }

    SECTION("Transact(Transact(Open(RecordOpenWriteNoMemoryValidation())), Open(RecordOpenWrite()), Open(Assign))")
    {
        AutoRTFM::Transact([&]
        {
            AutoRTFM::Transact([&]
            {
                AutoRTFM::Open([&]
                {
                    AutoRTFM::RecordOpenWriteNoMemoryValidation(&I);
                });
            });
            AutoRTFM::Open([&]
            {
                AutoRTFM::RecordOpenWrite(&I);
            });
            AutoRTFM::Open([&]
            {
                I = 42;
            });
        });

        REQUIRE_THAT(WarningContext.GetWarnings(),
            Catch::Matchers::VectorContains(FString(AutoRTFMTestUtils::kMemoryModifiedWarning)));
    }

    SECTION("Transact(Open(RecordOpenWriteNoMemoryValidation()), Open(RecordOpenWrite()), Open(Assign)) <large>")
    {
        FLargeStruct LargeStruct{};
        AutoRTFM::Transact([&]
        {
            AutoRTFM::Open([&]
            {
                AutoRTFM::RecordOpenWriteNoMemoryValidation(&LargeStruct);
            });
            AutoRTFM::Open([&]
            {
                AutoRTFM::RecordOpenWrite(&LargeStruct);
            });
            AutoRTFM::Open([&]
            {
                LargeStruct.V[255] = 42;
            });
        });

        REQUIRE_THAT(WarningContext.GetWarnings(),
            Catch::Matchers::VectorContains(FString(AutoRTFMTestUtils::kMemoryModifiedWarning)));
    }
}
