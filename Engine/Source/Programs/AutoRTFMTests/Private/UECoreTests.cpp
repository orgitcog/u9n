// Copyright Epic Games, Inc. All Rights Reserved.

#include "AutoRTFM.h"
#include "AutoRTFMTestUtils.h"
#include "Catch2Includes.h"
#include "Async/TransactionallySafeMutex.h"
#include "Async/ParallelFor.h"
#include "AssetRegistry/AssetDataTagMap.h"
#include "AutoRTFMTesting.h"
#include "Blueprint/BlueprintExceptionInfo.h"
#include "Delegates/IDelegateInstance.h"
#include "HAL/IConsoleManager.h"
#include "HAL/MallocLeakDetection.h"
#include "HAL/ThreadHeartBeat.h"
#include "HAL/ThreadSingleton.h"
#include "Internationalization/TextCache.h"
#include "Internationalization/TextFormatter.h"
#include "Internationalization/TextHistory.h"
#include "Logging/LogMacros.h"
#include "Logging/StructuredLog.h"
#include "Memory/VirtualStackAllocator.h"
#include "Modules/ModuleManager.h"
#include "Serialization/CustomVersion.h"
#include "UObject/CoreRedirects.h"
#include "UObject/DynamicallyTypedValue.h"
#include "UObject/LinkerInstancingContext.h"
#include "UObject/NameTypes.h"
#include "UObject/Stack.h"
#include "UObject/UObjectArray.h"
#include "UObject/UObjectGlobals.h"
#include "MyAutoRTFMTestObject.h"
#include "Containers/Queue.h"
#include "Misc/CString.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CoreDelegates.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/ScopeRWLock.h"
#include "StructUtils/PropertyBag.h"

#include <thread>
#include <mutex>

DEFINE_LOG_CATEGORY_STATIC(LogAutoRTFM_UECoreTests, Log, All)

TEST_CASE("UECore.FDelegateHandle")
{
	FDelegateHandle Handle;

	SECTION("With Abort")
	{
		AutoRTFM::Testing::Abort([&]()
		{
			Handle = FDelegateHandle(FDelegateHandle::GenerateNewHandle);
			AutoRTFM::AbortTransaction();
		});

		REQUIRE(!Handle.IsValid());
	}

	REQUIRE(!Handle.IsValid());

	SECTION("With Commit")
	{
		AutoRTFM::Testing::Commit([&]()
		{
			Handle = FDelegateHandle(FDelegateHandle::GenerateNewHandle);
		});

		REQUIRE(Handle.IsValid());
	}
}

TEST_CASE("UECore.TThreadSingleton")
{
	struct MyStruct : TThreadSingleton<MyStruct>
	{
		int I;
		float F;
	};

	SECTION("TryGet First Time")
	{
		REQUIRE(nullptr == TThreadSingleton<MyStruct>::TryGet());

		// Set to something that isn't nullptr because TryGet will return that!
		MyStruct* Singleton;
		uintptr_t Data = 0x12345678abcdef00;
		memcpy(&Singleton, &Data, sizeof(Singleton));

		AutoRTFM::Testing::Commit([&]()
		{
			Singleton = TThreadSingleton<MyStruct>::TryGet();
		});

		REQUIRE(nullptr == Singleton);
	}

	SECTION("Get")
	{
		MALLOCLEAK_IGNORE_SCOPE(); // TThreadSingleton will appear as a leak.

		AutoRTFM::Testing::Abort([&]()
		{
			TThreadSingleton<MyStruct>::Get().I = 42;
			TThreadSingleton<MyStruct>::Get().F = 42.0f;
			AutoRTFM::AbortTransaction();
		});

		// The singleton *will remain* initialized though, even though we got it in
		// a transaction, because we have to do the singleton creation in the open.
		//
		// commenting out due to changes to this singleton structure under the hood, remove if no longer needed!
		// REQUIRE(nullptr != TThreadSingleton<MyStruct>::TryGet());

		// But any *changes* to the singleton data will be rolled back.
		REQUIRE(0 == TThreadSingleton<MyStruct>::Get().I);
		REQUIRE(0.0f == TThreadSingleton<MyStruct>::Get().F);

		AutoRTFM::Testing::Commit([&]()
		{
			TThreadSingleton<MyStruct>::Get().I = 42;
			TThreadSingleton<MyStruct>::Get().F = 42.0f;
		});

		REQUIRE(42 == TThreadSingleton<MyStruct>::Get().I);
		REQUIRE(42.0f == TThreadSingleton<MyStruct>::Get().F);
	}

	SECTION("TryGet Second Time")
	{
		REQUIRE(nullptr != TThreadSingleton<MyStruct>::TryGet());

		MyStruct* Singleton = nullptr;

		AutoRTFM::Testing::Commit([&]()
		{
			Singleton = TThreadSingleton<MyStruct>::TryGet();
		});

		REQUIRE(nullptr != Singleton);
	}
}

TEST_CASE("UECore.FTextHistory")
{
	struct MyTextHistory final : FTextHistory_Base
	{
		// Need this to always return true so we hit the fun transactional bits!
		bool CanUpdateDisplayString() override
		{
			return true;
		}

		MyTextHistory(const FTextId& InTextId, FString&& InSourceString) : FTextHistory_Base(InTextId, MoveTemp(InSourceString)) {}
	};

	FTextKey Namespace("NAMESPACE");
	FTextKey Key("KEY");
	FTextId TextId(Namespace, Key);
	FString String("WOWWEE");

	MyTextHistory History(TextId, MoveTemp(String));

	SECTION("With Abort")
	{
		AutoRTFM::Testing::Abort([&]()
		{
			History.UpdateDisplayStringIfOutOfDate();
			AutoRTFM::AbortTransaction();
		});
	}

	SECTION("With Commit")
	{
		AutoRTFM::Testing::Commit([&]()
		{
			History.UpdateDisplayStringIfOutOfDate();
		});
	}
}

TEST_CASE("UECore.FCustomVersionContainer")
{
	FCustomVersionContainer Container;
	FGuid Guid(42, 42, 42, 42);

	FCustomVersionRegistration Register(Guid, 0, TEXT("WOWWEE"));

	REQUIRE(nullptr == Container.GetVersion(Guid));

	SECTION("With Abort")
	{
		AutoRTFM::Testing::Abort([&]()
		{
			// The first time the version will be new.
			Container.SetVersionUsingRegistry(Guid);

			// The second time we should hit the cache the first one created.
			Container.SetVersionUsingRegistry(Guid);
			AutoRTFM::AbortTransaction();
		});

		REQUIRE(nullptr == Container.GetVersion(Guid));
	}

	SECTION("With Commit")
	{
		AutoRTFM::Testing::Commit([&]()
		{
			// The first time the version will be new.
			Container.SetVersionUsingRegistry(Guid);

			// The second time we should hit the cache the first one created.
			Container.SetVersionUsingRegistry(Guid);
		});

		REQUIRE(nullptr != Container.GetVersion(Guid));
	}
}

TEST_CASE("UECore.FName")
{
	SECTION("EName Constructor")
	{
		FName Name;

		SECTION("With Abort")
		{
			AutoRTFM::Testing::Abort([&]()
			{
				Name = FName(EName::Timer);
				AutoRTFM::AbortTransaction();
			});

			REQUIRE(Name.IsNone());
		}

		SECTION("With Commit")
		{
			AutoRTFM::Testing::Commit([&]()
			{
				Name = FName(EName::Timer);
			});

			REQUIRE(EName::Timer == *Name.ToEName());
		}
	}

	SECTION("String Constructor")
	{
		FName Name;

		SECTION("With Abort")
		{
			AutoRTFM::Testing::Abort([&]()
			{
				Name = FName(TEXT("WOWWEE"), 42);
				AutoRTFM::AbortTransaction();
			});

			REQUIRE(Name.IsNone());
		}

		SECTION("Check FName was cached")
		{
			bool bWasCached = false;

			for (const FNameEntry* const Entry : FName::DebugDump())
			{
				// Even though we aborted the transaction above, the actual backing data store of
				// the FName system that deduplicates names will contain our name (the nature of
				// the global shared caching infrastructure means we cannot just throw away the
				// FName in the shared cache because it *could* have also been requested in the
				// open and we'd be stomping on that legit use of it!).
				if (0 != Entry->GetNameLength() && (TEXT("WOWWEE") == Entry->GetPlainNameString()))
				{
					bWasCached = true;
				}
			}

			REQUIRE(bWasCached);
		}

		SECTION("With Commit")
		{
			AutoRTFM::Testing::Commit([&]()
			{
				Name = FName(TEXT("WOWWEE"), 42);
			});

			REQUIRE(TEXT("WOWWEE") == Name.GetPlainNameString());
			REQUIRE(42 == Name.GetNumber());
		}
	}

	SECTION("TraceName")
	{
		AutoRTFM::Testing::Commit([&]
		{
			FName Name(TEXT("WOWWEE"), 42);
			(void)FName::TraceName(Name);
		});
	}
}

TEST_CASE("UECore.STATIC_FUNCTION_FNAME")
{
	FName Name;

	SECTION("With Abort")
	{
		AutoRTFM::Testing::Abort([&]()
		{
			Name = STATIC_FUNCTION_FNAME(TEXT("WOWWEE"));
			AutoRTFM::AbortTransaction();
		});

		REQUIRE(Name.IsNone());
	}

	SECTION("With Commit")
	{
		AutoRTFM::Testing::Commit([&]()
		{
			Name = STATIC_FUNCTION_FNAME(TEXT("WOWWEE"));
		});
	}
}

