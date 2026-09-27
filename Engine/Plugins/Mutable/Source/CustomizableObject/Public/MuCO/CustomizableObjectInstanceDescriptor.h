// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCO/MultilayerProjector.h"
#include "MuCO/CustomizableObjectParameterTypeDefinitions.h"
#include "Math/RandomStream.h"

#include "CustomizableObjectInstanceDescriptor.generated.h"

#define UE_API CUSTOMIZABLEOBJECT_API

class UTexture2D;
enum class ECustomizableObjectProjectorType : uint8;

class FArchive;
class UCustomizableInstancePrivate;
class UCustomizableObject;
class UCustomizableObjectInstance;
class FDescriptorHash;
class FMutableUpdateCandidate;
struct FCustomizableObjectComponentIndex;

typedef TMap<const UCustomizableObjectInstance*, FMutableUpdateCandidate> FMutableInstanceUpdateMap;

namespace mu
{
	class FParameters;
	
	template<typename Type>
	class Ptr;
}


/** Internal use only! Currently in /Public due to UCustomizableObjectInstance::Descriptor.
 *
 * Set of parameters + state that defines a CustomizableObjectInstance.
 *
 * This object has the same parameters + state interface as UCustomizableObjectInstance.
 * UCustomizableObjectInstance must share the same interface. Any public methods added here should also end up in the Instance. */
USTRUCT()
struct FCustomizableObjectInstanceDescriptor
{
	GENERATED_BODY()

	FCustomizableObjectInstanceDescriptor() = default;
	
	UE_API explicit FCustomizableObjectInstanceDescriptor(UCustomizableObject& Object);

	/** Serialize this object. 
	 *
	 * Backwards compatibility is not guaranteed.
 	 * Multilayer Projectors not supported.
	 *
  	 * @param bUseCompactDescriptor If true it assumes the compiled objects are the same on both ends of the serialisation */
	UE_API void SaveDescriptor(FArchive &Ar, bool bUseCompactDescriptor);

	/** Deserialize this object. Does not support Multilayer Projectors! */

	/** Deserialize this object.
     *
	 * Backwards compatibility is not guaranteed.
	 * Multilayer Projectors not supported */
	UE_API void LoadDescriptor(FArchive &Ar);

	// Could return nullptr in some rare situations, so check first
	UE_API UCustomizableObject* GetCustomizableObject() const;

	UE_API void SetCustomizableObject(UCustomizableObject* InCustomizableObject);
	
	UE_API bool GetBuildParameterRelevancy() const;
	
	UE_API void SetBuildParameterRelevancy(bool Value);
	
	/** Update all parameters to be up to date with the Mutable Core parameters. */
	UE_API void ReloadParameters();
	
	UE_API void SetFirstRequestedLOD(const TMap<FName, uint8>& FirstRequestedLOD);

	UE_API const TMap<FName, uint8>& GetFirstRequestedLOD() const;

	// ------------------------------------------------------------
	// FParameters
	// ------------------------------------------------------------

	UE_API const TArray<FCustomizableObjectBoolParameterValue>& GetBoolParameters() const;
	
	UE_API const TArray<FCustomizableObjectIntParameterValue>& GetIntParameters() const;

	UE_API const TArray<FCustomizableObjectFloatParameterValue>& GetFloatParameters() const;
	
	UE_API const TArray<FCustomizableObjectAssetParameterValue>& GetTextureParameters() const;

	UE_API const TArray<FCustomizableObjectAssetParameterValue>& GetMeshParameters() const;

	UE_API const TArray<FCustomizableObjectVectorParameterValue>& GetVectorParameters() const;

	UE_API const TArray<FCustomizableObjectProjectorParameterValue>& GetProjectorParameters() const;
	
	UE_API const TArray<FCustomizableObjectTransformParameterValue>& GetTransformParameters() const;
	
	/** Return true if there are any parameters. */
	UE_API bool HasAnyParameters() const;

	/** Gets the value of the int parameter with name "ParamName". */
	UE_API const FString& GetIntParameterSelectedOption(const FString& ParamName, int32 RangeIndex = -1) const;

	/** Sets the selected option of an int parameter by the option's name. */
	UE_API void SetIntParameterSelectedOption(int32 IntParamIndex, const FString& SelectedOption, int32 RangeIndex = -1);

