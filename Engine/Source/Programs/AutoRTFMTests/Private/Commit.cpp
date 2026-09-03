// Copyright Epic Games, Inc. All Rights Reserved.

#include "AutoRTFMTesting.h"
#include "Catch2Includes.h"

#include <map>
#include <vector>

TEST_CASE("Commit.Nop")
{
    REQUIRE(AutoRTFM::ETransactionResult::Committed == AutoRTFM::Transact([&] () { }));
}

TEST_CASE("Commit")
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
    REQUIRE(
        AutoRTFM::ETransactionResult::Committed ==
        AutoRTFM::Transact([&] () {
            x = 5;
            for (size_t n = 10; n--;)
                v.push_back(2 * n);
            m.clear();
            m[10].push_back(11);
            m[12].push_back(13);
            m[12].push_back(14);
        }));
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

TEST_CASE("Commit.Large")
{
    std::vector<int> v;
    std::map<int, std::vector<int>> m;
    for (unsigned i = 0; i < 1000; ++i)
    {
        v.push_back(i);
    }
    for (unsigned i = 0; i < 1000; ++i)
    {
        for (unsigned j = 0; j < 10; ++j)
        {
            m[i + j].push_back(i + j);
        }
    }
    AutoRTFM::Commit([&]
    {
        for (unsigned i = 0; i < 10000; ++i)
        {
            v.push_back(i);
        }
        for (unsigned i = 0; i < 10000; ++i)
        {
            for (unsigned j = 0; j < 5; ++j)
            {
                m[i + j].push_back(i + j);
            }
        }
    });
}

static int SCommitKey;

TEST_CASE("Commit.PushOnCommitHandler_NoAbort")
{
	int Value = 55;

	AutoRTFM::Testing::Commit([&]
	{
		Value = 66;
		AutoRTFM::PushOnCommitHandler(&SCommitKey, [&Value]() { Value = 77; });
		Value = 88;
	});

	REQUIRE(Value == 77);
}

TEST_CASE("Commit.PushOnCommitHandler_WithAbort")
{
	AutoRTFM::Testing::Abort([&]
	{
		AutoRTFM::PushOnCommitHandler(&SCommitKey, []() { FAIL("Unreachable"); });
		AutoRTFM::AbortTransaction();
	});
}

TEST_CASE("Commit.PushOnCommitHandler_MutableCapture")
{
	FString Message = "Hello";

	AutoRTFM::Testing::Commit([&]
	{
		AutoRTFM::PushOnCommitHandler(&SCommitKey, [&Message]() mutable
		{
			Message += " World!";
		});
	});

	REQUIRE(Message == "Hello World!");
}

TEST_CASE("Commit.PushOnCommitHandler_WithPop_NoAbort")
{
	AutoRTFM::Testing::Commit([&]
	{
		AutoRTFM::PushOnCommitHandler(&SCommitKey, []() { FAIL("Unreachable!"); });
		AutoRTFM::PopOnCommitHandler(&SCommitKey);
	});
}

TEST_CASE("Commit.PushOnCommitHandler_WithPopAll_NoAbort")
{
	AutoRTFM::Commit([&]
	{
		AutoRTFM::PushOnCommitHandler(&SCommitKey, []() { FAIL("Unreachable!"); });
		AutoRTFM::PopAllOnCommitHandlers(&SCommitKey);
	});
}

TEST_CASE("Commit.PushOnCommitHandler_WithPop_WithAbort")
{
	AutoRTFM::Testing::Abort([&]
	{
		AutoRTFM::PushOnCommitHandler(&SCommitKey, []() { FAIL("Unreachable!"); });
		AutoRTFM::PopOnCommitHandler(&SCommitKey);
		AutoRTFM::AbortTransaction();
	});
}

TEST_CASE("Commit.PushOnCommitHandler_WithPopAll_WithAbort")
{
	AutoRTFM::Testing::Abort([&]
	{
		AutoRTFM::PushOnCommitHandler(&SCommitKey, []() { FAIL("Unreachable!"); });
		AutoRTFM::PopAllOnCommitHandlers(&SCommitKey);
		AutoRTFM::AbortTransaction();
	});
}

