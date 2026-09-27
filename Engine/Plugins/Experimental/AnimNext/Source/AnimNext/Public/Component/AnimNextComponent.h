// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimNextPublicVariablesProxy.h"
#include "Components/ActorComponent.h"
#include "Module/ModuleHandle.h"
#include "Param/ParamType.h"
#include "TraitCore/TraitEvent.h"
#include "Variables/IAnimNextVariableProxyHost.h"
#include "Module/AnimNextModuleInitMethod.h"
#include "Module/ModuleTaskContext.h"
#include "Module/TaskRunLocation.h"

#include "AnimNextComponent.generated.h"

struct FAnimNextComponentInstanceData;
class UAnimNextComponentWorldSubsystem;
class UAnimNextModule;

namespace UE::AnimNext
{
	struct FProxyVariablesContext;
}

namespace UE::AnimNext::UncookedOnly
{
	struct FUtils;
}

UCLASS(MinimalAPI, meta = (BlueprintSpawnableComponent))
class UAnimNextComponent : public UActorComponent, public IAnimNextVariableProxyHost
{
	GENERATED_BODY()

	// UActorComponent interface
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// IAnimNextVariableProxyHost interface
	virtual void FlipPublicVariablesProxy(const UE::AnimNext::FProxyVariablesContext& InContext) override;

#if WITH_EDITOR
	// Called back to refresh any cached data on module compilation
	void OnModuleCompiled();
#endif

	// (Re-)create the public variable proxy
	void CreatePublicVariablesProxy();

	// Destroy the public variable proxy
	void DestroyPublicVariablesProxy();

public:
	// Sets a module variable's value.
	// @param    Name     The name of the variable to set
	// @param    Value    The value to set the variable to
	UFUNCTION(BlueprintCallable, Category = "UAF", CustomThunk, meta = (CustomStructureParam = Value, UnsafeDuringActorConstruction))
	ANIMNEXT_API void SetVariable(UPARAM(meta = (CustomWidget = "VariableName")) FName Name, int32 Value);

	/**
	 * Module variable value getters.
	 * Numeric types (bool, (u)int32, (u)int64, float, double) support type conversion.
	 * Struct & Object types will be const, if you need to modify them use SetVariable[Struct, Object]Ref
	 * @param    Name     The name of the variable to get
	 * @return            The variable value on success, None otherwise
	 */
	ANIMNEXT_API TOptional<bool> GetVariableBool(const FName Name) const;
	ANIMNEXT_API TOptional<uint8> GetVariableByte(const FName Name) const;
	ANIMNEXT_API TOptional<int32> GetVariableInt32(const FName Name) const;
	ANIMNEXT_API TOptional<uint32> GetVariableUInt32(const FName Name) const;
	ANIMNEXT_API TOptional<int64> GetVariableInt64(const FName Name) const;
	ANIMNEXT_API TOptional<uint64> GetVariableUInt64(const FName Name) const; 
	ANIMNEXT_API TOptional<float> GetVariableFloat(const FName Name) const;
	ANIMNEXT_API TOptional<double> GetVariableDouble(const FName Name) const;
	ANIMNEXT_API TOptional<FName> GetVariableName(const FName Name) const;
	ANIMNEXT_API TOptional<FString> GetVariableString(const FName Name) const;
	ANIMNEXT_API TOptional<uint8> GetVariableEnum(const FName Name, const UEnum* RequestedEnum) const;
	ANIMNEXT_API TOptional<FConstStructView> GetVariableStruct(const FName Name, const UScriptStruct* RequestedStruct = nullptr) const;
	ANIMNEXT_API TOptional<const UObject*> GetVariableObject(const FName Name, const UClass* RequestedClass = nullptr) const;
	ANIMNEXT_API TOptional<const UClass*> GetVariableClass(const FName Name) const;
	ANIMNEXT_API TOptional<FSoftObjectPath> GetVariableSoftPath(const FName Name) const;

