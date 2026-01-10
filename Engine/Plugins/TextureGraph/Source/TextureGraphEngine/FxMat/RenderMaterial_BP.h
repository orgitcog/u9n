// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "RenderMaterial.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInstanceConstant.h"
#include "UObject/StrongObjectPtr.h"

class FxMaterial;
typedef std::shared_ptr< FxMaterial> FxMaterialPtr;

// Helper container of MIC for the different static switch permutations of a Material
// Used by the caller of a RenderMaterial_BP to pick a specific permutation different from the default set.
// See TG_Expression_MaterialBase for a usage example
struct TEXTUREGRAPHENGINE_API FMaterialInstanceStaticSwitchPermutationMap
{
	int32												DefaultKey = 0;
	FStaticParameterSet									DefaultStaticParameterSet;
	TMap<int32, TStrongObjectPtr<UMaterialInstanceConstant>>	PermutationsMap;

	static TSharedPtr<FMaterialInstanceStaticSwitchPermutationMap> Create(UMaterialInterface* InMaterial);
	UMaterialInterface*				GetRootMaterial();
	int32							KeyFromStaticSwitchParameters(const TArray<FStaticSwitchParameter>& Parameters);
	UMaterialInstanceConstant*		GetMaterialInstance(const TArray<FStaticSwitchParameter>& Parameters);
};

class TEXTUREGRAPHENGINE_API RenderMaterial_BP : public RenderMaterial
{
protected:

	UMaterialInterface*				Material = nullptr;			/// The base material that is used for this job 
	TStrongObjectPtr<UMaterialInstanceConstant> MaterialInstance;			/// An instance of the material

	CHashPtr						HashValue;					/// The hash for this material
	bool							RequestMaterialValidation = true;		/// 
	bool							MaterialInstanceValidated = false;		/// 

	FxMaterialPtr					FXMaterialObj;

	UCanvas*						Canvas = nullptr;

	void							DrawMaterial(UMaterialInterface* RenderMaterial, FVector2D ScreenPosition, FVector2D ScreenSize,
									FVector2D CoordinatePosition, FVector2D CoordinateSize=FVector2D::UnitVector, float Rotation=0.f,
									FVector2D PivotPoint=FVector2D(0.5f,0.5f)) const;
public:
									RenderMaterial_BP(FString Name, UMaterialInterface* InMaterial);

	virtual							~RenderMaterial_BP() override;

	static bool						ValidateMaterialCompatible(UMaterialInterface* InMaterial);


	virtual AsyncPrepareResult		PrepareResources(const TransformArgs& Args) override;

	//////////////////////////////////////////////////////////////////////////
	/// BlobTransform Implementation
	//////////////////////////////////////////////////////////////////////////
	virtual void					Bind(int32 Value, const ResourceBindInfo& BindInfo) override;
	virtual void					Bind(float Value, const ResourceBindInfo& BindInfo) override;
	virtual void					Bind(const FLinearColor& Value, const ResourceBindInfo& BindInfo) override;
	virtual void					Bind(const FIntVector4& Value, const ResourceBindInfo& BindInfo) override;
	virtual void					Bind(const FMatrix& Value, const ResourceBindInfo& BindInfo) override;
	virtual void					BindStruct(const char* ValueAddress, size_t StructSize, const ResourceBindInfo& BindInfo) override;
	virtual CHashPtr				Hash() const override;
	virtual std::shared_ptr<BlobTransform> DuplicateInstance(FString InName) override;

	//////////////////////////////////////////////////////////////////////////
	/// RenderMaterial Implementation
	//////////////////////////////////////////////////////////////////////////
	virtual void					BlitTo(FRHICommandListImmediate& RHI, UTextureRenderTarget2D* DstRT, const RenderMesh* MeshObj, int32 TargetId) const override;
	virtual void					SetTexture(FName InName, const UTexture* Texture) const override;
	virtual void					SetArrayTexture(FName InName, const std::vector<const UTexture*>& Textures) const override;
	virtual void					SetInt(FName InName, int32 Value) const override;
	virtual void					SetFloat(FName InName, float Value) const override;
	virtual void					SetColor(FName InName, const FLinearColor& Value) const override;
	virtual void					SetIntVector4(FName InName, const FIntVector4& Value) const override;
	virtual void					SetMatrix(FName InName, const FMatrix& Value) const override;
	
	//////////////////////////////////////////////////////////////////////////
	/// Inline functions
	////////////////////////////////////////////////////////////////////////// 
	FORCEINLINE UMaterialInterface*					GetMaterial() { return Material; }
	FORCEINLINE UMaterialInstanceConstant*			Instance() { return MaterialInstance.Get(); }
	FORCEINLINE const UMaterialInstanceConstant*	Instance() const { return MaterialInstance.Get(); }
};

typedef std::shared_ptr<RenderMaterial_BP> RenderMaterial_BPPtr;