	/** Sets the selected option of an int parameter, by the option's name */
	UE_API void SetIntParameterSelectedOption(const FString& ParamName, const FString& SelectedOptionName, int32 RangeIndex = -1);

	/** Gets the value of a float parameter with name "FloatParamName". */
	UE_API float GetFloatParameterSelectedOption(const FString& FloatParamName, int32 RangeIndex = -1) const;

	/** Sets the float value "FloatValue" of a float parameter with index "FloatParamIndex". */
	UE_API void SetFloatParameterSelectedOption(const FString& FloatParamName, float FloatValue, int32 RangeIndex = -1);

	/** Gets the value of a texture parameter with name "TextureParamName". */
	UE_API FName GetTextureParameterSelectedOption(const FString& TextureParamName, int32 RangeIndex) const;

	/** Sets the texture value "TextureValue" of a texture parameter with index "TextureParamIndex". */
	UE_API void SetTextureParameterSelectedOption(const FString& TextureParamName, const FString& TextureValue, int32 RangeIndex);
	
	/** Gets the value of a color parameter with name "ColorParamName". */
	UE_API FLinearColor GetColorParameterSelectedOption(const FString& ColorParamName) const;

	/** Sets the color value "ColorValue" of a color parameter with index "ColorParamIndex". */
	UE_API void SetColorParameterSelectedOption(const FString& ColorParamName, const FLinearColor& ColorValue);

	/** Gets the value of a transform parameter with name "TransformParamName". */
	UE_API FTransform GetTransformParameterSelectedOption(const FString& TransformParamName) const;

	/** Sets the transform value "TransformValue" of a transform parameter with name "TransformParamName". */
	UE_API void SetTransformParameterSelectedOption(const FString& TransformParamName, const FTransform& TransformValue);

	/** Gets the value of the bool parameter with name "BoolParamName". */
	UE_API bool GetBoolParameterSelectedOption(const FString& BoolParamName) const;

	/** Sets the bool value "BoolValue" of a bool parameter with name "BoolParamName". */
	UE_API void SetBoolParameterSelectedOption(const FString& BoolParamName, bool BoolValue);

	/** Sets the vector value "VectorValue" of a bool parameter with index "VectorParamIndex". */
	UE_API void SetVectorParameterSelectedOption(const FString& VectorParamName, const FLinearColor& VectorValue);

	/** Sets the projector values of a projector parameter with index "ProjectorParamIndex". */
	UE_API void SetProjectorValue(const FString& ProjectorParamName,
		const FVector& Pos, const FVector& Direction, const FVector& Up, const FVector& Scale,
		float Angle,
		int32 RangeIndex = -1);

	/** Set only the projector position. */
	UE_API void SetProjectorPosition(const FString& ProjectorParamName, const FVector& Pos, int32 RangeIndex = -1);

	/** Set only the projector direction. */
	UE_API void SetProjectorDirection(const FString& ProjectorParamName, const FVector& Direction, int32 RangeIndex = -1);
	
	/** Set only the projector up vector. */
	UE_API void SetProjectorUp(const FString& ProjectorParamName, const FVector& Up, int32 RangeIndex = -1);

	/** Set only the projector scale. */
	UE_API void SetProjectorScale(const FString& ProjectorParamName, const FVector& Scale, int32 RangeIndex = -1);

	/** Set only the cylindrical projector angle. */
	UE_API void SetProjectorAngle(const FString& ProjectorParamName, float Angle, int32 RangeIndex = -1);
	
	/** Get the projector values of a projector parameter with index "ProjectorParamIndex". */
	UE_API void GetProjectorValue(const FString& ProjectorParamName,
		FVector& OutPos, FVector& OutDirection, FVector& OutUp, FVector& OutScale,
		float& OutAngle, ECustomizableObjectProjectorType& OutType,
		int32 RangeIndex = -1) const;

	/** Float version. See GetProjectorValue. */
	UE_API void GetProjectorValueF(const FString& ProjectorParamName,
		FVector3f& OutPos, FVector3f& OutDirection, FVector3f& OutUp, FVector3f& OutScale,
		float& OutAngle, ECustomizableObjectProjectorType& OutType,
		int32 RangeIndex = -1) const;

