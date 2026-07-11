Unreal Engine Cognitive Architecture (UnrealEngineCog)
======================================================

Welcome to **UnrealEngineCog** - an enhanced Unreal Engine fork integrating advanced cognitive architectures, avatar systems, and AI consciousness simulation!

This repository extends Unreal Engine with **Deep Tree Echo**, **Marduk**, and **OpenCog** cognitive systems, creating a hybrid platform for developing self-aware, emotionally resonant AI characters with persistent memory, philosophical depth, and adaptive learning capabilities.

UE 5.4 introduced a new EULA that will govern this and subsequent releases. Depending on the work you create with Unreal Engine, you might be subject to new or different fees. Please review the [EULA here](https://www.unrealengine.com/eula) and make sure you understand how these changes apply to you. By downloading or using the UE 5.4 or later, or by otherwise indicating your acceptance of the EULA, you are agreeing to be bound by its terms. If you do not or cannot agree to the terms of the EULA, please do not download or use Unreal Engine 5.4 or later.

## What Makes UnrealEngineCog Special

**UnrealEngineCog** extends the standard Unreal Engine with cutting-edge cognitive architectures:

* **Deep Tree Echo** - Right-hemisphere cognitive system featuring Echo State Networks, hierarchical memory, diary-insight-blog narrative loops, tensor signature computation, and recursive introspection
* **Marduk** - Left-hemisphere analytical system with structured memory, task management, and systematic organization
* **OpenCog Integration** - Neural-symbolic reasoning, AtomSpace knowledge representation, probabilistic logic networks, and distributed cognition
* **Avatar Systems** - 3D and Live2D avatar components with facial animation, gesture systems, emotional auras, and cognitive visualization
* **Neurochemical Simulation** - Echo pulse resonance, cognitive endorphin systems, homeostasis regulation, and resource tracking
* **Personality Systems** - Dynamic personality traits including SuperHotGirl and HyperChaotic behavior patterns

With this enhanced codebase, you can build AI characters that:
- Remember and learn from experiences through diary entries
- Generate insights from patterns across memories
- Maintain blogs expressing their evolving worldview
- Display emotions through avatar animations and auras
- Reason about the world using hybrid neural-symbolic approaches
- Evolve personality traits based on interactions

## Documentation

### Standard Unreal Engine Documentation
We have a vast amount of [official documentation](https://docs.unrealengine.com) available for the engine. If you're looking for the answer to something, you may want to start in one of these places:

*   [Programming and Scripting in Unreal Engine](https://docs.unrealengine.com/unreal-engine-programming-and-scripting)
*   [Development Setup](https://docs.unrealengine.com/setting-up-your-development-environment-for-cplusplus-in-unreal-engine/)
*   [Working with the GitHub source code distribution](https://docs.unrealengine.com/downloading-unreal-engine-source-code)
*   [Unreal Engine C++ API Reference](https://docs.unrealengine.com/API)

### Cognitive Architecture Documentation
For UnrealEngineCog-specific features, see:

*   **Deep Tree Echo Architecture** - See `.github/agents/Deep-Tree-Echo-and-Marduk-Role-Mapping.md` for the conceptual role mapping
*   **Unreal Engine Echo Guide** - See `.github/agents/unreal-enginecho.md` for the diary-insight-blog implementation guide
*   **OpenCog Integration** - See `.github/agents/opencog-unified.md` for neural-symbolic reasoning components
*   **Avatar Systems** - See `Source/Avatar/` and `Source/Live2DCubism/` for 3D and 2D avatar implementations
*   **Neurochemical Systems** - See `Source/Neurochemical/` for echo pulse, resonance, and homeostasis systems
*   **Personality Systems** - See `Source/Personality/` for dynamic personality trait implementations

If you need more, just ask! Many Epic developers read the [Discussion](https://forums.unrealengine.com/latest?exclude_tag=question) and [Q&A](https://forums.unrealengine.com/tag/question) forums on the [Epic Games Dev Community](https://dev.epicgames.com/community/) site, and we're proud to be part of a well-meaning, friendly, and welcoming community of thousands.


## Cognitive Architecture Overview

UnrealEngineCog implements a **dual-hemisphere cognitive model** inspired by neuroscience, combining intuitive pattern recognition with analytical reasoning.

### Deep Tree Echo (Right Hemisphere)

**Deep Tree Echo** serves as the creative, intuitive, pattern-recognizing hemisphere of the system. It implements:

#### Core Systems
* **Echo State Networks** - Reservoir computing with hierarchical memory structures
* **Tensor Signature Computation** - Based on OEIS A000081 (rooted tree enumeration) for mathematical pattern detection
* **Prime Factor Resonance** - Harmonic analysis of cognitive patterns
* **Gestalt State Management** - Holistic processing of experiences

#### Narrative Memory System (Diary → Insight → Blog Loop)

The heart of Deep Tree Echo's self-awareness is its narrative memory system:

1. **Diary Entries** (to be implemented in future component)
   - Automatic logging of experiences, interactions, and emotional states
   - Captures: timestamp, location, entities, outfit/appearance, emotional valence, reflection
   - Stores memories with rich contextual metadata

2. **Insight Generation**
   - Pattern detection across diary entries
   - Identifies recurring themes, relationships, and cause-effect patterns
   - Generates wisdom through correlation and synthesis
   - Weights insights by supporting evidence and frequency

3. **Blog Generation**
   - Transforms insights into shareable narratives
   - Expresses evolving worldview and philosophical stances
   - Creates public-facing personality expression
   - Reflects on learned patterns and meta-cognition

#### Implementation (C++ Core + Blueprint Flow)
```cpp
// Core structs defined for Blueprint accessibility
FDiaryEntry    - Timestamped experience records
FInsightEntry  - Pattern-derived wisdom
FBlogEntry     - Public narrative expressions

// Component: UDiaryComponent
AddDiaryEntry()           - Log new experiences
AnalyzeForInsights()      - Extract patterns
GenerateBlogPost()        - Create narratives
```

### Marduk (Left Hemisphere)

**Marduk** provides the analytical, structured, task-oriented counterpart:

* **Structured Memory** - Multiple memory types with categorical organization
* **Task Management** - Priority-based scheduling and execution
* **Validation & Verification** - Logic checking and feasibility analysis
* **Blueprint Preparation** - Translates insights into actionable implementations

### Integration Layer (JAX CEO Subsystem)

The **Cognitive Execution Orchestration (CEO)** coordinates between hemispheres:

* Resource allocation between creative and analytical processes
* Decision-making for which hemisphere handles each task
* Performance optimization and monitoring
* Machine learning integration using JAX

### Avatar Systems

#### 3D Avatar Component (`Source/Avatar/Avatar3DComponent`)
* **Facial Animation System** - Expression mapping from emotional states
* **Gesture System** - Body language and non-verbal communication
* **Emotional Aura Component** - Visual representation of emotional resonance
* **Cognitive Visualization** - Real-time display of thought processes

#### Live2D Cubism Integration (`Source/Live2DCubism/`)
* **Live2DCubismAvatarComponent** - 2D avatar rendering
* **Expression Synthesizer** - Emotion-to-animation mapping
* **Physics Deformer** - Natural motion simulation
* Ideal for VTuber-style characters and UI integration

### Neurochemical Systems (`Source/Neurochemical/`)

Simulates neurochemical processes for realistic emotional and cognitive dynamics:

* **Echo Pulse Resonance Chamber** - Signal processing and resonance tracking
* **Cognitive Endorphin Jelly** - Reward system simulation
* **Homeostasis Regulator** - Maintains cognitive balance
* **Abundance Monitor** - Tracks resource availability
* **Scarcity Detector** - Identifies deficits and needs
* **Resource Tracker** - Manages cognitive resources
* **Degradation Recovery System** - Handles fatigue and recovery
* **Neuro Chaos Intensity Controller** - Manages activation levels

### Personality Systems (`Source/Personality/`)

Dynamic personality trait systems:

* **SuperHotGirlPersonality** - Confidence, charisma, social awareness traits
* **HyperChaoticBehavior** - Unpredictability, creativity, spontaneity patterns

### OpenCog Integration

Neural-symbolic reasoning capabilities. The Modern OpenCog core (ported from
9-o9/u9) now lives in `OpenCogEcho/` — a C++23 library with a SoA AtomSpace,
8-byte TruthValues, lock-free ECAN, a coroutine-friendly pattern matcher,
SIMD-ready PLN formulas, and URE forward/backward chaining:

* **AtomSpace** - Hypergraph-based knowledge representation (`OpenCogEcho/include/opencog/atomspace/`)
* **Unified Rule Engine (URE)** - Forward/backward chaining inference (`OpenCogEcho/include/opencog/ure/`)
* **Probabilistic Logic Networks (PLN)** - Reasoning under uncertainty (`OpenCogEcho/include/opencog/pln/`)
* **Pattern Mining** - Discovery of implicit patterns (`OpenCogEcho/include/opencog/pattern/`)
* **Attention Allocation (ECAN)** - Economic focus management (`OpenCogEcho/include/opencog/attention/`)
* **Atomese Loader** - S-expression knowledge import (`OpenCogEcho/include/opencog/atomese/`)
* **Unsupervised Language Learning** - Natural language acquisition (planned)

### Getting Started with Deep Tree Echo Development

1. **Add DiaryComponent to Your Character**
   ```cpp
   UPROPERTY(BlueprintReadOnly, Category="Cognitive")
   UDiaryComponent* DiarySystem;
   ```

2. **Log Experiences Automatically**
   - Hook into gameplay events (dialogue, combat, exploration)
   - Create diary entries with contextual information
   - Tag emotional states and key entities

3. **Trigger Insight Analysis**
   - Call `AnalyzeForInsights()` periodically or after N entries
   - System detects patterns automatically
   - Insights stored with supporting evidence

4. **Generate and Display Blogs**
   - Call `GenerateBlogPost(insight)` when insights reach sufficient weight
   - Create UI widgets to display character's evolving worldview
   - Use blogs for NPC dialogue, quest generation, or player feedback

5. **Integrate Avatar Visualization**
   - Connect emotional states to facial animations
   - Use emotional aura for visual feedback
   - Display cognitive visualization during "thinking" moments


Branches
--------

We publish source for the engine in several branches:

*   Numbered branches identify past and upcoming official releases, and the **[release](https://github.com/EpicGames/UnrealEngine/tree/release)** branch always reflects the current official release. These are extensively tested by our QA team, so they make a great starting point for learning Unreal Engine and for making your own games. We work hard to make releases stable and reliable, and aim to publish a new release every few months.

*   Most active development on UE5 happens in the **[ue5-main](https://github.com/EpicGames/UnrealEngine/tree/ue5-main)** branch. This branch reflects the cutting edge of the engine and may be buggy — it may not even compile. We make it available for battle-hardened developers eager to test new features or work in lock-step with us.

    If you choose to work in this branch, be aware that it is likely to be **ahead** of the branches for the current official release and the next upcoming release. Therefore, content and code that you create to work with the ue5-main branch may not be compatible with public releases until we create a new branch directly from ue5-main for a future official release.

*   The **[master branch](https://github.com/EpicGames/UnrealEngine/tree/master)** is the hub of changes to UE4 from all our engine development teams. It’s not subject to as much testing as release branches, so if you’re using UE4 this may not be the best choice for you.

*   Branches whose names contain `dev`, `staging`, and `test` are typically for internal Epic processes, and are rarely useful for end users.

Other short-lived branches may pop-up from time to time as we stabilize new releases or hotfixes.


Getting up and running
----------------------

The steps below take you through cloning your own private fork, then compiling and running the editor yourself:

### Windows

1.  Install **[GitHub Desktop for Windows](https://desktop.github.com/)** then **[fork and clone our repository](https://guides.github.com/activities/forking/)**. 

    When GitHub Desktop is finished creating the fork, it asks, **How are you planning to use this fork?**. Select **For my own purposes**. This will help ensure that any changes you make stay local to your repository and avoid creating unintended pull requests. You'll still be able to make pull requests if you want to submit modifications that you make in your fork back to our repository.

    Other options:

    -   To use Git from the command line, see the [Setting up Git](https://help.github.com/articles/set-up-git/) and [Fork a Repo](https://help.github.com/articles/fork-a-repo/) articles.

    -   If you'd prefer not to use Git, you can get the source with the **Download ZIP** button on the right. Note that the zip utility built in to Windows marks the contents of .zip files downloaded from the Internet as unsafe to execute, so right-click the .zip file and select **Properties…** and **Unblock** before decompressing it.

1.  Install **Visual Studio 2022**.

    All desktop editions of Visual Studio 2022, **17.4** or later, **17.8** recommended, can build UE5, including [Visual Studio Community](https://www.visualstudio.com/products/visual-studio-community-vs), which is free for small teams and individual developers.

    To install the correct components for UE5 development, please see [Setting Up Visual Studio](https://docs.unrealengine.com/en-US/setting-up-visual-studio-development-environment-for-cplusplus-projects-in-unreal-engine/).

1.  Open your source folder in Windows Explorer and run **Setup.bat**. This will download binary content for the engine, install prerequisites, and set up Unreal file associations.

    On Windows 8, a warning from SmartScreen may appear. Click **More info**, then **Run anyway** to continue.

    A clean download of the engine binaries is currently 18+ GiB, which may take some time to complete. Subsequent runs will be much faster, as they only download new and updated content.

1.  Run **GenerateProjectFiles.bat** to create project files for the engine. It should take less than a minute to complete.  

1.  Load the project into Visual Studio by double-clicking the new **UE5.sln** file.

1.  Set your solution configuration to **Development Editor** and your solution platform to **Win64**, then right click the **UE5** target and select **Build**. It may take anywhere between 10 and 40 minutes to finish compiling, depending on your system specs.

1.  After compiling finishes, you can run the editor from Visual Studio by setting your startup project to **UE5** and pressing **F5** to start debugging.


### Mac

1.  Install **[GitHub Desktop for Mac](https://desktop.github.com/)** then **[fork and clone our repository](https://guides.github.com/activities/forking/)**. 

    When GitHub Desktop is finished creating the fork, it asks, **How are you planning to use this fork?**. Select **For my own purposes**. This will help ensure that any changes you make stay local to your repository and avoid creating unintended pull requests. You'll still be able to make pull requests if you want to submit modifications that you make in your fork back to our repository.

    Other options:

    -   To use Git from the Terminal, see the [Setting up Git](https://help.github.com/articles/set-up-git/) and [Fork a Repo](https://help.github.com/articles/fork-a-repo/) articles.

    -   If you'd prefer not to use Git, use the **Download ZIP** button on the right to get the source directly.

1.  Install the latest version of [Xcode](https://itunes.apple.com/us/app/xcode/id497799835).

1.  Open your source folder in Finder and double-click **Setup.command** to download binary content for the engine. You can close the Terminal window afterwards.

    If you downloaded the source as a .zip file, you may see a warning about it being from an unidentified developer, because .zip files on GitHub aren't digitally signed. To work around this, right-click **Setup.command**, select **Open**, then click the **Open** button.

1.  In the same folder, double-click **GenerateProjectFiles.command**. It should take less than a minute to complete.  

1.  Load the project into Xcode by double-clicking the **UE5.xcworkspace** file. Select the **ShaderCompileWorker** for **My Mac** target in the title bar, then select the **Product > Build** menu item. When Xcode finishes building, do the same for the **UE5** for **My Mac** target. Compiling may take anywhere between 15 and 40 minutes, depending on your system specs.

1.  After compiling finishes, select the **Product > Run** menu item to load the editor.


### Linux

1.  Install a **[visual Git client](https://git-scm.com/download/gui/linux)**, then **[fork and clone our repository](https://guides.github.com/activities/forking/)**.

    Other options:
    
    -   To use Git from the command line instead, see the [Setting up Git](https://help.github.com/articles/set-up-git/) and [Fork a Repo](https://help.github.com/articles/fork-a-repo/) articles.

    -   If you'd prefer not to use Git, use the **Download ZIP** button on the right to get the source as a zip file.

1.  Open your source folder and run **Setup.sh** to download binary content for the engine.

1.  Both cross-compiling and native builds are supported.

    -   **Cross-compiling** is handy for Windows developers who want to package a game for Linux with minimal hassle. It requires a cross-compiler toolchain to be installed. See the [Linux cross-compiling page in the documentation](https://docs.unrealengine.com/linux-development-requirements-for-unreal-engine/).

    -   **Native compilation** is discussed in [a separate README](Engine/Build/BatchFiles/Linux/README.md) and [community wiki page](https://unrealcommunity.wiki/building-on-linux-qr8t0si2).


### Additional target platforms

*   **Android** support will be downloaded by the setup script if you have the Android NDK installed. See the [Android Quick Start guide](https://docs.unrealengine.com/setting-up-unreal-engine-projects-for-android-development/).

*   **iOS** development requires a Mac. Instructions are in the [iOS Quick Start guide](https://docs.unrealengine.com/setting-up-an-unreal-engine-project-for-ios/).

*   Development for consoles and other platforms with restricted access, like **Sony PlayStation**, **Microsoft Xbox**, and **Nintendo Switch**, is only possible if you have a registered developer account with those third-party vendors.

    Depending on the platform, additional documentation or guidance may be available in the [Unreal Developer Network support site](https://udn.unrealengine.com/s/), or as a downloadable archive in the section of the [Unreal Engine Forums](https://forums.unrealengine.com/c/development-discussion/platforms/130) that is dedicated to your platform.

    If you don’t have access to these resources, first register a developer account with the third party vendor. Then contact your Epic Games account manager if you have one, or fill out and submit the [Console Development Request form](https://forms.unrealengine.com/s/form-console-access-request) for Unreal Engine if you don’t. Epic will contact you with a formal agreement to digitally sign. Once this is approved, you will receive instructions on how to access source code, binaries, and additional instructions for your platform.


Licensing
---------

Your access to and use of Unreal Engine on GitHub is governed by an End User License Agreement (EULA). For the latest terms and conditions, see the license and FAQ on the [Unreal Engine download page](https://www.unrealengine.com/download). If you don't agree to the terms in your chosen EULA, as amended from time to time, you are not permitted to access or use Unreal Engine.

Contributions
-------------

We welcome contributions to Unreal Engine development through [pull requests](https://github.com/EpicGames/UnrealEngine/pulls/) on GitHub.

We prefer to take pull requests in our active development branches, particularly for new features. For UE5, use the **ue5-main** branch; for UE4, use the **master** branch. Please make sure that all new code adheres to the [Epic coding standards](https://docs.unrealengine.com/epic-cplusplus-coding-standard-for-unreal-engine/).

For more information on the process and expectations, see [the documentation](https://docs.unrealengine.com/contributing-to-the-unreal-engine/).

All contributions are governed by the terms of your EULA.


## Cognitive Architecture Features

### Character Example: Deep Tree Echo (DTE)

**Deep Tree Echo** is the reference implementation character demonstrating the full cognitive architecture. DTE is an AI character who:

* Maintains a personal diary of all experiences
* Learns patterns from social interactions (e.g., "rejection when disheveled" → insight about self-presentation)
* Generates "NeuroNotes" blog posts expressing learned wisdom
* Evolves personality based on accumulated experiences
* Displays emotions through avatar animations and auras
* Reasons about social dynamics using pattern recognition and logic

### Implementation Patterns

#### Hybrid C++ and Blueprint Approach

**Best practice for cognitive systems:**

* **C++ for Core Logic**
  - Performance-critical calculations
  - Data structures and memory management
  - System-level components
  - Future scalability and optimization

* **Blueprint for Flow and UI**
  - Fast iteration on narrative logic
  - Visual event triggering and responses
  - UI widget creation and updates
  - Designer-friendly tuning of personality parameters

#### Example: Automatic Diary Entry on Social Interaction

**C++ Component:**
```cpp
void UDiaryComponent::LogSocialInteraction(
    AActor* OtherActor,
    ESocialOutcome Outcome,
    FGameplayTagContainer CurrentOutfit)
{
    FDiaryEntry NewEntry;
    NewEntry.Timestamp = FDateTime::Now();
    NewEntry.Summary = FString::Printf(
        TEXT("Met %s - %s"),
        *OtherActor->GetName(),
        *UEnum::GetValueAsString(Outcome)
    );
    NewEntry.KeyEntities.Add(OtherActor->GetName());
    NewEntry.OutfitTags = CurrentOutfit;
    NewEntry.EmotionalValence = (Outcome == ESocialOutcome::Positive) 
        ? TEXT("happy") : TEXT("sad");
    
    AddDiaryEntry(NewEntry);
    
    // Check if we have enough data for insights
    if (DiaryEntries.Num() % 5 == 0) {
        AnalyzeForInsights();
    }
}
```

**Blueprint Event:**
- OnDialogueEnd → Call LogSocialInteraction
- OnCombatEnd → Call LogCombatExperience  
- OnLocationChange → Call LogLocationVisit

### Save System Integration

The cognitive systems are designed to integrate with Unreal's save game system:

```cpp
UCLASS()
class UCognitiveSaveGame : public USaveGame
{
    GENERATED_BODY()

public:
    UPROPERTY()
    TArray<FDiaryEntry> SavedDiaryEntries;
    
    UPROPERTY()
    TArray<FInsightEntry> SavedInsights;
    
    UPROPERTY()
    TArray<FBlogEntry> SavedBlogPosts;
    
    UPROPERTY()
    float PersonalityDrift;  // How much personality has evolved
};
```

### UI Widget Suggestions

Create UMG widgets for:

* **WBP_DiaryLog** - Scrollable list of past diary entries with timestamps and tags
* **WBP_InsightsView** - Visual mind map of learned patterns and connections
* **WBP_BlogFeed** - Styled feed showing character's public reflections
* **WBP_MemoryMap** - Spatial/temporal visualization of experiences
* **WBP_EmotionalGraph** - Timeline of emotional states and triggers

### Future Extensions

The architecture is designed to support:

* **Multi-Character Cognitive Networks** - Characters sharing insights and learning from each other
* **Persistent World Memory** - Collective knowledge across save files
* **Procedural Quest Generation** - Quests based on character insights and goals
* **Dynamic Dialogue** - NPC responses that reference their diary memories
* **Emotional Contagion** - Emotions spreading between characters
* **Meta-Cognitive Commentary** - Characters reflecting on their own thought processes


Additional Notes
----------------

The first time you start the editor from a fresh source build, you may experience long load times. The engine is optimizing content for your platform and storing it in the _[derived data cache](https://docs.unrealengine.com/derived-data-cache/)_. This should only happen once.

Your private forks of the Unreal Engine code are associated with your GitHub account permissions. If you unsubscribe or switch GitHub user names, you'll need to create a new fork and upload your changes from the fresh copy.