TEST_CASE("Commit.PushOnCommitHandler_Duplicates1")
{
	bool bHit = false;

	AutoRTFM::Testing::Commit([&]
	{
		AutoRTFM::PushOnCommitHandler(&SCommitKey, [&bHit]() { bHit = 77; });
		AutoRTFM::PushOnCommitHandler(&SCommitKey, []() { FAIL("Unreachable!"); });
		AutoRTFM::PopOnCommitHandler(&SCommitKey);
	});

	// The first push on commit will still go through.
	REQUIRE(bHit);
}

TEST_CASE("Commit.PushOnCommitHandler_PopAll_Duplicates")
{
	AutoRTFM::Testing::Commit([&]
	{
		AutoRTFM::PushOnCommitHandler(&SCommitKey, []() { FAIL("Unreachable!"); });
		AutoRTFM::PushOnCommitHandler(&SCommitKey, []() { FAIL("Unreachable!"); });
		AutoRTFM::PopAllOnCommitHandlers(&SCommitKey);
	});
}

TEST_CASE("Commit.PushOnCommitHandler_Duplicates2")
{
	int Value = 55;
	
	AutoRTFM::Testing::Commit([&]
	{
		Value = 66;
		AutoRTFM::PushOnCommitHandler(&SCommitKey, [&Value]() { Value += 13; });
		AutoRTFM::PushOnCommitHandler(&SCommitKey, [&Value]() { Value *= 11; });
		Value = 99;
	});

	REQUIRE(Value == 1232);
}