	/** Get the current projector position for the parameter with the given name. */
	UE_API FVector GetProjectorPosition(const FString & ParamName, int32 RangeIndex = -1) const;

	/** Get the current projector direction vector for the parameter with the given name. */
	UE_API FVector GetProjectorDirection(const FString & ParamName, int32 RangeIndex = -1) const;

	/** Get the current projector up vector for the parameter with the given name. */
	UE_API FVector GetProjectorUp(const FString & ParamName, int32 RangeIndex = -1) const;

	/** Get the current projector scale for the parameter with the given name. */
	UE_API FVector GetProjectorScale(const FString & ParamName, int32 RangeIndex = -1) const;

	/** Get the current cylindrical projector angle for the parameter with the given name. */
	UE_API float GetProjectorAngle(const FString& ParamName, int32 RangeIndex = -1) const;

	/** Get the current projector type for the parameter with the given name. */
	UE_API ECustomizableObjectProjectorType GetProjectorParameterType(const FString& ParamName, int32 RangeIndex = -1) const;

	/** Get the current projector for the parameter with the given name. */
	UE_API FCustomizableObjectProjector GetProjector(const FString& ParamName, int32 RangeIndex) const;
	
	/** Finds the parameter with name ParamName in the array of its type, returns the index if found, INDEX_NONE otherwise. */
	UE_API int32 FindTypedParameterIndex(const FString& ParamName, EMutableParameterType Type) const;

	// Parameter Ranges

	/** Gets the range of values of the projector with ParamName, returns -1 if the parameter does not exist. */
	UE_API int32 GetProjectorValueRange(const FString& ParamName) const;

	/** Gets the range of values of the int with ParamName, returns -1 if the parameter does not exist. */
	UE_API int32 GetIntValueRange(const FString& ParamName) const;

	/** Gets the range of values of the float with ParamName, returns -1 if the parameter does not exist. */
	UE_API int32 GetFloatValueRange(const FString& ParamName) const;

	/** Gets the range of values of the texture with ParamName, returns -1 if the parameter does not exist. */
	UE_API int32 GetTextureValueRange(const FString& ParamName) const;

	/** Increases the range of values of the integer with ParamName, returns the index of the new integer value, -1 otherwise.
	 * The added value is initialized with the first integer option and is the last one of the range. */
	UE_API int32 AddValueToIntRange(const FString& ParamName);

	/** Increases the range of values of the float with ParamName, returns the index of the new float value, -1 otherwise.
	 * The added value is initialized with 0.5f and is the last one of the range. */
	UE_API int32 AddValueToFloatRange(const FString& ParamName);

	/** Increases the range of values of the float with ParamName, returns the index of the new float value, -1 otherwise. 
	 * The added value is not initialized. */
	UE_API int32 AddValueToTextureRange(const FString& ParamName);

	/** Increases the range of values of the projector with ParamName, returns the index of the new projector value, -1 otherwise.
	 * The added value is initialized with the default projector as set up in the editor and is the last one of the range. */
	UE_API int32 AddValueToProjectorRange(const FString& ParamName);

	/** Remove the FRangeIndex element of the integer range of values from the parameter ParamName. If Range index is -1, removes the last element.
		Returns the index of the last valid integer, -1 if no values left. */
	UE_API int32 RemoveValueFromIntRange(const FString& ParamName, int32 RangeIndex = -1);

	/** Remove the FRangeIndex element of the float range of values from the parameter ParamName. If Range index is -1, removes the last element.
		Returns the index of the last valid float, -1 if no values left. */
	UE_API int32 RemoveValueFromFloatRange(const FString& ParamName, int32 RangeIndex = -1);

	/** Remove the last of the texture range of values from the parameter ParamName, returns the index of the last valid float, -1 if no values left. */
	UE_API int32 RemoveValueFromTextureRange(const FString& ParamName);

	/** Remove the FRangeIndex element of the texture range of values from the parameter ParamName, returns the index of the last valid float, -1 if no values left. */
	UE_API int32 RemoveValueFromTextureRange(const FString& ParamName, int32 RangeIndex);

