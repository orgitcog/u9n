// Avatar Material Manager - Production Implementation
// Deep Tree Echo AGI Avatar System

#include "AvatarMaterialManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Texture.h"

UAvatarMaterialManager::UAvatarMaterialManager()
{
    OwnerMesh = nullptr;
}

void UAvatarMaterialManager::Initialize(USkeletalMeshComponent* SkeletalMesh)
{
    if (!SkeletalMesh)
    {
        UE_LOG(LogTemp, Error, TEXT("AvatarMaterialManager: Cannot initialize with null skeletal mesh"));
        return;
    }

    OwnerMesh = SkeletalMesh;
    
    // Create dynamic material instances for all materials
    CreateDynamicMaterialInstances();
    
    // Map material indices to slots
    MapMaterialSlots();
    
    UE_LOG(LogTemp, Log, TEXT("AvatarMaterialManager: Initialized with %d material instances"), 
           MaterialInstances.Num());
}

void UAvatarMaterialManager::CreateDynamicMaterialInstances()
{
    if (!OwnerMesh)
    {
        return;
    }

    MaterialInstances.Empty();

    // Get all materials from the skeletal mesh
    TArray<UMaterialInterface*> Materials = OwnerMesh->GetMaterials();
    
    for (int32 i = 0; i < Materials.Num(); ++i)
    {
        UMaterialInterface* Material = Materials[i];
        if (Material)
        {
            // Create dynamic material instance
            UMaterialInstanceDynamic* DMI = OwnerMesh->CreateAndSetMaterialInstanceDynamic(i);
            
            if (DMI)
            {
                // Identify which slot this material belongs to
                EAvatarMaterialSlot Slot = IdentifyMaterialSlot(Material);
                MaterialInstances.Add(Slot, DMI);
                SlotMapping.Add(i, Slot);
                
                UE_LOG(LogTemp, Log, TEXT("Created DMI for slot %d: %s"), 
                       (int32)Slot, *Material->GetName());
            }
        }
    }
}

void UAvatarMaterialManager::MapMaterialSlots()
{
    // Material slot mapping is already done in CreateDynamicMaterialInstances
    // This function can be used for additional custom mapping if needed
}

EAvatarMaterialSlot UAvatarMaterialManager::IdentifyMaterialSlot(UMaterialInterface* Material) const
{
    if (!Material)
    {
        return EAvatarMaterialSlot::Skin;
    }

    FString MaterialName = Material->GetName().ToLower();

    // Identify material type based on naming convention
    if (MaterialName.Contains(TEXT("skin")) || MaterialName.Contains(TEXT("body")))
    {
        return EAvatarMaterialSlot::Skin;
    }
    else if (MaterialName.Contains(TEXT("hair")))
    {
        return EAvatarMaterialSlot::Hair;
    }
    else if (MaterialName.Contains(TEXT("eye")))
    {
        return EAvatarMaterialSlot::Eyes;
    }
    else if (MaterialName.Contains(TEXT("outfit")) || MaterialName.Contains(TEXT("cloth")))
    {
        return EAvatarMaterialSlot::Outfit;
    }
    else if (MaterialName.Contains(TEXT("accessory")) || MaterialName.Contains(TEXT("jewelry")))
    {
        return EAvatarMaterialSlot::Accessories;
    }
    else if (MaterialName.Contains(TEXT("teeth")))
    {
        return EAvatarMaterialSlot::Teeth;
    }
    else if (MaterialName.Contains(TEXT("tongue")))
    {
        return EAvatarMaterialSlot::Tongue;
    }

    // Default to skin
    return EAvatarMaterialSlot::Skin;
}

void UAvatarMaterialManager::UpdateMaterialParameters(const FAvatarMaterialParameters& Parameters)
{
    CurrentParameters = Parameters;

    UpdateSkinParameters();
    UpdateHairParameters();
    UpdateEyeParameters();
    UpdateGlobalEffects();
}