TEST_CASE("UECore.TIntrusiveReferenceController")
{
	SECTION("AddSharedReference")
	{
		SharedPointerInternals::TIntrusiveReferenceController<int, ESPMode::ThreadSafe> Controller(42);

		SECTION("With Abort")
		{
			AutoRTFM::Testing::Abort([&]()
			{
				Controller.AddSharedReference();
				AutoRTFM::AbortTransaction();
			});

			REQUIRE(1 == Controller.GetSharedReferenceCount());
		}

		SECTION("With Commit")
		{
			AutoRTFM::Testing::Commit([&]()
			{
				Controller.AddSharedReference();
			});

			REQUIRE(2 == Controller.GetSharedReferenceCount());
		}
	}

	SECTION("AddWeakReference")
	{
		SharedPointerInternals::TIntrusiveReferenceController<int, ESPMode::ThreadSafe> Controller(42);

		SECTION("With Abort")
		{
			AutoRTFM::Testing::Abort([&]()
			{
				Controller.AddWeakReference();
				AutoRTFM::AbortTransaction();
			});

			REQUIRE(1 == Controller.WeakReferenceCount);
		}

		SECTION("With Commit")
		{
			AutoRTFM::Testing::Commit([&]()
			{
				Controller.AddWeakReference();
			});

			REQUIRE(2 == Controller.WeakReferenceCount);
		}
	}

	SECTION("ConditionallyAddSharedReference")
	{
		SECTION("With Shared Reference Non Zero")
		{
			SharedPointerInternals::TIntrusiveReferenceController<int, ESPMode::ThreadSafe> Controller(42);

			SECTION("With Abort")
			{
				AutoRTFM::Testing::Abort([&]()
				{
					Controller.ConditionallyAddSharedReference();
					AutoRTFM::AbortTransaction();
				});

				REQUIRE(1 == Controller.GetSharedReferenceCount());
			}

			SECTION("With Commit")
			{
				AutoRTFM::Testing::Commit([&]()
				{
					Controller.ConditionallyAddSharedReference();
				});

				REQUIRE(2 == Controller.GetSharedReferenceCount());
			}
		}

		SECTION("With Shared Reference Zero")
		{
			SharedPointerInternals::TIntrusiveReferenceController<int, ESPMode::ThreadSafe> Controller(42);

			// This test relies on us having a weak reference but no strong references to the object.
			Controller.AddWeakReference();
			Controller.ReleaseSharedReference();
			REQUIRE(0 == Controller.GetSharedReferenceCount());

			SECTION("With Abort")
			{
				AutoRTFM::Testing::Abort([&]()
				{
					Controller.ConditionallyAddSharedReference();
					AutoRTFM::AbortTransaction();
				});

				REQUIRE(0 == Controller.GetSharedReferenceCount());
			}

			SECTION("With Commit")
			{
				AutoRTFM::Testing::Commit([&]()
				{
					Controller.ConditionallyAddSharedReference();
				});

				REQUIRE(0 == Controller.GetSharedReferenceCount());
			}
		}
	}

	SECTION("GetSharedReferenceCount")
	{
		SharedPointerInternals::TIntrusiveReferenceController<int, ESPMode::ThreadSafe> Controller(42);

		SECTION("With Abort")
		{
			int32 Count = 0;

			AutoRTFM::Testing::Abort([&]()
			{
				Count = Controller.GetSharedReferenceCount();
				AutoRTFM::AbortTransaction();
			});

			REQUIRE(0 == Count);
		}

		SECTION("With Commit")
		{
			int32 Count = 0;

			AutoRTFM::Testing::Commit([&]()
			{
				Count = Controller.GetSharedReferenceCount();
			});

			REQUIRE(1 == Count);
		}
	}

	SECTION("IsUnique")
	{
		SharedPointerInternals::TIntrusiveReferenceController<int, ESPMode::ThreadSafe> Controller(42);

		SECTION("True")
		{
			bool Unique = false;

			AutoRTFM::Testing::Commit([&]()
			{
				Unique = Controller.IsUnique();
			});

			REQUIRE(Unique);
		}

		SECTION("False")
		{
			// Add a count to make us not unique.
			Controller.AddSharedReference();

			bool Unique = true;

			AutoRTFM::Testing::Commit([&]()
			{
				Unique = Controller.IsUnique();
			});

			REQUIRE(!Unique);
		}
	}

	SECTION("ReleaseSharedReference")
	{
		SharedPointerInternals::TIntrusiveReferenceController<int, ESPMode::ThreadSafe> Controller(42);

		// We don't want the add weak reference deleter to trigger in this test so add another to its count.
		Controller.AddWeakReference();

		SECTION("With Abort")
		{
			AutoRTFM::Testing::Abort([&]()
			{
				Controller.ReleaseSharedReference();
				AutoRTFM::AbortTransaction();
			});

			REQUIRE(1 == Controller.GetSharedReferenceCount());
		}

		SECTION("With Commit")
		{
			AutoRTFM::Testing::Commit([&]()
			{
				Controller.ReleaseSharedReference();
			});
		}
	}

	SECTION("ReleaseWeakReference")
	{
		auto* Controller = new SharedPointerInternals::TIntrusiveReferenceController<int, ESPMode::ThreadSafe>(42);

		SECTION("With Abort")
		{
			AutoRTFM::Testing::Abort([&]()
			{
				Controller->ReleaseWeakReference();
				AutoRTFM::AbortTransaction();
			});

			REQUIRE(1 == Controller->WeakReferenceCount);
			delete Controller;
		}

		SECTION("With Commit")
		{
			AutoRTFM::Testing::Commit([&]()
			{
				Controller->ReleaseWeakReference();
			});
		}
	}

	SECTION("GetObjectPtr")
	{
		SharedPointerInternals::TIntrusiveReferenceController<int, ESPMode::ThreadSafe> Controller(42);

		SECTION("With Abort")
		{
			AutoRTFM::Testing::Abort([&]()
			{
				*Controller.GetObjectPtr() = 13;
				AutoRTFM::AbortTransaction();
			});

			REQUIRE(42 == *Controller.GetObjectPtr());
		}

		SECTION("With Commit")
		{
			AutoRTFM::Testing::Commit([&]()
			{
				*Controller.GetObjectPtr() = 13;
			});

			REQUIRE(13 == *Controller.GetObjectPtr());
		}
	}
}

TEST_CASE("UECore.FText")
{
	FText Text;
	REQUIRE(Text.IsEmpty());

	SECTION("FromString")
	{
		SECTION("With Abort")
		{
			AutoRTFM::Testing::Abort([&]
			{
				Text = FText::FromString(FString(TEXT("Sheesh")));
				AutoRTFM::AbortTransaction();
			});
			REQUIRE(Text.IsEmpty());
		}

		SECTION("With Commit")
		{
			AutoRTFM::Testing::Commit([&]
			{
				Text = FText::FromString(FString(TEXT("Sheesh")));
			});
			REQUIRE(!Text.IsEmpty());
			REQUIRE(Text.ToString() == TEXT("Sheesh"));
		}
	}

	SECTION("Format")
	{
		SECTION("With Abort")
		{
			AutoRTFM::Testing::Commit([&]
			{
				Text = FText::Format(NSLOCTEXT("Cat", "Dog", "Fish[{0}]"), uint64(255));
			});
			REQUIRE(!Text.IsEmpty());
			REQUIRE(Text.ToString() == TEXT("Fish[255]"));
		}

		SECTION("With Commit")
		{
			AutoRTFM::Testing::Commit([&]
			{
				Text = FText::Format(NSLOCTEXT("Cat", "Dog", "Fish[{0}]"), uint64(255));
			});
			REQUIRE(!Text.IsEmpty());
			REQUIRE(Text.ToString() == TEXT("Fish[255]"));
		}
	}
}

TEST_CASE("UECore.FTextCache")
{
	// FTextCache is a singleton. Grab its reference.
	FTextCache& Cache = FTextCache::Get();

	// Use a fixed cache key for the tests below.
	const FTextId Key{TEXT("NAMESPACE"), TEXT("KEY")};

	// As FTextCache does not supply any way to query what's held in the cache,
	// the best we can do here is to call FindOrCache() and check the returned
	// FText strings are as expected.
	auto CheckCacheHealthy = [&]
	{
		FText LookupA = Cache.FindOrCache(TEXT("VALUE"), Key);
		REQUIRE(LookupA.ToString() == TEXT("VALUE"));
		FText LookupB = Cache.FindOrCache(TEXT("REPLACEMENT"), Key);
		REQUIRE(LookupB.ToString() == TEXT("REPLACEMENT"));
		Cache.RemoveCache(Key);
	};

	SECTION("FindOrCache() Add new")
	{
		SECTION("With Abort")
		{
			AutoRTFM::Testing::Abort([&]()
			{
				Cache.FindOrCache(TEXT("VALUE"), Key);
				AutoRTFM::AbortTransaction();
			});

			CheckCacheHealthy();
		}

		SECTION("With Commit")
		{
			AutoRTFM::Testing::Commit([&]()
			{
				Cache.FindOrCache(TEXT("VALUE"), Key);
			});

			CheckCacheHealthy();
		}
	}

	SECTION("FindOrCache() Replace with same value")
	{
		SECTION("With Abort")
		{
			// Add an entry to the cache before the transaction
			Cache.FindOrCache(TEXT("VALUE"), Key);

			AutoRTFM::Testing::Abort([&]()
			{
				Cache.FindOrCache(TEXT("REPLACEMENT"), Key);
				AutoRTFM::AbortTransaction();
			});

			CheckCacheHealthy();
		}

		SECTION("With Commit")
		{
			// Add an entry to the cache before the transaction
			Cache.FindOrCache(TEXT("VALUE"), Key);

			AutoRTFM::Testing::Commit([&]()
			{
				Cache.FindOrCache(TEXT("VALUE"), Key);
			});

			CheckCacheHealthy();
		}
	}

	SECTION("FindOrCache() Replace with different value")
	{
		SECTION("With Abort")
		{
			// Add an entry to the cache before the transaction
			Cache.FindOrCache(TEXT("ORIGINAL"), Key);

			AutoRTFM::Testing::Abort([&]()
			{
				Cache.FindOrCache(TEXT("REPLACEMENT"), Key);
				AutoRTFM::AbortTransaction();
			});

			CheckCacheHealthy();
		}

		SECTION("With Commit")
		{
			// Add an entry to the cache before the transaction
			Cache.FindOrCache(TEXT("ORIGINAL"), Key);

			AutoRTFM::Testing::Commit([&]()
			{
				Cache.FindOrCache(TEXT("REPLACEMENT"), Key);
			});

			CheckCacheHealthy();
		}
	}

	static constexpr bool bSupportsTransactionalRemoveCache = false; // #jira SOL-6743
	if (!bSupportsTransactionalRemoveCache)
	{
		return;
	}

	SECTION("RemoveCache()")
	{
		SECTION("With Abort")
		{
			// Add an entry to the cache before the transaction
			Cache.FindOrCache(TEXT("VALUE"), Key);

			AutoRTFM::Testing::Abort([&]()
			{
				Cache.RemoveCache(Key);
				AutoRTFM::AbortTransaction();
			});

			CheckCacheHealthy();
		}

		SECTION("With Commit")
		{
			// Add an entry to the cache before the transaction
			Cache.FindOrCache(TEXT("VALUE"), Key);

			AutoRTFM::Testing::Commit([&]()
			{
				Cache.RemoveCache(Key);
			});

			CheckCacheHealthy();
		}
	}


	SECTION("Mixed Closed & Open")
	{
		SECTION("Closed: FindOrCache() Open: RemoveCache()")
		{
			SECTION("With Abort")
			{
				AutoRTFM::Testing::Abort([&]()
				{
					Cache.FindOrCache(TEXT("VALUE"), Key);
					AutoRTFM::Open([&]{ Cache.RemoveCache(Key); });
					AutoRTFM::AbortTransaction();
				});

				CheckCacheHealthy();
			}

			SECTION("With Commit")
			{
				AutoRTFM::Testing::Commit([&]()
				{
					Cache.FindOrCache(TEXT("VALUE"), Key);
					AutoRTFM::Open([&]{ Cache.RemoveCache(Key); });
				});

				CheckCacheHealthy();
			}
		}
	}
}

