# Deep Tree Echo Avatar - Outfit & Accessory System

**Asset Type:** Material System and Outfit Manager  
**Purpose:** Dynamic outfit and accessory customization  
**Integration:** Controlled by Avatar3DComponent and AvatarMaterialManager

---

## Overview

The outfit and accessory system allows for dynamic customization of the avatar's appearance. It includes a library of clothing items, accessories, and materials that can be mixed and matched to create unique looks. The system is designed to be extensible, allowing for new outfits and accessories to be added easily.

---

## System Architecture

### Component Hierarchy

```
UOutfitManagerComponent (Main Controller)
├── Outfit Library (Data Asset)
│   ├── Outfit Sets (e.g., "Casual", "Formal", "Cyberpunk")
│   └── Individual Items (e.g., "T-Shirt", "Jeans", "Jacket")
├── Accessory Library (Data Asset)
│   ├── Glasses
│   ├── Jewelry (earrings, necklaces)
│   ├── Hats
│   └── Bags
└── Material Library (Data Asset)
    ├── Fabrics (cotton, leather, silk)
    ├── Metals (gold, silver, chrome)
    └── Plastics (matte, glossy)
```

### Outfit Manager Component

```cpp
// OutfitManagerComponent.h
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OutfitManagerComponent.generated.h"

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UNREALENGINE_API UOutfitManagerComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UOutfitManagerComponent();

    UFUNCTION(BlueprintCallable, Category = "Outfit")
    void SetOutfit(FName OutfitSetName);

    UFUNCTION(BlueprintCallable, Category = "Outfit")
    void SetClothingItem(EClothingSlot Slot, FName ItemName);

    UFUNCTION(BlueprintCallable, Category = "Outfit")
    void SetAccessory(EAccessorySlot Slot, FName AccessoryName);

    UFUNCTION(BlueprintCallable, Category = "Outfit")
    void SetMaterial(EClothingSlot Slot, UMaterialInterface* Material);

protected:
    UPROPERTY(EditAnywhere, Category = "Outfit")
    UDataAsset* OutfitLibrary;

    UPROPERTY(EditAnywhere, Category = "Outfit")
    UDataAsset* AccessoryLibrary;

    UPROPERTY(EditAnywhere, Category = "Outfit")
    UDataAsset* MaterialLibrary;

private:
    void ApplyOutfit();
    void ApplyAccessories();
};
```

---

## Material System for Outfits

### Master Outfit Material (M_DTE_Outfit)

This master material is used for all clothing items. It supports a wide range of fabric types and effects.

#### Material Parameters

| Parameter Name | Type | Default Value | Description |
|----------------|------|---------------|-------------|
| BaseColor | Vector3 | (1, 1, 1) | Base color of the fabric |
| BaseColorTexture | Texture2D | White | Fabric texture (e.g., denim, cotton) |
| NormalMap | Texture2D | Flat Normal | Fabric normal map |
| Roughness | Scalar | 0.8 | Fabric roughness |
| Metallic | Scalar | 0.0 | Fabric metallic property |
| Specular | Scalar | 0.5 | Fabric specular property |
| EmissiveColor | Vector3 | (0, 0, 0) | Emissive color for glowing effects |
| EmissiveIntensity | Scalar | 0.0 | Intensity of emissive glow |
| PatternTexture | Texture2D | White | Overlay pattern (e.g., stripes, dots) |
| PatternScale | Scalar | 1.0 | Scale of the pattern |
| PatternColor | Vector3 | (0, 0, 0) | Color of the pattern |

### Material Functions

- **MF_Fabric_Weave:** Simulates fabric weave patterns
- **MF_Fabric_Wrinkles:** Adds dynamic wrinkles based on animation
- **MF_Pattern_Overlay:** Overlays patterns onto the base fabric
- **MF_Dirt_Grime:** Adds dirt and grime for a worn look

---

## Accessory System

### Master Accessory Material (M_DTE_Accessory)

This master material is used for all accessories. It supports metals, plastics, and other materials.

#### Material Parameters

| Parameter Name | Type | Default Value | Description |
|----------------|------|---------------|-------------|
| BaseColor | Vector3 | (1, 1, 1) | Base color of the material |
| Roughness | Scalar | 0.2 | Material roughness |
| Metallic | Scalar | 1.0 | Material metallic property |
| Specular | Scalar | 0.8 | Material specular property |
| Refraction | Scalar | 1.5 | Refraction for glass/gems |
| EmissiveColor | Vector3 | (0, 0, 0) | Emissive color for glowing effects |

### Accessory Types

- **Glasses:** Use refractive material for lenses
- **Jewelry:** Use metallic materials with high specular
- **Hats:** Use fabric materials
- **Bags:** Use leather or fabric materials

---

## Integration with Avatar

### Outfit Switching

```cpp
// In Avatar3DComponent.cpp
void UAvatar3DComponent::ChangeOutfit(FName OutfitName)
{
    if (OutfitManager)
    {
        OutfitManager->SetOutfit(OutfitName);
    }
}
```

### Dynamic Material Changes

```cpp
// In AvatarMaterialManager.cpp
void UAvatarMaterialManager::SetOutfitMaterial(EClothingSlot Slot, UMaterialInterface* Material)
{
    // Get the skeletal mesh component
    USkeletalMeshComponent* MeshComp = GetOwner()->FindComponentByClass<USkeletalMeshComponent>();
    if (!MeshComp)
    {
        return;
    }
    
    // Get the material slot for the clothing item
    int32 MaterialSlotIndex = GetMaterialSlotForClothing(Slot);
    if (MaterialSlotIndex != INDEX_NONE)
    {
        // Create and set dynamic material instance
        UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Material, this);
        MeshComp->SetMaterial(MaterialSlotIndex, MID);
    }
}
```

---

## Conclusion

The outfit and accessory system provides a powerful and flexible way to customize the avatar's appearance. With a library of outfits, accessories, and materials, the possibilities for unique looks are endless. This system is critical for creating a personalized and engaging experience for the user.
