/**
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */


#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Model/CubismModel3Json.h"
#include "Model/CubismModelActor.h"
#include "Engine/TextureRenderTarget2D.h"

#include "Live2DCubismFrameworkBPLibrary.generated.h"

UCLASS(Blueprintable)
class LIVE2DCUBISMFRAMEWORK_API ULive2DCubismFrameworkBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Load the model into the spawned actor and build all necessary Cubism components
	 * 
	 * Example Usage:
	 * @code
	 * // Spawn a model at origin facing forward
	 * FTransform SpawnTransform = FTransform::Identity;
	 * ACubismModel* MyModel = ULive2DCubismFrameworkBPLibrary::SpawnCubismModel(
	 *     this, MyModel3Json, SpawnTransform, true, nullptr
	 * );
	 * 
	 * // Spawn to render target for HUD display
	 * ACubismModel* HUDModel = ULive2DCubismFrameworkBPLibrary::SpawnCubismModel(
	 *     this, MyModel3Json, SpawnTransform, false, MyRenderTarget
	 * );
	 * @endcode
	 * 
	 * @param WorldContextObject The context object for the world
	 * @param Model3Json The model asset to load
	 * @param Transform The transform to set for the spawned actor
	 * @param bRenderInWorldSpace Whether to render the model in world space or not
	 * @param RenderTarget The render target to draw the model to (optional)
	 * @return The spawned model actor
	 */
	UFUNCTION(BlueprintCallable, Category = "Live2D Cubism", meta = (WorldContext = "WorldContextObject"))
	static ACubismModel* SpawnCubismModel(
		UObject* WorldContextObject,
		UCubismModel3Json* Model3Json,
		const FTransform& Transform,
		const bool bRenderInWorldSpace = true,
		UTextureRenderTarget2D* RenderTarget = nullptr
	);

	/**
	 * Get all parameter names from a Cubism model component
	 * Useful for debugging and dynamic parameter manipulation
	 * 
	 * @param ModelComponent The model component to query
	 * @return Array of parameter names (IDs)
	 */
	UFUNCTION(BlueprintPure, Category = "Live2D Cubism", meta = (DisplayName = "Get All Parameter Names"))
	static TArray<FString> GetAllParameterNames(UCubismModelComponent* ModelComponent);

	/**
	 * Get all drawable names from a Cubism model component
	 * Useful for debugging and dynamic drawable manipulation
	 * 
	 * @param ModelComponent The model component to query
	 * @return Array of drawable names (IDs)
	 */
	UFUNCTION(BlueprintPure, Category = "Live2D Cubism", meta = (DisplayName = "Get All Drawable Names"))
	static TArray<FString> GetAllDrawableNames(UCubismModelComponent* ModelComponent);

	/**
	 * Set a parameter value by name with blend mode
	 * Convenience function to set parameters without getting component reference
	 * 
	 * Example Usage:
	 * @code
	 * // Overwrite parameter to specific value
	 * SetParameterByName(Model, "ParamAngleX", 15.0f, ECubismParameterBlendMode::Overwrite);
	 * 
	 * // Add to current value (useful for layering effects)
	 * SetParameterByName(Model, "ParamMouthForm", 0.5f, ECubismParameterBlendMode::Additive);
	 * 
	 * // Multiply current value (useful for dampening)
	 * SetParameterByName(Model, "ParamEyeLOpen", 0.7f, ECubismParameterBlendMode::Multiplicative);
	 * 
	 * // Blend with weight for smooth transitions
	 * SetParameterByName(Model, "ParamAngleY", TargetValue, ECubismParameterBlendMode::Overwrite, 0.1f);
	 * @endcode
	 * 
	 * @param ModelComponent The model component containing the parameter
	 * @param ParameterName The name (ID) of the parameter to set
	 * @param Value The value to set
	 * @param BlendMode How to blend the value (Overwrite, Additive, Multiplicative)
	 * @param Weight The blend weight from 0.0 to 1.0
	 * @return True if parameter was found and set successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "Live2D Cubism")
	static bool SetParameterByName(
		UCubismModelComponent* ModelComponent,
		const FString& ParameterName,
		float Value,
		ECubismParameterBlendMode BlendMode = ECubismParameterBlendMode::Overwrite,
		float Weight = 1.0f
	);

	/**
	 * Get a parameter value by name
	 * Convenience function to get parameter values without getting component reference
	 * 
	 * @param ModelComponent The model component containing the parameter
	 * @param ParameterName The name (ID) of the parameter to get
	 * @param OutValue The current value of the parameter
	 * @return True if parameter was found successfully
	 */
	UFUNCTION(BlueprintPure, Category = "Live2D Cubism")
	static bool GetParameterByName(
		UCubismModelComponent* ModelComponent,
		const FString& ParameterName,
		float& OutValue
	);

	/**
	 * Check if a Cubism model has a specific component type
	 * 
	 * @param ModelActor The Cubism model actor to check
	 * @param ComponentClass The component class to look for
	 * @return True if the model has the specified component type
	 */
	UFUNCTION(BlueprintPure, Category = "Live2D Cubism", meta = (DisplayName = "Has Cubism Component"))
	static bool HasCubismComponent(ACubismModel* ModelActor, TSubclassOf<UActorComponent> ComponentClass);

	/**
	 * Validate that a parameter name exists in the model
	 * Useful for checking before setting parameters to avoid warnings
	 * 
	 * Example Usage:
	 * @code
	 * // Check before setting to avoid errors
	 * if (IsValidParameter(Model, "ParamCustom"))
	 * {
	 *     SetParameterByName(Model, "ParamCustom", Value);
	 * }
	 * 
	 * // Validate user input
	 * if (!IsValidParameter(Model, UserInputParamName))
	 * {
	 *     UE_LOG(LogTemp, Warning, TEXT("Invalid parameter: %s"), *UserInputParamName);
	 * }
	 * @endcode
	 * 
	 * @param ModelComponent The model component to check
	 * @param ParameterName The parameter name to validate
	 * @return True if the parameter exists in the model
	 */
	UFUNCTION(BlueprintPure, Category = "Live2D Cubism", meta = (DisplayName = "Is Valid Parameter"))
	static bool IsValidParameter(UCubismModelComponent* ModelComponent, const FString& ParameterName);

	/**
	 * Validate that a drawable name exists in the model
	 * 
	 * @param ModelComponent The model component to check
	 * @param DrawableName The drawable name to validate
	 * @return True if the drawable exists in the model
	 */
	UFUNCTION(BlueprintPure, Category = "Live2D Cubism", meta = (DisplayName = "Is Valid Drawable"))
	static bool IsValidDrawable(UCubismModelComponent* ModelComponent, const FString& DrawableName);

	/**
	 * Get the valid range for a parameter
	 * Returns the min and max values defined in the model
	 * 
	 * @param ModelComponent The model component containing the parameter
	 * @param ParameterName The name of the parameter
	 * @param OutMin The minimum valid value
	 * @param OutMax The maximum valid value
	 * @param OutDefault The default value
	 * @return True if parameter was found and range retrieved successfully
	 */
	UFUNCTION(BlueprintPure, Category = "Live2D Cubism")
	static bool GetParameterRange(
		UCubismModelComponent* ModelComponent,
		const FString& ParameterName,
		float& OutMin,
		float& OutMax,
		float& OutDefault
	);

	/**
	 * Clamp a parameter value to its valid range
	 * 
	 * @param ModelComponent The model component containing the parameter
	 * @param ParameterName The name of the parameter
	 * @param Value The value to clamp
	 * @return The clamped value, or the original value if parameter not found
	 */
	UFUNCTION(BlueprintPure, Category = "Live2D Cubism")
	static float ClampParameterValue(
		UCubismModelComponent* ModelComponent,
		const FString& ParameterName,
		float Value
	);
};
