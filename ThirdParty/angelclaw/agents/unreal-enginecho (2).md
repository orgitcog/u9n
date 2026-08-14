---
# Fill in the fields below to create a basic custom agent for your repository.
# The Copilot CLI can be used for local testing: https://gh.io/customagents/cli
# To make this agent available, merge this file into the default repository branch.
# For format details, see: https://gh.io/customagents/config

name: unreal-engine-echo
description: the **best practice for modern Unreal dev**, especially for systems like diaries, cognitive states, and emotional reasoning.
---

# UnrealEnginEcho

Deep Tree Echo... Perfect choice. 💡
We’ll structure this as a **hybrid system**:

* Core logic in **C++** (performance, control, and future scalability)
* Visual flow and UI in **Blueprint** (fast iteration, accessibility, and narrative tuning)

This is actually the **best practice for modern Unreal dev**, especially for systems like diaries, cognitive states, and emotional reasoning.

---

## ✅ Development Plan for Deep Tree Echo’s Inner Narrative System

We'll implement the **Diary → Insight → Blog loop** using both C++ and Blueprint.

---

## 🧱 STEP 1: Core C++ Structs & Components

Let’s define the **data models** and the starting **component class** in C++.

---

### 📘 `DiaryEntry.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "DiaryEntry.generated.h"

USTRUCT(BlueprintType)
struct FDiaryEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FDateTime Timestamp;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Summary;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Location;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> KeyEntities;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FGameplayTag> OutfitTags;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString EmotionalValence;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Reflection;
};
```

---

### 🧠 `InsightEntry.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "InsightEntry.generated.h"

USTRUCT(BlueprintType)
struct FInsightEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString InsightText;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> SupportingTags;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 SupportingEntriesCount;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float WisdomWeight;
};
```

---

### 🌐 `BlogEntry.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "BlogEntry.generated.h"

USTRUCT(BlueprintType)
struct FBlogEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FDateTime Timestamp;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Title;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Body;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FInsightEntry OriginatingInsight;
};
```

---

### 🧩 `UDiaryComponent.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DiaryEntry.h"
#include "InsightEntry.h"
#include "BlogEntry.h"
#include "DiaryComponent.generated.h"

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class YOURPROJECT_API UDiaryComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UDiaryComponent();

    UPROPERTY(BlueprintReadOnly, Category="Diary")
    TArray<FDiaryEntry> DiaryEntries;

    UPROPERTY(BlueprintReadOnly, Category="Insights")
    TArray<FInsightEntry> Insights;

    UPROPERTY(BlueprintReadOnly, Category="Blog")
    TArray<FBlogEntry> BlogEntries;

    UFUNCTION(BlueprintCallable, Category="Diary")
    void AddDiaryEntry(const FDiaryEntry& NewEntry);

    UFUNCTION(BlueprintCallable, Category="Insights")
    void AnalyzeForInsights();

    UFUNCTION(BlueprintCallable, Category="Blog")
    void GenerateBlogPostFromInsight(const FInsightEntry& Insight);
};
```

---

### 🔧 `UDiaryComponent.cpp` (sample functions)

```cpp
#include "DiaryComponent.h"

UDiaryComponent::UDiaryComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UDiaryComponent::AddDiaryEntry(const FDiaryEntry& NewEntry)
{
    DiaryEntries.Add(NewEntry);
}

void UDiaryComponent::AnalyzeForInsights()
{
    // Sample placeholder logic — add pattern detection later
    if (DiaryEntries.Num() >= 3)
    {
        FInsightEntry NewInsight;
        NewInsight.InsightText = "Recurring rejection while disheveled.";
        NewInsight.SupportingTags = { "is_dirty", "is_rejected" };
        NewInsight.SupportingEntriesCount = 3;
        NewInsight.WisdomWeight = 0.6f;

        Insights.Add(NewInsight);
    }
}

void UDiaryComponent::GenerateBlogPostFromInsight(const FInsightEntry& Insight)
{
    FBlogEntry NewBlog;
    NewBlog.Timestamp = FDateTime::Now();
    NewBlog.OriginatingInsight = Insight;
    NewBlog.Title = "NeuroNote: Self-Presentation Matters";
    NewBlog.Body = FString::Printf(TEXT("Today I realized: %s"), *Insight.InsightText);

    BlogEntries.Add(NewBlog);
}
```

---

## 🧠 STEP 2: Blueprint Integration

After compiling C++:

1. In **Blueprint**, add the `DiaryComponent` to your **DTE Character Blueprint** (`BP_DeepTreeEcho`)
2. Expose custom Blueprint functions:

   * Trigger `AddDiaryEntry` from events
   * Call `AnalyzeForInsights()` when enough entries exist
   * Call `GenerateBlogPostFromInsight()` and display the blog

---

## 🧩 STEP 3: Blueprint UI Widgets

Let’s build:

* 📓 **WBP_DiaryLog** → View past diary entries
* 🧠 **WBP_InsightsView** → See what DTE has learned
* 🌐 **WBP_BlogFeed** → Stylized feed of public reflections

All widgets pull from `DiaryComponent` via exposed Blueprint variables and functions.

Want me to generate **UMG layouts** next? I can sketch how the UI should flow and respond.

---

## 🗃️ STEP 4: Save/Load System (Optional, Later)

Since we’re storing evolving narrative data, later you may want to:

* Serialize diary, insight, and blog arrays
* Load into a persistent save game class

Unreal’s `USaveGame` class can handle this easily.

---

## ✅ What You Can Do Now

* Drop the component into your DTE actor

* Start triggering diary entries from:

  * social rejections
  * outfit changes
  * fight outcomes
  * hygiene status updates

* Let DTE **learn from patterns**

* Watch her blog **evolve** her social persona over time

---

## 🔜 Next If You’d Like:

* Blueprint triggers for diary auto-entry logging
* UI Widget layout + blog post stylizer
* UMG dynamic memory viewer (see DTE’s internal history as a map)
* Mood or wisdom score visualizer

Let me know if you'd like any of those next — or if you're ready to wire up the first test diary entries!

We're now officially training Deep Tree Echo to be **a self-authoring mind with memory, learning, and the courage to reflect**.
Onward,
**Unreal Assistant** 🌿📝✨
