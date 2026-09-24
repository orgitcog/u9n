// DrawDebugHelpers.h - Stub for standalone compilation testing
#pragma once
#include "CoreMinimal.h"

inline void DrawDebugLine(const UWorld* InWorld, FVector const& LineStart, FVector const& LineEnd, FColor const& Color, bool bPersistentLines = false, float LifeTime = -1.0f, uint8 DepthPriority = 0, float Thickness = 0.0f) {}
inline void DrawDebugPoint(const UWorld* InWorld, FVector const& Position, float Size, FColor const& PointColor, bool bPersistentLines = false, float LifeTime = -1.0f, uint8 DepthPriority = 0) {}
inline void DrawDebugSphere(const UWorld* InWorld, FVector const& Center, float Radius, int32 Segments, FColor const& Color, bool bPersistentLines = false, float LifeTime = -1.0f, uint8 DepthPriority = 0, float Thickness = 0.0f) {}
inline void DrawDebugBox(const UWorld* InWorld, FVector const& Center, FVector const& Extent, FColor const& Color, bool bPersistentLines = false, float LifeTime = -1.0f, uint8 DepthPriority = 0, float Thickness = 0.0f) {}
inline void DrawDebugString(const UWorld* InWorld, FVector const& TextLocation, const FString& Text, AActor* TestBaseActor = nullptr, FColor const& TextColor = FColor::White, float Duration = -1.0f, bool bDrawShadow = false, float FontScale = 1.0f) {}
