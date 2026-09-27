// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/Optional.h"
#include "Text3DTypes.h"
#include "Text3DCharacterBase.generated.h"

/** Holds data for a single character in Text3D */
UCLASS(MinimalAPI, AutoExpandCategories=(Character))
class UText3DCharacterBase : public UObject
{
	GENERATED_BODY()

public:
	static TEXT3D_API FName GetRelativeLocationPropertyName();
	static TEXT3D_API FName GetRelativeRotationPropertyName();
	static TEXT3D_API FName GetRelativeScalePropertyName();
	static TEXT3D_API FName GetVisiblePropertyName();

	TEXT3D_API FTransform& GetTransform(bool bInReset);

#if WITH_EDITORONLY_DATA
	void SetCharacter(const FString& InCharacter)
	{
		Character = InCharacter;
	}

	const FString& GetCharacter() const
	{
		return Character;
	}
#endif

	void SetGlyphIndex(uint32 InGlyphIndex);
	uint32 GetGlyphIndex() const;

	void SetMeshBounds(const FBox& InBounds);
	const FBox& GetMeshBounds() const;

	void SetMeshOffset(const FVector& InOffset);
	const FVector& GetMeshOffset() const;

	UFUNCTION()
	TEXT3D_API void SetRelativeLocation(const FVector& InLocation);
	const FVector& GetRelativeLocation() const
	{
		return RelativeLocation;
	}

	UFUNCTION()
	TEXT3D_API void SetRelativeRotation(const FRotator& InRotation);
	const FRotator& GetRelativeRotation() const
	{
		return RelativeRotation;
	}

	UFUNCTION()
	TEXT3D_API void SetRelativeScale(const FVector& InScale);
	const FVector& GetRelativeScale() const
	{
		return RelativeScale;
	}

	UFUNCTION()
	TEXT3D_API void SetVisibility(bool bInVisibility);
	bool GetVisibility() const
	{
		return bVisible;
	}

	/** Get character custom kerning */
	virtual float GetCharacterKerning() const
	{
		return 0.f;
	}

	/** Reset properties to their initial state when character is recycled */
	TEXT3D_API virtual void ResetCharacterState();

protected:
	//~ Begin UObject
#if WITH_EDITOR
	virtual void PostEditUndo() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& InEvent) override;
#endif
	//~ End UObject

	void OnCharacterDataChanged(EText3DRendererFlags InFlags) const;

#if WITH_EDITORONLY_DATA
	/** Used for TitleProperty of the array to display this instead of the index */
	UPROPERTY(VisibleInstanceOnly, Category="Character", Transient, meta=(EditCondition="false", EditConditionHides, AllowPrivateAccess="true"))
	FString Character;
#endif

	UPROPERTY(EditInstanceOnly, Setter, Getter, Category="Character", meta=(AllowPrivateAccess = "true"))
	FVector RelativeLocation = FVector::ZeroVector;
	
	UPROPERTY(EditInstanceOnly, Setter, Getter, Category="Character", meta=(AllowPrivateAccess = "true"))
	FRotator RelativeRotation = FRotator::ZeroRotator;
	
	UPROPERTY(EditInstanceOnly, Setter, Getter, Category="Character", meta=(Delta="0.05", MotionDesignVectorWidget, AllowPreserveRatio="XYZ", ClampMin="0", AllowPrivateAccess = "true"))
	FVector RelativeScale = FVector::OneVector;

	UPROPERTY(EditInstanceOnly, Setter="SetVisibility", Getter="GetVisibility", Category="Character", meta=(AllowPrivateAccess = "true"))
	bool bVisible = true;

	/** Final transform after all extensions are applied */
	FTransform Transform = FTransform::Identity;

	/** Glyph that represents this character */
	uint32 GlyphIndex = 0;

	/** Actual mesh render bounds */
	FBox MeshBounds = FBox(ForceInitToZero);

	/** Offset around mesh due to font face */
	FVector MeshOffset = FVector::ZeroVector;
};
