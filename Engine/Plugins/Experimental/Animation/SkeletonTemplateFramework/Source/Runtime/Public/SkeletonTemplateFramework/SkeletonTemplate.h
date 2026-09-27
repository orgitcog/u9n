// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SkeletonTemplateFramework/NamedElementTypes.h"

#include "SkeletonTemplate.generated.h"

// A skeleton named attribute
USTRUCT()
struct FSkeletonNamedAttribute
{
	GENERATED_BODY()

	// The name of the attribute
	UPROPERTY(EditAnywhere, Category = "Settings")
	FName Name;

	// The name of the parent attribute (optional)
	UPROPERTY(EditAnywhere, Category = "Settings")
	FName ParentName;

	// The type of the attribute
	UPROPERTY(EditAnywhere, Category = "Settings")
	TObjectPtr<const UClass> Type;
};

// A skeleton named attribute set
USTRUCT()
struct FSkeletonNamedAttributeSet
{
	GENERATED_BODY()

	// The name of the attribute set
	UPROPERTY(EditAnywhere, Category = "Settings")
	FName Name;

	// The list of attributes within this set
	UPROPERTY(EditAnywhere, Category = "Settings")
	TArray<FName> NamedAttributes;

	// TODO: Add a hierarchy table, it maps named attributes to which sub-parts are included
	// Sub-parts are defined per attribute type (e.g. FTransform has rotation/translation/scale sub-parts)
};

// A struct stored for each entry in a named attribute mapping
USTRUCT()
struct FSkeletonNamedAttributeMappingEntry
{
	GENERATED_BODY()

	// The attribute in the set that this value relates to
	UPROPERTY(EditAnywhere, Category = "Settings")
	FName AttributeName;

	// The actual payload data for this entry
	UPROPERTY(EditAnywhere, Category = "Settings")
	TObjectPtr<USkeletonTemplateBaseType> Value;
};

// A mapping struct for each named attribute mapping to describe what payload type to store for each unique type of named attribute
USTRUCT()
struct FSkeletonNamedAttributeMappingType
{
	GENERATED_BODY()

	// The source named attribute type
	UPROPERTY(EditAnywhere, Category = "Settings")
	TObjectPtr<const UClass> SourceType;

	// The target mapping payload type
	UPROPERTY(EditAnywhere, Category = "Settings")
	TObjectPtr<const UClass> TargetType;
};

// A skeleton named attribute mapping
USTRUCT()
struct FSkeletonNamedAttributeMapping
{
	GENERATED_BODY()

	// The name of the attribute mapping
	UPROPERTY(EditAnywhere, Category = "Settings")
	FName Name;

	// The name of the source attribute set to use as keys in our mapping
	UPROPERTY(EditAnywhere, Category = "Settings")
	FName SourceAttributeSet;

	// A mapping struct for each named attribute mapping to describe what payload type to store for each unique type of named attribute
	UPROPERTY(EditAnywhere, Category = "Settings")
	TArray<FSkeletonNamedAttributeMappingType> MappingTypes;

	// The hierarchy table that contains the mapping data
	UPROPERTY(EditAnywhere, Category = "Settings")
	TArray<FSkeletonNamedAttributeMappingEntry> TableData;
};

// A skeleton template specifies a list of attributes that skeletons can contain as well
// as sets and data mappings. A skeleton binding asset can then map each attribute to
// a skeleton bone/curve/etc as well as override any set/mapping values inherited from
// its template.
UCLASS(MinimalAPI, BlueprintType)
class USkeletonTemplate : public UObject
{
	GENERATED_BODY()

public:
	USkeletonTemplate(const FObjectInitializer& ObjectInitializer);

	// Attributes

	// Returns topologically sorted attributes, i.e. parents are defined before they're referenced by children
	SKELETONTEMPLATEFRAMEWORKRUNTIME_API const TArray<FSkeletonNamedAttribute>& GetNamedAttributes() const;

	SKELETONTEMPLATEFRAMEWORKRUNTIME_API bool AddNamedAttribute(const FSkeletonNamedAttribute& NewNamedAttribute);

	SKELETONTEMPLATEFRAMEWORKRUNTIME_API bool RenameNamedAttribute(const FName OldAttributeName, const FName NewAttributeName);

	SKELETONTEMPLATEFRAMEWORKRUNTIME_API bool ReparentNamedAttribute(const FName AttributeName, const FName NewParentName);

	enum class ERemoveNamedAttributeBehavior
	{
		RemoveChildren,
		MoveChildrenToParent,
	};

