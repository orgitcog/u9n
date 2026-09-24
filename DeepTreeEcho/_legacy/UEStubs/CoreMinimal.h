// CoreMinimal.h - Stub header for standalone compilation testing
// This file provides minimal type definitions to allow syntax checking
// outside of the Unreal Engine environment.
//
// IMPORTANT: This file is ONLY for development validation.
// In production, this file should NOT be included - the actual
// Unreal Engine CoreMinimal.h will be used instead.

#pragma once

#ifndef COREMINIMAL_STUB_H
#define COREMINIMAL_STUB_H

// Standard library includes
#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <map>
#include <set>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <mutex>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <cfloat>

// =============================================================================
// UNREAL ENGINE TYPE STUBS
// =============================================================================

// Basic types
using int8 = int8_t;
using int16 = int16_t;
using int32 = int32_t;
using int64 = int64_t;
using uint8 = uint8_t;
using uint16 = uint16_t;
using uint32 = uint32_t;
using uint64 = uint64_t;

// Character type
using TCHAR = char;

// String types
class FString : public std::string {
public:
    FString() = default;
    FString(const char* s) : std::string(s) {}
    FString(const std::string& s) : std::string(s) {}
    
    const TCHAR* operator*() const { return c_str(); }
    bool IsEmpty() const { return empty(); }
    int32 Len() const { return static_cast<int32>(length()); }
    
    static FString Printf(const TCHAR* Format, ...) { return FString(); }
    static FString FromInt(int32 Val) { return FString(std::to_string(Val)); }
    
    FString& Append(const FString& Other) { append(Other); return *this; }
    
    bool Contains(const TCHAR* SubStr) const { return find(SubStr) != std::string::npos; }
    bool Contains(const FString& SubStr) const { return find(SubStr) != std::string::npos; }
    
    int32 Find(const TCHAR* SubStr) const {
        size_t pos = find(SubStr);
        return pos != std::string::npos ? static_cast<int32>(pos) : -1;
    }
    
    FString Left(int32 Count) const { return FString(substr(0, Count)); }
    FString Right(int32 Count) const { return FString(substr(length() - Count)); }
    FString Mid(int32 Start, int32 Count = INT32_MAX) const { return FString(substr(Start, Count)); }
    
    void ToLowerInline() { std::transform(begin(), end(), begin(), ::tolower); }
    void ToUpperInline() { std::transform(begin(), end(), begin(), ::toupper); }
    FString ToLower() const { FString Result = *this; Result.ToLowerInline(); return Result; }
    FString ToUpper() const { FString Result = *this; Result.ToUpperInline(); return Result; }
};

class FName {
public:
    FName() = default;
    FName(const char* s) : Name(s) {}
    FName(const FString& s) : Name(s) {}
    
    bool IsNone() const { return Name.empty(); }
    FString ToString() const { return Name; }
    
private:
    FString Name;
};

class FText {
public:
    FText() = default;
    static FText FromString(const FString& s) { return FText(); }
    FString ToString() const { return FString(); }
    bool IsEmpty() const { return true; }
};



// =============================================================================
// CONTAINER TYPES
// =============================================================================

template<typename T>
class TArray : public std::vector<T> {
public:
    using std::vector<T>::vector;
    
    int32 Num() const { return static_cast<int32>(this->size()); }
    bool IsEmpty() const { return this->empty(); }
    void Add(const T& Item) { this->push_back(Item); }
    void AddUnique(const T& Item) {
        if (std::find(this->begin(), this->end(), Item) == this->end()) {
            this->push_back(Item);
        }
    }
    void Empty() { this->clear(); }
    void Reset() { this->clear(); }
    bool Contains(const T& Item) const {
        return std::find(this->begin(), this->end(), Item) != this->end();
    }
    int32 Find(const T& Item) const {
        auto it = std::find(this->begin(), this->end(), Item);
        return it != this->end() ? static_cast<int32>(it - this->begin()) : -1;
    }
    void RemoveAt(int32 Index) {
        if (Index >= 0 && Index < static_cast<int32>(this->size())) {
            this->erase(this->begin() + Index);
        }
    }
    T& Last() { return this->back(); }
    const T& Last() const { return this->back(); }
    void SetNum(int32 NewNum) { this->resize(NewNum); }
    void Reserve(int32 Number) { this->reserve(Number); }
    bool IsValidIndex(int32 Index) const { return Index >= 0 && Index < static_cast<int32>(this->size()); }
    T& operator[](int32 Index) { return std::vector<T>::operator[](Index); }
    const T& operator[](int32 Index) const { return std::vector<T>::operator[](Index); }
    