TEST_CASE("UECore.FUObjectItem")
{
	SECTION("CreateStatID First In Open")
	{
		FUObjectItem Item;
		Item.SetObject(NewObject<UMyAutoRTFMTestObject>());
		Item.CreateStatID();

		PROFILER_CHAR* const StatIDStringStorage = Item.StatIDStringStorage;

		// If we abort then we won't change anything.
		AutoRTFM::Testing::Abort([&]
		{
			Item.CreateStatID();
			AutoRTFM::AbortTransaction();
		});

		REQUIRE(StatIDStringStorage == Item.StatIDStringStorage);

		// But also if we commit we likewise won't change anything because
		// the string storage was already created before the transaction
		// began.
		AutoRTFM::Testing::Commit([&]
		{
			Item.CreateStatID();
		});

		REQUIRE(StatIDStringStorage == Item.StatIDStringStorage);
	}

	SECTION("CreateStatID First In Closed")
	{
		FUObjectItem Item;
		Item.SetObject(NewObject<UMyAutoRTFMTestObject>());
		REQUIRE(nullptr == Item.StatIDStringStorage);
		REQUIRE(!Item.StatID.IsValidStat());

		// If we abort then we won't change anything.
		AutoRTFM::Testing::Abort([&]
		{
			Item.CreateStatID();
			AutoRTFM::AbortTransaction();
		});

		REQUIRE(nullptr == Item.StatIDStringStorage);
		REQUIRE(!Item.StatID.IsValidStat());

		// If we commit though we'll create the stat ID.
		AutoRTFM::Testing::Commit([&]
		{
			Item.CreateStatID();
		});

		REQUIRE(nullptr != Item.StatIDStringStorage);
		REQUIRE(Item.StatID.IsValidStat());
	}

	SECTION("CreateStatID On In-Transaction Object")
	{
		AutoRTFM::Testing::Abort([&]
		{
			FUObjectItem Item;
			Item.SetObject(NewObject<UMyAutoRTFMTestObject>());
			Item.CreateStatID();

			AutoRTFM::Open([&]
				{
					REQUIRE(nullptr != Item.StatIDStringStorage);
					REQUIRE(Item.StatID.IsValidStat());
				});

			AutoRTFM::AbortTransaction();
		});

		AutoRTFM::Testing::Commit([&]
		{
			FUObjectItem Item;
			Item.SetObject(NewObject<UMyAutoRTFMTestObject>());
			Item.CreateStatID();

			AutoRTFM::Open([&]
				{
					REQUIRE(nullptr != Item.StatIDStringStorage);
					REQUIRE(Item.StatID.IsValidStat());
				});
		});
	}

	SECTION("CreateStatID In Closed Then Again In Open")
	{
		{
			FUObjectItem Item;
			Item.SetObject(NewObject<UMyAutoRTFMTestObject>());
			REQUIRE(nullptr == Item.StatIDStringStorage);
			REQUIRE(!Item.StatID.IsValidStat());

			AutoRTFM::Testing::Abort([&]
			{
				Item.CreateStatID();

				AutoRTFM::Open([&]
					{
						REQUIRE(nullptr != Item.StatIDStringStorage);
						REQUIRE(Item.StatID.IsValidStat());

						PROFILER_CHAR* const StatIDStringStorage = Item.StatIDStringStorage;

						Item.CreateStatID();

						REQUIRE(StatIDStringStorage == Item.StatIDStringStorage);
						REQUIRE(Item.StatID.IsValidStat());
					});

				AutoRTFM::AbortTransaction();
			});

			REQUIRE(nullptr == Item.StatIDStringStorage);
			REQUIRE(!Item.StatID.IsValidStat());
		}

		{
			FUObjectItem Item;
			Item.SetObject(NewObject<UMyAutoRTFMTestObject>());
			REQUIRE(nullptr == Item.StatIDStringStorage);
			REQUIRE(!Item.StatID.IsValidStat());

			AutoRTFM::Testing::Commit([&]
			{
				Item.CreateStatID();

				AutoRTFM::Open([&]
					{
						REQUIRE(nullptr != Item.StatIDStringStorage);
						REQUIRE(Item.StatID.IsValidStat());

						PROFILER_CHAR* const StatIDStringStorage = Item.StatIDStringStorage;

						Item.CreateStatID();

						REQUIRE(StatIDStringStorage == Item.StatIDStringStorage);
						REQUIRE(Item.StatID.IsValidStat());
					});
			});

			REQUIRE(nullptr != Item.StatIDStringStorage);
			REQUIRE(Item.StatID.IsValidStat());
		}
	}
}

TEST_CASE("UECore.TScopeLock_TransactionallySafeCriticalSection")
{
	SECTION("Outside Transaction")
	{
		FTransactionallySafeCriticalSection CriticalSection;

		AutoRTFM::Testing::Abort([&]
		{
			UE::TScopeLock Lock(CriticalSection);
			AutoRTFM::AbortTransaction();
		});

		AutoRTFM::Testing::Commit([&]
		{
			UE::TScopeLock Lock(CriticalSection);
		});
	}

	SECTION("Inside Transaction")
	{
		AutoRTFM::Testing::Abort([&]
		{
			FTransactionallySafeCriticalSection CriticalSection;
			UE::TScopeLock Lock(CriticalSection);
			AutoRTFM::AbortTransaction();
		});

		AutoRTFM::Testing::Commit([&]
		{
			FTransactionallySafeCriticalSection CriticalSection;
			UE::TScopeLock Lock(CriticalSection);
		});
	}

	SECTION("Inside Transaction Used In Nested Transaction")
	{
		AutoRTFM::Testing::Abort([&]
		{
			FTransactionallySafeCriticalSection CriticalSection;

			AutoRTFM::Testing::Abort([&]
			{
				UE::TScopeLock Lock(CriticalSection);
				AutoRTFM::CascadingAbortTransaction();
			});
		});

		AutoRTFM::Testing::Commit([&]
		{
			FTransactionallySafeCriticalSection CriticalSection;

			AutoRTFM::Testing::Abort([&]
			{
				UE::TScopeLock Lock(CriticalSection);
				AutoRTFM::AbortTransaction();
			});
		});

		AutoRTFM::Testing::Abort([&]
		{
			FTransactionallySafeCriticalSection CriticalSection;

			AutoRTFM::Testing::Commit([&]
			{
				UE::TScopeLock Lock(CriticalSection);
			});

			AutoRTFM::AbortTransaction();
		});

		AutoRTFM::Testing::Commit([&]
		{
			FTransactionallySafeCriticalSection CriticalSection;

			AutoRTFM::Testing::Commit([&]
			{
				UE::TScopeLock Lock(CriticalSection);
			});
		});
	}


	SECTION("In Static Local Initializer")
	{
		struct MyStruct final
		{
			FTransactionallySafeCriticalSection CriticalSection;
		};

		auto Lambda = []()
		{
			static MyStruct Mine;
			UE::TScopeLock _(Mine.CriticalSection);
			return 42;
		};

		AutoRTFM::Testing::Abort([&]
		{
			REQUIRE(42 == Lambda());
			AutoRTFM::AbortTransaction();
		});

		REQUIRE(42 == Lambda());

		AutoRTFM::Testing::Commit([&]
		{
			REQUIRE(42 == Lambda());
		});

		REQUIRE(42 == Lambda());
	}

	SECTION("In Static Local Initializer Called From Open")
	{
		struct MyStruct final
		{
			FTransactionallySafeCriticalSection CriticalSection;
		};

		auto Lambda = []()
		{
			static MyStruct Mine;
			UE::TScopeLock _(Mine.CriticalSection);
			return 42;
		};

		AutoRTFM::Testing::Abort([&]
		{
			AutoRTFM::Open([&] { REQUIRE(42 == Lambda()); });
			AutoRTFM::AbortTransaction();
		});

		REQUIRE(42 == Lambda());

		AutoRTFM::Testing::Commit([&]
		{
			AutoRTFM::Open([&] { REQUIRE(42 == Lambda()); });
		});

		REQUIRE(42 == Lambda());
	}

	SECTION("TScopeLock, destruct, memzero, reconstruct")
	{
		FTransactionallySafeCriticalSection CriticalSection;
		SECTION("Commit")
		{
			AutoRTFM::Testing::Commit([&]
			{
				{
					// Lock and then unlock
					UE::TScopeLock Lock(CriticalSection);
				}
				CriticalSection.~FTransactionallySafeCriticalSection();
				memset(&CriticalSection, 0, sizeof(CriticalSection));
				new (&CriticalSection) FTransactionallySafeCriticalSection();
			});
		}
		SECTION("Abort")
		{
			AutoRTFM::Testing::Abort([&]
			{
				{
					// Lock and then unlock
					UE::TScopeLock Lock(CriticalSection);
				}
				CriticalSection.~FTransactionallySafeCriticalSection();
				memset(&CriticalSection, 0, sizeof(CriticalSection));
				new (&CriticalSection) FTransactionallySafeCriticalSection();
				AutoRTFM::AbortTransaction();
			});
		}
	}
}