	SKELETONTEMPLATEFRAMEWORKRUNTIME_API bool RemoveNamedAttribute(const FName AttributeName, const ERemoveNamedAttributeBehavior Behavior = ERemoveNamedAttributeBehavior::MoveChildrenToParent);

	SKELETONTEMPLATEFRAMEWORKRUNTIME_API bool SetNamedAttributeType(const FName AttributeName, const TObjectPtr<const UClass> NewType);

	SKELETONTEMPLATEFRAMEWORKRUNTIME_API const FSkeletonNamedAttribute* const FindNamedAttribute(const FName AttributeName) const;

	// Sets
	
	SKELETONTEMPLATEFRAMEWORKRUNTIME_API const TArray<FSkeletonNamedAttributeSet>& GetNamedAttributeSets() const;

	SKELETONTEMPLATEFRAMEWORKRUNTIME_API const FSkeletonNamedAttributeSet* const FindNamedAttributeSet(const FName AttributeSetName) const;

	SKELETONTEMPLATEFRAMEWORKRUNTIME_API bool AddAttributeSet(const FName AttributeSetName);

	SKELETONTEMPLATEFRAMEWORKRUNTIME_API bool RemoveAttributeSet(const FName AttributeSetName);
	
	SKELETONTEMPLATEFRAMEWORKRUNTIME_API bool AddAttributeToSet(const FName AttributeSetName, const FName AttributeName);

	SKELETONTEMPLATEFRAMEWORKRUNTIME_API bool RemoveAttributeFromSet(const FName AttributeSetName, const FName AttributeName);

	SKELETONTEMPLATEFRAMEWORKRUNTIME_API bool RenameNamedAttributeSet(const FName AttributeSetName, const FName NewAttributeSetName);

	// Mappings

	SKELETONTEMPLATEFRAMEWORKRUNTIME_API const TArray<FSkeletonNamedAttributeMapping>& GetNamedAttributeMappings() const;

	SKELETONTEMPLATEFRAMEWORKRUNTIME_API const FSkeletonNamedAttributeMapping* const FindNamedAttributeMapping(const FName AttributeMappingName) const;

	SKELETONTEMPLATEFRAMEWORKRUNTIME_API bool AddAttributeMapping(const FName AttributeMappingName);

	SKELETONTEMPLATEFRAMEWORKRUNTIME_API bool RemoveAttributeMapping(const FName AttributeMappingName);

	SKELETONTEMPLATEFRAMEWORKRUNTIME_API bool RenameNamedAttributeMapping(const FName AttributeMappingName, const FName NewAttributeMappingName);

	SKELETONTEMPLATEFRAMEWORKRUNTIME_API bool SetNamedAttributeMappingSourceSet(const FName AttributeMappingName, const FName NewSourceSet);

protected:
	FSkeletonNamedAttributeMapping* FindMutableNamedAttributeMapping(const FName AttributeMappingName);

	FSkeletonNamedAttributeSet* FindMutableNamedAttributeSet(const FName AttributeSetName);

	FSkeletonNamedAttribute* FindMutableNamedAttribute(const FName AttributeName);

	int32 FindNamedAttributeIndex(const FName AttributeName);
	
	int32 FindNamedAttributeSetIndex(const FName AttributeSetName);
	
	int32 FindNamedAttributeMappingIndex(const FName AttributeMappingName);
	
	void SortNamedAttributes();

	// TODO Parent Template Support:
	// A parent template means a few things:
	//    - Named Attributes come from the parent, these are immutable: cannot rename, change type, change parent, cannot be removed
	//    - New named attributes can be added, if the parent template changes and an attribute is removed that the
	//      current template relied on (e.g. had a child attribute under, a set that used it), we retain it as if we had authored it
	//    - Named Sets come from the parent, its name is immutable, but its content can change (can add/remove attributes, can add/remove sub-parts)
	//    - Named sets can be added
	//    - Named sets from the parent cannot be removed
	//    - Named Mappings come from the parent, its name is immutable, its type is immutable, but its content can change (can change source set, can change mapped values)
	//    - Named mappings can be added
	//    - Named mappings from the parent cannot be removed
	//    - Named mappings that depend on a modified set that is removed by the parent will force that set to be retained locally
	//    - Generally speaking, if a template modifies something from the parent or creates new entries that depend on parent values
	//      then if the parent removes them, we will retain them locally as if they had been authored
	//      However, if we did not change any of these values and we do not depend on them, when the parent removes them, they are removed here as well
	//    - When we need to display a value to the user, it can come from:
	//        - If we modified it locally, we show that value
	//        - If we haven't modified it, we check if it is overridden by a parent entry, if it is we display that value (e.g. set beneath to X)
	//        - If we haven't modified it, and no parent entry overrides a value, we display the parent value
	//        - If we haven't modified it, and we have no parent, we display the type's default value
	//    - This means that we have:
	//        - source values from the parent template
	//        - source values from the local template (this), stored as a delta from the parent template (e.g. only what we modified locally)
	//        - effective values that combine the parent + local
	//    - Effective values is what is shown to users in the UI and what is returned from queries, it's an implementation detail
	//    - As such, we need to retain hashes of the parent values to detect changes so that we may rebuild our effective values
	//    - We need to make sure we cannot select ourself or another parent within the inheritance chain as our parent template
	//      as this might create a reference cycle