	/** @return enum value of specified type. */
	template <typename T>
	TOptional<T> GetVariableEnum(const FName Name) const
	{
		static_assert(TIsEnum<T>::Value, "Should only call this with enum types");

		TOptional<uint8> Result = GetVariableEnum(Name, StaticEnum<T>());
		if (!Result.IsSet())
		{
			return TOptional<T>();
		}
		return TOptional<T>(static_cast<T>(Result.GetValue()));
	}

	/** @return struct reference of specified type. */
	template <typename T>
	TOptional<const T*> GetVariableStruct(const FName Name) const
	{
		TOptional<FConstStructView> Result = GetVariableStruct(Name, TBaseStructure<T>::Get());
		if (!Result.IsSet())
		{
			return TOptional<const T*>();
		}
		if (const T* ValuePtr = Result.GetValue().GetPtr<const T>())
		{
			return TOptional<const T*>(ValuePtr);
		}
		return TOptional<const T*>();
	}

	/** @return object pointer value of specified type. */
	template <typename T>
	TOptional<const T*> GetVariableObject(const FName Name) const
	{
		static_assert(TIsDerivedFrom<T, UObject>::Value, "Should only call this with object types");

		TOptional<const UObject*> Result = GetVariableObject(Name, T::StaticClass());
		if (!Result.IsSet())
		{
			return TOptional<const T*>();
		}
		if (Result.GetValue() == nullptr)
		{
			return TOptional<const T*>(nullptr);
		}
		if (const T* Object = Cast<const T>(Result.GetValue()))
		{
			return TOptional<const T*>(Object);
		}
		return TOptional<const T*>();
	}

	/**
	 * Module variable value setters.
	 * @param    Name     The name of the variable to set
	 * @param    Value    The value to set the variable to
	 * @return            True on success, false otherwise
	 */
	ANIMNEXT_API bool SetVariableBool(const FName Name, const bool bInValue);
	ANIMNEXT_API bool SetVariableByte(const FName Name, const uint8 InValue);
	ANIMNEXT_API bool SetVariableInt32(const FName Name, const int32 InValue);
	ANIMNEXT_API bool SetVariableUInt32(const FName Name, const uint32 InValue);
	ANIMNEXT_API bool SetVariableInt64(const FName Name, const int64 InValue);
	ANIMNEXT_API bool SetVariableUInt64(const FName Name, const uint64 InValue);
	ANIMNEXT_API bool SetVariableFloat(const FName Name, const float InValue);
	ANIMNEXT_API bool SetVariableDouble(const FName Name, const double InValue);
	ANIMNEXT_API bool SetVariableName(const FName Name, const FName InValue);
	ANIMNEXT_API bool SetVariableString(const FName Name, const FString& InValue);
	ANIMNEXT_API bool SetVariableEnum(const FName Name, const uint8 InValue, const UEnum* Enum);
	ANIMNEXT_API bool SetVariableStruct(const FName Name, FConstStructView InValue);
	ANIMNEXT_API bool SetVariableStructRef(const FName Name, TFunctionRef<void(FStructView)> InStructRefSetter, const UScriptStruct* RequestedStruct = nullptr);
	ANIMNEXT_API bool SetVariableObject(const FName Name, UObject* InValue);
	ANIMNEXT_API bool SetVariableObjectRef(const FName Name, TFunctionRef<void(UObject*)> InObjectRefSetter, const UClass* RequestedClass = nullptr);
	ANIMNEXT_API bool SetVariableClass(const FName Name, UClass* InValue);
	ANIMNEXT_API bool SetVariableSoftPath(const FName Name, const FSoftObjectPath& InValue);
	ANIMNEXT_API bool SetVariableArrayRef(const FName Name, TFunctionRef<void(FPropertyBagArrayRef&)> InArrayRefSetter);

	/** Sets enum variable with specified type. */
	template <typename T>
	bool SetVariableEnum(const FName Name, const T InValue)
	{
		static_assert(TIsEnum<T>::Value, "Should only call this with enum types");
		return SetVariableEnum(Name, static_cast<uint8>(InValue), StaticEnum<T>());
	}