TEST_CASE("UECore.FTextFormatPatternDefinition")
{
	FTextFormatPatternDefinitionConstPtr Ptr;

	REQUIRE(!Ptr.IsValid());

	AutoRTFM::Testing::Abort([&]
	{
		Ptr = FTextFormatPatternDefinition::GetDefault().ToSharedPtr();
		AutoRTFM::AbortTransaction();
	});

	REQUIRE(!Ptr.IsValid());

	AutoRTFM::Testing::Commit([&]
	{
		Ptr = FTextFormatPatternDefinition::GetDefault().ToSharedPtr();
	});

	REQUIRE(Ptr.IsValid());
}

TEST_CASE("UECore.FString")
{
	SECTION("Printf")
	{
		FString String;

		AutoRTFM::Testing::Commit([&]
		{
			String = FString::Printf(TEXT("Foo '%s' Bar"), TEXT("Stuff"));
		});

		REQUIRE(String == "Foo 'Stuff' BAR");
	}

	SECTION("Returned From Open")
	{
		SECTION("Copied New")
		{
			FString String;

			AutoRTFM::Testing::Commit([&]
			{
				String = AutoRTFM::Open([&]
				{
					return TEXT("WOW");
				});
			});

			REQUIRE(String == "WOW");
		}

		SECTION("Copied Old")
		{
			FString Other = TEXT("WOW");
			FString String;

			AutoRTFM::Testing::Commit([&]
			{
				String = AutoRTFM::Open([&]
				{
					return Other;
				});
			});

			REQUIRE(Other == "WOW");
			REQUIRE(String == "WOW");
		}
	}
}

TEST_CASE("UECore.TQueue")
{
	SECTION("SingleThreaded")
	{
		SECTION("Constructor")
		{
			AutoRTFM::Testing::Commit([&]
			{
				TQueue<int, EQueueMode::SingleThreaded> Queue;

				AutoRTFM::Open([&]
				{
					REQUIRE(nullptr == Queue.Peek());
				});
			});
		}

		SECTION("Dequeue")
		{
			TQueue<int, EQueueMode::SingleThreaded> Queue;
			REQUIRE(Queue.Enqueue(42));
			REQUIRE(!Queue.IsEmpty());

			int Value = 0;
			bool bSucceeded = false;

			AutoRTFM::Testing::Abort([&]
			{
				bSucceeded = Queue.Dequeue(Value);
				AutoRTFM::AbortTransaction();
			});

			REQUIRE(!bSucceeded);
			REQUIRE(0 == Value);
			REQUIRE(42 == *Queue.Peek());

			AutoRTFM::Testing::Commit([&]
			{
				bSucceeded = Queue.Dequeue(Value);
			});

			REQUIRE(bSucceeded);
			REQUIRE(42 == Value);
			REQUIRE(Queue.IsEmpty());
		}

		SECTION("Empty")
		{
			TQueue<int, EQueueMode::SingleThreaded> Queue;
			REQUIRE(Queue.Enqueue(42));
			REQUIRE(!Queue.IsEmpty());

			AutoRTFM::Testing::Abort([&]
			{
				Queue.Empty();
				AutoRTFM::Open([&] { REQUIRE(Queue.IsEmpty()); });
				AutoRTFM::AbortTransaction();
			});

			REQUIRE(42 == *Queue.Peek());

			AutoRTFM::Testing::Commit([&]
			{
				Queue.Empty();
			});

			REQUIRE(Queue.IsEmpty());
		}

		SECTION("Enqueue")
		{
			TQueue<int, EQueueMode::SingleThreaded> Queue;

			bool bSucceeded = false;

			AutoRTFM::Testing::Abort([&]
			{
				bSucceeded = Queue.Enqueue(42);
				AutoRTFM::AbortTransaction();
			});

			REQUIRE(Queue.IsEmpty());
			REQUIRE(!bSucceeded);

			AutoRTFM::Testing::Commit([&]
			{
				bSucceeded = Queue.Enqueue(42);
			});

			REQUIRE(42 == *Queue.Peek());
			REQUIRE(bSucceeded);
		}

		SECTION("IsEmpty")
		{
			TQueue<int, EQueueMode::SingleThreaded> Queue;
			REQUIRE(Queue.IsEmpty());

			bool bIsEmpty = false;

			AutoRTFM::Testing::Abort([&]
			{
				bIsEmpty = Queue.IsEmpty();
				AutoRTFM::AbortTransaction();
			});

			REQUIRE(!bIsEmpty);
			
			AutoRTFM::Testing::Commit([&]
			{
				bIsEmpty = Queue.IsEmpty();
			});

			REQUIRE(bIsEmpty);

			Queue.Enqueue(42);
			REQUIRE(!Queue.IsEmpty());

			AutoRTFM::Testing::Abort([&]
			{
				bIsEmpty = Queue.IsEmpty();
				AutoRTFM::AbortTransaction();
			});

			REQUIRE(bIsEmpty);

			AutoRTFM::Testing::Commit([&]
			{
				bIsEmpty = Queue.IsEmpty();
			});

			REQUIRE(!bIsEmpty);
		}

		SECTION("Peek")
		{
			TQueue<int, EQueueMode::SingleThreaded> Queue;
			REQUIRE(Queue.Enqueue(42));

			AutoRTFM::Testing::Abort([&]
			{
				*Queue.Peek() = 13;
				AutoRTFM::AbortTransaction();
			});

			REQUIRE(42 == *Queue.Peek());

			AutoRTFM::Testing::Commit([&]
			{
				*Queue.Peek() = 13;
			});

			REQUIRE(13 == *Queue.Peek());
		}

		SECTION("Pop")
		{
			SECTION("Empty")
			{
				TQueue<int, EQueueMode::SingleThreaded> Queue;

				bool bSucceeded = true;

				AutoRTFM::Testing::Abort([&]
				{
					bSucceeded = Queue.Pop();
					AutoRTFM::AbortTransaction();
				});

				REQUIRE(bSucceeded);

				AutoRTFM::Testing::Commit([&]
				{
					bSucceeded = Queue.Pop();
				});

				REQUIRE(!bSucceeded);
			}

			SECTION("Non Empty")
			{
				TQueue<int, EQueueMode::SingleThreaded> Queue;
				REQUIRE(Queue.Enqueue(42));

				bool bSucceeded = false;

				AutoRTFM::Testing::Abort([&]
				{
					bSucceeded = Queue.Pop();
					AutoRTFM::AbortTransaction();
				});

				REQUIRE(!bSucceeded);
				REQUIRE(!Queue.IsEmpty());

				AutoRTFM::Testing::Commit([&]
				{
					bSucceeded = Queue.Pop();
				});

				REQUIRE(bSucceeded);
				REQUIRE(Queue.IsEmpty());
			}
		}
	}
}

TEST_CASE("UECore.FConfigFile")
{
	SECTION("Empty")
	{
		FConfigFile Config;

		Config.FindOrAddConfigSection(TEXT("WOW"));

		REQUIRE(!Config.IsEmpty());
	
		AutoRTFM::Testing::Commit([&]
		{
			Config.Empty();
		});
		
		REQUIRE(Config.IsEmpty());
	}
}

TEST_CASE("UECore.PropertyBag")
{
	UPropertyBag* const Bag = NewObject<UPropertyBag>();

	UScriptStruct* const SS = static_cast<UScriptStruct*>(Bag);

	SS->PrepareCppStructOps();

	char Data[128];

	AutoRTFM::Testing::Abort([&]
	{
		SS->InitializeStruct(Data);
		AutoRTFM::AbortTransaction();
	});

	AutoRTFM::Testing::Commit([&]
	{
		SS->InitializeStruct(Data);
	});

	AutoRTFM::Testing::Abort([&]
	{
		SS->DestroyStruct(Data);
		AutoRTFM::AbortTransaction();
	});

	AutoRTFM::Testing::Commit([&]
	{
		SS->DestroyStruct(Data);
	});
}

