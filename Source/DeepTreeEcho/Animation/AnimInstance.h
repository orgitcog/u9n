// Animation/AnimInstance.h - Stub for standalone compilation testing
#pragma once
#include "CoreMinimal.h"

class UAnimInstance : public UObject {
public:
    virtual void NativeInitializeAnimation() {}
    virtual void NativeUpdateAnimation(float DeltaSeconds) {}
    
    APawn* TryGetPawnOwner() const { return nullptr; }
    
    float GetInstanceAssetPlayerLength(int32 AssetPlayerIndex) { return 0.0f; }
    float GetInstanceAssetPlayerTime(int32 AssetPlayerIndex) { return 0.0f; }
    
    void Montage_Play(class UAnimMontage* MontageToPlay, float InPlayRate = 1.0f) {}
    void Montage_Stop(float InBlendOutTime, class UAnimMontage* Montage = nullptr) {}
    bool Montage_IsPlaying(class UAnimMontage* Montage) const { return false; }
};

class UAnimMontage : public UObject {
public:
    float GetPlayLength() const { return 0.0f; }
};

class USkeletalMeshComponent : public USceneComponent {
public:
    UAnimInstance* GetAnimInstance() const { return nullptr; }
    void SetAnimInstanceClass(UClass* NewClass) {}
    void PlayAnimation(class UAnimationAsset* NewAnimToPlay, bool bLooping) {}
};