void UAvatarMaterialManager::UpdateSkinParameters()
{
    UMaterialInstanceDynamic* SkinMaterial = GetMaterialInstance(EAvatarMaterialSlot::Skin);
    if (!SkinMaterial)
    {
        return;
    }

    // Set skin parameters
    SkinMaterial->SetScalarParameterValue(TEXT("Smoothness"), CurrentParameters.SkinSmoothness);
    SkinMaterial->SetScalarParameterValue(TEXT("Subsurface"), CurrentParameters.SkinSubsurface);
    SkinMaterial->SetScalarParameterValue(TEXT("Translucency"), CurrentParameters.SkinTranslucency);
    SkinMaterial->SetScalarParameterValue(TEXT("BlushIntensity"), CurrentParameters.BlushIntensity);
    SkinMaterial->SetVectorParameterValue(TEXT("BlushColor"), CurrentParameters.BlushColor);
    
    // Global effects on skin
    SkinMaterial->SetScalarParameterValue(TEXT("AuraIntensity"), CurrentParameters.AuraIntensity);
    SkinMaterial->SetVectorParameterValue(TEXT("AuraColor"), CurrentParameters.AuraColor);
    SkinMaterial->SetScalarParameterValue(TEXT("GlitchIntensity"), CurrentParameters.GlitchIntensity);
}

void UAvatarMaterialManager::UpdateHairParameters()
{
    UMaterialInstanceDynamic* HairMaterial = GetMaterialInstance(EAvatarMaterialSlot::Hair);
    if (!HairMaterial)
    {
        return;
    }

    // Set hair parameters
    HairMaterial->SetScalarParameterValue(TEXT("Anisotropy"), CurrentParameters.HairAnisotropy);
    HairMaterial->SetScalarParameterValue(TEXT("ShimmerIntensity"), CurrentParameters.HairShimmerIntensity);
    HairMaterial->SetScalarParameterValue(TEXT("ShimmerSpeed"), CurrentParameters.HairShimmerSpeed);
    HairMaterial->SetScalarParameterValue(TEXT("ChaosColorShift"), CurrentParameters.ChaosColorShift);
    
    // Echo resonance glow
    HairMaterial->SetScalarParameterValue(TEXT("EchoGlow"), CurrentParameters.EchoResonance);
}

void UAvatarMaterialManager::UpdateEyeParameters()
{
    UMaterialInstanceDynamic* EyeMaterial = GetMaterialInstance(EAvatarMaterialSlot::Eyes);
    if (!EyeMaterial)
    {
        return;
    }

    // Set eye parameters
    EyeMaterial->SetScalarParameterValue(TEXT("PupilDilation"), CurrentParameters.PupilDilation);
    EyeMaterial->SetScalarParameterValue(TEXT("SparkleIntensity"), CurrentParameters.EyeSparkleIntensity);
    EyeMaterial->SetScalarParameterValue(TEXT("Moisture"), CurrentParameters.EyeMoisture);
    
    // Sparkle position (using individual X and Y parameters)
    EyeMaterial->SetScalarParameterValue(TEXT("SparklePositionX"), CurrentParameters.SparklePosition.X);
    EyeMaterial->SetScalarParameterValue(TEXT("SparklePositionY"), CurrentParameters.SparklePosition.Y);
}

void UAvatarMaterialManager::UpdateGlobalEffects()
{
    // Apply global effects to all materials
    for (auto& Pair : MaterialInstances)
    {
        UMaterialInstanceDynamic* DMI = Pair.Value;
        if (DMI)
        {
            DMI->SetScalarParameterValue(TEXT("GlobalGlitchIntensity"), CurrentParameters.GlitchIntensity);
            DMI->SetScalarParameterValue(TEXT("CognitiveLoad"), CurrentParameters.CognitiveLoad);
        }
    }
}

void UAvatarMaterialManager::SetScalarParameter(EAvatarMaterialSlot Slot, FName ParameterName, float Value)
{
    UMaterialInstanceDynamic* DMI = GetMaterialInstance(Slot);
    if (DMI)
    {
        DMI->SetScalarParameterValue(ParameterName, Value);
    }
}

void UAvatarMaterialManager::SetVectorParameter(EAvatarMaterialSlot Slot, FName ParameterName, FLinearColor Value)
{
    UMaterialInstanceDynamic* DMI = GetMaterialInstance(Slot);
    if (DMI)
    {
        DMI->SetVectorParameterValue(ParameterName, Value);
    }
}

void UAvatarMaterialManager::SetTextureParameter(EAvatarMaterialSlot Slot, FName ParameterName, UTexture* Texture)
{
    UMaterialInstanceDynamic* DMI = GetMaterialInstance(Slot);
    if (DMI)
    {
        DMI->SetTextureParameterValue(ParameterName, Texture);
    }
}

UMaterialInstanceDynamic* UAvatarMaterialManager::GetMaterialInstance(EAvatarMaterialSlot Slot) const
{
    const UMaterialInstanceDynamic* const* FoundDMI = MaterialInstances.Find(Slot);
    return FoundDMI ? *FoundDMI : nullptr;
}