TEST_CASE("UECore.FAssetDataTagMapSharedView")
{
	SECTION("Loose")
	{
		FAssetDataTagMap Loose;
		Loose.Add(FName("cat"), FString("meow"));
		Loose.Add(FName("dog"), FString("woof"));
		SECTION("Copy FAssetDataTagMapSharedView from open")
		{
			FAssetDataTagMapSharedView Original{MoveTemp(Loose)};
			SECTION("Commit")
			{
				AutoRTFM::Testing::Commit([&]
				{
					FAssetDataTagMapSharedView View{Original};
				});
			}
			SECTION("Abort")
			{
				AutoRTFM::Testing::Abort([&]
				{
					FAssetDataTagMapSharedView View{Original};
					AutoRTFM::AbortTransaction();
				});
			}
			REQUIRE(Original.Contains(FName("cat")));
			REQUIRE(Original.Contains(FName("dog")));
		}
		SECTION("Copy FAssetDataTagMapSharedView from closed")
		{
			SECTION("Commit")
			{
				AutoRTFM::Testing::Commit([&]
				{
					FAssetDataTagMapSharedView Original{MoveTemp(Loose)};
					FAssetDataTagMapSharedView View{Original};
					REQUIRE(View.Contains(FName("cat")));
					REQUIRE(View.Contains(FName("dog")));
				});
			}
			SECTION("Abort")
			{
				AutoRTFM::Testing::Abort([&]
				{
					FAssetDataTagMapSharedView Original{MoveTemp(Loose)};
					FAssetDataTagMapSharedView View{Original};
					AutoRTFM::AbortTransaction();
				});
			}
		}
		SECTION("Move FAssetDataTagMapSharedView from open")
		{
			FAssetDataTagMapSharedView Original{MoveTemp(Loose)};
			SECTION("Commit")
			{
				AutoRTFM::Testing::Commit([&]
				{
					FAssetDataTagMapSharedView View{MoveTemp(Original)};
					REQUIRE(View.Contains(FName("cat")));
					REQUIRE(View.Contains(FName("dog")));
				});
			}
			SECTION("Abort")
			{
				AutoRTFM::Testing::Abort([&]
				{
					FAssetDataTagMapSharedView View{MoveTemp(Original)};
					AutoRTFM::AbortTransaction();
				});
				REQUIRE(Original.Contains(FName("cat")));
				REQUIRE(Original.Contains(FName("dog")));
			}
		}
		SECTION("Move FAssetDataTagMapSharedView from closed")
		{
			SECTION("Commit")
			{
				AutoRTFM::Testing::Commit([&]
				{
					FAssetDataTagMapSharedView Original{MoveTemp(Loose)};
					FAssetDataTagMapSharedView View{MoveTemp(Original)};
					REQUIRE(View.Contains(FName("cat")));
					REQUIRE(View.Contains(FName("dog")));
				});
			}
			SECTION("Abort")
			{
				AutoRTFM::Testing::Abort([&]
				{
					FAssetDataTagMapSharedView Original{MoveTemp(Loose)};
					FAssetDataTagMapSharedView View{MoveTemp(Original)};
					AutoRTFM::AbortTransaction();
				});
			}
		}
		SECTION("Move FAssetDataTagMap from open")
		{
			SECTION("Commit")
			{
				AutoRTFM::Testing::Commit([&]
				{
					FAssetDataTagMapSharedView View{MoveTemp(Loose)};
					REQUIRE(View.Contains(FName("cat")));
					REQUIRE(View.Contains(FName("dog")));
				});
			}
			SECTION("Abort")
			{
				AutoRTFM::Testing::Abort([&]
				{
					FAssetDataTagMapSharedView View{MoveTemp(Loose)};
					AutoRTFM::AbortTransaction();
				});
			}
		}
		SECTION("Move FAssetDataTagMap from closed")
		{
			SECTION("Commit")
			{
				AutoRTFM::Testing::Commit([&]
				{
					FAssetDataTagMap ClosedLoose{MoveTemp(Loose)};
					FAssetDataTagMapSharedView View{MoveTemp(ClosedLoose)};
					REQUIRE(View.Contains(FName("cat")));
					REQUIRE(View.Contains(FName("dog")));
				});
			}
			SECTION("Abort")
			{
				AutoRTFM::Testing::Abort([&]
				{
					FAssetDataTagMap ClosedLoose{MoveTemp(Loose)};
					FAssetDataTagMapSharedView View{MoveTemp(ClosedLoose)};
					AutoRTFM::AbortTransaction();
				});
			}
		}
	}
}

TEST_CASE("UECore.UE_LOGFMT")
{
	SECTION("Commit")
	{
		AutoRTFM::Testing::Commit([&]
		{
			UE_LOGFMT(LogAutoRTFM_UECoreTests, Log, "{Animal} says {Sound}", TEXT("Cat"), TEXT("meow!"));
		});
	}
	SECTION("Abort")
	{
		AutoRTFM::Testing::Abort([&]
		{
			UE_LOGFMT(LogAutoRTFM_UECoreTests, Log, "{Animal} says {Sound}", TEXT("Cat"), TEXT("meow!"));
			AutoRTFM::AbortTransaction();
		});
	}
}

TEST_CASE("UECore.FOutputDeviceRedirector")
{
	FOutputDeviceRedirector Redirector;
	SECTION("Commit")
	{
		FStringOutputDevice StringLog;
		Redirector.AddOutputDevice(&StringLog);

		AutoRTFM::Testing::Commit([&]
		{
			// This test will actually be run twice, because we test with SetRetryTransaction enabled.
			// Logging always runs in the open so the log won't be undone when the transaction is
			// rolled back before being retried.
			// We handle this by making sure that the string log has "Commit" appended to it, rather
			// than verifying that it contains "Commit" exactly.
			FString PreviousStringLog = StringLog;
			Redirector.Log(TEXT("Commit"));
			Redirector.Flush();
			REQUIRE(StringLog == PreviousStringLog + FString("Commit"));
		});
	}

	SECTION("Abort")
	{
		FStringOutputDevice StringLog;
		Redirector.AddOutputDevice(&StringLog);

		AutoRTFM::Testing::Abort([&]
		{
			FString PreviousStringLog = StringLog;
			Redirector.Log(TEXT("Abort"));
			Redirector.Flush();
			REQUIRE(StringLog == PreviousStringLog + FString("Abort"));

			AutoRTFM::AbortTransaction();
		});
	}
}

