// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageUiInterface.h"

#include "WidgetPurposeColumns.generated.h"

/** Column used to store the type of a widget purpose */
USTRUCT()
struct FWidgetPurposeColumn : public FEditorDataStorageColumn
{
	GENERATED_BODY()

	/** The type of the purpose, used to determine how factories are registered for it */
	UE::Editor::DataStorage::IUiProvider::EPurposeType PurposeType;

	/** The unique ID of this purpose that can be used to reference it instead of the row handle */
	UE::Editor::DataStorage::IUiProvider::FPurposeID PurposeID;
};

/** Column used to store the name of a widget purpose split into 3 parts (E.g "SceneOutliner.Cell.Large") */
USTRUCT()
struct FWidgetPurposeNameColumn : public FEditorDataStorageColumn
{
	GENERATED_BODY()

	UPROPERTY(meta = (Searchable))
	FName Namespace;

	UPROPERTY(meta = (Searchable))
	FName Name;

	UPROPERTY(meta = (Searchable))
	FName Frame;
};


/** Column to store info about a widget factory */
USTRUCT()
struct FWidgetFactoryColumn : public FEditorDataStorageColumn
{
	GENERATED_BODY()

	/** Row handle of the purpose the widget factory belongs to */
	UE::Editor::DataStorage::RowHandle PurposeRowHandle;
};

/** Column used to store the widget constructor used by a widget factory by its TypeInfo
 *  A factory can either have FWidgetFactoryConstructorTypeInfoColumn or FWidgetFactoryConstructorColumn
 */
USTRUCT()
struct FWidgetFactoryConstructorTypeInfoColumn : public FEditorDataStorageColumn
{
	GENERATED_BODY()

	TWeakObjectPtr<const UScriptStruct> Constructor;
};

/** Column used to store the widget constructor used by a widget factory */
USTRUCT()
struct FWidgetFactoryConstructorColumn : public FEditorDataStorageColumn
{
	GENERATED_BODY()

	TUniquePtr<FTypedElementWidgetConstructor> Constructor;
};

template<>
struct TStructOpsTypeTraits<FWidgetFactoryConstructorColumn> : public TStructOpsTypeTraitsBase2<FWidgetFactoryConstructorColumn>
{
	enum
	{
		WithCopy = false
	};
};

/** Column used to store the query conditions used by a widget factory
 *  If the column is not present the factory doesn't match against any conditions (i.e is a general purpose factory)
 */
USTRUCT()
struct FWidgetFactoryConditionsColumn : public FEditorDataStorageColumn
{
	GENERATED_BODY()

	UE::Editor::DataStorage::Queries::FConditions Conditions;
};