TEST_CASE("Commit.PushOnCommitHandler_Order")
{
	SECTION("HandlerSandwich")
	{
		SECTION("WithoutPop")
		{
			int Value = 37;
			
			AutoRTFM::Testing::Commit([&]
			{
				AutoRTFM::OnCommit([&Value] { REQUIRE(99 == Value); Value += 1; });
				AutoRTFM::PushOnCommitHandler(&SCommitKey, [&Value]() { REQUIRE(100 == Value); Value += 2; });
				AutoRTFM::OnCommit([&Value] { REQUIRE(102 == Value); Value += 3; });

				Value = 99;
			});

			REQUIRE(Value == 105);
		}

		SECTION("WithPop")
		{
			int Value = 37;
			
			AutoRTFM::Testing::Commit([&]
			{
				AutoRTFM::OnCommit([&Value] { REQUIRE(99 == Value); Value += 1; });
				AutoRTFM::PushOnCommitHandler(&SCommitKey, [&Value]() { FAIL("Unreachable!"); });
				AutoRTFM::OnCommit([&Value] { REQUIRE(100 == Value); Value += 3; });

				AutoRTFM::PopOnCommitHandler(&SCommitKey);

				Value = 99;
			});

			REQUIRE(Value == 103);
		}
	}

	SECTION("HandlerInChild")
	{
		SECTION("WithoutPop")
		{
			int Value = 37;
			
			AutoRTFM::Testing::Commit([&]
			{
				AutoRTFM::OnCommit([&Value]
				{
					REQUIRE(99 == Value);
					Value += 1;
				});

				// Make a child transaction.
				AutoRTFM::Testing::Commit([&]
				{
					AutoRTFM::PushOnCommitHandler(&SCommitKey, [&Value]()
					{
						REQUIRE(100 == Value);
						Value += 2;
					});
				});

				AutoRTFM::OnCommit([&Value]
				{
					REQUIRE(102 == Value);
					Value += 3;
				});

				Value = 99;
			});

			REQUIRE(105 == Value);
		}

		SECTION("WithPop")
		{
			int Value = 37;
			
			AutoRTFM::Testing::Commit([&]
			{
				AutoRTFM::OnCommit([&Value] { REQUIRE(99 == Value); Value += 1; });

				// Make a child transaction.
				AutoRTFM::Testing::Commit([&]
				{
					AutoRTFM::PushOnCommitHandler(&SCommitKey, [&Value]()
					{
						REQUIRE(false);
					});
				});

				AutoRTFM::OnCommit([&Value] { REQUIRE(100 == Value); Value += 3; });

				// Bit funky, but we can pop the child's push here!
				AutoRTFM::PopOnCommitHandler(&SCommitKey);

				Value = 99;
			});

			REQUIRE(Value == 103);
		}

		SECTION("AbortInChild")
		{
			int Value = 99;
			
			AutoRTFM::Testing::Commit([&]
			{
				AutoRTFM::OnCommit([&Value] { REQUIRE(37 == Value); Value += 1; });

				// Make a child transaction.
				AutoRTFM::Testing::Abort([&]
				{
					AutoRTFM::PushOnCommitHandler(&SCommitKey, [&Value]() { FAIL("Unreachable!"); });
					AutoRTFM::AbortTransaction();
				});

				AutoRTFM::OnCommit([&Value] { REQUIRE(38 == Value); Value += 3; });

				Value = 37;
			});

			REQUIRE(Value == 41);
		}

		SECTION("PopInChild")
		{
			int Value = 99;
			
			AutoRTFM::Testing::Commit([&]
			{
				AutoRTFM::PushOnCommitHandler(&SCommitKey, [&Value] { Value = 42; });
				
				AutoRTFM::Testing::Commit([&]
				{
					AutoRTFM::PopOnCommitHandler(&SCommitKey);
				});
			});

			REQUIRE(99 == Value);
		}

		SECTION("PopInChildAndAbort")
		{
			int Value = 99;
			
			AutoRTFM::Testing::Commit([&]
			{
				AutoRTFM::PushOnCommitHandler(&SCommitKey, [&Value] { Value = 42; });
				
				AutoRTFM::Testing::Abort([&]
				{
					AutoRTFM::PopOnCommitHandler(&SCommitKey);
					AutoRTFM::AbortTransaction();
				});
			});

			REQUIRE(42 == Value);
		}

		SECTION("PopInChildsChild")
		{
			int Value = 99;
			
			AutoRTFM::Testing::Commit([&]
			{
				AutoRTFM::PushOnCommitHandler(&SCommitKey, [&Value] { Value = 42; });
				
				AutoRTFM::Testing::Commit([&]
				{
					AutoRTFM::Testing::Commit([&]
					{
						AutoRTFM::PopOnCommitHandler(&SCommitKey);
					});
				});
			});

			REQUIRE(99 == Value);
		}

		SECTION("PopAllInChild")
		{
			int Value = 99;
			
			AutoRTFM::Testing::Commit([&]
			{
				AutoRTFM::PushOnCommitHandler(&SCommitKey, [&Value] { Value = 42; });
				AutoRTFM::Testing::Commit([&]
				{
					AutoRTFM::PushOnCommitHandler(&SCommitKey, [&Value] { Value = 13; });
					AutoRTFM::PopAllOnCommitHandlers(&SCommitKey);
				});
			});

			REQUIRE(99 == Value);
		}

		SECTION("PopAllInChildAbort")
		{
			int Value = 99;
			
			AutoRTFM::Testing::Commit([&]
			{
				AutoRTFM::PushOnCommitHandler(&SCommitKey, [&Value] { Value = 42; });
				
				AutoRTFM::Testing::Abort([&]
				{
					AutoRTFM::PushOnCommitHandler(&SCommitKey, [&Value] { Value = 13; });
					AutoRTFM::PopAllOnCommitHandlers(&SCommitKey);

					// This abort will ensure that the pop-all cannot affect the outer transactions
					// push on commit!
					AutoRTFM::AbortTransaction();
				});
			});

			REQUIRE(42 == Value);
		}
	}
}

// Test functions that may have LLVM byval() attributed parameters
// This is to test for FORT-823033
TEST_CASE("Commit.LargeStruct")
{
	struct FLargeStruct
	{
		int Ints[32];

		static int Sum(FLargeStruct Struct)
		{
			int Result = 0;
			for (int I : Struct.Ints)
			{
				Result += I;
			}
			return Result;
		}
	};

	FLargeStruct Struct
	{
		{
			0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,
			0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,
			0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,
			0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,
		}
	};

	const int Expected = FLargeStruct::Sum(Struct);

	int Result = 0;
	AutoRTFM::Testing::Commit([&]
	{
		Result = FLargeStruct::Sum(Struct);
	});
	REQUIRE(Expected == Result);
}