    template<typename Predicate>
    void Sort(Predicate Pred) { std::sort(this->begin(), this->end(), Pred); }
    void Sort() { std::sort(this->begin(), this->end()); }
    
    T* GetData() { return this->data(); }
    const T* GetData() const { return this->data(); }
    
    bool Remove(const T& Item) {
        auto it = std::find(this->begin(), this->end(), Item);
        if (it != this->end()) {
            this->erase(it);
            return true;
        }
        return false;
    }
    
    int32 RemoveAll(const T& Item) {
        auto newEnd = std::remove(this->begin(), this->end(), Item);
        int32 Count = static_cast<int32>(this->end() - newEnd);
        this->erase(newEnd, this->end());
        return Count;
    }
    
    void Append(const TArray<T>& Other) {
        this->insert(this->end(), Other.begin(), Other.end());
    }
    
    void Append(TArray<T>&& Other) {
        this->insert(this->end(), std::make_move_iterator(Other.begin()), std::make_move_iterator(Other.end()));
    }
};

// UE-style pair wrapper for TMap iteration
template<typename KeyType, typename ValueType>
struct TMapPair {
    const KeyType& Key;
    ValueType& Value;
    TMapPair(const KeyType& K, ValueType& V) : Key(K), Value(V) {}
};

template<typename KeyType, typename ValueType>
class TMap {
private:
    std::map<KeyType, ValueType> InternalMap;
    
public:
    TMap() = default;
    
    int32 Num() const { return static_cast<int32>(InternalMap.size()); }
    void Add(const KeyType& Key, const ValueType& Value) { InternalMap[Key] = Value; }
    bool Contains(const KeyType& Key) const { return InternalMap.find(Key) != InternalMap.end(); }
    ValueType* Find(const KeyType& Key) {
        auto it = InternalMap.find(Key);
        return it != InternalMap.end() ? &it->second : nullptr;
    }
    const ValueType* Find(const KeyType& Key) const {
        auto it = InternalMap.find(Key);
        return it != InternalMap.end() ? &it->second : nullptr;
    }
    void Empty() { InternalMap.clear(); }
    bool Remove(const KeyType& Key) { return InternalMap.erase(Key) > 0; }
    
    ValueType& operator[](const KeyType& Key) { return InternalMap[Key]; }
    const ValueType& operator[](const KeyType& Key) const { return InternalMap.at(Key); }
    
    // Iterator support with Key/Value access
    class Iterator {
    public:
        using MapIterator = typename std::map<KeyType, ValueType>::iterator;
        MapIterator It;
        Iterator(MapIterator it) : It(it) {}
        bool operator!=(const Iterator& Other) const { return It != Other.It; }
        Iterator& operator++() { ++It; return *this; }
        struct Pair {
            KeyType Key;
            ValueType Value;
            Pair(const KeyType& K, const ValueType& V) : Key(K), Value(V) {}
        };
        Pair operator*() const { return Pair(It->first, It->second); }
    };
    
    class ConstIterator {
    public:
        using MapIterator = typename std::map<KeyType, ValueType>::const_iterator;
        MapIterator It;
        ConstIterator(MapIterator it) : It(it) {}
        bool operator!=(const ConstIterator& Other) const { return It != Other.It; }
        ConstIterator& operator++() { ++It; return *this; }
        struct Pair {
            const KeyType& Key;
            const ValueType& Value;
            Pair(const KeyType& K, const ValueType& V) : Key(K), Value(V) {}
        };
        Pair operator*() const { return Pair(It->first, It->second); }
    };
    
    Iterator begin() { return Iterator(InternalMap.begin()); }
    Iterator end() { return Iterator(InternalMap.end()); }
    ConstIterator begin() const { return ConstIterator(InternalMap.begin()); }
    ConstIterator end() const { return ConstIterator(InternalMap.end()); }
};

template<typename T>
class TSet : public std::set<T> {
public:
    using std::set<T>::set;
    
    int32 Num() const { return static_cast<int32>(this->size()); }
    void Add(const T& Item) { this->insert(Item); }
    bool Contains(const T& Item) const { return this->find(Item) != this->end(); }
    void Empty() { this->clear(); }
    bool Remove(const T& Item) { return this->erase(Item) > 0; }
};