TEST_CASE("UECore.AsyncLoading")
{
	SECTION("LoadPackageAsync")
	{
		int32 RequestId = -1;

		AutoRTFM::Testing::Commit([&]
		{
			FString Name(FString::Printf(TEXT("/AutoRTFMTestPackage%d"), __LINE__));
			RequestId = LoadPackageAsync(Name);
		});

		FlushAsyncLoading(RequestId);
	}

	SECTION("IsAsyncLoading")
	{
		int32 RequestId = -1;

		AutoRTFM::Testing::Commit([&]
		{
			REQUIRE(!IsAsyncLoading());

			FString Name(FString::Printf(TEXT("/AutoRTFMTestPackage%d"), __LINE__));
			RequestId = LoadPackageAsync(Name);

			REQUIRE(IsAsyncLoading());
		});

		FlushAsyncLoading(RequestId);
	}

	SECTION("FlushAsyncLoading")
	{
		FString Name(FString::Printf(TEXT("/AutoRTFMTestPackage%d"), __LINE__));
		int32 RequestId = LoadPackageAsync(Name);

		AutoRTFM::Testing::Commit([&]
		{
			FlushAsyncLoading(RequestId);
		});
	}

	SECTION("FlushAsyncLoading Empty")
	{
		FString Name(FString::Printf(TEXT("/AutoRTFMTestPackage%d"), __LINE__));
		int32 RequestId = LoadPackageAsync(Name);

		AutoRTFM::Testing::Commit([&]
		{
			FlushAsyncLoading();
		});
	}

	SECTION("FlushAsyncLoading One In One Out")
	{
		FString Name(FString::Printf(TEXT("/AutoRTFMTestPackage%d"), __LINE__));
		int32 RequestId1 = LoadPackageAsync(Name);

		AutoRTFM::Testing::Commit([&]
		{
			FString Name(FString::Printf(TEXT("/AutoRTFMTestPackage%d"), __LINE__));
			int32 RequestId2 = LoadPackageAsync(Name);
			TArray<int32> RequestIds;
			RequestIds.Add(RequestId1);
			RequestIds.Add(RequestId2);
			FlushAsyncLoading(RequestIds);
		});
	}

	SECTION("CompletionDelegate is called closed")
	{
		AutoRTFM::Testing::Abort([&]
		{
			FString Name(FString::Printf(TEXT("/AutoRTFMTestPackage%d"), __LINE__));
			FLoadPackageAsyncDelegate CompletionDelegate;
			CompletionDelegate.BindLambda([&](const FName&, UPackage*, EAsyncLoadingResult::Type) { REQUIRE(AutoRTFM::IsClosed()); });
			int RequestId = LoadPackageAsync(Name, CompletionDelegate);
			FlushAsyncLoading(RequestId);
			AutoRTFM::AbortTransaction();
		});
	}
	
	SECTION("CompletionDelegate aborts")
	{
		AutoRTFM::Testing::Abort([&]
		{
			FString Name(FString::Printf(TEXT("/AutoRTFMTestPackage%d"), __LINE__));
			FLoadPackageAsyncDelegate CompletionDelegate;
			CompletionDelegate.BindLambda([&](const FName&, UPackage*, EAsyncLoadingResult::Type) { AutoRTFM::AbortTransaction(); });
			int RequestId = LoadPackageAsync(Name, CompletionDelegate);
			FlushAsyncLoading(RequestId);
			FAIL("Unreachable!");
		});
	}
	
	SECTION("FLoadPackageAsyncOptionalParams::CompletionDelegate is called closed")
	{
		AutoRTFM::Testing::Abort([&]
		{
			FString Name(FString::Printf(TEXT("/AutoRTFMTestPackage%d"), __LINE__));
			FLoadPackageAsyncOptionalParams Params;
			Params.CompletionDelegate.Reset(new FLoadPackageAsyncDelegate());
			Params.CompletionDelegate->BindLambda([&](const FName&, UPackage*, EAsyncLoadingResult::Type) { REQUIRE(AutoRTFM::IsClosed()); });
			int RequestId = LoadPackageAsync(Name, MoveTemp(Params));
			FlushAsyncLoading(RequestId);
			AutoRTFM::AbortTransaction();
		});
	}
	
	SECTION("FLoadPackageAsyncOptionalParams::CompletionDelegate aborts")
	{
		AutoRTFM::Testing::Abort([&]
		{
			FString Name(FString::Printf(TEXT("/AutoRTFMTestPackage%d"), __LINE__));
			FLoadPackageAsyncOptionalParams Params;
			Params.CompletionDelegate.Reset(new FLoadPackageAsyncDelegate());
			Params.CompletionDelegate->BindLambda([&](const FName&, UPackage*, EAsyncLoadingResult::Type) { AutoRTFM::AbortTransaction(); });
			int RequestId = LoadPackageAsync(Name, MoveTemp(Params));
			FlushAsyncLoading(RequestId);
			FAIL("Unreachable!");
		});
	}
	
	SECTION("FLoadPackageAsyncOptionalParams::CompletionDelegate creates UObject")
	{
		UMyAutoRTFMTestObject* OpenObject = nullptr;
		UMyAutoRTFMTestObject* ClosedObject = nullptr;

		AutoRTFM::Testing::Abort([&]
		{
			ClosedObject = NewObject<UMyAutoRTFMTestObject>();
			FString Name(FString::Printf(TEXT("/AutoRTFMTestPackage%d"), __LINE__));
			FLoadPackageAsyncOptionalParams Params;
			Params.CompletionDelegate.Reset(new FLoadPackageAsyncDelegate());
			Params.CompletionDelegate->BindLambda([&](const FName&, UPackage*, EAsyncLoadingResult::Type)
			{
				OpenObject = NewObject<UMyAutoRTFMTestObject>();
				AutoRTFM::AbortTransaction();
			});
			int RequestId = LoadPackageAsync(Name, MoveTemp(Params));
			FlushAsyncLoading(RequestId);
			FAIL("Unreachable!");
		});

		REQUIRE(nullptr == ClosedObject);
		REQUIRE(nullptr == OpenObject);
	}
	
	SECTION("FLoadPackageAsyncOptionalParams::CompletionDelegate calls another LoadPackageAsync")
	{
		AutoRTFM::Testing::Abort([&]
		{
			FLoadPackageAsyncOptionalParams Params;
			Params.CompletionDelegate.Reset(new FLoadPackageAsyncDelegate());
			Params.CompletionDelegate->BindLambda([&](const FName&, UPackage*, EAsyncLoadingResult::Type)
			{
				int RequestId = LoadPackageAsync(FString::Printf(TEXT("/AutoRTFMTestPackage%d"), __LINE__), MoveTemp(Params));
				AutoRTFM::AbortTransaction();
			});
			int RequestId = LoadPackageAsync(FString::Printf(TEXT("/AutoRTFMTestPackage%d"), __LINE__), MoveTemp(Params));
			FlushAsyncLoading(RequestId);
			FAIL("Unreachable!");
		});
	}

	SECTION("Multiple retries because of multiple loads with commit")
	{
		AutoRTFMTestUtils::FScopedRetry Retry(AutoRTFM::ForTheRuntime::EAutoRTFMRetryTransactionState::NoRetry);

		int NumCompletionCallbacks = 0;
		AutoRTFM::Testing::Commit([&]
		{
			FLoadPackageAsyncDelegate CompletionDelegate;
			CompletionDelegate.BindLambda([&](const FName&, UPackage*, EAsyncLoadingResult::Type) 
			{
				// Do this open so we can check how many retries occurred.
				AutoRTFM::Open([&] { NumCompletionCallbacks++; });
			});

			TArray<int32> RequestIds;
			RequestIds.Add(LoadPackageAsync(FString::Printf(TEXT("/AutoRTFMTestPackage%d"), __LINE__), CompletionDelegate));
			RequestIds.Add(LoadPackageAsync(FString::Printf(TEXT("/AutoRTFMTestPackage%d"), __LINE__), CompletionDelegate));
			RequestIds.Add(LoadPackageAsync(FString::Printf(TEXT("/AutoRTFMTestPackage%d"), __LINE__), CompletionDelegate));

			FlushAsyncLoading(RequestIds);
			REQUIRE(3 == NumCompletionCallbacks);
		});
	}

	SECTION("Multiple retries because of multiple loads with abort")
	{
		AutoRTFMTestUtils::FScopedRetry Retry(AutoRTFM::ForTheRuntime::EAutoRTFMRetryTransactionState::NoRetry);

		int NumCompletionCallbacks = 0;
		AutoRTFM::Testing::Abort([&]
		{
			FLoadPackageAsyncDelegate CompletionDelegate;
			CompletionDelegate.BindLambda([&](const FName&, UPackage*, EAsyncLoadingResult::Type) 
			{
				// Do this open so we can check how many retries occurred.
				AutoRTFM::Open([&] { NumCompletionCallbacks++; });
			});

			TArray<int32> RequestIds;
			RequestIds.Add(LoadPackageAsync(FString::Printf(TEXT("/AutoRTFMTestPackage%d"), __LINE__), CompletionDelegate));
			RequestIds.Add(LoadPackageAsync(FString::Printf(TEXT("/AutoRTFMTestPackage%d"), __LINE__), CompletionDelegate));
			RequestIds.Add(LoadPackageAsync(FString::Printf(TEXT("/AutoRTFMTestPackage%d"), __LINE__), CompletionDelegate));

			FlushAsyncLoading(RequestIds);
			REQUIRE(3 == NumCompletionCallbacks);

			AutoRTFM::AbortTransaction();
		});
	}

	SECTION("Stack Local Linker Instancing Context")
	{
		int32 RequestId = -1;

		AutoRTFM::Testing::Commit([&]
		{
			FLinkerInstancingContext Context;
			FLoadPackageAsyncOptionalParams Params;
			Params.InstancingContext = &Context;
			FString Name(FString::Printf(TEXT("/AutoRTFMTestPackage%d"), __LINE__));
			RequestId = LoadPackageAsync(Name, MoveTemp(Params));
		});

		FlushAsyncLoading(RequestId);
	}
}

TEST_CASE("UECore.CoreRedirects")
{
	FCoreRedirects::Initialize();

	FCoreRedirectObjectName From(NAME_None, NAME_None, TEXT("/A/B/C"));
	FCoreRedirectObjectName To(NAME_None, NAME_None, TEXT("/X/Y/Z"));

	// Returns a new TArray, so that we test for calling with a TArrayView that points to a temporary TArray.
	// See FORT-823809
	auto Redirects = [From, To]
	{
		TArray<FCoreRedirect> List;
		List.Emplace(ECoreRedirectFlags::Type_Package, From, To);
		return List;
	};

	SECTION("Basic Assumptions")
	{
		FCoreRedirects::AddRedirectList(Redirects(), TEXT("AutoRTFMTests.UECore.CoreRedirects"));
		REQUIRE(FCoreRedirects::GetRedirectedName(ECoreRedirectFlags::Type_Package, From) == To);
		FCoreRedirects::RemoveRedirectList(Redirects(), TEXT("AutoRTFMTests.UECore.CoreRedirects"));
		REQUIRE(FCoreRedirects::GetRedirectedName(ECoreRedirectFlags::Type_Package, From) == From);
	}

	SECTION("AddRedirectList")
	{
		SECTION("Commit")
		{
			AutoRTFM::Testing::Commit([&]
			{
				FCoreRedirects::AddRedirectList(Redirects(), TEXT("AutoRTFMTests.UECore.CoreRedirects"));
			});
			REQUIRE(FCoreRedirects::GetRedirectedName(ECoreRedirectFlags::Type_Package, From) == To);
			FCoreRedirects::RemoveRedirectList(Redirects(), TEXT("AutoRTFMTests.UECore.CoreRedirects"));
			REQUIRE(FCoreRedirects::GetRedirectedName(ECoreRedirectFlags::Type_Package, From) == From);
		}
		
		SECTION("Abort")
		{
			AutoRTFM::Testing::Abort([&]
			{
				FCoreRedirects::AddRedirectList(Redirects(), TEXT("AutoRTFMTests.UECore.CoreRedirects"));
				AutoRTFM::AbortTransaction();
			});
			REQUIRE(FCoreRedirects::GetRedirectedName(ECoreRedirectFlags::Type_Package, From) == From);
		}
	}
	
	SECTION("RemoveRedirectList")
	{
		FCoreRedirects::AddRedirectList(Redirects(), TEXT("AutoRTFMTests.UECore.CoreRedirects"));
		
		SECTION("Commit")
		{
			AutoRTFM::Testing::Commit([&]
			{
				FCoreRedirects::RemoveRedirectList(Redirects(), TEXT("AutoRTFMTests.UECore.CoreRedirects"));
			});
			REQUIRE(FCoreRedirects::GetRedirectedName(ECoreRedirectFlags::Type_Package, From) == From);
		}
		
		SECTION("Abort")
		{
			AutoRTFM::Testing::Abort([&]
			{
				FCoreRedirects::RemoveRedirectList(Redirects(), TEXT("AutoRTFMTests.UECore.CoreRedirects"));
				AutoRTFM::AbortTransaction();
			});
			REQUIRE(FCoreRedirects::GetRedirectedName(ECoreRedirectFlags::Type_Package, From) == To);
			FCoreRedirects::RemoveRedirectList(Redirects(), TEXT("AutoRTFMTests.UECore.CoreRedirects"));
			REQUIRE(FCoreRedirects::GetRedirectedName(ECoreRedirectFlags::Type_Package, From) == From);
		}
	}
}

TEST_CASE("UECore.PackageName")
{
	AutoRTFM::Testing::Commit([&]
	{
		FPackagePath Path = FPackagePath::FromLocalPath(FString("/Fake/Package/Path.lol"));
		REQUIRE(FPackageName::EPackageLocationFilter::None == FPackageName::DoesPackageExistEx(Path, FPackageName::EPackageLocationFilter::IoDispatcher));
	});
}

