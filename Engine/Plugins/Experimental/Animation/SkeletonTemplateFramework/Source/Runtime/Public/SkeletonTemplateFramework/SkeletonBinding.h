// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SkeletonTemplateFramework/SkeletonTemplate.h"

#include "SkeletonBinding.generated.h"

class USkeleton;

// A skeleton attribute binding
USTRUCT()
struct FSkeletonAttributeBinding
{
	GENERATED_BODY()

	// The name of the attribute in the bound template
	UPROPERTY(EditAnywhere, Category = "Settings")
	FName AttributeName;

	// The name of the bone in the bound skeleton
	UPROPERTY(EditAnywhere, Category = "Settings")
	FName BoneName;
};

// A skeleton binding specifies how a skeleton binds to a template. A binding asset specifies
// which named attributes bones map to and it can override the named sets/mappings from the
// source template as needed for that skeleton.
UCLASS(MinimalAPI, BlueprintType)
class USkeletonBinding : public UObject
{
	GENERATED_BODY()

public:
	USkeletonBinding(const FObjectInitializer& ObjectInitializer);

	// Sets up the named attributes using the provided template
	SKELETONTEMPLATEFRAMEWORKRUNTIME_API bool InitializeFrom(const TObjectPtr<USkeletonTemplate> InTemplate, const TObjectPtr<USkeleton> InSkeleton);

	SKELETONTEMPLATEFRAMEWORKRUNTIME_API const FSkeletonNamedAttribute* const FindNamedAttribute(const FName AttributeName) const;
	
	SKELETONTEMPLATEFRAMEWORKRUNTIME_API TObjectPtr<USkeleton> GetSkeleton() const;
	
	SKELETONTEMPLATEFRAMEWORKRUNTIME_API TObjectPtr<USkeletonTemplate> GetSkeletonTemplate() const;
	
	SKELETONTEMPLATEFRAMEWORKRUNTIME_API const TArray<FSkeletonNamedAttribute>& GetNamedAttributes() const;

	SKELETONTEMPLATEFRAMEWORKRUNTIME_API const TArray<FSkeletonNamedAttributeSet>& GetNamedAttributeSets() const;

	SKELETONTEMPLATEFRAMEWORKRUNTIME_API const TArray<FSkeletonNamedAttributeMapping>& GetNamedAttributeMappings() const;

	// Returns the array containing all schema named attributes that are not currently bound
	SKELETONTEMPLATEFRAMEWORKRUNTIME_API TArray<FSkeletonNamedAttribute> GetUnboundSchemaNamedAttributes() const;

	SKELETONTEMPLATEFRAMEWORKRUNTIME_API const FSkeletonAttributeBinding* const FindAttributeBinding(const FName BindingAttributeName) const;
	
	SKELETONTEMPLATEFRAMEWORKRUNTIME_API bool BindAttribute(const FName BindingAttributeName, const FName TemplateAttributeName);

	SKELETONTEMPLATEFRAMEWORKRUNTIME_API bool UnbindTemplateNamedAttribute(const FName TemplateAttributeName);

	SKELETONTEMPLATEFRAMEWORKRUNTIME_API bool UnbindBindingNamedAttribute(const FName BindingAttributeName);

	SKELETONTEMPLATEFRAMEWORKRUNTIME_API void GetChildNamedAttributes(const FName BindingAttributeName, TArray<FName>& OutChildren);

	// Sets

	SKELETONTEMPLATEFRAMEWORKRUNTIME_API bool AddNamedAttributeToSet(const FName AttributeSetName, const FName AttributeName);

	SKELETONTEMPLATEFRAMEWORKRUNTIME_API bool RemoveNamedAttributeFromSet(const FName AttributeSetName, const FName AttributeName);

	// Mappings

	SKELETONTEMPLATEFRAMEWORKRUNTIME_API const FSkeletonNamedAttributeMapping* const FindNamedAttributeMapping(const FName AttributeMappingName) const;
	
protected:

	FSkeletonNamedAttributeSet* FindMutableNamedAttributeSet(const FName AttributeSetName);
	
	int32 FindAttributeBindingIndex(const FName BindingAttributeName) const;

	FSkeletonNamedAttributeMappingEntry CreateDefaultMappingEntry(const FSkeletonNamedAttributeMapping& AttributeMapping, const FSkeletonNamedAttribute& NamedAttribute);

	// The skeleton template to bind
	// We inherit everything from our parent template, allowing us to override things as needed
	UPROPERTY(EditAnywhere, Category = "Settings")
	TObjectPtr<USkeletonTemplate> Template;

	// The skeleton to bind
	// Bones/curves/etc from our skeleton can be bound to named attributes within the template
	// Any unbound entries are considered named attributes as well
	UPROPERTY(EditAnywhere, Category = "Settings")
	TObjectPtr<USkeleton> Skeleton;

	// The list of bound attributes
	UPROPERTY(EditAnywhere, Category = "Settings")
	TArray<FSkeletonAttributeBinding> AttributeBindings;

	// The list of attributes within this binding, includes all entries from the skeleton
	UPROPERTY(VisibleAnywhere, Category = "Settings")
	TArray<FSkeletonNamedAttribute> NamedAttributes;

	// The list of attribute sets within this binding (includes sets from the template and any overrides we might have)
	// TODO: A binding can only override sets not add/remove them
	UPROPERTY(EditAnywhere, Category = "Settings")
	TArray<FSkeletonNamedAttributeSet> NamedAttributeSets;

	// The list of attribute mappings within this binding (includes mappings from the template and any overrides we might have)
	// TODO: A binding can only override mappings not add/remove them
	UPROPERTY(EditAnywhere, Category = "Settings")
	TArray<FSkeletonNamedAttributeMapping> NamedAttributeMappings;

	// The cached skeleton GUID, if it differs from the one in the bound skeleton,
	// we are stale and we need to recompute our effective data
	UPROPERTY()
	FGuid CachedSkeletonGuid;

	// The cached template hash value, if the hash differs it means our effective data
	// is stale and needs to be recomputed
	UPROPERTY()
	uint32 CachedParentTemplateHash = 0;
};
