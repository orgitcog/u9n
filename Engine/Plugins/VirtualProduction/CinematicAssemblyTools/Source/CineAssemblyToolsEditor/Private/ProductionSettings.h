// Copyright Epic Games, Inc. All Rights Reserved.
#pragma  once

#include "Engine/DeveloperSettings.h"

#include "Misc/FrameRate.h"

#include "ProductionSettings.generated.h"

DECLARE_MULTICAST_DELEGATE(FOnProductionListChanged);
DECLARE_MULTICAST_DELEGATE(FOnActiveProductionChanged);

/**
 * Options for determining the hierarchical bias of subsequences
 */
UENUM()
enum class ESubsequencePriority : uint8
{
	TopDown,
	BottomUp
};

/**
 * Properties of a folder in the production's template folder hierarchy
 */
USTRUCT()
struct FFolderTemplate
{
	GENERATED_BODY()

	UPROPERTY(config, EditAnywhere, Category = "Default")
	FString InternalPath;

	UPROPERTY(config, EditAnywhere, Category = "Default")
	bool bCreateIfMissing = true;
};

/**
 * Collection of production settings to override the project/editor behavior
 */
USTRUCT(BlueprintType)
struct FCinematicProduction
{
	GENERATED_BODY()

public:
	FCinematicProduction();

	/** Unique ID of the production */
	UPROPERTY(BlueprintReadOnly, Category = "Default", meta = (IgnoreForMemberInitializationTest))
	FGuid ProductionID;

	/** Production Name */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Default")
	FString ProductionName;

	/** The default frame rate to set for new Level Sequences */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Default")
	FFrameRate DefaultDisplayRate;

	/** The default frame number (using the default frame rate) that new Level Sequences should start at */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Default")
	int32 DefaultStartFrame;

	/** Controls whether subsequences override parent sequences, or vice versa */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Default")
	ESubsequencePriority SubsequencePriority = ESubsequencePriority::BottomUp;

	/** List of Naming Token namespaces that should not be evaluated */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Default")
	TSet<FString> NamingTokenNamespaceDenyList;

	/** List of default names for specific asset types */
	UPROPERTY(config, VisibleAnywhere, BlueprintReadOnly, Category = "Default")
	TMap<TObjectPtr<const UClass>, FString> DefaultAssetNames;

	/** List of folder paths that represent a template folder hierarchy to be used for this production */
	UPROPERTY(config, EditAnywhere, Category = "Default")
	TArray<FFolderTemplate> TemplateFolders;
};

/**
 * Cinematic Production Settings
 */
