// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EnhancedActionKeyMapping.h"
#include "GameplayTagContainer.h"

#include "InputMappingContext.generated.h"

#define UE_API ENHANCEDINPUT_API

struct FKey;

class UInputAction;

/**
 * Options for an input mapping context being filtered based on the current input mode of the player.
 */
UENUM()
enum class EMappingContextInputModeFilterOptions : uint8
{
	/**
	 * This mapping context should use the project's default input mode query.
	 * 
	 * @see UEnhancedInputDeveloperSettings::DefaultMappingContextInputModeQuery
	 * 
	 * (Project Settings -> Engine -> Enhanced Input -> Input Modes)
	 */
	UseProjectDefaultQuery,

	/**
	 * This mapping context should use a custom input mode query instead of the project default.
	 */
	UseCustomQuery,

	/**
	 * This Input mapping context should not be filtered based on the current mode, effectively ignoring
	 * the current mode.
	 */
	DoNotFilter
};

/**
 * Options for how multiple registrations of an input mapping context should be tracked.
 */
UENUM()
enum class EMappingContextRegistrationTrackingMode : uint8
{
	/**
	 * This is the default behavior.
	 * Registrations of the Input Mapping Context are not tracked. The mapping context will be unregistered when removing it the first time, no matter how many times it has been added.
	 */
	Untracked,

	/**
	 * Track how many times the IMC is added and keeps the IMC applied until the IMC is removed the same number of times.
	 * This allows multiple systems to use the same Input Mapping Context without needing to check if any other systems are still using the same Input Mapping Context.
	 *
	 * Warnings will be logged if Input Mapping Contexts with this tracking mode are still applied at deinitialization, the expectation is that there will be a RemoveMappingContext() call for every call to AddMappingContext() when using this mode.
	 * 
	 * @see IEnhancedInputSubsystemInterface::ValidateTrackedMappingContextsAreUnregistered
	 */
	CountRegistrations
};

/**
* UInputMappingContext : A collection of key to action mappings for a specific input context
* Could be used to:
*	Store predefined controller mappings (allow switching between controller config variants). TODO: Build a system allowing redirects of UInputMappingContexts to handle this.
*	Define per-vehicle control mappings
*	Define context specific mappings (e.g. I switch from a gun (shoot action) to a grappling hook (reel in, reel out, disconnect actions).
*	Define overlay mappings to be applied on top of existing control mappings (e.g. Hero specific action mappings in a MOBA)
*/
UCLASS(MinimalAPI, BlueprintType, config = Input)
class UInputMappingContext : public UDataAsset
{
	GENERATED_BODY()

protected:
	// List of key to action mappings.
	UPROPERTY(config, BlueprintReadOnly, EditAnywhere, Category = "Mappings")
	TArray<FEnhancedActionKeyMapping> Mappings;

public:

#if WITH_EDITOR
	UE_API virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

protected:
	
	/**
     * Defines how this input mapping context should be filtered based on the current input mode. 
     *
     * Default is Use Project Default Query.
     * 
     * @Note: bEnableInputModeFiltering must be enabled in the UEnhancedInputDeveloperSettings for this to be considered.
     */
    UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Input Modes", meta = (EditCondition="ShouldShowInputModeQuery()"))
	EMappingContextInputModeFilterOptions InputModeFilterOptions = EMappingContextInputModeFilterOptions::UseProjectDefaultQuery;
	
	/**
	 * Tag Query which will be matched against the current Enhanced Input Subsystem's input mode if InputModeFilterOptions is set to UseCustomQuery. 
	 *
	 * If this tag query does not match with the current input mode tag container, then the mappings
	 * will not be processed.
	 *
	 * @Note: bEnableInputModeFiltering must be enabled in the UEnhancedInputDeveloperSettings for this to be considered.
	 */
	UPROPERTY(config, BlueprintReadOnly, EditAnywhere, Category = "Input Modes", meta = (EditConditionHides, EditCondition="ShouldShowInputModeQuery() && InputModeFilterOptions == EMappingContextInputModeFilterOptions::UseCustomQuery"))
	FGameplayTagQuery InputModeQueryOverride;

	/**
	 * Select the behaviour when multiple AddMappingContext() calls are made for this Input Mapping Context
	 */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Registration")
	EMappingContextRegistrationTrackingMode RegistrationTrackingMode = EMappingContextRegistrationTrackingMode::Untracked;

public:

	/**
	* @return True if this mapping context should be filtered based on the current input mode.
	*/
	UE_API bool ShouldFilterMappingByInputMode() const;

	/**
	 * @return The tag query which should be used when deciding whether this mapping context should be filterd
	 * out based on the current input mode or not. 
	 */
	UE_API FGameplayTagQuery GetInputModeQuery() const;

	/**
	 * @return The registration tracking mode that this IMC is using. 
	 */
	EMappingContextRegistrationTrackingMode GetRegistrationTrackingMode() const { return RegistrationTrackingMode; }
	
	// Localized context descriptor
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Description", DisplayName = "Description")
	FText ContextDescription;

	friend class FInputContextDetails;
	friend class FActionMappingsNodeBuilderEx;

	/** UFUNCTION helper to be used as en edit condition for displaying input mode query releated properties. */
	UFUNCTION()
	static UE_API bool ShouldShowInputModeQuery();

	/**
	* Mapping accessors.
	* Note: Use UEnhancedInputLibrary::RequestRebuildControlMappingsForContext to invoke changes made to an FEnhancedActionKeyMapping
	*/
	const TArray<FEnhancedActionKeyMapping>& GetMappings() const { return Mappings; }
	FEnhancedActionKeyMapping& GetMapping(TArray<FEnhancedActionKeyMapping>::SizeType Index) { return Mappings[Index]; }

	// TODO: Don't want to encourage Map/Unmap calls here where context switches would be desirable. These are intended for use in the config/binding screen only.

	/**
	* Map a key to an action within the mapping context.
	*/
	UFUNCTION(BlueprintCallable, Category = "Mapping")
	UE_API FEnhancedActionKeyMapping& MapKey(const UInputAction* Action, FKey ToKey);

	/**
	* Unmap a key from an action within the mapping context.
	*/
	UFUNCTION(BlueprintCallable, Category = "Mapping")
	UE_API void UnmapKey(const UInputAction* Action, FKey Key);
	
	/**
	* Unmap all key maps to an action within the mapping context.
	*/
	UFUNCTION(BlueprintCallable, Category = "Mapping")
	UE_API void UnmapAllKeysFromAction(const UInputAction* Action);

	/**
	* Unmap everything within the mapping context.
	*/
	UFUNCTION(BlueprintCallable, Category = "Mapping")
	UE_API void UnmapAll();
};

// ************************************************************************************************
// ************************************************************************************************
// ************************************************************************************************

#undef UE_API
