// Copyright Epic Games, Inc. All Rights Reserved.

#include "AutoRTFM.h"
#include "AutoRTFMTesting.h"
#include "Catch2Includes.h"
#include "Templates/RefCounting.h"

namespace
{
	struct RefCountedInt : public FRefCountBase
	{
		RefCountedInt(int InValue) : Value(InValue) {}
		int Value;
	};

	struct RefCountedMixinInt : public TRefCountingMixin<RefCountedMixinInt>
	{
		RefCountedMixinInt(int InValue) : Value(InValue) {}
		int Value;
	};

	struct ThreadSafeRefCountedInt : public FThreadSafeRefCountedObject
	{
		ThreadSafeRefCountedInt(int InValue) : Value(InValue) {}
		int Value;
	};
}

TEMPLATE_TEST_CASE("CheckRefCounts", "[RefCountPtr]", RefCountedInt, RefCountedMixinInt, ThreadSafeRefCountedInt)
{
	// Refcounts should be correct outside of a transaction.
	SECTION("Non-transactional refcounts should be accurate")
	{
		TestType* Ptr = new TestType(42);

		REQUIRE(Ptr->GetRefCount() == 0);
		       (Ptr->AddRef().CheckAtLeast(1));
		REQUIRE(Ptr->GetRefCount() == 1);
		       (Ptr->AddRef().CheckAtLeast(2));
		REQUIRE(Ptr->GetRefCount() == 2);
		REQUIRE(Ptr->Release() == 1);
		REQUIRE(Ptr->GetRefCount() == 1);
		       (Ptr->AddRef().CheckAtLeast(2));
		REQUIRE(Ptr->GetRefCount() == 2);
		REQUIRE(Ptr->Release() == 1);
		REQUIRE(Ptr->GetRefCount() == 1);
		REQUIRE(Ptr->Release() == 0);

		// Ptr is dead
	}

	// Refcounts can be inflated inside a transaction, since releases are deferred.
	SECTION("Transactional refcounts should represent a lower bound")
	{
		AutoRTFM::Testing::Commit([]
		{
			TestType* Ptr = new TestType(42);

			REQUIRE(Ptr->GetRefCount() >= 0);
				   (Ptr->AddRef().CheckAtLeast(1));
			REQUIRE(Ptr->GetRefCount() >= 1);
				   (Ptr->AddRef().CheckAtLeast(2));
			REQUIRE(Ptr->GetRefCount() >= 2);
			REQUIRE(Ptr->Release() >= 1);
			REQUIRE(Ptr->GetRefCount() >= 1);
				   (Ptr->AddRef().CheckAtLeast(2));
			REQUIRE(Ptr->GetRefCount() >= 2);
			REQUIRE(Ptr->Release() >= 1);
			REQUIRE(Ptr->GetRefCount() >= 1);
			REQUIRE(Ptr->Release() >= 0);

			// Ptr is dead
		});
	}
}

TEMPLATE_TEST_CASE("PreviouslyAllocated", "[RefCountPtr]", RefCountedInt, RefCountedMixinInt, ThreadSafeRefCountedInt)
{
	SECTION("Using new T()")
	{
		TRefCountPtr<TestType> Foo(new TestType(42));

		AutoRTFM::Testing::Commit([&]
		{
			// Make a copy to bump the reference count.
			TRefCountPtr<TestType> Copy = Foo;
			Copy->Value = 13;
		});

		REQUIRE(Foo->Value == 13);
		REQUIRE(Foo.GetRefCount() == 1);
	}

	SECTION("Using MakeRefCount<T>")
	{
		TRefCountPtr<TestType> Foo = MakeRefCount<TestType>(42);

		AutoRTFM::Testing::Commit([&]
		{
			// Make a copy to bump the reference count.
			TRefCountPtr<TestType> Copy = Foo;
			Copy->Value = 13;
		});

		REQUIRE(Foo->Value == 13);
		REQUIRE(Foo.GetRefCount() == 1);
	}
}

