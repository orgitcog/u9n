// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "IDetailCustomization.h"
#include "UObject/WeakObjectPtrTemplates.h"

class FReply;
class IPropertyHandle;
struct FNaniteDisplacedMeshParams;
template <typename ObjectType> class TAttribute;

class UNaniteDisplacedMesh;

/**
 * Customization of the details view for the NaniteDisplacedMesh
 */
class FNaniteDisplacedMeshDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:
	FReply ApplyNaniteDisplacedMeshParams();

	bool IsApplyNaniteDisplacedMeshParamsNeeded() const;

	bool DoesParamDifferFromOriginalValue(TSharedPtr<IPropertyHandle> Handle, int32 ObjectIndex);

	void ResetParamToOriginalValue(TSharedPtr<IPropertyHandle> Handle, int32 ObjectIndex);

	TAttribute<bool> GetCanEditAttribute(int32 ObjectIndex);

private:
	TArray<TPair<TWeakObjectPtr<UNaniteDisplacedMesh>, FNaniteDisplacedMeshParams>> CustomizedPairs;
};
