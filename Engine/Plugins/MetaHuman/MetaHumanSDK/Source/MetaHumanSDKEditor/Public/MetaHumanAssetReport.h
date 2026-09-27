// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UObject/Object.h"

#include "MetaHumanAssetReport.generated.h"

/** The status of an operation represented by a report */
UENUM(BlueprintType)
enum class EMetaHumanOperationResult : uint8
{
	Success,
	Failure
};

/** A line in a report representing some Info, a Warning or an Error. Can optionally reference a specific item or file. **/
USTRUCT(BlueprintType)
struct FMetaHumanAssetReportItem
{
	GENERATED_BODY()

	/* The message to display to the user */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = " MetaHuman SDK | Reports ")
	FText Message;

	/* The relevant object (if any) in the project to which this message relates */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = " MetaHuman SDK | Reports ")
	TObjectPtr<const UObject> ProjectItem = nullptr;

	/* The file path (if any) to which this message relates */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = " MetaHuman SDK | Reports ")
	FString SourceItem;
};


/** A report generated when an asset is imported or tested for MetaHuman compatibility */
UCLASS(BlueprintType)
class METAHUMANSDKEDITOR_API UMetaHumanAssetReport : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Set the subject for the report, typically the name of the asset being tested or imported
	 *
	 * @param InSubject The Name to appear in the title of the report
	 */
	UFUNCTION(BlueprintCallable, Category = " MetaHuman SDK | Reports ")
	void SetSubject(const FString& InSubject);

	/**
	 * Adds a user-facing message to appear in the report. This will not flag the report as containing warnings or as
	 * having failed and will be discarded if SetVerbose is not called with a value of true
	 *
	 * @param Message The localized informational message
	 */
	UFUNCTION(BlueprintCallable, Category = " MetaHuman SDK | Reports ")
	void AddVerbose(const FMetaHumanAssetReportItem& Message);

	/**
	 * Adds a user-facing message to appear in the report. This will not flag the report as containing warnings or as
	 * having failed.
	 *
	 * @param Message The localized informational message
	 */
	UFUNCTION(BlueprintCallable, Category = " MetaHuman SDK | Reports ")
	void AddInfo(const FMetaHumanAssetReportItem& Message);

	/**
	 * Adds a user-facing message to appear in the report. This will flag the report as containing warnings but will
	 * not flag it as having failed.
	 *
	 * @param Message The localized warning message
	 */
	UFUNCTION(BlueprintCallable, Category = " MetaHuman SDK | Reports ")
	void AddWarning(const FMetaHumanAssetReportItem& Message);

	/**
	 * Adds a user-facing message to appear in the report. This will flag the report as containing warnings and as
	 * having failed.
	 *
	 * @param Message The localized error message
	 */
	UFUNCTION(BlueprintCallable, Category = " MetaHuman SDK | Reports ")
	void AddError(const FMetaHumanAssetReportItem& Message);

	/** Generates an HTML representation of the report */
	UFUNCTION(BlueprintCallable, Category = " MetaHuman SDK | Reports ")
	FString GenerateHtmlReport() const;

	/** Generates a JSON representation of the report */
	UFUNCTION(BlueprintCallable, Category = " MetaHuman SDK | Reports ")
	FString GenerateJsonReport() const;

	/** Generates a plain text representation of the report */
	UFUNCTION(BlueprintCallable, Category = " MetaHuman SDK | Reports ")
	FString GenerateRawReport() const;

	/** Generates a representation of the report suitable for use in an SRichText control */
	UFUNCTION(BlueprintCallable, Category = " MetaHuman SDK | Reports ")
	FText GenerateRichTextReport() const;

	/** Determine whether the report represents a successful operation or not */
	UFUNCTION(BlueprintCallable, Category = " MetaHuman SDK | Reports ")
	EMetaHumanOperationResult GetReportResult() const;

	/** Determine whether the report contains non-informational messages */
	UFUNCTION(BlueprintCallable, Category = " MetaHuman SDK | Reports ")
	bool HasWarnings() const;

	/**
	 * Set whether warnings should be reported as errors
	 */
	UFUNCTION(BlueprintCallable, Category = " MetaHuman SDK | Reports ")
	void SetWarningsAsErrors(const bool Value);

	/**
	 * Set whether to include verbose items in the report
	 */
	UFUNCTION(BlueprintCallable, Category = " MetaHuman SDK | Reports ")
	void SetVerbose(const bool Value);

	/**
	 * The Subject of the Report
	 */
	UPROPERTY()
	FString Subject;

	/**
	* The Info Items in the Report
	*/
	UPROPERTY()
	TArray<FMetaHumanAssetReportItem> Infos;

	/**
	* The Warnings in the Report
	*/
	UPROPERTY()
	TArray<FMetaHumanAssetReportItem> Warnings;

	/**
	* The Errors in the Report
	*/
	UPROPERTY()
	TArray<FMetaHumanAssetReportItem> Errors;

private:
	bool bWarningsAsErrors;
	bool bVerbose;
};