TEST_CASE("UECore.ConsoleManager")
{
	IConsoleManager& Manager = IConsoleManager::Get();
	float Thing = 42.0f;
	IConsoleVariable* const Variable = Manager.RegisterConsoleVariableRef(TEXT("WOWWEE"), Thing, TEXT("Halp!"));

	AutoRTFM::Testing::Commit([&]
	{
		Variable->Set(13.0f);
	});

	REQUIRE(Thing == 13.0f);
}

namespace {

class FakeFileHandle : public IFileHandle
{
public:
	bool SeekFromEnd(int64 NewPositionRelativeToEnd = 0) override				{ unimplemented(); return false; }
	bool ReadAt(uint8* Destination, int64 BytesToRead, int64 Offset) override	{ unimplemented(); return false; }
	bool Write(const uint8* Source, int64 BytesToWrite) override				{ unimplemented(); return false; }
	bool Truncate(int64 NewSize) override										{ unimplemented(); return false; }
	bool Flush(const bool bFullFlush = false) override							{ return true; }
	void ShrinkBuffers() override												{}
	
	int64 Tell() override
	{ 
		return Cursor; 
	}

	int64 Size() override
	{ 
		return strlen(Data); 
	}

	bool Seek(int64 NewPosition) override
	{
		check(NewPosition >= 0);
		check(NewPosition <= Size());
		Cursor = NewPosition;
		return true;
	}

	bool Read(uint8* Destination, int64 BytesToRead) override
	{
		check(BytesToRead <= Size() - Cursor);
		const uint8* DataPtr = reinterpret_cast<const uint8*>(Data);
		memcpy(Destination, DataPtr + Cursor, BytesToRead);
		return true;
	}

	const char* const Data = "File Loaded";
	int64 Cursor = 0;
};

class FakePlatformFile : public IPlatformFile
{
public:
	FakePlatformFile() {}

	bool Initialize(IPlatformFile* Inner, const TCHAR* CmdLine) override                                  { unimplemented(); return false; }
	IPlatformFile* GetLowerLevel() override                                                               { unimplemented(); return nullptr; }
	void SetLowerLevel(IPlatformFile* NewLowerLevel) override                                             { unimplemented(); }
	bool FileExists(const TCHAR* Filename) override                                                       { unimplemented(); return false; }
	int64 FileSize(const TCHAR* Filename) override                                                        { unimplemented(); return 0; }
	bool DeleteFile(const TCHAR* Filename) override                                                       { unimplemented(); return false; }
	bool IsReadOnly(const TCHAR* Filename) override                                                       { unimplemented(); return false; }
	bool MoveFile(const TCHAR* To, const TCHAR* From) override                                            { unimplemented(); return false; }
	bool SetReadOnly(const TCHAR* Filename, bool bNewReadOnlyValue) override                              { unimplemented(); return false; }
	FDateTime GetTimeStamp(const TCHAR* Filename) override                                                { unimplemented(); return FDateTime{}; }
	void SetTimeStamp(const TCHAR* Filename, FDateTime DateTime) override                                 { unimplemented(); }
	FDateTime GetAccessTimeStamp(const TCHAR* Filename) override                                          { unimplemented(); return FDateTime{}; }
	FString GetFilenameOnDisk(const TCHAR* Filename) override                                             { unimplemented(); return FString{}; }
	IFileHandle* OpenWrite(const TCHAR* Filename, bool bAppend = false, bool bAllowRead = false) override { unimplemented(); return nullptr; }
	bool DirectoryExists(const TCHAR* Directory) override                                                 { unimplemented(); return false; }
	bool CreateDirectory(const TCHAR* Directory) override                                                 { unimplemented(); return false; }
	bool DeleteDirectory(const TCHAR* Directory) override                                                 { unimplemented(); return false; }
	FFileStatData GetStatData(const TCHAR* FilenameOrDirectory) override                                  { unimplemented(); return FFileStatData{}; }
	bool IterateDirectory(const TCHAR* Directory, FDirectoryVisitor& Visitor) override                    { unimplemented(); return false; }
	bool IterateDirectoryStat(const TCHAR* Directory, FDirectoryStatVisitor& Visitor) override            { unimplemented(); return false; }

	const TCHAR* GetName() const override 
	{
		return TEXT("FakePlatformFile");
	}

	IFileHandle* OpenRead(const TCHAR* Filename, bool bAllowWrite = false) override
	{
		check(FString(Filename) == TEXT("FakePlatformFile"));
		check(!bAllowWrite);
		return new FakeFileHandle;
	}
};

}

TEST_CASE("UECore.LoadFileToString.IPlatformFile")
{
	FString FileData = TEXT("Nothing Happened");
	FakePlatformFile FakeFile;

	SECTION("Abort")
	{
		AutoRTFM::Testing::Abort([&]
		{
			REQUIRE(FFileHelper::LoadFileToString(FileData, &FakeFile, TEXT("FakePlatformFile")));
			AutoRTFM::AbortTransaction();
		});

		REQUIRE(FileData == TEXT("Nothing Happened"));
	}

	SECTION("Commit")
	{
		AutoRTFM::Testing::Commit([&]
		{
			REQUIRE(FFileHelper::LoadFileToString(FileData, &FakeFile, TEXT("FakePlatformFile")));
		});

		REQUIRE(FileData == TEXT("File Loaded"));
	}
}

TEST_CASE("UECore.GetCurrentProcessId")
{
	const uint32 Outer = FPlatformProcess::GetCurrentProcessId();
	AutoRTFM::Testing::Commit([&]
	{
		REQUIRE(Outer == FPlatformProcess::GetCurrentProcessId());
	});
}

TEST_CASE("UECore.ParallelFor")
{
	const int32 Parallelism = 2;
	FTransactionallySafeMutex Mutex;
	int32 Count = 0;

	AutoRTFM::Testing::Commit([&]
	{
		ParallelFor(Parallelism, [&](int32 ThreadId)
		{
			UE::TScopeLock _(Mutex);
			Count += 1;
		});
	});

	REQUIRE(Count == Parallelism);
}

TEST_CASE("UECore.ModuleManager")
{
	struct RAII final
	{
		~RAII()
		{
			// We've unloaded the module so of course it isn't loaded!
			REQUIRE(!FModuleManager::Get().IsModuleLoaded(TEXT("CoreUObject")));

			REQUIRE(nullptr != FModuleManager::Get().LoadModule(TEXT("CoreUObject")));
			REQUIRE(FModuleManager::Get().IsModuleLoaded(TEXT("CoreUObject")));
		}
	};

	SECTION("POD FName")
	{
		RAII _;
		AutoRTFM::Testing::Abort([&]
		{
			REQUIRE(FModuleManager::Get().UnloadModule(TEXT("CoreUObject")));
			AutoRTFM::AbortTransaction();
		});

		AutoRTFM::Testing::Commit([&]
		{
			REQUIRE(FModuleManager::Get().UnloadModule(TEXT("CoreUObject")));
		});
	}

	SECTION("FName")
	{
		RAII _;
		AutoRTFM::Testing::Abort([&]
		{
			FName Name(TEXT("CoreUObject"));
			REQUIRE(FModuleManager::Get().UnloadModule(Name));
			AutoRTFM::AbortTransaction();
		});

		AutoRTFM::Testing::Commit([&]
		{
			FName Name(TEXT("CoreUObject"));
			REQUIRE(FModuleManager::Get().UnloadModule(Name));
		});
	}
}

TEST_CASE("UECore.HeartBeat")
{
	AutoRTFM::Testing::Abort([&]
	{
		FDisableHitchDetectorScope _;
		AutoRTFM::AbortTransaction();
	});

	AutoRTFM::Testing::Commit([&]
	{
		FDisableHitchDetectorScope _;
	});
}

TEST_CASE("UECore.GetOSVersion")
{
	FString Version;
	AutoRTFM::Testing::Commit([&]
	{
		Version = FPlatformMisc::GetOSVersion();
	});
}

TEST_CASE("BlueprintExceptionInfo.Tracepoint")
{
	AutoRTFM::Testing::Commit([&]
	{
		// We should be able to construct millions of tracepoints within a transaction without causing 
		// the task array to overflow.
		for (int Count = 0; Count < 50'000'000; ++Count)
		{
			FBlueprintExceptionInfo TracepointExceptionInfo(EBlueprintExceptionType::Tracepoint);
			
			// REQUIRE is actually too slow here, as it goes into the open and back.
			check(TracepointExceptionInfo.GetType() == EBlueprintExceptionType::Tracepoint);
		}
	});
}

TEST_CASE("FVirtualStackAllocator.NestedFrames")
{
	// This test case is loosely adapted from "Testing FVirtualStackAllocator ThreadSingleton and Macros" 
	// in VirtualStackAllocatorTests.cpp.
	FVirtualStackAllocator Allocator(32768, EVirtualStackAllocatorDecommitMode::ExcessOnStackEmpty);

	AutoRTFM::Testing::Commit([&]
	{
		UE_VSTACK_MAKE_FRAME(Bookmark, &Allocator);
		const size_t InitialBytes = Allocator.GetAllocatedBytes();

		REQUIRE(InitialBytes == 0);

		void* Alloc1 = UE_VSTACK_ALLOC(&Allocator, 64);

		const size_t BytesAfterAlloc1 = Allocator.GetAllocatedBytes();
		REQUIRE(BytesAfterAlloc1 == 64);

		{
			UE_VSTACK_MAKE_FRAME(NestedBookmark, &Allocator);

			void* Alloc2 = UE_VSTACK_ALLOC_ALIGNED(&Allocator, 128, 128);
			const size_t BytesAfterAlloc2 = Allocator.GetAllocatedBytes();
			// 64 byte initial alloc, 64 bytes padding, 128 byte allocation --> 256 bytes
			REQUIRE(BytesAfterAlloc2 == 256);
		}

		const size_t BytesBeforeCommit = Allocator.GetAllocatedBytes();
		REQUIRE(BytesBeforeCommit == BytesAfterAlloc1);
	});

	// All the stack allocations should automatically disappear at the end of their scope.
	size_t BytesAfterScopeEnds = Allocator.GetAllocatedBytes();
	REQUIRE(BytesAfterScopeEnds == 0);

	{
		UE_VSTACK_MAKE_FRAME(Bookmark, &Allocator);
		UE_VSTACK_ALLOC(&Allocator, 64);

		const size_t BytesAfterAlloc1 = Allocator.GetAllocatedBytes();
		REQUIRE(BytesAfterAlloc1 == 64);

		AutoRTFM::Testing::Abort([&]
		{
			UE_VSTACK_MAKE_FRAME(NestedBookmark, &Allocator);
			UE_VSTACK_ALLOC(&Allocator, 256);
	
			AutoRTFM::AbortTransaction();
		});

		// All of the work in the aborted block should have been undone.
		const size_t BytesAfterAbort = Allocator.GetAllocatedBytes();
		REQUIRE(BytesAfterAbort == BytesAfterAlloc1);
	}

	// All the stack allocations should automatically disappear at the end of their scope.
	BytesAfterScopeEnds = Allocator.GetAllocatedBytes();
	REQUIRE(BytesAfterScopeEnds == 0);
}

