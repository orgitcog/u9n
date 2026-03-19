// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/SceneComponent.h"
#include "Containers/Ticker.h"
#include "Text3DTypes.h"
#include "UObject/ObjectMacros.h"
#include "Text3DComponent.generated.h"

class UText3DCharacterBase;
class UText3DCharacterExtensionBase;
class FTextLayout;
class ITextLayoutMarshaller;
class UFont;
class UMaterialInterface;
class UText3DLayoutExtensionBase;
class UText3DGeometryExtensionBase;
class UText3DMaterialExtensionBase;
class UText3DRendererBase;
class UText3DRenderingExtensionBase;
class UText3DLayoutEffectBase;
struct FTypefaceEntry;

UCLASS(MinimalAPI
	, ClassGroup = (Text3D)
	, PrioritizeCategories="Text Character Layout Geometry Material Rendering Transform"
	, HideCategories=(Replication,Collision,HLOD,Physics,Networking,Input,Actor,Cooking,LevelInstance,Streaming,DataLayers,WorldPartition)
	, meta = (BlueprintSpawnableComponent))
class UText3DComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	TEXT3D_API UText3DComponent();

	/**
	 * Delegate called after text is rebuilt
	 */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTextGenerated);

	/**
	 * Delegate called after text is rebuilt
	 */
	DECLARE_MULTICAST_DELEGATE(FTextGeneratedNative);
	FTextGeneratedNative::RegistrationType& OnTextGenerated()
	{
		return TextGeneratedNativeDelegate;
	}
	
	DECLARE_MULTICAST_DELEGATE_TwoParams(FTextUpdated, UText3DComponent*, EText3DRendererFlags);
	FTextUpdated::RegistrationType& OnTextPostUpdate()
	{
		return TextPostUpdateDelegate;
	}

	FTextUpdated::RegistrationType& OnTextPreUpdate()
	{
		return TextPreUpdateDelegate;
	}

	/** Get the text value and signal the primitives to be rebuilt */
	const FText& GetText() const
	{
		return Text;
	}

	/** Set the text value and signal the primitives to be rebuilt */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|Text3D")
	TEXT3D_API void SetText(const FText& Value);

	/**
	 * Returns the Text property, after being formatted by the FormatText virtual function.
	 * If FormatText is not overriden, the return FText will be the same as the Text property.
	 */
	UFUNCTION(BlueprintCallable, Category = "Text3D")
	TEXT3D_API const FText& GetFormattedText() const;

	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|Text3D")
	TEXT3D_API void SetEnforceUpperCase(bool bInEnforceUpperCase);

	bool GetEnforceUpperCase() const
	{
		return bEnforceUpperCase;
	}

	/** Get the text font and signal the primitives to be rebuilt */
	UFont* GetFont() const
	{
		return Font;
	}

	/** Set the text font and signal the primitives to be rebuilt */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|Text3D")
	TEXT3D_API void SetFont(UFont* InFont);

	/** Set whether an outline is applied. */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|Text3D")
	TEXT3D_API void SetHasOutline(const bool bValue);

	/** Set the outline width. */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|Text3D")
	TEXT3D_API void SetOutlineExpand(const float Value);

	/** Set the text extrusion size and signal the primitives to be rebuilt */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|Text3D")
	TEXT3D_API void SetExtrude(const float Value);

	/** Set the 3d bevel value */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|Text3D")
	TEXT3D_API void SetBevel(const float Value);

	/** Set the 3d bevel type */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|Text3D")
	TEXT3D_API void SetBevelType(const EText3DBevelType Value);

	/** Set the amount of segments that will be used to tessellate the Bevel */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|Text3D")
	TEXT3D_API void SetBevelSegments(const int32 Value);

	/** Get the text front material */
	TEXT3D_API UMaterialInterface* GetFrontMaterial() const;

	/** Set the text front material */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|Text3D")
	TEXT3D_API void SetFrontMaterial(UMaterialInterface* Value);

	/** Get the text bevel material */
	TEXT3D_API UMaterialInterface* GetBevelMaterial() const;

	/** Set the text bevel material */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|Text3D")
	TEXT3D_API void SetBevelMaterial(UMaterialInterface* Value);
	
	/** Get the text extrude material */
	TEXT3D_API UMaterialInterface* GetExtrudeMaterial() const;

	/** Set the text extrude material */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|Text3D")
	TEXT3D_API void SetExtrudeMaterial(UMaterialInterface* Value);

	/** Get the text back material */
	TEXT3D_API UMaterialInterface* GetBackMaterial() const;

	/** Set the text back material */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|Text3D")
	TEXT3D_API void SetBackMaterial(UMaterialInterface* Value);

	/** Get the kerning value and signal the primitives to be rebuilt */
	TEXT3D_API float GetKerning() const;

	/** Set the kerning value and signal the primitives to be rebuilt */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|Text3D")
	TEXT3D_API void SetKerning(const float Value);

	/** Get the line spacing value and signal the primitives to be rebuilt */
	TEXT3D_API float GetLineSpacing() const;

	/** Set the line spacing value and signal the primitives to be rebuilt */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|Text3D")
	TEXT3D_API void SetLineSpacing(const float Value);

	/** Get the word spacing value and signal the primitives to be rebuilt */
	TEXT3D_API float GetWordSpacing() const;

	/** Set the word spacing value and signal the primitives to be rebuilt */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|Text3D")
	TEXT3D_API void SetWordSpacing(const float Value);

	/** Get the horizontal alignment value and signal the primitives to be rebuilt */
	TEXT3D_API EText3DHorizontalTextAlignment GetHorizontalAlignment() const;

	/** Set the horizontal alignment value and signal the primitives to be rebuilt */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|Text3D")
	TEXT3D_API void SetHorizontalAlignment(const EText3DHorizontalTextAlignment value);

	/** Get the vertical alignment and signal the primitives to be rebuilt */
	TEXT3D_API EText3DVerticalTextAlignment GetVerticalAlignment() const;

	/** Set the vertical alignment and signal the primitives to be rebuilt */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|Text3D")
	TEXT3D_API void SetVerticalAlignment(const EText3DVerticalTextAlignment value);

	/** Whether a maximum width is specified */
	TEXT3D_API bool HasMaxWidth() const;

	/** Enable / Disable a Maximum Width */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|Text3D")
	TEXT3D_API void SetHasMaxWidth(const bool Value);

	/** Get the Maximum Width - If width is larger, mesh will scale down to fit MaxWidth value */
	TEXT3D_API float GetMaxWidth() const;

	/** Set the Maximum Width - If width is larger, mesh will scale down to fit MaxWidth value */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|Text3D")
	TEXT3D_API void SetMaxWidth(const float Value);

	/** Get the Maximum Width Handling - Whether to wrap before scaling when the text size reaches the max width */
	TEXT3D_API EText3DMaxWidthHandling GetMaxWidthHandling() const;

	/** Set the Maximum Width Handling - Whether to wrap before scaling when the text size reaches the max width */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|Text3D")
	TEXT3D_API void SetMaxWidthHandling(const EText3DMaxWidthHandling Value);

	/** Whether a maximum height is specified */
	TEXT3D_API bool HasMaxHeight() const;

	/** Enable / Disable a Maximum Height */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|Text3D")
	TEXT3D_API void SetHasMaxHeight(const bool Value);

	/** Get the Maximum Height - If height is larger, mesh will scale down to fit MaxHeight value */
	TEXT3D_API float GetMaxHeight() const;

	/** Set the Maximum Height - If height is larger, mesh will scale down to fit MaxHeight value */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|Text3D")
	TEXT3D_API void SetMaxHeight(const float Value);

	/** Get if the mesh should scale proportionally when Max Width/Height is set */
	TEXT3D_API bool ScalesProportionally() const;

	/** Set if the mesh should scale proportionally when Max Width/Height is set */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|Text3D")
	TEXT3D_API void SetScaleProportionally(const bool Value);

	/** Get the value of CastShadow. */
	TEXT3D_API bool CastsShadow() const;

	/** Set the value of CastShadow. */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|Text3D")
	TEXT3D_API void SetCastShadow(bool NewCastShadow);

	/** Get whole text rendered bounds */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|Text3D")
	TEXT3D_API void GetBounds(FVector& Origin, FVector& BoxExtent) const;

	/** Get whole text rendered bounds */
	FBox GetBounds() const;

	/** Get the typeface */
	FName GetTypeface() const
	{
		return Typeface;
	}

	/** Set the typeface */
	TEXT3D_API void SetTypeface(const FName InTypeface);

	UFUNCTION(BlueprintCallable, Category = "Text3D")
	void SetTextRendererClass(const TSubclassOf<UText3DRendererBase>& InClass);

	UFUNCTION(BlueprintPure, Category = "Text3D")
	TSubclassOf<UText3DRendererBase> GetTextRendererClass() const
	{
		return TextRendererClass;
	}

	const FText3DStatistics& GetStatistics() const
	{
		return Statistics;
	}

	EText3DRendererFlags GetUpdateFlags() const;

	TEXT3D_API void RequestUpdate(EText3DRendererFlags InFlags, bool bInImmediate = false);

	uint32 GetTypeFaceIndex() const;

	const FTypefaceEntry* GetTypeFaceEntry() const;
	
	UText3DRendererBase* GetTextRenderer() const;

	UFUNCTION(BlueprintCallable, Category="Text3D", meta=(DeterminesOutputType="InExtensionClass"))
	UText3DLayoutExtensionBase* GetLayoutExtension(TSubclassOf<UText3DLayoutExtensionBase> InExtensionClass);

	UText3DLayoutExtensionBase* GetLayoutExtension() const
	{
		return LayoutExtension;
	}

	template <typename InType UE_REQUIRES(TIsDerivedFrom<InType, UText3DLayoutExtensionBase>::Value)>
	InType* GetCastedLayoutExtension() const
	{
		return Cast<InType>(LayoutExtension);
	}

	UFUNCTION(BlueprintCallable, Category="Text3D", meta=(DeterminesOutputType="InExtensionClass"))
	UText3DMaterialExtensionBase* GetMaterialExtension(TSubclassOf<UText3DMaterialExtensionBase> InExtensionClass);

	UText3DMaterialExtensionBase* GetMaterialExtension() const
	{
		return MaterialExtension;
	}

	template <typename InType UE_REQUIRES(TIsDerivedFrom<InType, UText3DMaterialExtensionBase>::Value)>
	InType* GetCastedMaterialExtension() const
	{
		return Cast<InType>(MaterialExtension);
	}

	UFUNCTION(BlueprintCallable, Category="Text3D", meta=(DeterminesOutputType="InExtensionClass"))
	UText3DGeometryExtensionBase* GetGeometryExtension(TSubclassOf<UText3DGeometryExtensionBase> InExtensionClass);

	UText3DGeometryExtensionBase* GetGeometryExtension() const
	{
		return GeometryExtension;
	}

	template <typename InType UE_REQUIRES(TIsDerivedFrom<InType, UText3DGeometryExtensionBase>::Value)>
	InType* GetCastedGeometryExtension() const
	{
		return Cast<InType>(GeometryExtension);
	}

	UFUNCTION(BlueprintCallable, Category="Text3D", meta=(DeterminesOutputType="InExtensionClass"))
	UText3DRenderingExtensionBase* GetRenderingExtension(TSubclassOf<UText3DRenderingExtensionBase> InExtensionClass);
	
	UText3DRenderingExtensionBase* GetRenderingExtension() const
	{
		return RenderingExtension;
	}
	
	template <typename InType UE_REQUIRES(TIsDerivedFrom<InType, UText3DRenderingExtensionBase>::Value)>
	InType* GetCastedRenderingExtension() const
	{
		return Cast<InType>(RenderingExtension);
	}

	UFUNCTION(BlueprintCallable, Category="Text3D", meta=(DeterminesOutputType="InEffectClass"))
	TArray<UText3DLayoutEffectBase*> GetLayoutEffects(TSubclassOf<UText3DLayoutEffectBase> InEffectClass);

	TConstArrayView<UText3DLayoutEffectBase*> GetLayoutEffects() const
	{
		return LayoutEffects;
	}
	
	UText3DCharacterExtensionBase* GetCharacterExtension() const
	{
		return CharacterExtension;
	}
	
	template <typename InType UE_REQUIRES(TIsDerivedFrom<InType, UText3DCharacterExtensionBase>::Value)>
	InType* GetCastedCharacterExtension() const
	{
		return Cast<InType>(CharacterExtension);
	}

	TEXT3D_API uint16 GetCharacterCount() const;

	TEXT3D_API UText3DCharacterBase* GetCharacter(uint16 InCharacterIndex) const;

	template <typename InType UE_REQUIRES(TIsDerivedFrom<InType, UText3DCharacterBase>::Value)>
	InType* GetCastedCharacter(uint16 InCharacterIndex) const
	{
		return Cast<InType>(GetCharacter(InCharacterIndex));
	}

	TEXT3D_API void ForEachCharacter(const TFunctionRef<void(UText3DCharacterBase*, uint16, uint16)>& InFunctor) const;

	template <typename InType UE_REQUIRES(TIsDerivedFrom<InType, UText3DCharacterBase>::Value)>
	void ForEachCastedCharacter(const TFunctionRef<void(InType*, uint16, uint16)>& InFunctor) const
	{
		ForEachCharacter([&InFunctor](UText3DCharacterBase* InCharacter, uint16 InCharacterIndex, uint16 InCharacterCount)
		{
			InFunctor(Cast<InType>(InCharacter), InCharacterIndex, InCharacterCount);
		});
	}