	/** Remove the FRangeIndex element of the projector range of values from the parameter ParamName. If Range index is -1, removes the last element.
		Returns the index of the last valid projector, -1 if no values left.
	*/
	UE_API int32 RemoveValueFromProjectorRange(const FString& ParamName, int32 RangeIndex = -1);

	// ------------------------------------------------------------
   	// States
   	// ------------------------------------------------------------

	/** Get the current optimization state. */
	UE_API int32 GetState() const;

	/** Get the current optimization state. */	
	UE_API FString GetCurrentState() const;

	/** Set the current optimization state. */
	UE_API void SetState(int32 InState);

	/** Set the current optimization state. */
	UE_API void SetCurrentState(const FString& StateName);

	// ------------------------------------------------------------

	UE_API void SetRandomValues();
	
	UE_API void SetRandomValuesFromStream(const FRandomStream& InStream);

	UE_API void SetDefaultValue(int32 ParamIndex);
	UE_API void SetDefaultValues();
	
	// ------------------------------------------------------------
	// Multilayer Projectors
	// ------------------------------------------------------------

	/** @return true if ParamName belongs to a multilayer projector parameter. */
	UE_API bool IsMultilayerProjector(const FString& ParamName) const;

	// Layers
	/** @return number of layers of the projector with name ParamName,-1 if invalid or not found. */
	UE_API int32 NumProjectorLayers(const FName& ParamName) const;

	/** Creates a new layer for the multilayer projector with name ParamName. */
	UE_API void CreateLayer(const FName& ParamName, int32 Index);

	/** Removes the layer at Index from the multilayer projector with name ParamName. */
	UE_API void RemoveLayerAt(const FName& ParamName, int32 Index);

	/** @return copy of the layer at Index for the multilayer projector with name ParamName. */
	UE_API FMultilayerProjectorLayer GetLayer(const FName& ParamName, int32 Index) const;

	/** Updates the parameters of the layer at Index from the multilayer projector with name ParamName. */
	UE_API void UpdateLayer(const FName& ParamName, int32 Index, const FMultilayerProjectorLayer& Layer);

	/** Return a Mutable Core object containing all parameters. */
	UE_API TSharedPtr<mu::FParameters> GetParameters() const;

	UE_API FString ToString() const;
	
	UPROPERTY()
	TObjectPtr<UCustomizableObject> CustomizableObject = nullptr;

	UPROPERTY()
	TArray<FCustomizableObjectBoolParameterValue> BoolParameters;

	UPROPERTY()
	TArray<FCustomizableObjectIntParameterValue> IntParameters;
	
	UPROPERTY()
	TArray<FCustomizableObjectFloatParameterValue> FloatParameters;

	UPROPERTY()
	TArray<FCustomizableObjectAssetParameterValue> TextureParameters;

	UPROPERTY()
	TArray<FCustomizableObjectAssetParameterValue> MeshParameters;

	UPROPERTY()
	TArray<FCustomizableObjectVectorParameterValue> VectorParameters;
	
	UPROPERTY()
	TArray<FCustomizableObjectProjectorParameterValue> ProjectorParameters;

	UPROPERTY()
	TArray<FCustomizableObjectTransformParameterValue> TransformParameters;

	/** Mutable parameters optimization state. Transient UProperty to make it translatable. */
	UPROPERTY(Transient)
	int32 State = 0;
	
	/** If this is set to true, when updating the instance an additional step will be performed to calculate the list of instance parameters that are relevant for the current parameter values. */
	bool bBuildParameterRelevancy = false;

	/** These are the LODs Mutable can generate based on user input and quality settings, they MUST NOT be used in an update (Mutable thread). */
	TMap<FName, uint8> MinLOD;

	/** These are the MinLODs based on the active quality setting */
	TMap<FName, uint8> QualitySettingMinLODs;

	/** RequestedLODs per component to generate.
	 * Key is the Component name. Value represents the first LOD to generate. Zero means all LODs. */
	TMap<FName, uint8> FirstRequestedLOD;
	
	// Friends
	friend FDescriptorHash;
	friend UCustomizableObjectInstance;
	friend UCustomizableInstancePrivate;
	friend FMultilayerProjector;
	friend FMutableUpdateCandidate;
};

#undef UE_API
