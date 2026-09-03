// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassEntityElementTypes.h"
#include "Subsystems/Subsystem.h"


namespace UE::Mass
{
	template<typename T>
	concept CFragment = TIsDerivedFrom<typename TRemoveReference<T>::Type, FMassFragment>::Value;

	template<typename T>
	concept CTag = TIsDerivedFrom<typename TRemoveReference<T>::Type, FMassTag>::Value;

	template<typename T>
	concept CChunkFragment = TIsDerivedFrom<typename TRemoveReference<T>::Type, FMassChunkFragment>::Value;

	template<typename T>
	concept CSharedFragment = TIsDerivedFrom<typename TRemoveReference<T>::Type, FMassSharedFragment>::Value;

	template<typename T>
	concept CConstSharedFragment = TIsDerivedFrom<typename TRemoveReference<T>::Type, FMassConstSharedFragment>::Value;

	template<typename T>
	concept CNonTag = CFragment<T> || CChunkFragment<T> || CSharedFragment<T> || CConstSharedFragment<T>;

	template<typename T>
	concept CElement = CNonTag<T> || CTag<T>;

	template<typename T>
	concept CSubsystem = TIsDerivedFrom<typename TRemoveReference<T>::Type, USubsystem>::Value;

	namespace Private
	{
		template<CElement T>
		struct TElementTypeHelper
		{
			using Type = std::conditional_t<CFragment<T>, FMassFragment
				, std::conditional_t<CTag<T>, FMassTag
				, std::conditional_t<CChunkFragment<T>, FMassChunkFragment
				, std::conditional_t<CSharedFragment<T>, FMassSharedFragment
				, std::conditional_t<CConstSharedFragment<T>, FMassConstSharedFragment
				, void>>>>>;
		};
	}

	template<typename T>
	using TElementType = typename Private::TElementTypeHelper<T>::Type;
}