// =============================================================================
// SMART POINTERS
// =============================================================================

template<typename T>
using TSharedPtr = std::shared_ptr<T>;

template<typename T>
using TWeakPtr = std::weak_ptr<T>;

template<typename T>
using TUniquePtr = std::unique_ptr<T>;

template<typename T, typename... Args>
TSharedPtr<T> MakeShared(Args&&... args) {
    return std::make_shared<T>(std::forward<Args>(args)...);
}

template<typename T, typename... Args>
TUniquePtr<T> MakeUnique(Args&&... args) {
    return std::make_unique<T>(std::forward<Args>(args)...);
}

// TFunction - UE wrapper for std::function
template<typename Signature>
using TFunction = std::function<Signature>;

// Cast function
template<typename To, typename From>
To* Cast(From* Src) {
    return dynamic_cast<To*>(Src);
}

template<typename To, typename From>
const To* Cast(const From* Src) {
    return dynamic_cast<const To*>(Src);
}

// =============================================================================
// MATH TYPES
// =============================================================================

struct FVector {
    float X = 0.0f, Y = 0.0f, Z = 0.0f;
    
    FVector() = default;
    FVector(float InX, float InY, float InZ) : X(InX), Y(InY), Z(InZ) {}
    
    FVector operator+(const FVector& V) const { return FVector(X + V.X, Y + V.Y, Z + V.Z); }
    FVector operator-(const FVector& V) const { return FVector(X - V.X, Y - V.Y, Z - V.Z); }
    FVector operator*(float Scale) const { return FVector(X * Scale, Y * Scale, Z * Scale); }
    FVector operator/(float Scale) const { return FVector(X / Scale, Y / Scale, Z / Scale); }
    
    friend FVector operator*(float Scale, const FVector& V) { return FVector(V.X * Scale, V.Y * Scale, V.Z * Scale); }
    
    float Size() const { return std::sqrt(X*X + Y*Y + Z*Z); }
    float SizeSquared() const { return X*X + Y*Y + Z*Z; }
    FVector GetSafeNormal() const {
        float S = Size();
        return S > 0.0001f ? (*this) / S : FVector();
    }
    
    static float DotProduct(const FVector& A, const FVector& B) {
        return A.X * B.X + A.Y * B.Y + A.Z * B.Z;
    }
    static FVector CrossProduct(const FVector& A, const FVector& B) {
        return FVector(
            A.Y * B.Z - A.Z * B.Y,
            A.Z * B.X - A.X * B.Z,
            A.X * B.Y - A.Y * B.X
        );
    }
    
    static float Dist(const FVector& A, const FVector& B) {
        return (A - B).Size();
    }
    static float DistSquared(const FVector& A, const FVector& B) {
        return (A - B).SizeSquared();
    }
    
    bool Normalize(float Tolerance = 0.0001f) {
        float S = Size();
        if (S > Tolerance) {
            X /= S; Y /= S; Z /= S;
            return true;
        }
        return false;
    }
    
    static const FVector ZeroVector;
    static const FVector OneVector;
    static const FVector UpVector;
    static const FVector ForwardVector;
    static const FVector RightVector;
};

inline const FVector FVector::ZeroVector(0.0f, 0.0f, 0.0f);
inline const FVector FVector::OneVector(1.0f, 1.0f, 1.0f);
inline const FVector FVector::UpVector(0.0f, 0.0f, 1.0f);
inline const FVector FVector::ForwardVector(1.0f, 0.0f, 0.0f);
inline const FVector FVector::RightVector(0.0f, 1.0f, 0.0f);

struct FVector2D {
    float X = 0.0f, Y = 0.0f;
    
    FVector2D() = default;
    FVector2D(float InX, float InY) : X(InX), Y(InY) {}
    
    FVector2D operator+(const FVector2D& V) const { return FVector2D(X + V.X, Y + V.Y); }
    FVector2D operator-(const FVector2D& V) const { return FVector2D(X - V.X, Y - V.Y); }
    FVector2D operator*(float Scale) const { return FVector2D(X * Scale, Y * Scale); }
    
    float Size() const { return std::sqrt(X*X + Y*Y); }
};

struct FRotator {
    float Pitch = 0.0f, Yaw = 0.0f, Roll = 0.0f;
    
