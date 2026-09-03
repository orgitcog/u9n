// Copyright Epic Games, Inc. All Rights Reserved.


#include "PCapDataTable.h"
#include "PCapDatabase.h"
#include "DataTableEditorUtils.h"

UPCapDataTable::UPCapDataTable()
{
	RowStruct = FPCapRecordBase::StaticStruct();
	bStripFromClientBuilds = true; //Prevent any Performance Capture data from cooking into client builds as this is editor-only tooling
	
	OnDataTableChanged().AddUObject(this, &UPCapDataTable::DataTableModified);
}

UPCapDataTable::~UPCapDataTable()
{

}

void UPCapDataTable::DataTableModified() const
{
	OnDatatableModified.Broadcast();
}

bool UPCapDataTable::RemoveTableRow(FName RowName)
{
	return FDataTableEditorUtils::RemoveRow(this, RowName);
}

bool UPCapDataTable::DuplicateTableRow(FName SourceRow, FName NewRow)
{
	if (FDataTableEditorUtils::DuplicateRow(this, SourceRow, NewRow) !=NULL)
	{
		return true;
	}
	return false;
}

bool UPCapDataTable::AddTableRow(FName NewRow)
{
	if(FDataTableEditorUtils::AddRow(this, NewRow) == nullptr)
	{
		return false;
	}
	return true;
}

bool UPCapDataTable::InsertTableRow(FName SelectedRow, FName NewRow, bool bAbove)
{
	ERowInsertionPosition Postion = ERowInsertionPosition::Above;
	if(!bAbove)
	{
		Postion = ERowInsertionPosition::Below;	
	}
	
	if(FDataTableEditorUtils::AddRowAboveOrBelowSelection(this, SelectedRow, NewRow, Postion)!=NULL)
	{
		return true;
	}
	return false;
}

