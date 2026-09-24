// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"
#include "Widgets/Views/SHeaderRow.h"
#include "LiveLinkDeviceCapability.generated.h"


class SWidget;
class ULiveLinkDevice;
class ULiveLinkDeviceCapability;


/** Helper passed to GenerateWidgetForColumn. */
struct FLiveLinkDeviceWidgetArguments
{
	TDelegate<bool()> IsRowSelected;
};


/**
 * Base class for all device capabilities.
 *
 * Every IInterface has a corresponding UInterface, which exists only as a
 * class default object (CDO), and provides a place to manage centralized
 * state related to the capability.
 */
UINTERFACE(Blueprintable)
class LIVELINKDEVICE_API ULiveLinkDeviceCapability : public UInterface
{
	GENERATED_BODY()

public:
	struct FDeviceTableColumnDesc
	{
		// TODO: Default show/hide, sorting hint, ?
	};

public:
	ULiveLinkDeviceCapability()
	{
		ensure(HasAllFlags(RF_ClassDefaultObject));
	}

	/** Called at completion of Live Link device subsystem initialization. */
	virtual void OnDeviceSubsystemInitialized() {}

	/** Called at the beginning of Live Link device subsystem de-initialization. */
	virtual void OnDeviceSubsystemDeinitializing() {}

	/** Return device table widget columns this capability provides. */
	const TMap<FName, FDeviceTableColumnDesc>& GetTableColumns() const
	{
		return TableColumns;
	}

	/** Configure the header for the specified column. */
	virtual SHeaderRow::FColumn::FArguments& GenerateHeaderForColumn(
		const FName InColumnId,
		SHeaderRow::FColumn::FArguments& InArgs
	)
	{
		// This base class implementation should not get called.
		// If your capability defines table columns, you need to implement this.
		ensure(false);
		return InArgs;
	}

	/** Optional; allows a capability to provide a default widget the device can fall back to. */
	virtual TSharedPtr<SWidget> GenerateWidgetForColumn(
		const FName InColumnId,
		const FLiveLinkDeviceWidgetArguments& InArgs,
		ULiveLinkDevice* InDevice
	)
	{
		return nullptr;
	}

protected:
	/**
	 * Call this from your derived class to define a new device table column.
	 *
	 * @return The full, namespaced column identifier for future reference.
	 */
	FName RegisterTableColumn(const FName InColumnShortName)
	{
		const FName ColumnId = ExpandColumnShortName(InColumnShortName);
		TableColumns.Emplace(ColumnId, {});
		return ColumnId;
	}

	FName ExpandColumnShortName(FName InShortName) const
	{
		FNameBuilder NameBuilder;
		GetClass()->GetFName().AppendString(NameBuilder);
		NameBuilder.AppendChar('.');
		InShortName.AppendString(NameBuilder);
		return FName(NameBuilder.ToView());
	}

private:
	TMap<FName, FDeviceTableColumnDesc> TableColumns;
};


class LIVELINKDEVICE_API ILiveLinkDeviceCapability
{
	GENERATED_BODY()
};