void UAvatarMaterialManager::BatchUpdateParameters(const TArray<FName>& ParameterNames, const TArray<float>& Values)
{
    if (ParameterNames.Num() != Values.Num())
    {
        UE_LOG(LogTemp, Warning, TEXT("BatchUpdateParameters: Parameter count mismatch"));
        return;
    }

    // Apply to all materials
    for (auto& Pair : MaterialInstances)
    {
        UMaterialInstanceDynamic* DMI = Pair.Value;
        if (DMI)
        {
            for (int32 i = 0; i < ParameterNames.Num(); ++i)
            {
                DMI->SetScalarParameterValue(ParameterNames[i], Values[i]);
            }
        }
    }
}

void UAvatarMaterialManager::ApplyEmotionalBlush(float Intensity, const FLinearColor& Color)
{
    CurrentParameters.BlushIntensity = FMath::Clamp(Intensity, 0.0f, 1.0f);
    CurrentParameters.BlushColor = Color;
    UpdateSkinParameters();
}

void UAvatarMaterialManager::SetPupilDilation(float Dilation)
{
    CurrentParameters.PupilDilation = FMath::Clamp(Dilation, 0.0f, 1.0f);
    UpdateEyeParameters();
}

void UAvatarMaterialManager::EnableSuperHotGirlAesthetic(bool bEnable)
{
    if (bEnable)
    {
        // Enhance eye sparkle
        CurrentParameters.EyeSparkleIntensity = 0.9f;
        
        // Add subtle blush
        CurrentParameters.BlushIntensity = 0.3f;
        CurrentParameters.BlushColor = FLinearColor(1.0f, 0.6f, 0.6f, 1.0f);
        
        // Increase hair shimmer
        CurrentParameters.HairShimmerIntensity = 0.7f;
        
        // Smooth skin
        CurrentParameters.SkinSmoothness = 0.8f;
        
        // Update all parameters
        UpdateMaterialParameters(CurrentParameters);
        
        UE_LOG(LogTemp, Log, TEXT("Super-hot-girl aesthetic enabled"));
    }
    else
    {
        // Reset to defaults
        CurrentParameters.EyeSparkleIntensity = 0.5f;
        CurrentParameters.BlushIntensity = 0.0f;
        CurrentParameters.HairShimmerIntensity = 0.5f;
        CurrentParameters.SkinSmoothness = 0.6f;
        
        UpdateMaterialParameters(CurrentParameters);
        
        UE_LOG(LogTemp, Log, TEXT("Super-hot-girl aesthetic disabled"));
    }
}

void UAvatarMaterialManager::SetEyeSparkle(float Intensity, const FVector2D& Position)
{
    CurrentParameters.EyeSparkleIntensity = FMath::Clamp(Intensity, 0.0f, 1.0f);
    CurrentParameters.SparklePosition = Position;
    UpdateEyeParameters();
}

void UAvatarMaterialManager::SetHairShimmer(float Intensity, float Speed)
{
    CurrentParameters.HairShimmerIntensity = FMath::Clamp(Intensity, 0.0f, 1.0f);
    CurrentParameters.HairShimmerSpeed = FMath::Max(Speed, 0.0f);
    UpdateHairParameters();
}

void UAvatarMaterialManager::ApplyGlitchEffect(float Intensity)
{
    CurrentParameters.GlitchIntensity = FMath::Clamp(Intensity, 0.0f, 1.0f);
    UpdateGlobalEffects();
}

void UAvatarMaterialManager::SetChaosColorShift(float Shift)
{
    CurrentParameters.ChaosColorShift = FMath::Clamp(Shift, 0.0f, 1.0f);
    UpdateHairParameters();
}

void UAvatarMaterialManager::SetEchoResonance(float Resonance, const FLinearColor& Color)
{
    CurrentParameters.EchoResonance = FMath::Clamp(Resonance, 0.0f, 1.0f);
    
    // Apply echo resonance to hair as glow
    UpdateHairParameters();
    
    // Also apply as aura
    SetEmotionalAura(Resonance * 0.5f, Color);
}

void UAvatarMaterialManager::SetCognitiveLoadVisualization(float Load)
{
    CurrentParameters.CognitiveLoad = FMath::Clamp(Load, 0.0f, 1.0f);
    UpdateGlobalEffects();
}

void UAvatarMaterialManager::SetEmotionalAura(float Intensity, const FLinearColor& Color)
{
    CurrentParameters.AuraIntensity = FMath::Clamp(Intensity, 0.0f, 1.0f);
    CurrentParameters.AuraColor = Color;
    UpdateSkinParameters();
}