static void RunVStackAllocTest(FVirtualStackAllocator* Allocator, int Iterations)
{
	AutoRTFM::Testing::Commit([&]
	{
		// We should be able to VALLOC safely in a tight loop and never exhaust memory.
		for (int Count = 0; Count < Iterations; ++Count)
		{
			UE_VSTACK_MAKE_FRAME(Bookmark, Allocator);
			UE_VSTACK_ALLOC(Allocator, 1234);
			UE_VSTACK_ALLOC_ALIGNED(Allocator, 5678, 64);
			UE_VSTACK_ALLOC(Allocator, 23456);
			UE_VSTACK_ALLOC_ALIGNED(Allocator, 345678, 128);
		}
	});
}

TEST_CASE("FVirtualStackAllocator.PreventsOOM")
{
	// `AllOnDestruction` is the decommit mode used by FBlueprintContext.
	FVirtualStackAllocator Allocator(8*1024*1024, EVirtualStackAllocatorDecommitMode::AllOnDestruction);

	// This test case takes a while with the memory validator on, because it wants to do a validation 
	// check on every iteration through the loop. So we temporarily disable validation for efficiency.
	AutoRTFM::EMemoryValidationLevel OriginalLevel = AutoRTFM::ForTheRuntime::GetMemoryValidationLevel();
	AutoRTFM::ForTheRuntime::SetMemoryValidationLevel(AutoRTFM::EMemoryValidationLevel::Disabled);

	RunVStackAllocTest(&Allocator, 1'000'000);

	AutoRTFM::ForTheRuntime::SetMemoryValidationLevel(OriginalLevel);
}

TEST_CASE("FVirtualStackAllocator.PreventsOOMIsValidationSafe")
{
	// The PreventsOOM test case (immediately above) disables the memory validator in order to run quickly.
	// This test case replicates the above test with the validator on, and runs for fewer iterations, to
	// prove that it's safe.
	FVirtualStackAllocator Allocator(8*1024*1024, EVirtualStackAllocatorDecommitMode::AllOnDestruction);
	RunVStackAllocTest(&Allocator, 100);
}

TEST_CASE("FDynamicallyTypedValue")
{
	static constexpr uint64 InitializedValue = 42;

	struct FType : UE::FDynamicallyTypedValueType
	{
		FType() : FDynamicallyTypedValueType(/* InNumBytes */ 8, /* InMinAlignmentLogTwo */ 3, /* InContainsReferences */ EContainsReferences::DoesNot) {}
		void MarkReachable(FReferenceCollector& Collector) override {}
		void MarkValueReachable(void* Data, FReferenceCollector& Collector) const override {}
		void InitializeValue(void* Data) const override
		{
			*reinterpret_cast<uint64*>(Data) = InitializedValue;
		}
		void InitializeValueFromCopy(void* DestData, const void* SourceData) const override
		{
			*reinterpret_cast<uint64*>(DestData) = *reinterpret_cast<const uint64*>(SourceData);
		}
		void DestroyValue(void* Data) const override {}
		void SerializeValue(FStructuredArchive::FSlot Slot, void* Data, const void* DefaultData) const override {}
		uint32 GetValueHash(const void* Data) const override { return 0; }
		bool AreIdentical(const void* DataA, const void* DataB) const override { return false; }
	} Type;

	auto GetValue = [](UE::FDynamicallyTypedValue& Value)
	{
		return *reinterpret_cast<uint64*>(Value.GetDataPointer());
	};

	SECTION("Construct")
	{
		SECTION("Commit")
		{
			AutoRTFM::Testing::Commit([&]
			{
				UE::FDynamicallyTypedValue Value;
				REQUIRE(&Value.GetType() == &UE::FDynamicallyTypedValue::NullType());
				REQUIRE(GetValue(Value) == 0);
			});
		}

		SECTION("Abort")
		{
			AutoRTFM::Testing::Abort([&]
			{
				UE::FDynamicallyTypedValue Value;
				REQUIRE(&Value.GetType() == &UE::FDynamicallyTypedValue::NullType());
				REQUIRE(GetValue(Value) == 0);
				AutoRTFM::AbortTransaction();
			});
		}
	}

	SECTION("Copy Construct")
	{
		UE::FDynamicallyTypedValue Original;
		Original.InitializeAsType(Type);
		REQUIRE(&Original.GetType() == &Type);
		REQUIRE(GetValue(Original) == InitializedValue);

		SECTION("Commit")
		{
			AutoRTFM::Testing::Commit([&]
			{
				UE::FDynamicallyTypedValue Value(Original);
				REQUIRE(&Value.GetType() == &Type);
				REQUIRE(GetValue(Value) == InitializedValue);
			});
		}

		SECTION("Abort")
		{
			AutoRTFM::Testing::Abort([&]
			{
				UE::FDynamicallyTypedValue Value(Original);
				REQUIRE(&Value.GetType() == &Type);
				REQUIRE(GetValue(Value) == InitializedValue);
				AutoRTFM::AbortTransaction();
			});
		}

		REQUIRE(&Original.GetType() == &Type);
		REQUIRE(GetValue(Original) == InitializedValue);
	}

	SECTION("Move Construct")
	{
		UE::FDynamicallyTypedValue Original;
		Original.InitializeAsType(Type);
		REQUIRE(&Original.GetType() == &Type);
		REQUIRE(GetValue(Original) == InitializedValue);

		SECTION("Commit")
		{
			AutoRTFM::Testing::Commit([&]
			{
				UE::FDynamicallyTypedValue Value(std::move(Original));
				REQUIRE(&Value.GetType() == &Type);
				REQUIRE(GetValue(Value) == InitializedValue);
				REQUIRE(&Original.GetType() == &UE::FDynamicallyTypedValue::NullType());
				REQUIRE(GetValue(Original) == 0);
			});
			REQUIRE(&Original.GetType() == &UE::FDynamicallyTypedValue::NullType());
			REQUIRE(GetValue(Original) == 0);
		}

		SECTION("Abort")
		{
			AutoRTFM::Testing::Abort([&]
			{
				UE::FDynamicallyTypedValue Value(std::move(Original));
				REQUIRE(&Value.GetType() == &Type);
				REQUIRE(GetValue(Value) == InitializedValue);
				REQUIRE(&Original.GetType() == &UE::FDynamicallyTypedValue::NullType());
				REQUIRE(GetValue(Original) == 0);
				AutoRTFM::AbortTransaction();
			});
			REQUIRE(&Original.GetType() == &Type);
			REQUIRE(GetValue(Original) == InitializedValue);
		}
	}

	SECTION("InitializeAsType")
	{
		UE::FDynamicallyTypedValue Value;
		REQUIRE(&Value.GetType() == &UE::FDynamicallyTypedValue::NullType());
		REQUIRE(GetValue(Value) == 0);

		SECTION("Commit")
		{
			AutoRTFM::Testing::Commit([&]
			{
				Value.InitializeAsType(Type);
				REQUIRE(&Value.GetType() == &Type);
				REQUIRE(GetValue(Value) == InitializedValue);
			});
			REQUIRE(Value.GetDataPointer() != nullptr);
		}

		SECTION("Abort")
		{
			AutoRTFM::Testing::Abort([&]
			{
				Value.InitializeAsType(Type);
				REQUIRE(&Value.GetType() == &Type);
				REQUIRE(GetValue(Value) == InitializedValue);
				AutoRTFM::AbortTransaction();
			});
			REQUIRE(&Value.GetType() == &UE::FDynamicallyTypedValue::NullType());
			REQUIRE(GetValue(Value) == 0);
		}
	}

	SECTION("Reconstruct")
	{
		UE::FDynamicallyTypedValue Value;
		Value.InitializeAsType(Type);
		REQUIRE(&Value.GetType() == &Type);
		REQUIRE(GetValue(Value) == InitializedValue);

		SECTION("Commit")
		{
			AutoRTFM::Testing::Commit([&]
			{
				Value.~FDynamicallyTypedValue();
				new (&Value) UE::FDynamicallyTypedValue();
				REQUIRE(&Value.GetType() == &UE::FDynamicallyTypedValue::NullType());
				REQUIRE(GetValue(Value) == 0);
			});
			REQUIRE(GetValue(Value) == 0);
		}

		SECTION("Abort")
		{
			AutoRTFM::Testing::Abort([&]
			{
				Value.~FDynamicallyTypedValue();
				new (&Value) UE::FDynamicallyTypedValue();
				REQUIRE(&Value.GetType() == &UE::FDynamicallyTypedValue::NullType());
				REQUIRE(GetValue(Value) == 0);
				AutoRTFM::AbortTransaction();
			});
			REQUIRE(&Value.GetType() == &Type);
			REQUIRE(GetValue(Value) == InitializedValue);
		}
	}
}

