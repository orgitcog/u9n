// BehaviorTree/BlackboardComponent.h - Stub for standalone compilation testing
#pragma once
#include "CoreMinimal.h"

class UBlackboardComponent : public UActorComponent {
public:
    void SetValueAsObject(const FName& KeyName, UObject* Value) {}
    void SetValueAsVector(const FName& KeyName, FVector Value) {}
    void SetValueAsFloat(const FName& KeyName, float Value) {}
    void SetValueAsBool(const FName& KeyName, bool Value) {}
    void SetValueAsInt(const FName& KeyName, int32 Value) {}
    void SetValueAsString(const FName& KeyName, const FString& Value) {}
    
    UObject* GetValueAsObject(const FName& KeyName) const { return nullptr; }
    FVector GetValueAsVector(const FName& KeyName) const { return FVector(); }
    float GetValueAsFloat(const FName& KeyName) const { return 0.0f; }
    bool GetValueAsBool(const FName& KeyName) const { return false; }
    int32 GetValueAsInt(const FName& KeyName) const { return 0; }
    FString GetValueAsString(const FName& KeyName) const { return FString(); }
};
