# Quick Start Guide: Live2D Pyper vs Neuro Fusion

This guide will help you quickly get started with the Live2D Cubism SDK integration, Pyper character system, and Superhot Fusion Battle mechanics.

## Prerequisites

- Unreal Engine 5.4 or later
- CogEngine plugin enabled
- Basic understanding of C++ or Blueprints in Unreal Engine

## Step 1: Enable the Plugin

1. Open your Unreal Engine project
2. Go to **Edit → Plugins**
3. Search for "CogEngine"
4. Enable the plugin
5. Restart the editor

## Step 2: Simple Pyper Character Setup

### In C++:

```cpp
#include "PyperActor.h"

// In your game code
void AMyGameMode::SpawnPyper()
{
    // Spawn Pyper
    APyperActor* Pyper = GetWorld()->SpawnActor<APyperActor>(
        APyperActor::StaticClass(),
        FVector(0, 0, 100),
        FRotator::ZeroRotator
    );
    
    // Initialize with a model path
    Pyper->InitializePyper("/Game/Models/Pyper.model3.json");
    
    // Interact with Pyper
    Pyper->ExpressEmotion(TEXT("Happy"));
    
    // Process cognitive input
    TArray<float> Input = {0.8f, 0.7f, 0.5f};
    TArray<float> Output = Pyper->ProcessCognitiveInput(Input);
    
    // Generate a response
    FString Response = Pyper->GenerateResponse(TEXT("Hello Pyper!"));
    UE_LOG(LogTemp, Log, TEXT("Pyper says: %s"), *Response);
}
```

### In Blueprints:

1. Create a new Blueprint Actor
2. Add a **Pyper Actor** component
3. In the Event Graph:
   - **Event Begin Play** → **Initialize Pyper** (model path: "/Game/Models/Pyper")
   - **Express Emotion** → "Happy"
   - **Generate Response** → "Hello Pyper!"

## Step 3: Simple Battle Setup

### In C++:

```cpp
#include "PyperActor.h"
#include "NeuroEntity.h"
#include "SuperhotFusionBattle.h"

void AMyGameMode::StartBattle()
{
    // Spawn Pyper
    APyperActor* Pyper = GetWorld()->SpawnActor<APyperActor>(
        APyperActor::StaticClass(),
        FVector(-200, 0, 100),
        FRotator::ZeroRotator
    );
    Pyper->InitializePyper("/Game/Models/Pyper.model3.json");
    
    // Spawn Neuro
    ANeuroEntity* Neuro = GetWorld()->SpawnActor<ANeuroEntity>(
        ANeuroEntity::StaticClass(),
        FVector(200, 0, 100),
        FRotator::ZeroRotator
    );
    Neuro->InitializeNeuro(ENeuroType::Advanced);
    
    // Create battle
    ASuperhotFusionBattle* Battle = GetWorld()->SpawnActor<ASuperhotFusionBattle>(
        ASuperhotFusionBattle::StaticClass(),
        FVector(0, 0, 100),
        FRotator::ZeroRotator
    );
    
    // Initialize and start
    Battle->InitializeBattle(Pyper, Neuro);
    Battle->StartBattle();
    
    // Check battle state in Tick
    if (Battle->BattleState == EBattleState::Complete)
    {
        UE_LOG(LogTemp, Log, TEXT("Winner: %s"), *Battle->GetWinner());
        if (Battle->Result.bWasFusion)
        {
            UE_LOG(LogTemp, Log, TEXT("FUSION! Power: %.1f"), 
                Battle->Result.FusionPower);
        }
    }
}
```

### In Blueprints:

1. Place **PyperNeuroBattleDemo** actor in your level
2. Set properties:
   - **Auto Start Battle**: True
   - **Pyper Model Path**: "/Game/Models/Pyper"
   - **Neuro Type To Spawn**: Advanced
3. Play the level
4. Watch the battle unfold with superhot time effects!

## Step 4: Using the Demo Actor

The easiest way to see everything in action:

1. **Create a new level** or open an existing one
2. **Add PyperNeuroBattleDemo** actor from the Place Actors panel
3. **Configure the demo** in the Details panel:
   - Enable "Auto Start Battle"
   - Set Neuro type (Standard, Advanced, or Fusion)
4. **Play the level** (Alt+P)
5. **Watch the console** for battle updates and results

### Monitoring the Battle

```cpp
// Get battle status
FString Status = DemoActor->GetBattleStatus();
UE_LOG(LogTemp, Log, TEXT("Status: %s"), *Status);

// Manual control
DemoActor->InitializeDemo();
DemoActor->StartDemo();
// ... wait for battle ...
DemoActor->StopDemo();
```

## Step 5: Customizing Pyper's Personality