	// TODO Attribute Selection:
	// Named Sets refer to Named Attributes but they are just generic FNames
	// Similar to how AnimNext Variables/Parameters were handled, we need a dropdown of valid attributes
	// Alternatively, if the Named Sets are stored as a hierarchy table, then it needs to display the same topology as named mappings
	// The attribute selection will also be needed later when we wish to refer to attributes from external systems (e.g. Anim Graph)
	// There, a graph will specify the template it uses and we'll restrict the attributes shown in a dropdown

	// TODO: Attribute Type Support:
	// We wish to support any built-in/user type but not every type known to man
	// We need a registry system where we specify what types to expose as valid attribute types
	// We'll need to provide additional information per type (e.g. sub-parts, default value), this
	// is where we would specify that (e.g. some adapter we register which specifies the type + metadata)

	// TODO: Mapping Type Support:
	// Similar to Attribute Types, we need a registry to handle mapping types but here it is
	// to provide supplemental information. It should be possible to put any type in a mapping,
	// but we need to be able to specify extra metadata. For example, if we want a 'value type'
	// mapping to store the default value/bind pose of attributes, this meta-type will be the
	// mapping type, but each mapped entry will have its own type derived from its attribute type
	// (e.g. FTransform attributes have an FTransform, float attributes have a float)
	// 
	// We also need to be able to specify, through native code, built-in mappings. Some mappings
	// are accessed through native code with hardcoded names (e.g. bind pose, translation retarget options)
	// and those mappings thus have a name that cannot be edited by the user. It might make sense
	// in the UI to break both types of mapping down when we add a new one: Built-In and Custom (or some other name)
	// And so the registry can also be used to specify these.
	// We have to prevent user specifies names from colliding with built-in ones, this is something we must
	// enforce and validate on load/cook (or when we display the UI)

	// An optional parent template we can derive from
	// We inherit everything from our parent template, allowing us to override things as needed
	//UPROPERTY(EditAnywhere, Category = "Settings")
	//TObjectPtr<USkeletonTemplate> ParentTemplate;

	// The list of attributes within this template
	UPROPERTY(VisibleAnywhere, Category = "Settings")
	TArray<FSkeletonNamedAttribute> NamedAttributes;

	// The list of attribute sets within this template
	UPROPERTY(EditAnywhere, Category = "Settings")
	TArray<FSkeletonNamedAttributeSet> NamedAttributeSets;

	// The list of attribute mappings within this template
	UPROPERTY(EditAnywhere, Category = "Settings")
	TArray<FSkeletonNamedAttributeMapping> NamedAttributeMappings;

	// The list of attributes within this template, including the parent template
	//UPROPERTY(EditAnywhere, Category = "Settings")
	//TArray<FSkeletonNamedAttribute> EffectiveNamedAttributes;

	// The list of attribute sets within this template, including the parent template
	//UPROPERTY(EditAnywhere, Category = "Settings")
	//TArray<FSkeletonNamedAttributeSet> EffectiveNamedAttributeSets;

	// The list of attribute mappings within this template, including the parent template
	//UPROPERTY(EditAnywhere, Category = "Settings")
	//TArray<FSkeletonNamedAttributeMapping> EffectiveNamedAttributeMappings;

	// The is a hash of our user authored + parent values that derived templates can rely on to
	// detect staleness
	//UPROPERTY(VisibleAnywhere, Category = "Internals")
	//uint32 AssetHash = 0;

	// The cached parent template hash value, if the hash differs it means our effective data
	// is stale and needs to be recomputed from the parent and our user authored values
	//UPROPERTY(VisibleAnywhere, Category = "Internals")
	//uint32 CachedParentTemplateHash = 0;
};
