// Copyright Epic Games, Inc. All Rights Reserved.

#include "AudioMotorModelComponent.h"
#include "IAudioMotorSimOutput.h"
#include "IAudioMotorSim.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Math/Color.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AudioMotorModelComponent)

namespace AudioMotorModelComponentCVars
{
	FString DebugPrintMotorModel = TEXT("0");
	FAutoConsoleVariableRef CVarDebugPrintMotorModel(
		TEXT("Fort.VehicleAudio.DebugMotorModel"),
		DebugPrintMotorModel,
		TEXT("Prints the motor model component data.\n0: Disable, 1: Enable, or substring to filter"),
		ECVF_Default);
}


namespace AudioMotorModelComponentPrivate	
{
	template <typename ParamType>
	void DebugPrintParam(const FName ParamName, ParamType Value, const FColor& TextColor, const UObject* WorldContextObject)
	{
		static_assert(
		std::is_same_v<ParamType, float> ||
		std::is_same_v<ParamType, int32> ||
		std::is_same_v<ParamType, bool>,
		"Unsupported parameter type.");
				
		const FString ParamNameAsString = ParamName.ToString();
		FString Message;

		const UObject* Owner = WorldContextObject ? WorldContextObject->GetOuter() : WorldContextObject;
		const FString ContextObjectName = Owner ? Owner->GetName() : TEXT("");
		
		// NOTE: This currently only supports the parameter types currently being used.
		// New cases will need to be added for new parameter types.
		if constexpr (std::is_same_v<ParamType, float>)
		{
			Message = FString::Printf(TEXT("[%s] %s: %f"), *ContextObjectName, *ParamNameAsString, Value);
		}
		else if constexpr (std::is_same_v<ParamType, int32>)
		{
			Message = FString::Printf(TEXT("[%s] %s: %d"), *ContextObjectName, *ParamNameAsString, Value);
		}
		else if constexpr (std::is_same_v<ParamType, bool>)
		{
			Message = FString::Printf(TEXT("[%s] %s: %s"), *ContextObjectName, *ParamNameAsString, Value ? TEXT("True") : TEXT("False"));
		}

		constexpr bool bPrintToScreen = true;
		constexpr bool bPrintToLog = false;
		constexpr float TimeToDisplay = 0.f;

		const FString AudioMotorModelComponentDebugValue = AudioMotorModelComponentCVars::DebugPrintMotorModel;
	
		if(!AudioMotorModelComponentDebugValue.IsEmpty() && !Message.Contains(AudioMotorModelComponentDebugValue))
		{
			return;
		}

		FName DebugKey = FName(FString::Printf(TEXT("%s_%s"), *ContextObjectName, *ParamNameAsString));
		
		UKismetSystemLibrary::PrintString(
			WorldContextObject,
			Message,
			bPrintToScreen,
			bPrintToLog,
			TextColor,
			TimeToDisplay,
			DebugKey);
	}
	
    void DebugPrintString(const FString& Message, const FColor& TextColor, const UObject* WorldContextObject)
    { 
    	constexpr bool bPrintToScreen = true;
    	constexpr bool bPrintToLog = false;
    	constexpr float TimeToDisplay = 0.f;

		const UObject* Owner = WorldContextObject ? WorldContextObject->GetOuter() : WorldContextObject;
		const FString ContextObjectName = Owner ? Owner->GetName() : TEXT("");
		FString StringToPrint = FString::Printf(TEXT("[%s] %s"), *ContextObjectName, *Message);

		const FString AudioMotorModelComponentDebugValue = AudioMotorModelComponentCVars::DebugPrintMotorModel;
		
		if(!AudioMotorModelComponentDebugValue.IsEmpty() && !StringToPrint.Contains(AudioMotorModelComponentDebugValue))
		{
			return;
		}
    	
    	UKismetSystemLibrary::PrintString(
    		WorldContextObject,
    		StringToPrint,
			bPrintToScreen,
    		bPrintToLog,
    		TextColor,
    		TimeToDisplay,
    		FName(StringToPrint));
    }
}

void UAudioMotorModelComponent::Update(const FAudioMotorSimInputContext& Input)
{
	CachedInputContext = Input;
	
	for(const FMotorSimEntry& Entry : SimComponents)
	{
		if(Entry.Sim && Entry.Sim->GetEnabled())
		{
			Entry.Sim->Update(CachedInputContext, CachedRuntimeContext);
		}
	}

	for(TScriptInterface<IAudioMotorSimOutput>& Component : AudioComponents)
	{
		if(Component)
		{
			Component->Update(CachedInputContext, CachedRuntimeContext);
		}
	}
	
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

	const FString& AudioMotorModelComponentDebugValue = AudioMotorModelComponentCVars::DebugPrintMotorModel;
	
	if(AudioMotorModelComponentDebugValue != TEXT("0") && AudioMotorModelComponentDebugValue.ToLower() != TEXT("false"))
	{
		DebugPrintInputContext();
		DebugPrintRuntimeInfo();
	}
#endif
}
	
void UAudioMotorModelComponent::Reset()
{
	for(FMotorSimEntry& Entry : SimComponents)
	{
		if(Entry.Sim.GetObject() && Entry.Sim.GetObject()->IsValidLowLevel())
		{
			Entry.Sim->Reset();
		}
	}
}

void UAudioMotorModelComponent::StartOutput()
{
	for(TScriptInterface<IAudioMotorSimOutput> Component : AudioComponents)
	{
		if(Component)
		{
			Component->StartOutput();
		}
	}
}
	
void UAudioMotorModelComponent::StopOutput()
{
	for(TScriptInterface<IAudioMotorSimOutput> Component : AudioComponents)
	{
		if(Component)
		{
			Component->StopOutput();
		}
	}
}