UCLASS(config=Engine, defaultconfig)
class UProductionSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UProductionSettings() = default;

	//~ Begin UDeveloperSettings overrides
	virtual FName GetContainerName() const override;
	virtual FName GetCategoryName() const override;
	virtual FName GetSectionName() const override;
	//~ End UDeveloperSettings overrides

	//~ Begin UObject overrides
	virtual void PostInitProperties() override;
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	//~ End UObject overrides

	/** The customization is allowed access to the private properties */
	friend class FProductionSettingsCustomization;

	/** Returns a copy of the production list */
	const TArray<FCinematicProduction> GetProductions() const;

	/** Returns a copy of the production matching the input production ID (if it exists) */
	TOptional<const FCinematicProduction> GetProduction(FGuid ProductionID) const;

	/** Adds a new empty production to the list */
	void AddProduction();

	/** Adds the input production to the list */
	void AddProduction(const FCinematicProduction& ProductionToAdd);

	/** Adds a duplicate of the input production to the list */
	void DuplicateProduction(FGuid ProductionID);

	/** Removes the production matching the input ID from the list */
	void DeleteProduction(FGuid ProductionID);

	/** Renames the production matching the input ID */
	void RenameProduction(FGuid ProductionID, FString NewName);

	/** Returns a copy of the active production (if there is one) */
	TOptional<const FCinematicProduction> GetActiveProduction() const;

	/** Returns the unique ID of the active production */
	FGuid GetActiveProductionID() const;

	/** Sets the active production based on the input production ID */
	void SetActiveProduction(FGuid ProductionID);

	/** Returns true if the input production ID matches the ID of the active production */
	bool IsActiveProduction(FGuid ProductionID) const;

	/** Returns the DefaultDisplayRate of the active production, or the underlying level sequence setting if there is no active production */
	FFrameRate GetActiveDisplayRate() const;

	/** Returns the DefaultStartFrame of the active production, or the value based on the underlying movie scene tools setting if there is no active production */
	int32 GetActiveStartFrame() const;

	/** Returns the SubsequencePriority of the active production, or the default config value if there is no active production */
	ESubsequencePriority GetActiveSubsequencePriority() const;

	/** Sets the DefaultDisplayRate of the production matching the input ID */
	void SetDisplayRate(FGuid ProductionID, FFrameRate DisplayRate);

	/** Sets the DefaultStartFrame of the production matching the input ID */
	void SetStartFrame(FGuid ProductionID, int32 StartFrame);

	/** Sets the SubsequencePriority of the production matching the input ID */
	void SetSubsequencePriority(FGuid ProductionID, ESubsequencePriority Priority);

	/** Adds a Naming Token namespace to the DenyList of the production matching the input ID */
	void AddNamespaceToDenyList(FGuid ProductionID, const FString& Namespace);

	/** Removes a Naming Token namespace from the DenyList of the production matching the input ID */
	void RemoveNamespaceFromDenyList(FGuid ProductionID, const FString& Namespace);

	/** Adds a new entry into the DefaultAssetNames map of the production matching the input ID */
	void AddAssetNaming(FGuid ProductionID, const UClass* AssetClass, const FString& DefaultName);

	/** Removes an entry from the DefaultAssetNames map of the production matching the input ID */
	void RemoveAssetNaming(FGuid ProductionID, const UClass* AssetClass);

	/** Add a new path to the input production's list of template folders */
	void AddTemplateFolder(FGuid ProductionID, const FString& Path, bool bCreateIfMissing=true);

	/** Removes a path from the input production's list of template folders */
	void RemoveTemplateFolder(FGuid ProductionID, const FString& Path);

	/** Sets the input production's template folder hierarchy to the input array of template folders */
	void SetTemplateFolderHierarchy(FGuid ProductionID, const TArray<FFolderTemplate>& TemplateHierarchy);

	/** Sets the DefaultDisplayRate of the active production */
	void SetActiveDisplayRate(FFrameRate DisplayRate);

	/** Sets the DefaultStartFrame of the active production */
	void SetActiveStartFrame(int32 StartFrame);

	/** Sets the SubsequencePriority of the active production */
	void SetActiveSubsequencePriority(ESubsequencePriority Priority);

	/** Returns a new unique production name */
	FString GetUniqueProductionName() const;
	FString GetUniqueProductionName(const FString& BaseName) const;

	/** Returns the delegate that broadcasts when a production is added/removed */
	FOnProductionListChanged& OnProductionListChanged() { return ProductionListChangedDelegate; }

	/** Returns the delegate that broadcasts when the active production changes */
	FOnActiveProductionChanged& OnActiveProductionChanged() { return ActiveProductionChangedDelegate; }

private:
	/** Applies overrides to various project settings based on the active production settings */
	void ApplyProjectOverrides();

	/** Overrides the DefaultDisplayRate in the level sequence project settings based on the active production setting */
	void OverrideDefaultDisplayRate();

	/** Overrides the DefaultStartTime in the movie scene tools project settings based on the active production settings */
	void OverrideDefaultStartTime();

	/** Writes out the Hierarchical bias value (based on the Subsequence Priority setting) to EditorPerProjectUserSettings.ini */
	void OverrideSubsequenceHierarchicalBias();

	/** Overrides the DefaultAssetNames property of the asset tools project settings based on the active production setting */
	void OverrideDefaultAssetNames();

	/** Apply the active production's namespace deny list to the input set of namespace names */
	void FilterNamingTokenNamespaces(TSet<FString>& Namespaces);

	/** Set the active production name and write out the new value to EditorPerProjectUserSettings.ini */
	void SetActiveProductionName();

	/** Cache the default project settings that are overridden by the active production, used to reset when there is no active production */
	void CacheProjectDefaults();

	/** Returns a pointer to the active production (if it exists) */
	const FCinematicProduction* GetActiveProductionPtr() const;

	/** Try to update the default config file (will attempt to make the file writable if needed) */
	void UpdateConfig();

private:
	/** Name of the active production */
	UPROPERTY(VisibleAnywhere, Category = "Default", meta = (DisplayName = "Active Production"))
	FString ActiveProductionName;

	/** List of available productions in this project */
	UPROPERTY(config, EditAnywhere, Category = "Default")
	TArray<FCinematicProduction> Productions;

	/** ID of the active production (in the Productions array) */
	FGuid ActiveProductionID;

	/** Cached default project settings that are overridden by the active production, used to reset when there is no active production */
	FString ProjectDefaultDisplayRate;
	float ProjectDefaultStartTime;

	/** Default asset names previously registered with AssetTools, used to reset them when the active production changes */
	TMap<const UClass*, FString> ProjectDefaultAssetNames;

	/** Original tooltip text for sequencer settings */
	FString OriginalDefaultDisplayRateTooltip;
	FString OriginalDefaultStartTimeTooltip;

	/** Delegate that broadcasts when a production is added/removed */
	FOnProductionListChanged ProductionListChangedDelegate;

	/** Delegate that broadcasts when the active production changes */
	FOnActiveProductionChanged ActiveProductionChangedDelegate;
};
