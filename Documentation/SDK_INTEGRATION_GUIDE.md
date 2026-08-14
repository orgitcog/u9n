# Live2D Cubism SDK Integration Guide

**Document ID**: DTE-SDK-IG-001  
**Version**: 1.0  
**Date**: December 13, 2025  
**For**: Track A Development Team

## 1. Overview

This guide provides step-by-step instructions for integrating the Live2D Cubism SDK for Native into the UnrealEngineCog project. It is designed for the senior C++ developer assigned to Track A and covers all phases from SDK acquisition through final testing.

## 2. Prerequisites

Before beginning the integration, ensure you have the following:

- Unreal Engine 5.3+ installed and configured
- Visual Studio 2022 (Windows) or Xcode (macOS) or appropriate Linux development tools
- Git access to the UnrealEngineCog repository
- Administrative access to acquire SDK licenses
- Understanding of Unreal Engine's build system (UBT)
- Experience with graphics programming and rendering pipelines

## 3. Phase A1: SDK Acquisition and Setup (Days 1-3)

### 3.1 SDK License Acquisition (Day 1)

The Live2D Cubism SDK for Native is required for this integration. The SDK is available from Live2D Inc.

**Steps**:

1. Visit the Live2D Cubism SDK download page: https://www.live2d.com/en/download/cubism-sdk/
2. Select "Cubism SDK for Native" (not Unity or Web)
3. Review the license terms carefully. For commercial use, you will need to purchase a license.
4. Download the SDK package for your development platform (Windows/macOS/Linux)
5. Extract the SDK to a temporary location for review

**SDK Structure**:
```
CubismSdkForNative-X.X.X/
├── Core/                    # Core SDK binaries and headers
│   ├── dll/                 # Platform-specific libraries
│   └── include/             # C++ header files
├── Framework/               # High-level C++ framework
│   ├── src/                 # Framework source code
│   └── include/             # Framework headers
├── Samples/                 # Example applications
└── Docs/                    # API documentation
```

**Deliverable**: SDK downloaded and license acquired

### 3.2 SDK Integration into Project Structure (Day 2)

Integrate the SDK files into the UnrealEngineCog project structure.

**Steps**:

1. Create the ThirdParty directory structure:
```bash
cd /path/to/UnrealEngineCog
mkdir -p Source/Live2DCubism/ThirdParty/CubismSDK/Core
mkdir -p Source/Live2DCubism/ThirdParty/CubismSDK/Framework
```

2. Copy SDK files:
```bash
# Copy Core headers
cp -r /path/to/CubismSdkForNative/Core/include/* \
      Source/Live2DCubism/ThirdParty/CubismSDK/Core/include/

# Copy Core libraries
cp -r /path/to/CubismSdkForNative/Core/dll/* \
      Source/Live2DCubism/ThirdParty/CubismSDK/Core/lib/

# Copy Framework source
cp -r /path/to/CubismSdkForNative/Framework/src/* \
      Source/Live2DCubism/ThirdParty/CubismSDK/Framework/src/

# Copy Framework headers
cp -r /path/to/CubismSdkForNative/Framework/include/* \
      Source/Live2DCubism/ThirdParty/CubismSDK/Framework/include/
```

**Deliverable**: SDK files integrated into project structure

### 3.3 Build System Configuration (Day 3)

Configure Unreal Build Tool (UBT) to compile and link the SDK.

**Edit `Source/Live2DCubism/Live2DCubism.Build.cs`**:

```csharp
using UnrealBuildTool;
using System.IO;

public class Live2DCubism : ModuleRules
{
    public Live2DCubism(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[] 
        { 
            "Core", 
            "CoreUObject", 
            "Engine",
            "RenderCore",
            "RHI"
        });

        PrivateDependencyModuleNames.AddRange(new string[] 
        { 
            "Slate", 
            "SlateCore" 
        });

        // Add Cubism SDK include paths
        string CubismSDKPath = Path.Combine(ModuleDirectory, "ThirdParty", "CubismSDK");
        string CubismCorePath = Path.Combine(CubismSDKPath, "Core");
        string CubismFrameworkPath = Path.Combine(CubismSDKPath, "Framework");

        PublicIncludePaths.Add(Path.Combine(CubismCorePath, "include"));
        PublicIncludePaths.Add(Path.Combine(CubismFrameworkPath, "src"));

        // Add Cubism SDK library paths
        string LibPath = Path.Combine(CubismCorePath, "lib");

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            PublicAdditionalLibraries.Add(Path.Combine(LibPath, "windows", "x86_64", "Live2DCubismCore.lib"));
            RuntimeDependencies.Add(Path.Combine(LibPath, "windows", "x86_64", "Live2DCubismCore.dll"));
        }
        else if (Target.Platform == UnrealTargetPlatform.Mac)
        {
            PublicAdditionalLibraries.Add(Path.Combine(LibPath, "macos", "libLive2DCubismCore.a"));
        }
        else if (Target.Platform == UnrealTargetPlatform.Linux)
        {
            PublicAdditionalLibraries.Add(Path.Combine(LibPath, "linux", "x86_64", "libLive2DCubismCore.so"));
        }

        // Define preprocessor macros
        PublicDefinitions.Add("CSM_CORE_WIN32_DLL=1");
    }
}
```

**Test Build**:
```bash
# Regenerate project files
./GenerateProjectFiles.sh  # or .bat on Windows

# Build the project
# In Visual Studio: Build > Build Solution
# Or via command line:
UnrealBuildTool.exe UnrealEngine Win64 Development -Project="path/to/UnrealEngineCog.uproject"
```

**Deliverable**: Project compiles successfully with SDK integrated

## 4. Phase A2: Core SDK Integration (Days 4-8)

### 4.1 Framework Initialization (Day 4)

Replace the placeholder implementation in `CubismSDKIntegration.cpp`.

**Location**: `Source/Live2DCubism/CubismSDKIntegration.cpp`

**Implementation**:

```cpp
#include "CubismSDKIntegration.h"
#include "CubismFramework.hpp"
#include "ICubismAllocator.hpp"
#include "Model/CubismUserModel.hpp"

using namespace Live2D::Cubism::Framework;

// Custom allocator for Cubism SDK
class CubismUnrealAllocator : public Csm::ICubismAllocator
{
public:
    void* Allocate(const Csm::csmSizeType size) override
    {
        return FMemory::Malloc(size);
    }

    void Deallocate(void* memory) override
    {
        FMemory::Free(memory);
    }

    void* AllocateAligned(const Csm::csmSizeType size, const Csm::csmUint32 alignment) override
    {
        return FMemory::Malloc(size, alignment);
    }

    void DeallocateAligned(void* alignedMemory) override
    {
        FMemory::Free(alignedMemory);
    }
};

static CubismUnrealAllocator g_CubismAllocator;

bool UCubismSDKIntegration::InitializeCubismSDK()
{
    // Initialize Cubism Framework
    CubismFramework::StartUp(&g_CubismAllocator);

    // Initialize Cubism Framework with options
    CubismFramework::Initialize();

    bIsInitialized = true;

    UE_LOG(LogTemp, Log, TEXT("Cubism SDK initialized successfully"));
    return true;
}

void UCubismSDKIntegration::ShutdownCubismSDK()
{
    if (bIsInitialized)
    {
        CubismFramework::Dispose();
        bIsInitialized = false;
        UE_LOG(LogTemp, Log, TEXT("Cubism SDK shut down"));
    }
}
```

**Test**: Create a simple test that initializes and shuts down the SDK.

**Deliverable**: SDK initialization working

### 4.2 Model Loading System (Days 5-6)

Implement the model loading functionality.

**Implementation**:

```cpp
#include "Model/CubismMoc.hpp"
#include "Model/CubismModel.hpp"
#include "Utils/CubismJson.hpp"

bool UCubismSDKIntegration::LoadCubismModel(const FString& ModelPath)
{
    // Convert Unreal path to platform path
    FString FullPath = FPaths::ProjectContentDir() + ModelPath;
    
    // Load .model3.json file
    TArray<uint8> JsonData;
    if (!FFileHelper::LoadFileToArray(JsonData, *FullPath))
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to load model file: %s"), *FullPath);
        return false;
    }

    // Parse JSON
    Csm::CubismJson* modelJson = Csm::CubismJson::Create((const Csm::csmByte*)JsonData.GetData(), JsonData.Num());

    // Get model file path from JSON
    FString ModelDir = FPaths::GetPath(FullPath);
    FString MocFileName = FString(modelJson->GetString("FileReferences.Moc"));
    FString MocFilePath = ModelDir / MocFileName;

    // Load .moc3 file
    TArray<uint8> MocData;
    if (!FFileHelper::LoadFileToArray(MocData, *MocFilePath))
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to load moc file: %s"), *MocFilePath);
        Csm::CubismJson::Delete(modelJson);
        return false;
    }

    // Create Moc
    Csm::CubismMoc* moc = Csm::CubismMoc::Create(MocData.GetData(), MocData.Num());
    if (!moc)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to create Cubism Moc"));
        Csm::CubismJson::Delete(modelJson);
        return false;
    }

    // Create Model from Moc
    CurrentModel = moc->CreateModel();
    if (!CurrentModel)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to create Cubism Model"));
        Csm::CubismMoc::Delete(moc);
        Csm::CubismJson::Delete(modelJson);
        return false;
    }

    // Load textures
    LoadModelTextures(modelJson, ModelDir);

    // Cleanup
    Csm::CubismJson::Delete(modelJson);

    UE_LOG(LogTemp, Log, TEXT("Cubism model loaded successfully: %s"), *ModelPath);
    return true;
}

void UCubismSDKIntegration::LoadModelTextures(Csm::CubismJson* modelJson, const FString& ModelDir)
{
    // Get texture count
    int32 TextureCount = modelJson->GetArraySize("FileReferences.Textures");

    for (int32 i = 0; i < TextureCount; i++)
    {
        FString TextureFileName = FString(modelJson->GetString("FileReferences.Textures", i));
        FString TexturePath = ModelDir / TextureFileName;

        // Load texture using Unreal's texture loading
        UTexture2D* Texture = FImageUtils::ImportFileAsTexture2D(TexturePath);
        if (Texture)
        {
            ModelTextures.Add(Texture);
            UE_LOG(LogTemp, Log, TEXT("Loaded texture: %s"), *TexturePath);
        }
    }
}
```

**Deliverable**: Model loading from .model3.json files working

### 4.3 Parameter System (Days 7-8)

Implement parameter control.

**Implementation**:

```cpp
void UCubismSDKIntegration::SetParameter(const FString& ParameterID, float Value)
{
    if (!CurrentModel)
    {
        UE_LOG(LogTemp, Warning, TEXT("No model loaded"));
        return;
    }

    // Convert FString to const char*
    std::string ParamIdStr(TCHAR_TO_UTF8(*ParameterID));
    const Csm::csmChar* ParamId = ParamIdStr.c_str();

    // Find parameter index
    int32 ParamIndex = CurrentModel->GetParameterIndex(ParamId);
    if (ParamIndex < 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("Parameter not found: %s"), *ParameterID);
        return;
    }

    // Set parameter value
    CurrentModel->SetParameterValue(ParamIndex, Value);

    // Cache the value
    ParameterCache.Add(ParameterID, Value);
}

float UCubismSDKIntegration::GetParameter(const FString& ParameterID) const
{
    if (const float* CachedValue = ParameterCache.Find(ParameterID))
    {
        return *CachedValue;
    }
    return 0.0f;
}
```

**Deliverable**: Parameter system working with caching

## 5. Phase A3: Rendering Pipeline (Days 9-13)

### 5.1 Render Target Setup (Days 9-10)

Create the rendering infrastructure.

**Implementation**:

```cpp
void UCubismSDKIntegration::CreateRenderTarget(int32 Width, int32 Height)
{
    // Create render target
    RenderTarget = NewObject<UTextureRenderTarget2D>();
    RenderTarget->InitAutoFormat(Width, Height);
    RenderTarget->UpdateResourceImmediate(true);

    UE_LOG(LogTemp, Log, TEXT("Render target created: %dx%d"), Width, Height);
}
```

### 5.2 Drawing Implementation (Days 11-12)

Implement the actual rendering.

**Implementation**:

```cpp
void UCubismSDKIntegration::UpdateAndRender(float DeltaTime)
{
    if (!CurrentModel || !RenderTarget)
    {
        return;
    }

    // Update model
    CurrentModel->Update();

    // Get render command
    ENQUEUE_RENDER_COMMAND(CubismRenderCommand)(
        [this](FRHICommandListImmediate& RHICmdList)
        {
            this->RenderModel_RenderThread(RHICmdList);
        }
    );
}

void UCubismSDKIntegration::RenderModel_RenderThread(FRHICommandListImmediate& RHICmdList)
{
    // Set up rendering state
    FRHIRenderPassInfo RPInfo(RenderTarget->GetRenderTargetResource()->GetRenderTargetTexture(), ERenderTargetActions::Clear_Store);
    RHICmdList.BeginRenderPass(RPInfo, TEXT("CubismRender"));

    // Draw model using Cubism Renderer
    DrawCubismModel(RHICmdList);

    RHICmdList.EndRenderPass();
}
```

**Deliverable**: Model rendering to texture

## 6. Phase A4: Physics and Animation (Days 14-16)

### 6.1 Physics Integration (Day 14)

Implement physics simulation for hair and accessories.

**Implementation**:

```cpp
void UCubismSDKIntegration::UpdatePhysics(float DeltaTime, FVector2D Gravity, FVector2D Wind)
{
    if (!CurrentModel)
    {
        return;
    }

    // Update physics parameters
    SetParameter(TEXT("ParamPhysicsGravityX"), Gravity.X);
    SetParameter(TEXT("ParamPhysicsGravityY"), Gravity.Y);
    SetParameter(TEXT("ParamPhysicsWindX"), Wind.X);
    SetParameter(TEXT("ParamPhysicsWindY"), Wind.Y);
}
```

**Deliverable**: Physics simulation working

### 6.2 Component Integration (Day 16)

Connect to Avatar3DComponentEnhanced.

**Implementation**: Update `Avatar3DComponentEnhanced.cpp` to call SDK methods.

**Deliverable**: Full integration with existing components

## 7. Phase A5: Testing and Polish (Days 17-21)

### 7.1 Unit Testing (Days 17-18)

Create comprehensive tests.

**Test File**: `Source/Tests/CubismSDKTests.cpp`

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCubismSDKInitTest, 
    "UnrealEngineCog.Cubism.Initialization", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCubismSDKInitTest::RunTest(const FString& Parameters)
{
    UCubismSDKIntegration* SDK = NewObject<UCubismSDKIntegration>();
    
    TestNotNull(TEXT("SDK object created"), SDK);
    
    bool bInitSuccess = SDK->InitializeCubismSDK();
    TestTrue(TEXT("SDK initialized"), bInitSuccess);
    
    SDK->ShutdownCubismSDK();
    
    return true;
}
```

**Deliverable**: Comprehensive test suite

### 7.2 Documentation (Days 20-21)

Document the integration architecture and create usage examples.

**Deliverable**: Complete documentation

## 8. Common Issues and Troubleshooting

### Issue: Build Errors with SDK Headers

**Solution**: Ensure all include paths are correctly set in the .Build.cs file. Check that SDK version matches Unreal Engine requirements.

### Issue: Linking Errors

**Solution**: Verify that the correct library files are being linked for your platform. Check that DLL files are in the correct runtime directory.

### Issue: Model Not Rendering

**Solution**: Verify that the render target is correctly created and that the rendering thread command is being enqueued properly. Check that model textures are loaded.

### Issue: Parameters Not Updating

**Solution**: Ensure that `CurrentModel->Update()` is being called before rendering. Verify parameter IDs match those in the model.

## 9. Performance Optimization Tips

- Cache parameter indices instead of looking them up every frame
- Use render target pooling to avoid creating new textures
- Implement frustum culling for off-screen models
- Profile using Unreal's built-in profiling tools
- Consider using instancing for multiple models

## 10. Next Steps After Integration

Once the SDK integration is complete:

1. Test with actual Live2D models from Track B
2. Integrate with the personality trait system
3. Connect to the neurochemical simulation
4. Implement the diary-insight-blog narrative loop visualization
5. Optimize performance for target platforms

## 11. Resources

- Live2D Cubism SDK Documentation: https://docs.live2d.com/
- Unreal Engine Build System Documentation: https://docs.unrealengine.com/en-US/ProductionPipelines/BuildTools/
- Unreal Engine Rendering Documentation: https://docs.unrealengine.com/en-US/ProgrammingAndScripting/Rendering/

---

**Document Status**: Ready for Implementation  
**Last Updated**: December 13, 2025