void UAudioMotorModelComponent::AddMotorAudioComponent(TScriptInterface<IAudioMotorSimOutput> InComponent)
{
	if(InComponent)
	{
		AudioComponents.Add(InComponent);
	}
}
	
void UAudioMotorModelComponent::RemoveMotorAudioComponent(TScriptInterface<IAudioMotorSimOutput> InComponent)
{
	if(InComponent)
	{
		AudioComponents.Remove(InComponent);
	}
}
	
void UAudioMotorModelComponent::AddMotorSimComponent(TScriptInterface<IAudioMotorSim> InComponent, const int32 SortOrder)
{
	const FMotorSimEntry NewEntry = {InComponent, SortOrder};
	
	for(auto SimIt = SimComponents.CreateIterator(); SimIt; ++SimIt)
	{
		if(SimIt->SortOrder > SortOrder)
		{
			const int32 Index = SimIt.GetIndex();
			if(SimIt.GetIndex() == 0)
			{
				SimComponents.Insert(NewEntry, 0);
				return;
			}
			
			SimComponents.Insert(NewEntry, Index);
			return;
		}
	}
	
	SimComponents.Add(NewEntry);
}
	
void UAudioMotorModelComponent::RemoveMotorSimComponent(TScriptInterface<IAudioMotorSim> InComponent)
{
	for(auto SimIt = SimComponents.CreateIterator(); SimIt; ++SimIt)
	{
		if(SimIt->Sim == InComponent)
		{
			SimIt.RemoveCurrent();
			return;
		}
	}
}

void UAudioMotorModelComponent::ConfigureMotorSimComponents(const TArray<FInstancedStruct>& InConfigData)
{
	for(const FInstancedStruct& ConfigData : InConfigData)
	{
		for(FMotorSimEntry& SimComponent : SimComponents)
		{
			if(SimComponent.Sim.GetObject() && SimComponent.Sim.GetObject()->IsValidLowLevel())
			{
				SimComponent.Sim->ConfigMotorSim(ConfigData);
			}
		}
	}
}

void UAudioMotorModelComponent::RemoveAllMotorSimComponents()
{
	StopOutput();
	SimComponents.Empty();
}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
void UAudioMotorModelComponent::DebugPrintRuntimeInfo() const
{
	const FColor TextColor = FColor::Red;
	
	AudioMotorModelComponentPrivate::DebugPrintParam("RPM", CachedRuntimeContext.Rpm, TextColor, this);
	AudioMotorModelComponentPrivate::DebugPrintParam("Gear", CachedRuntimeContext.Gear, TextColor, this);
	AudioMotorModelComponentPrivate::DebugPrintParam("Volume", CachedRuntimeContext.Volume, TextColor, this);
	AudioMotorModelComponentPrivate::DebugPrintParam("Is Shifting", CachedRuntimeContext.bShifting, TextColor, this);
	AudioMotorModelComponentPrivate::DebugPrintParam("Pitch", CachedRuntimeContext.Pitch, TextColor, this);
	
	FString HeaderDebugMessage = FString::Printf(TEXT("=== [%s] Audio Motor Model Component Runtime Info ==="), *this->GetFName().ToString());
	AudioMotorModelComponentPrivate::DebugPrintString(HeaderDebugMessage, TextColor, this);
}

void UAudioMotorModelComponent::DebugPrintInputContext() const
{
	const FColor TextColor = FColor::Red;
	
	AudioMotorModelComponentPrivate::DebugPrintParam("DeltaTime", CachedInputContext.DeltaTime, TextColor, this);
	AudioMotorModelComponentPrivate::DebugPrintParam("Throttle", CachedInputContext.Throttle, TextColor, this);
	AudioMotorModelComponentPrivate::DebugPrintParam("Brake", CachedInputContext.Brake, TextColor, this);
	AudioMotorModelComponentPrivate::DebugPrintParam("Speed", CachedInputContext.Speed, TextColor, this);
	AudioMotorModelComponentPrivate::DebugPrintParam("ForwardSpeed", CachedInputContext.ForwardSpeed, TextColor, this);
	AudioMotorModelComponentPrivate::DebugPrintParam("SideSpeed", CachedInputContext.SideSpeed, TextColor, this);
	AudioMotorModelComponentPrivate::DebugPrintParam("UpSpeed", CachedInputContext.UpSpeed, TextColor, this);
	AudioMotorModelComponentPrivate::DebugPrintParam("SurfaceFrictionModifier", CachedInputContext.SurfaceFrictionModifier, TextColor, this);
	AudioMotorModelComponentPrivate::DebugPrintParam("MotorFrictionModifier", CachedInputContext.MotorFrictionModifier, TextColor, this);
	AudioMotorModelComponentPrivate::DebugPrintParam("Boost", CachedInputContext.Boost, TextColor, this);
	AudioMotorModelComponentPrivate::DebugPrintParam("bDriving", CachedInputContext.bDriving, TextColor, this);
	AudioMotorModelComponentPrivate::DebugPrintParam("bGrounded", CachedInputContext.bGrounded, TextColor, this);
	AudioMotorModelComponentPrivate::DebugPrintParam("bCanShift", CachedInputContext.bCanShift, TextColor, this);
	AudioMotorModelComponentPrivate::DebugPrintParam("bClutchEngaged", CachedInputContext.bClutchEngaged, TextColor, this);
	
	FString HeaderDebugMessage = FString::Printf(TEXT("=== [%s] Audio Motor Model Input Context ==="), *this->GetFName().ToString());
	AudioMotorModelComponentPrivate::DebugPrintString(HeaderDebugMessage, TextColor, this);
}
#endif