    FRotator() = default;
    FRotator(float InPitch, float InYaw, float InRoll) : Pitch(InPitch), Yaw(InYaw), Roll(InRoll) {}
    
    FVector Vector() const;
    FRotator GetNormalized() const { return *this; }
    
    static const FRotator ZeroRotator;
};

inline const FRotator FRotator::ZeroRotator(0.0f, 0.0f, 0.0f);

struct FQuat {
    float X = 0.0f, Y = 0.0f, Z = 0.0f, W = 1.0f;
    
    FQuat() = default;
    FQuat(float InX, float InY, float InZ, float InW) : X(InX), Y(InY), Z(InZ), W(InW) {}
    
    FQuat operator*(const FQuat& Q) const;
    FVector RotateVector(const FVector& V) const { return V; }
    
    static const FQuat Identity;
};

inline const FQuat FQuat::Identity(0.0f, 0.0f, 0.0f, 1.0f);

struct FTransform {
    FVector Translation;
    FQuat Rotation;
    FVector Scale3D = FVector::OneVector;
    
    FTransform() = default;
    FTransform(const FRotator& InRotation, const FVector& InTranslation, const FVector& InScale3D = FVector::OneVector)
        : Translation(InTranslation), Scale3D(InScale3D) {}
    
    FVector GetLocation() const { return Translation; }
    FRotator GetRotation() const { return FRotator(); }
    FVector GetScale3D() const { return Scale3D; }
    
    void SetLocation(const FVector& NewLocation) { Translation = NewLocation; }
    
    static const FTransform Identity;
};

inline const FTransform FTransform::Identity;

struct FLinearColor {
    float R = 0.0f, G = 0.0f, B = 0.0f, A = 1.0f;
    
    FLinearColor() = default;
    FLinearColor(float InR, float InG, float InB, float InA = 1.0f) : R(InR), G(InG), B(InB), A(InA) {}
    
    static const FLinearColor White;
    static const FLinearColor Black;
    static const FLinearColor Red;
    static const FLinearColor Green;
    static const FLinearColor Blue;
    static const FLinearColor Yellow;
    static const FLinearColor Cyan;
    static const FLinearColor Magenta;
    static const FLinearColor Orange;
    static const FLinearColor Gray;
};

inline const FLinearColor FLinearColor::White(1.0f, 1.0f, 1.0f, 1.0f);
inline const FLinearColor FLinearColor::Black(0.0f, 0.0f, 0.0f, 1.0f);
inline const FLinearColor FLinearColor::Red(1.0f, 0.0f, 0.0f, 1.0f);
inline const FLinearColor FLinearColor::Green(0.0f, 1.0f, 0.0f, 1.0f);
inline const FLinearColor FLinearColor::Blue(0.0f, 0.0f, 1.0f, 1.0f);
inline const FLinearColor FLinearColor::Yellow(1.0f, 1.0f, 0.0f, 1.0f);
inline const FLinearColor FLinearColor::Cyan(0.0f, 1.0f, 1.0f, 1.0f);
inline const FLinearColor FLinearColor::Magenta(1.0f, 0.0f, 1.0f, 1.0f);
inline const FLinearColor FLinearColor::Orange(1.0f, 0.5f, 0.0f, 1.0f);
inline const FLinearColor FLinearColor::Gray(0.5f, 0.5f, 0.5f, 1.0f);

struct FColor {
    uint8 R = 0, G = 0, B = 0, A = 255;
    
    FColor() = default;
    FColor(uint8 InR, uint8 InG, uint8 InB, uint8 InA = 255) : R(InR), G(InG), B(InB), A(InA) {}
    
    static const FColor White;
    static const FColor Black;
    static const FColor Red;
    static const FColor Green;
    static const FColor Blue;
    static const FColor Yellow;
    static const FColor Cyan;
    static const FColor Magenta;
    static const FColor Orange;
};

inline const FColor FColor::White(255, 255, 255, 255);
inline const FColor FColor::Black(0, 0, 0, 255);
inline const FColor FColor::Red(255, 0, 0, 255);
inline const FColor FColor::Green(0, 255, 0, 255);
inline const FColor FColor::Blue(0, 0, 255, 255);
inline const FColor FColor::Yellow(255, 255, 0, 255);
inline const FColor FColor::Cyan(0, 255, 255, 255);
inline const FColor FColor::Magenta(255, 0, 255, 255);
inline const FColor FColor::Orange(255, 165, 0, 255);