protected:
	//~ Begin USceneComponent
	/** Intercept and propagate a change on this component to all children. */
	TEXT3D_API virtual void OnVisibilityChanged() override;
	/** Intercept and propagate a change on this component to all children. */
	TEXT3D_API virtual void OnHiddenInGameChanged() override;
	//~ End USceneComponent

	//~ Begin UActorComponent
	TEXT3D_API virtual void OnComponentCreated() override;
	TEXT3D_API virtual void OnComponentDestroyed(bool bInDestroyingHierarchy) override;
	TEXT3D_API virtual void OnRegister() override;
	TEXT3D_API virtual void OnUnregister() override;
	//~ End UActorComponent
	
	//~ Begin UObject
	TEXT3D_API virtual void Serialize(FArchive& InArchive) override;
	TEXT3D_API virtual void PostLoad() override;
	TEXT3D_API virtual void PostEditImport() override;
	TEXT3D_API virtual void PostDuplicate(EDuplicateMode::Type InDuplicateMode) override;
#if WITH_EDITOR
	TEXT3D_API virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	TEXT3D_API virtual void PostTransacted(const FTransactionObjectEvent& TransactionEvent) override;
#endif
	//~ End UObject

#if WITH_EDITOR
	friend class UText3DEditorFontSubsystem;

	DECLARE_DELEGATE_RetVal_OneParam(UFont*, FOnResolveFontByName, const FString& InFontName)
	static TEXT3D_API FOnResolveFontByName OnResolveFontByNameDelegate;

	/** Legacy : used to resolve font by their names */
	TEXT3D_API void SetFontByName(const FString& InFontName);