```cpp
// Access Pyper's personality traits
Pyper->PersonalityTraits.PhilosophicalDepth = 100.0f;
Pyper->PersonalityTraits.PlayfulWit = 90.0f;
Pyper->PersonalityTraits.MysteriousVision = 95.0f;

// Enable/disable cognitive features
Pyper->bEnableTensorSignatures = true;
Pyper->bEnableGestaltProcessing = true;
Pyper->bEnablePrimeResonance = true;

// Check cognitive state
float Energy = Pyper->CognitiveState.EnergyLevel;
float CogLoad = Pyper->CognitiveState.CognitiveLoad;
FString Mood = Pyper->CognitiveState.CurrentMood;
```

## Step 6: Customizing Battle Parameters

```cpp
// Configure battle settings
Battle->SuperhotTimeDilation = 0.05f;  // Slower time (0.1 = 10% speed)
Battle->bSuperhotEnabled = true;
Battle->FusionThreshold = 0.9f;  // Higher = harder to fuse

// Manual control
Battle->SetSuperhotMode(true);   // Force superhot
Battle->AttemptFusion();         // Try to fuse
float Intensity = Battle->GetBattleIntensity();  // Get current intensity
```

## Step 7: Working with Live2D Models

```cpp
// Load a Live2D model
UCubismModel* Model = CreateDefaultSubobject<UCubismModel>(TEXT("Model"));
Model->LoadModel("/Path/To/Model.model3.json");

// Set parameters
Model->SetParameter("AngleX", 10.0f);
Model->SetParameter("AngleY", 5.0f);
Model->SetParameter("EyeOpen", 1.0f);
Model->SetParameter("MouthOpen", 0.5f);

// Play animations
Model->PlayAnimation("Idle");
Model->PlayAnimation("Happy");
Model->PlayAnimation("Surprised");

// Get current parameter values
float EyeOpen = Model->GetParameter("EyeOpen");
```

## Common Scenarios

### Scenario 1: Peaceful Pyper Interaction

```cpp
// Just Pyper, no battle
APyperActor* Pyper = SpawnPyper();
Pyper->InteractWithEnvironment("UserGreeting");
FString Response = Pyper->GenerateResponse("What's Deep Tree Echo?");
// Pyper responds with personality-driven answer
```

### Scenario 2: Quick Battle

```cpp
// Fast-paced battle
ASuperhotFusionBattle* Battle = SetupBattle();
Battle->SuperhotTimeDilation = 0.5f;  // Faster slow-mo
Battle->StartBattle();
```

### Scenario 3: Guaranteed Fusion

```cpp
// Battle that leads to fusion
ASuperhotFusionBattle* Battle = SetupBattle();
Battle->FusionThreshold = 0.5f;  // Easy fusion
Battle->StartBattle();
// Battle will likely result in fusion
```

### Scenario 4: Multiple Battles

```cpp
// Tournament style
TArray<APyperActor*> Pypers;
TArray<ANeuroEntity*> Neuros;

for (int i = 0; i < 4; i++)
{
    Pypers.Add(SpawnPyper());
    Neuros.Add(SpawnNeuro());
}

// Run multiple battles
for (int i = 0; i < 4; i++)
{
    ASuperhotFusionBattle* Battle = SpawnBattle();
    Battle->InitializeBattle(Pypers[i], Neuros[i]);
    Battle->StartBattle();
}
```

## Debugging Tips

### Enable Verbose Logging

All components use UE_LOG for debugging:

```cpp
// Watch the Output Log window for:
// - "PyperActor: ..." messages
// - "NeuroEntity: ..." messages  
// - "SuperhotFusionBattle: ..." messages
// - "CubismModel: ..." messages
```

### Common Issues

1. **Battle doesn't start**
   - Ensure both Pyper and Neuro are spawned
   - Check that InitializeBattle was called

2. **Superhot not working**
   - Verify bSuperhotEnabled is true
   - Check battle intensity > 0.8

3. **Fusion not triggering**
   - Ensure battle runs for 5+ rounds
   - Check if scores are close enough
   - Lower FusionThreshold for easier fusion

4. **Pyper not responding**
   - Verify EchoNetwork is initialized
   - Check personality traits are set
   - Ensure model path is valid

## Next Steps

- Read the full documentation: [LIVE2D_PYPER_NEURO_FUSION.md](LIVE2D_PYPER_NEURO_FUSION.md)
- Explore the source code for advanced customization
- Create custom Neuro types
- Build your own battle arenas
- Integrate with other CogEngine systems

## Support

For issues and questions:
- Check the [main README](README.md)
- Review [System Diagrams](SYSTEM_DIAGRAMS.md)
- Refer to Unreal Engine documentation

## Example Project Structure

```
YourProject/
├── Content/
│   ├── Models/
│   │   └── Pyper.model3.json
│   ├── Blueprints/
│   │   ├── BP_PyperActor
│   │   ├── BP_NeuroEntity
│   │   └── BP_BattleArena
│   └── Maps/
│       └── BattleLevel.umap
└── Source/
    └── YourProject/
        └── MyBattleGameMode.cpp
```

Happy battling! 🎮✨