// =============================================================================
// MATH UTILITIES
// =============================================================================

struct FMath {
    static float Abs(float A) { return std::abs(A); }
    static float Sin(float A) { return std::sin(A); }
    static float Cos(float A) { return std::cos(A); }
    static float Tan(float A) { return std::tan(A); }
    static float Asin(float A) { return std::asin(A); }
    static float Acos(float A) { return std::acos(A); }
    static float Atan(float A) { return std::atan(A); }
    static float Atan2(float Y, float X) { return std::atan2(Y, X); }
    static float Sqrt(float A) { return std::sqrt(A); }
    static float Pow(float A, float B) { return std::pow(A, B); }
    static float Exp(float A) { return std::exp(A); }
    static float Log(float A) { return std::log(A); }
    static float Log2(float A) { return std::log2(A); }
    
    static float Clamp(float X, float Min, float Max) { return std::clamp(X, Min, Max); }
    static int32 Clamp(int32 X, int32 Min, int32 Max) { return std::clamp(X, Min, Max); }
    
    static float Min(float A, float B) { return std::min(A, B); }
    static float Max(float A, float B) { return std::max(A, B); }
    static int32 Min(int32 A, int32 B) { return std::min(A, B); }
    static int32 Max(int32 A, int32 B) { return std::max(A, B); }
    
    static float Max3(float A, float B, float C) { return Max(Max(A, B), C); }
    static int32 Max3(int32 A, int32 B, int32 C) { return Max(Max(A, B), C); }
    static float Min3(float A, float B, float C) { return Min(Min(A, B), C); }
    static int32 Min3(int32 A, int32 B, int32 C) { return Min(Min(A, B), C); }
    
    static float Lerp(float A, float B, float Alpha) { return A + Alpha * (B - A); }
    static float FInterpTo(float Current, float Target, float DeltaTime, float InterpSpeed) {
        if (InterpSpeed <= 0.0f) return Target;
        float Dist = Target - Current;
        if (Dist * Dist < 0.00001f) return Target;
        return Current + Dist * Clamp(DeltaTime * InterpSpeed, 0.0f, 1.0f);
    }
    
    static int32 RandRange(int32 Min, int32 Max) { return Min + (rand() % (Max - Min + 1)); }
    static float FRandRange(float Min, float Max) { return Min + static_cast<float>(rand()) / RAND_MAX * (Max - Min); }
    static float FRand() { return static_cast<float>(rand()) / RAND_MAX; }
    
    static bool IsNearlyZero(float Value, float Tolerance = 0.0001f) { return Abs(Value) <= Tolerance; }
    static bool IsNearlyEqual(float A, float B, float Tolerance = 0.0001f) { return Abs(A - B) <= Tolerance; }
    
    static constexpr float PI = 3.14159265358979323846f;
    static constexpr float HALF_PI = PI / 2.0f;
    static constexpr float TWO_PI = PI * 2.0f;
    
    static float DegreesToRadians(float Degrees) { return Degrees * (PI / 180.0f); }
    static float RadiansToDegrees(float Radians) { return Radians * (180.0f / PI); }
    
    static int32 FloorToInt(float F) { return static_cast<int32>(std::floor(F)); }
    static int32 CeilToInt(float F) { return static_cast<int32>(std::ceil(F)); }
    static int32 RoundToInt(float F) { return static_cast<int32>(std::round(F)); }
    
    static float Tanh(float A) { return std::tanh(A); }
    static float Sinh(float A) { return std::sinh(A); }
    static float Cosh(float A) { return std::cosh(A); }
    
    static float Square(float A) { return A * A; }
    static int32 Square(int32 A) { return A * A; }
    
    static float Loge(float A) { return std::log(A); }
    static float Log10(float A) { return std::log10(A); }
};

// =============================================================================
// UNREAL OBJECT SYSTEM STUBS
// =============================================================================

// Forward declarations for tick types
enum ELevelTick : int;
struct FActorComponentTickFunction;

class UObject {
public:
    virtual ~UObject() = default;
    
    FName GetFName() const { return FName(); }
    FString GetName() const { return FString(); }
    class UClass* GetClass() const { return nullptr; }
    
    bool IsA(class UClass* SomeClass) const { return false; }
    