TEMPLATE_TEST_CASE("AbortWithPreviouslyAllocated", "[RefCountPtr]", RefCountedInt, RefCountedMixinInt, ThreadSafeRefCountedInt)
{
	SECTION("Using new T()")
	{
		TRefCountPtr<TestType> Foo(new TestType(42));

		AutoRTFM::Testing::Abort([&]
		{
			// Make a copy to bump the reference count.
			TRefCountPtr<TestType> Copy = Foo;
			Copy->Value = 13;

			AutoRTFM::AbortTransaction();
		});

		REQUIRE(Foo->Value == 42);
		REQUIRE(Foo.GetRefCount() == 1);
	}

	SECTION("Using MakeRefCount<T>")
	{
		TRefCountPtr<TestType> Foo = MakeRefCount<TestType>(42);

		AutoRTFM::Testing::Abort([&]
		{
			// Make a copy to bump the reference count.
			TRefCountPtr<TestType> Copy = Foo;
			Copy->Value = 13;

			AutoRTFM::AbortTransaction();
		});

		REQUIRE(Foo->Value == 42);
		REQUIRE(Foo.GetRefCount() == 1);
	}
}

TEMPLATE_TEST_CASE("AbortWithNewlyAllocated", "[RefCountPtr]", RefCountedInt, RefCountedMixinInt, ThreadSafeRefCountedInt)
{
	int Result = 42;

	SECTION("Using new T()")
	{
		AutoRTFM::Testing::Abort([&]
		{
			TRefCountPtr<TestType> Foo(new TestType(42));
			Result = Foo->Value;
			AutoRTFM::AbortTransaction();
		});
	}

	SECTION("Using new T() and a copy")
	{
		AutoRTFM::Testing::Abort([&]
		{
			TRefCountPtr<TestType> Foo(new TestType(42));
			TRefCountPtr<TestType> Copy = Foo;
			Result = Copy->Value;
			AutoRTFM::AbortTransaction();
		});
	}

	SECTION("Using MakeRefCount<T>")
	{
		AutoRTFM::Testing::Abort([&]
		{
			TRefCountPtr<TestType> Foo = MakeRefCount<TestType>(42);
			Result = Foo->Value;
			AutoRTFM::AbortTransaction();
		});
	}

	SECTION("Using MakeRefCount<T> and a copy")
	{
		AutoRTFM::Testing::Abort([&]
		{
			TRefCountPtr<TestType> Foo = MakeRefCount<TestType>(42);
			TRefCountPtr<TestType> Copy = Foo;
			Result = Copy->Value;
			AutoRTFM::AbortTransaction();
		});
	}

	REQUIRE(Result == 42);
}

TEMPLATE_TEST_CASE("OnCommitCapturingRefCountPtr", "[RefCountPtr]", RefCountedInt, RefCountedMixinInt, ThreadSafeRefCountedInt)
{
	TRefCountPtr<TestType> Foo = MakeRefCount<TestType>(42);

	SECTION("Committing")
	{
		AutoRTFM::Testing::Commit([&]
		{
			AutoRTFM::OnCommit([Foo]
			{
				Foo->Value = 13;
			});
		});

		REQUIRE(Foo->Value == 13);
	}

	SECTION("Aborting")
	{
		AutoRTFM::Testing::Abort([&]
		{
			AutoRTFM::OnCommit([Foo]
			{
				Foo->Value = 13;
			});
			AutoRTFM::AbortTransaction();
		});

		REQUIRE(Foo->Value == 42);
	}
}

TEMPLATE_TEST_CASE("OnAbortCapturingRefCountPtr", "[RefCountPtr]", RefCountedInt, RefCountedMixinInt, ThreadSafeRefCountedInt)
{
	TRefCountPtr<TestType> Foo = MakeRefCount<TestType>(42);

	SECTION("Committing")
	{
		AutoRTFM::Testing::Commit([&]
		{
			AutoRTFM::OnAbort([Foo]
			{
				Foo->Value = 13;
			});
		});

		// The test harness can decide to abort transactions if `ShouldRetryNonNestedTransactions` is set.
		REQUIRE((Foo->Value == 42 || AutoRTFM::ForTheRuntime::ShouldRetryNonNestedTransactions()));
	}

	SECTION("Aborting")
	{
		AutoRTFM::Testing::Abort([&]
		{
			AutoRTFM::OnAbort([Foo]
			{
				Foo->Value = 13;
			});
			AutoRTFM::AbortTransaction();
		});

		REQUIRE(Foo->Value == 13);
	}
}