#endif

	/** Get the type faces supported for the current font */
	UFUNCTION()
	TArray<FName> GetTypefaceNames() const;

	/**
	 * Will be called when text geometry is generated.
	 * Override it to customize text formatting in the final geometry, without affecting the Text property.
	 * Use GetFormattedText() to retrieve a FText with the result of this formatting.
	 */
	TEXT3D_API virtual void FormatText(FText& InOutText) const;

	/** Called when TextRendererClass is changed */
	void OnTextRendererClassChanged();

	/** Called when Text is changed */
	void OnTextChanged();

	/** Called when font options are changed */
	void OnFontPropertiesChanged();

	/** Todo: Remove for 5.7. Kept here for ABI Compatibility */
	void ScheduleTextUpdateNextTick();

	/** The renderer class to use to create the text geometry */
	UPROPERTY(EditAnywhere, Setter, Getter, Category="Text", NoClear, meta=(ShowDisplayNames, AllowPrivateAccess="true"))
	TSubclassOf<UText3DRendererBase> TextRendererClass;

	/** The text to generate a 3d mesh */
	UPROPERTY(EditAnywhere, Getter, Setter, Category = "Text", meta = (MultiLine = true, AllowPrivateAccess = "true"))
	FText Text;

	/** Original text formatted */
	UPROPERTY(Transient)
	TOptional<FText> FormattedText;

	/** Text font defines the style of rendered characters */
	UPROPERTY(EditAnywhere, Getter, Setter, Category = "Text", meta = (AdvancedFontPicker, AllowPrivateAccess = "true"))
	TObjectPtr<UFont> Font;

	/** Text font face, subset within font like bold, italic, regular */
	UPROPERTY(EditAnywhere, Getter, Setter, Category = "Text", meta = (AllowPrivateAccess = "true", GetOptions="GetTypefaceNames"))
	FName Typeface;

	/** Whether to force upper case for text */
	UPROPERTY(EditAnywhere, Getter = "GetEnforceUpperCase", Setter = "SetEnforceUpperCase", Category = "Text", meta = (AllowPrivateAccess="true"))
	bool bEnforceUpperCase = false;

	UPROPERTY(VisibleInstanceOnly, Instanced, Category="Character", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UText3DCharacterExtensionBase> CharacterExtension;

	UPROPERTY(VisibleInstanceOnly, Instanced, Category="Layout", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UText3DLayoutExtensionBase> LayoutExtension;

	UPROPERTY(EditInstanceOnly, Instanced, Category="LayoutEffects", meta=(AllowPrivateAccess="true"))
	TArray<TObjectPtr<UText3DLayoutEffectBase>> LayoutEffects;

	UPROPERTY(VisibleInstanceOnly, Instanced, Category="Geometry", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UText3DGeometryExtensionBase> GeometryExtension;

	UPROPERTY(VisibleInstanceOnly, Instanced, Category="Material", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UText3DMaterialExtensionBase> MaterialExtension;

	UPROPERTY(VisibleInstanceOnly, Instanced, Category="Rendering", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UText3DRenderingExtensionBase> RenderingExtension;

private:
	/** Active renderer in charge of generating text geometry */
	UPROPERTY()
	TObjectPtr<UText3DRendererBase> TextRenderer;

	UPROPERTY(BlueprintAssignable, Category = Events, meta = (AllowPrivateAccess = "true", DisplayName = "On Text Generated"))
	FTextGenerated TextGeneratedDelegate;

	FTextGeneratedNative TextGeneratedNativeDelegate;

	/** Called before text is updated */
	FTextUpdated TextPreUpdateDelegate;

	/** Called after text is updated */
	FTextUpdated TextPostUpdateDelegate;

	/** Flagged as true when text is being updated */
	std::atomic<bool> bIsUpdatingText;

	/** Used to determine and selectively perform the the type of rebuild requested. */
	EText3DRendererFlags UpdateFlags = EText3DRendererFlags::All;

	/** Text statistics cached since last text generation */
	FText3DStatistics Statistics;

	/** Planned update requested */
	FTSTicker::FDelegateHandle TextUpdateHandle;

	/** Todo: Remove for 5.7. Kept here for ABI Compatibility */
	void RebuildInternal(bool bCleanCache = false);

	/** Queues text rebuild for the next tick */
	void QueueRebuild(EText3DRendererFlags InFlags);

	/** Partially or fully rebuilds text depending on the given update flags */
	void Rebuild(EText3DRendererFlags InUpdateFlags, bool bCleanCache);

	/** Todo: Remove for 5.7. Kept here for ABI Compatibility */
	void ClearUpdateFlags();

	bool IsTypefaceAvailable(FName InTypeface) const;
	TArray<FTypefaceEntry> GetAvailableTypefaces() const;
	void RefreshTypeface();

	void UpdateStatistics();

	void OnComponentTransformed(USceneComponent*, EUpdateTransformFlags, ETeleportType);

#if WITH_EDITORONLY_DATA
	UE_DEPRECATED(5.6, "Use Material Extension instead")
	UPROPERTY()
	TObjectPtr<UMaterialInterface> FrontMaterial_DEPRECATED;

	UE_DEPRECATED(5.6, "Use Material Extension instead")
	UPROPERTY()
	TObjectPtr<UMaterialInterface> BevelMaterial_DEPRECATED;

	UE_DEPRECATED(5.6, "Use Material Extension instead")
	UPROPERTY()
	TObjectPtr<UMaterialInterface> ExtrudeMaterial_DEPRECATED;

	UE_DEPRECATED(5.6, "Use Material Extension instead")
	UPROPERTY()
	TObjectPtr<UMaterialInterface> BackMaterial_DEPRECATED;
	
	UE_DEPRECATED(5.6, "Use Geometry Extension instead")
	UPROPERTY()
	float Extrude_DEPRECATED = 5.0f;

	UE_DEPRECATED(5.6, "Use Geometry Extension instead")
	UPROPERTY()
	float Bevel_DEPRECATED = 0.0f;

	UE_DEPRECATED(5.6, "Use Geometry Extension instead")
	UPROPERTY()
	EText3DBevelType BevelType_DEPRECATED = EText3DBevelType::Convex;
		
	UE_DEPRECATED(5.6, "Use Geometry Extension instead")
	UPROPERTY()
	int32 BevelSegments_DEPRECATED = 8;

	UE_DEPRECATED(5.6, "Use Geometry Extension instead")
	UPROPERTY()
	bool bOutline_DEPRECATED = false;

	UE_DEPRECATED(5.6, "Use Geometry Extension instead")
	UPROPERTY()
	float OutlineExpand_DEPRECATED = 0.5f;

	UE_DEPRECATED(5.6, "Use Layout Extension instead")
	UPROPERTY()
	EText3DHorizontalTextAlignment HorizontalAlignment_DEPRECATED = EText3DHorizontalTextAlignment::Left;

	UE_DEPRECATED(5.6, "Use Layout Extension instead")
	UPROPERTY()
	EText3DVerticalTextAlignment VerticalAlignment_DEPRECATED = EText3DVerticalTextAlignment::FirstLine;

	UE_DEPRECATED(5.6, "Use Layout Extension instead")
	UPROPERTY()
	float Kerning_DEPRECATED = 0.0f;

	UE_DEPRECATED(5.6, "Use Layout Extension instead")
	UPROPERTY()
	float LineSpacing_DEPRECATED = 0.0f;

	UE_DEPRECATED(5.6, "Use Layout Extension instead")
	UPROPERTY()
	float WordSpacing_DEPRECATED = 0.0f;

	UE_DEPRECATED(5.6, "Use Layout Extension instead")
	UPROPERTY()
	bool bHasMaxWidth_DEPRECATED = false;

	UE_DEPRECATED(5.6, "Use Layout Extension instead")
	UPROPERTY()
	float MaxWidth_DEPRECATED = 500.f;

	UE_DEPRECATED(5.6, "Use Layout Extension instead")
	UPROPERTY()
	bool bHasMaxHeight_DEPRECATED = false;

	UE_DEPRECATED(5.6, "Use Layout Extension instead")
	UPROPERTY()
	EText3DMaxWidthHandling MaxWidthHandling_DEPRECATED = EText3DMaxWidthHandling::Scale;
	
	UE_DEPRECATED(5.6, "Use Layout Extension instead")
	UPROPERTY()
	float MaxHeight_DEPRECATED = 500.0f;

	UE_DEPRECATED(5.6, "Use Layout Extension instead")
	UPROPERTY()
	bool bScaleProportionally_DEPRECATED = true;
	
	UE_DEPRECATED(5.6, "Use Rendering Extension instead")
	UPROPERTY()
	bool bCastShadow_DEPRECATED = true;
#endif
};