	/** Sets struct variable with specified type. */
	template <typename T>
	bool SetVariableStruct(const FName Name, const T& InValue)
	{
		return SetVariableStruct(Name, FConstStructView::Make(InValue));
	}

	/** Sets object pointer variable with specified type. */
	template <typename T>
	bool SetVariableObject(const FName Name, T* InValue)
	{
		static_assert(TIsDerivedFrom<T, UObject>::Value, "Should only call this with object types");
		return SetVariableObject(Name, (UObject*)InValue);
	}

	// Whether this component is currently updating
	UFUNCTION(BlueprintCallable, Category = "UAF")
	ANIMNEXT_API bool IsEnabled() const;

	// Enable or disable this component's update
	UFUNCTION(BlueprintCallable, Category = "UAF")
	ANIMNEXT_API void SetEnabled(bool bEnabled);

	// Enable or disable debug drawing. Note only works in builds with UE_ENABLE_DEBUG_DRAWING enabled
	UFUNCTION(BlueprintCallable, Category = "UAF")
	ANIMNEXT_API void ShowDebugDrawing(bool bShowDebugDrawing);

	// Queue a task to run during execution 
	ANIMNEXT_API void QueueTask(FName InModuleEventName, TUniqueFunction<void(const UE::AnimNext::FModuleTaskContext&)>&& InTaskFunction, UE::AnimNext::ETaskRunLocation InLocation = UE::AnimNext::ETaskRunLocation::Before);

	// Queues an input trait event
	// Input events will be processed in the next graph update after they are queued
	ANIMNEXT_API void QueueInputTraitEvent(FAnimNextTraitEventPtr Event);

	// Get the handle to the registered module
	UE::AnimNext::FModuleHandle GetModuleHandle() const { return ModuleHandle; }

	// Find the tick function for the specified event
	// @param	InEventName			The event associated to the wanted tick function
	ANIMNEXT_API const FTickFunction* FindTickFunction(FName InEventName) const;

	// Add a prerequisite tick function dependency to the specified event
	// @param	InObject			The object that owns the tick function
	// @param	InTickFunction		The tick function to depend on
	// @param	InEventName			The event to add the dependency to
	ANIMNEXT_API void AddPrerequisite(UObject* InObject, FTickFunction& InTickFunction, FName InEventName);

	// Add a prerequisite dependency on the component's primary tick function to the specified event
	// The component will tick before the event
	// @param	Component			The component to add as a prerequisite 
	// @param	EventName			The event to add the dependency to
	UFUNCTION(BlueprintCallable, Category = "UAF")
	ANIMNEXT_API void AddComponentPrerequisite(UActorComponent* Component, FName EventName);

	// Add a subsequent tick function dependency to the specified event
	// @param	InObject			The object that owns the tick function
	// @param	InTickFunction		The tick function to depend on
	// @param	InEventName			The event to add the dependency to
	ANIMNEXT_API void AddSubsequent(UObject* InObject, FTickFunction& InTickFunction, FName InEventName);

	// Add a subsequent dependency on the component's primary tick function to the specified event
	// The component will tick after the event
	// @param	Component			The component to add as a subsequent of the event
	// @param	EventName			The event to add the dependency to
	UFUNCTION(BlueprintCallable, Category = "UAF")
	ANIMNEXT_API void AddComponentSubsequent(UActorComponent* Component, FName EventName);

	// Remove a prerequisite tick function dependency from the specified event
	// @param	InObject			The object that owns the tick function
	// @param	InTickFunction		The tick function that was depended on
	// @param	InEventName			The event to remove the dependency from
	ANIMNEXT_API void RemovePrerequisite(UObject* InObject, FTickFunction& InTickFunction, FName InEventName);

	// Remove a prerequisite on the component's primary tick function from the specified event
	// @param	Component			The component to remove as a prerequisite 
	// @param	EventName			The event to add the dependency to
	UFUNCTION(BlueprintCallable, Category = "UAF")
	ANIMNEXT_API void RemoveComponentPrerequisite(UActorComponent* Component, FName EventName);
	