    template<typename T>
    bool IsA() const { return false; }
    
    // Default-subobject creation (UE creates CDO-owned components this way).
    // The stub simply heap-allocates; ownership semantics are not modelled.
    template<typename TReturnType>
    TReturnType* CreateDefaultSubobject(FName SubobjectName, bool bTransient = false)
    {
        (void)SubobjectName;
        (void)bTransient;
        return new TReturnType();
    }
    
    // Lifecycle methods (for Super:: calls)
    virtual void BeginPlay() {}
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, struct FActorComponentTickFunction* ThisTickFunction) {}
};

class UClass : public UObject {
public:
    FName GetFName() const { return FName(); }
};

class AActor : public UObject {
public:
    virtual void BeginPlay() {}
    virtual void Tick(float DeltaTime) {}
    virtual void EndPlay(int EndPlayReason) {}
    
    FVector GetActorLocation() const { return FVector(); }
    FRotator GetActorRotation() const { return FRotator(); }
    void SetActorLocation(const FVector& NewLocation) {}
    void SetActorRotation(const FRotator& NewRotation) {}
    
    FVector GetActorForwardVector() const { return FVector::ForwardVector; }
    FVector GetActorRightVector() const { return FVector::RightVector; }
    FVector GetActorUpVector() const { return FVector::UpVector; }
    
    class UWorld* GetWorld() const { return nullptr; }
    
    template<typename T>
    T* FindComponentByClass() const { return nullptr; }
    
    template<typename T>
    TArray<T*> GetComponentsByClass() const { return TArray<T*>(); }
};

// Tick types definition
enum ELevelTick : int { LEVELTICK_TimeOnly, LEVELTICK_ViewportsOnly, LEVELTICK_All, LEVELTICK_PauseTick };
struct FActorComponentTickFunction { bool bCanEverTick = false; };

// Forward declarations for cognitive state types
struct FDeepTreeEchoCognitiveState {
    float Arousal = 0.5f;
    float Valence = 0.5f;
    float Dominance = 0.5f;
    float Attention = 0.5f;
    float Focus = 0.5f;
    float WisdomScore = 0.0f;
    TMap<FString, float> StateValues;
};

struct FEmotionalState {
    float Joy = 0.0f;
    float Sadness = 0.0f;
    float Anger = 0.0f;
    float Fear = 0.0f;
    float Surprise = 0.0f;
    float Disgust = 0.0f;
    float Trust = 0.0f;
    float Anticipation = 0.0f;
    float Arousal = 0.5f;
    float Valence = 0.5f;
};

struct FCognitiveState {
    float Attention = 0.5f;
    float Arousal = 0.5f;
    float Valence = 0.5f;
    float Focus = 0.5f;
    float Dominance = 0.5f;
    float Creativity = 0.5f;
    float WisdomScore = 0.0f;
    float ExecutiveFunction = 0.5f;
    float MotorReadiness = 0.5f;
    float EmotionalResonance = 0.5f;
    float SensoryIntegration = 0.5f;
    float ProprioceptiveAwareness = 0.5f;
    float ProcessingLoad = 0.0f;
    TMap<FString, float> StateValues;
};

// Note: Super is typically defined per-class in UE via GENERATED_BODY()
// For standalone compilation, we'll handle Super calls differently
// The actual source files need to use explicit parent class names

class UActorComponent : public UObject {
public:
    using Super = UActorComponent;  // For standalone compilation (self-reference for base)
    struct FTickFunction {
        bool bCanEverTick = false;
        bool bStartWithTickEnabled = true;
        float TickInterval = 0.0f;
    } PrimaryComponentTick;
    
    virtual void BeginPlay() {}
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) {}
    virtual void EndPlay(int EndPlayReason) {}
    
    AActor* GetOwner() const { return nullptr; }
    void SetComponentTickEnabled(bool bEnabled) {}
    bool IsComponentTickEnabled() const { return PrimaryComponentTick.bCanEverTick; }
    UWorld* GetWorld() const { return nullptr; }
};

class USceneComponent : public UActorComponent {
public:
    FVector GetComponentLocation() const { return FVector(); }
    FRotator GetComponentRotation() const { return FRotator(); }
    void SetWorldLocation(const FVector& NewLocation) {}
    void SetWorldRotation(const FRotator& NewRotation) {}
};

class APawn : public AActor {
public:
    class AController* GetController() const { return nullptr; }
};

