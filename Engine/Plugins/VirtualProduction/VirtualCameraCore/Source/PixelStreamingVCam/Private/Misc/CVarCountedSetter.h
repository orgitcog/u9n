// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/IConsoleManager.h"
#include "HAL/Platform.h"

namespace UE::PixelStreamingVCam
{
	/** Counter that while non-zero, sets the specified CVar a desired value and once it reaches zero, the CVar is reset to the previous value. */
	template<typename T>
	class TCVarCountedSetter
	{
	public:

		explicit TCVarCountedSetter(FString InName, T InDesiredValue)
			: Name(MoveTemp(InName)), DesiredValue(InDesiredValue)
		{}

		void Increment()
		{
			++Count;
			
			IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(*Name);
			if (!ensure(Variable))
			{
				return;
			}
			
			if (Count == 1)
			{
				Variable->GetValue(RestoreValue);
				Variable->Set(DesiredValue);
			}
		}
		
		void Decrement()
		{
			--Count;
			
			IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(*Name);
			if (!Variable)
			{
				return;
			}
			
			if (Count == 0)
			{
				Variable->Set(RestoreValue);
			}
		}

	private:

		const FString Name;
		const T DesiredValue;
		T RestoreValue;

		int32 Count = 0;
	};
}