	// Remove a prerequisite tick function dependency from the specified event
	// @param	InObject			The object that owns the tick function
	// @param	InTickFunction		The tick function that was depended on
	// @param	InEventName			The event to remove the dependency from
	ANIMNEXT_API void RemoveSubsequent(UObject* InObject, FTickFunction& InTickFunction, FName InEventName);

	// Remove a subsequent dependency on the component's primary tick function from the specified event
	// @param	Component			The component to remove as a subsequent of the event
	// @param	EventName			The event to add the dependency to
	UFUNCTION(BlueprintCallable, Category = "UAF")
	ANIMNEXT_API void RemoveComponentSubsequent(UActorComponent* Component, FName EventName);
	
	// Add a prerequisite anim next event dependency to the specified event
	// @param	InEventName			The event name in this component
	// @param	InTickFunction		The other component we want a prerequisite on
	// @param	InEventName			The other component's event name
	UFUNCTION(BlueprintCallable, Category = "UAF")
	ANIMNEXT_API void AddModuleEventPrerequisite(FName InEventName, UAnimNextComponent* OtherAnimNextComponent, FName OtherEventName);

	// Add a subsequent anim next event dependency to the specified event
	// @param	InEventName			The event name in this component
	// @param	InTickFunction		The other component we want to add a prerequisite to
	// @param	InEventName			The other component's event name
	UFUNCTION(BlueprintCallable, Category = "UAF")
	ANIMNEXT_API void AddModuleEventSubsequent(FName InEventName, UAnimNextComponent* OtherAnimNextComponent, FName OtherEventName);

	// Remove a prerequisite anim next event dependency from the specified event
	// @param	InEventName			The event name in this component
	// @param	InTickFunction		The other component we want to remove a prerequisite from
	// @param	InEventName			The other component's event name
	UFUNCTION(BlueprintCallable, Category = "UAF")
	ANIMNEXT_API void RemoveModuleEventPrerequisite(FName InEventName, UAnimNextComponent* OtherAnimNextComponent, FName OtherEventName);

	// Remove a subsequent anim next event dependency from the specified event
	// @param	InEventName			The event name in this component
	// @param	InTickFunction		The other component we want to remove a prerequisite to
	// @param	InEventName			The other component's event name
	UFUNCTION(BlueprintCallable, Category = "UAF")
	ANIMNEXT_API void RemoveModuleEventSubsequent(FName InEventName, UAnimNextComponent* OtherAnimNextComponent, FName OtherEventName);

	UFUNCTION(BlueprintCallable, Category = "UAF", meta=(DisplayName = "Get Module Handle"))
	ANIMNEXT_API FAnimNextModuleHandle BlueprintGetModuleHandle() const;

private:
	DECLARE_FUNCTION(execSetVariable);

private:
	friend struct UE::AnimNext::UncookedOnly::FUtils;
	friend class UAnimNextComponentWorldSubsystem;

	// The AnimNext module that this component will run
	UPROPERTY(EditAnywhere, Category="Module")
	TObjectPtr<UAnimNextModule> Module = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UAnimNextComponentWorldSubsystem> Subsystem = nullptr;

	// Handle to the registered module
	UE::AnimNext::FModuleHandle ModuleHandle;

	// Lock for public variables proxy
	mutable FRWLock PublicVariablesLock;

	// Proxy public variables
	UPROPERTY()
	FAnimNextPublicVariablesProxy PublicVariablesProxy;

	// Map from name->proxy variable index
	TMap<FName, int32> PublicVariablesProxyMap;

	// How to initialize the module
	UPROPERTY(EditAnywhere, Category="Module")
	EAnimNextModuleInitMethod InitMethod = EAnimNextModuleInitMethod::InitializeAndPauseInEditor;

	/** When checked, the module's debug drawing instructions are drawn in the viewport */
	UPROPERTY(EditAnywhere, Category = "Rendering")
	uint8 bShowDebugDrawing : 1 = false;
};