class UCharacterMovementComponent;

class ACharacter : public APawn {
public:
    UCharacterMovementComponent* GetCharacterMovement() const { return nullptr; }
    class USkeletalMeshComponent* GetMesh() const { return nullptr; }
    FVector GetVelocity() const { return FVector(); }
    void Jump() {}
    void StopJumping() {}
    bool CanJump() const { return true; }
};

class AController : public AActor {
public:
    APawn* GetPawn() const { return nullptr; }
};

class APlayerController : public AController {
public:
};

class UWorld : public UObject {
public:
    float GetTimeSeconds() const { return 0.0f; }
    float GetDeltaSeconds() const { return 0.0f; }
};

// =============================================================================
// DELEGATES
// =============================================================================

template<typename... ParamTypes>
class TMulticastDelegate {
public:
    void Broadcast(ParamTypes... Params) {}
    void AddLambda(std::function<void(ParamTypes...)> Lambda) {}
    void AddUObject(UObject* Object, void (UObject::*Method)(ParamTypes...)) {}
    void RemoveAll(UObject* Object) {}
    bool IsBound() const { return false; }
};

template<typename RetType, typename... ParamTypes>
class TDelegate {
public:
    RetType Execute(ParamTypes... Params) { return RetType(); }
    void BindLambda(std::function<RetType(ParamTypes...)> Lambda) {}
    bool IsBound() const { return false; }
};

#define DECLARE_MULTICAST_DELEGATE(DelegateName) \
    using DelegateName = TMulticastDelegate<>

#define DECLARE_MULTICAST_DELEGATE_OneParam(DelegateName, Param1Type) \
    using DelegateName = TMulticastDelegate<Param1Type>

#define DECLARE_MULTICAST_DELEGATE_TwoParams(DelegateName, Param1Type, Param2Type) \
    using DelegateName = TMulticastDelegate<Param1Type, Param2Type>

#define DECLARE_DELEGATE(DelegateName) \
    using DelegateName = TDelegate<void>

#define DECLARE_DELEGATE_RetVal(RetType, DelegateName) \
    using DelegateName = TDelegate<RetType>

// =============================================================================
// LOGGING
// =============================================================================

#define UE_LOG(Category, Verbosity, Format, ...) \
    do { /* stub */ } while(0)

#define DEFINE_LOG_CATEGORY(CategoryName) \
    struct CategoryName##_Stub {}

#define DEFINE_LOG_CATEGORY_STATIC(CategoryName, DefaultVerbosity, CompileTimeVerbosity) \
    struct CategoryName##_Stub {}

#define DECLARE_LOG_CATEGORY_EXTERN(CategoryName, DefaultVerbosity, CompileTimeVerbosity) \
    struct CategoryName##_Stub {}

// Log categories
struct LogTemp_Stub {};
#define LogTemp LogTemp_Stub()

// Verbosity levels
enum class ELogVerbosity { NoLogging, Fatal, Error, Warning, Display, Log, Verbose, VeryVerbose };

// =============================================================================
// ASSERTIONS
// =============================================================================

#define check(expr) do { (void)(expr); } while(0)
#define checkf(expr, format, ...) do { (void)(expr); } while(0)
#define ensure(expr) (expr)
#define ensureMsgf(expr, format, ...) (expr)
#define verify(expr) (expr)
#define verifyf(expr, format, ...) (expr)

// =============================================================================
// REFLECTION MACROS (Stubs)
// =============================================================================

#define GENERATED_BODY()
#define GENERATED_UCLASS_BODY()
#define GENERATED_USTRUCT_BODY()

#define UCLASS(...)
#define USTRUCT(...)
#define UENUM(...)
#define UPROPERTY(...)
#define UFUNCTION(...)
#define UMETA(...)

#define UINTERFACE(...)
#define GENERATED_IINTERFACE_BODY()

// =============================================================================
// MISCELLANEOUS
// =============================================================================

#define TEXT(x) x
#define TEXTVIEW(x) x

#define FORCEINLINE inline
#define FORCENOINLINE

#define WITH_EDITOR 0
#define WITH_EDITORONLY_DATA 0

// API export macros
#define DEEPTREEECHO_API
#define CORE_API
#define ENGINE_API

