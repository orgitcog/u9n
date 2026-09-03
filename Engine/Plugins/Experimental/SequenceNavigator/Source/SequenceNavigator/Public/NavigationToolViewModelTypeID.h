// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "NavigationToolCastableTypeTable.h"

namespace UE::SequenceNavigator
{

#define UE_NAVIGATIONTOOL_DECLARE_VIEW_MODEL_TYPE_ID(Type)							\
	static ::UE::Sequencer::TNavigationToolAutoRegisterViewModelTypeID<Type> ID;	\
	static void RegisterTypeID();

#define UE_NAVIGATIONTOOL_DECLARE_VIEW_MODEL_TYPE_ID_API(MODULE_API, Type)					\
	MODULE_API static ::UE::Sequencer::TNavigationToolAutoRegisterViewModelTypeID<Type> ID;	\
	MODULE_API static void RegisterTypeID();

#define UE_NAVIGATIONTOOL_DEFINE_VIEW_MODEL_TYPE_ID(Type)																			\
	::UE::Sequencer::TNavigationToolAutoRegisterViewModelTypeID<Type> Type::ID;														\
	void Type::RegisterTypeID()																										\
	{																																\
		Type::ID.ID        = ::UE::Sequencer::FNavigationToolViewModelTypeID::RegisterNewID();										\
		Type::ID.TypeTable = ::UE::Sequencer::FNavigationToolCastableTypeTable::MakeTypeTable<Type>((Type*)0, Type::ID.ID, #Type);	\
	}

struct FNavigationToolCastableTypeTable;

struct FNavigationToolViewModelTypeID
{
	friend uint32 GetTypeHash(FNavigationToolViewModelTypeID In)
	{
		return In.ID;
	}

	friend bool operator<(FNavigationToolViewModelTypeID A, FNavigationToolViewModelTypeID B)
	{
		return A.ID < B.ID;
	}

	friend bool operator==(FNavigationToolViewModelTypeID A, FNavigationToolViewModelTypeID B)
	{
		return A.ID == B.ID;
	}

	uint32 GetTypeID() const
	{
		return ID;
	}

	SEQUENCERCORE_API static uint32 RegisterNewID();

	FNavigationToolViewModelTypeID(FNavigationToolCastableTypeTable* InTypeTable, uint32 InID)
		: TypeTable(InTypeTable)
		, ID(InID)
	{}

private:
	FNavigationToolCastableTypeTable* TypeTable = nullptr;
	uint32 ID;
};

template<typename T>
struct TNavigationToolViewModelTypeID
{
	operator FNavigationToolViewModelTypeID() const
	{
		Register();
		return FNavigationToolViewModelTypeID{ TypeTable, ID };
	}

	uint32 GetTypeID() const
	{
		Register();
		return ID;
	}

	FNavigationToolCastableTypeTable* GetTypeTable() const
	{
		Register();
		return TypeTable;
	}

	void Register() const
	{
		if (!IsRegistered())
		{
			T::RegisterTypeID();
		}
	}

protected:
	friend T;

	bool IsRegistered() const
	{
		return ID != ~0u;
	}

	FNavigationToolCastableTypeTable* TypeTable = nullptr;
	uint32 ID = ~0u;
};

template<typename T>
struct TNavigationToolAutoRegisterViewModelTypeID : TNavigationToolViewModelTypeID<T>
{
};

} // namespace UE::SequenceNavigator