// Pair type
template<typename KeyType, typename ValueType>
struct TPair {
    KeyType Key;
    ValueType Value;
    TPair() = default;
    TPair(const KeyType& InKey, const ValueType& InValue) : Key(InKey), Value(InValue) {}
};

template<typename KeyType, typename ValueType>
TPair<KeyType, ValueType> MakeTuple(const KeyType& Key, const ValueType& Value) {
    return TPair<KeyType, ValueType>(Key, Value);
}

// Dynamic delegate macros (stubs)
#define DECLARE_DYNAMIC_MULTICAST_DELEGATE(DelegateName) \
    using DelegateName = TMulticastDelegate<>

#define DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(DelegateName, Param1Type, Param1Name) \
    using DelegateName = TMulticastDelegate<Param1Type>

#define DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(DelegateName, Param1Type, Param1Name, Param2Type, Param2Name) \
    using DelegateName = TMulticastDelegate<Param1Type, Param2Type>

#define DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(DelegateName, P1T, P1N, P2T, P2N, P3T, P3N) \
    using DelegateName = TMulticastDelegate<P1T, P2T, P3T>

// GC functions
inline void CollectGarbage(int Flags) {}

// Time
struct FDateTime {
    static FDateTime Now() { return FDateTime(); }
    static FDateTime UtcNow() { return FDateTime(); }
    FString ToString() const { return FString(); }
};

struct FTimespan {
    static FTimespan FromSeconds(double Seconds) { return FTimespan(); }
    static FTimespan FromMilliseconds(double Milliseconds) { return FTimespan(); }
    double GetTotalSeconds() const { return 0.0; }
    double GetTotalMilliseconds() const { return 0.0; }
};

// Platform time
struct FPlatformTime {
    static double Seconds() { 
        return std::chrono::duration<double>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count();
    }
    static uint64 Cycles64() { return 0; }
};

// Async
namespace EAsyncExecution {
    enum Type { TaskGraph, Thread, ThreadPool };
}

template<typename ResultType>
class TFuture {
public:
    ResultType Get() { return ResultType(); }
    bool IsReady() const { return true; }
    void Wait() {}
};

template<typename ResultType, typename... Args>
TFuture<ResultType> Async(int ExecutionType, std::function<ResultType()> Function) {
    return TFuture<ResultType>();
}

// JSON
class FJsonObject {
public:
    TSharedPtr<FJsonObject> GetObjectField(const FString& FieldName) const { return nullptr; }
    TArray<TSharedPtr<class FJsonValue>> GetArrayField(const FString& FieldName) const { return {}; }
    FString GetStringField(const FString& FieldName) const { return FString(); }
    double GetNumberField(const FString& FieldName) const { return 0.0; }
    bool GetBoolField(const FString& FieldName) const { return false; }
    bool HasField(const FString& FieldName) const { return false; }
    
    void SetObjectField(const FString& FieldName, TSharedPtr<FJsonObject> Value) {}
    void SetArrayField(const FString& FieldName, const TArray<TSharedPtr<class FJsonValue>>& Value) {}
    void SetStringField(const FString& FieldName, const FString& Value) {}
    void SetNumberField(const FString& FieldName, double Value) {}
    void SetBoolField(const FString& FieldName, bool Value) {}
};

class FJsonValue {
public:
    TSharedPtr<FJsonObject> AsObject() const { return nullptr; }
    TArray<TSharedPtr<FJsonValue>> AsArray() const { return {}; }
    FString AsString() const { return FString(); }
    double AsNumber() const { return 0.0; }
    bool AsBool() const { return false; }
};

// File utilities
struct FFileHelper {
    static bool SaveStringToFile(const FString& String, const TCHAR* Filename) { return true; }
    static bool LoadFileToString(FString& Result, const TCHAR* Filename) { return false; }
};

struct FPaths {
    static FString ProjectDir() { return FString(); }
    static FString ProjectContentDir() { return FString(); }
    static FString ProjectSavedDir() { return FString(); }
    static FString ProjectLogDir() { return FString(); }
    static FString Combine(const FString& A, const FString& B) { return A + "/" + B; }
};

// Module interface
class IModuleInterface {
public:
    virtual ~IModuleInterface() = default;
    virtual void StartupModule() {}
    virtual void ShutdownModule() {}
};

#define IMPLEMENT_MODULE(ModuleClass, ModuleName) \
    static ModuleClass ModuleName##_Instance;

#endif // COREMINIMAL_STUB_H
