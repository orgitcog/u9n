// Copyright Epic Games, Inc. All Rights Reserved.

#include "TypeFamily/ChannelTypeFamily.h"

#include "CoreGlobals.h"
#include "Containers/Map.h"
#include "DSP/ChannelMap.h"
#include "DSP/MultiMono.h"

namespace Audio
{
	namespace ChannelTypeFamilyPrviate
	{
		static FString MakePrettyString(const TArray<ESpeakerShortNames>& InEnums)
		{
			return FString::JoinBy(InEnums,TEXT(", "), [](ESpeakerShortNames InEnum) -> FString { return LexToString(InEnum); });
		}
	}
	
	class FChannelRegistryImpl final : public IChannelTypeRegistry 
	{
		TMap<FName, const FChannelTypeFamily*> Types;
		TMap<int32, const FChannelTypeFamily*> ChannelCountLookup;
	public:
		UE_NONCOPYABLE(FChannelRegistryImpl);
		FChannelRegistryImpl() = default;
		virtual ~FChannelRegistryImpl() override = default;
	protected:
		virtual bool RegisterType(const FName& InUniqueName, TRetainedRef<FTypeFamily> InType) override
		{
			static const FName ChannelTypeName = TEXT("Cat");
			const FTypeFamily* Type = FindTypeInternal(ChannelTypeName);
			if (Type && !InType.Get().IsA(Type))
			{
				// Not a cat. (maybe a dog?)
				return false;	
			}
			
			// TODO: thread safety here.
			if (Types.Find(InUniqueName) != nullptr)
			{
				return false;
			}
			
			// Safe to cast it and add.
			const FChannelTypeFamily* ChannelType = static_cast<const FChannelTypeFamily*>(&InType.Get());

			// Log.
			//UE_LOG(LogTemp, Display, TEXT("Registering %s, NumChannels=%d"), *ChannelType->GetName().GetPlainNameString(), ChannelType->NumChannels());
			
			// Add to master list of registered types.
			Types.Add(InUniqueName,ChannelType);

			// If this is a default, then add it to our Channel -> Type lookup table. Assert it's unique.
			if (const int32 NumChannels = ChannelType->NumChannels(); NumChannels > 0 && ChannelType->IsParentsDefault())
			{
				const FChannelTypeFamily*& Found = ChannelCountLookup.FindOrAdd(NumChannels);
				checkf(Found == nullptr, TEXT("%d Already registered for type %s"), NumChannels, *Found->GetName().ToString());
				Found = ChannelType;
			}
			return true;
		}	
		virtual const FTypeFamily* FindTypeInternal(const FName InUniqueName) const override
		{
			if (const FChannelTypeFamily* const * Found = Types.Find(InUniqueName))
			{
				return *Found;
			}
			return nullptr;
		}

		virtual const FChannelTypeFamily* FindChannelType(const int32 InNumChannels) override
		{
			if (const FChannelTypeFamily** Found = ChannelCountLookup.Find(InNumChannels))
			{
				check(!(*Found)->IsAbstract());
				return *Found;
			}

			// If we don't have a default lookup for this channel count, look up the first thing that matches.
			using FPair = decltype(Types)::ElementType;
			TArray<FPair> AllTypes = Types.Array();
			if (const FPair* Found = AllTypes.FindByPredicate([InNumChannels](const FPair& InPair) -> bool
			{
				return InPair.Value->NumChannels() == InNumChannels && !InPair.Value->IsAbstract();
			}))
			{
				return Found->Value;	
			}

			// Failed to find anything... :(
			return nullptr;
		}

		virtual TArray<const FChannelTypeFamily*> GetAllChannelFormats() const override
		{
			TArray<const FChannelTypeFamily*> AllFormats;
			Types.GenerateValueArray(AllFormats);
			return AllFormats;
		};
	};

	IChannelTypeRegistry& GetChannelRegistry()
	{
		static FChannelRegistryImpl Registry;
		return Registry;
	}

   FChannelTypeFamily::FChannelTypeFamily(
	   const FName& InUniqueName,
	   const FName& InFamilyTypeName,
	   const int32 InNumChannels,
	   FChannelTypeFamily* InParentType,
	   const FString& InFriendlyName,
	   const bool InbIsParentsDefault,
	   const bool InbIsAbstract)
	   : Super(InUniqueName, InParentType, InFriendlyName)
	   , bIsAbstract(InbIsAbstract)
	   , bIsParentsDefault(InbIsParentsDefault)
	   , NumChannelsPrivate(InNumChannels)
	   , FamilyType(InFamilyTypeName)
	{
		check(InNumChannels >= 0);
		check(!InUniqueName.IsNone());
		if (bIsParentsDefault)
		{
			check(InParentType);
			check(InParentType->DefaultChild == nullptr); 
			InParentType->DefaultChild = this;
		}
	}
	
	
	FDiscreteChannelTypeFamily::FDiscreteChannelTypeFamily(const FName& InUniqueName, FChannelTypeFamily* InParentType, const FString& InFriendlyName, const TArray<ESpeakerShortNames>& InOrder, const bool bIsParentsDefault, const bool bIsAbstract)
		: Super(InUniqueName, GetFamilyTypeName(), InOrder.Num(), InParentType, InFriendlyName, bIsParentsDefault, bIsAbstract)
		, Order(InOrder)
	{
		checkf(InParentType, TEXT("Type=%s, Has a Null Parent"), *GetName().ToString());
		auto GetParentNameSafe = [](FChannelTypeFamily* i) -> FString { return i ? i->GetName().ToString() : FString(); };
		UE_LOG(LogTemp, Display, TEXT("Unique=%s\tNumChannels=%d\tParent=%s\tFriendlyName=%s\tDefault=%s\tOrder=[%s]\tAbstract=%s"),
			*InUniqueName.ToString(),  InOrder.Num(), *GetParentNameSafe(InParentType), *InFriendlyName, ToCStr(LexToString(bIsParentsDefault)), *ChannelTypeFamilyPrviate::MakePrettyString(Order), ToCStr(LexToString(bIsAbstract)));
	}
	
	FChannelTypeFamily::FTranscoder FDiscreteChannelTypeFamily::GetTranscoder(
	const FDiscreteChannelTypeFamily& InFromType, const FGetTranscoderParams& InParams) const
	{
		// Exact match? We can just memcpy each channel.
		// TODO. in future these could be shared-ptrs from the main CAT memory block.
		if (&InFromType == this)
		{
			return [NumChannels = InFromType.NumChannels()](TArrayView<const float*> Src, TArrayView<float*> Dst, const int32 NumFrames)
			{
				for (int32 i = 0; i < NumChannels; ++i)
				{
					FMemory::Memcpy(Dst[i], Src[i], NumFrames * sizeof(float));
				}
			};
		}

		switch (InParams.TranscodeMethod)
		{
			case EChannelTranscodeMethod::ChannelDrop:
			{
				return [&](TArrayView<const float*> SrcChannels, TArrayView<float*> DstChannels, const int32 NumFrames)
				{
					// Copy everything destination wants, and nothing else.
					ensure(Order.Num() == NumChannelsPrivate);
					const TArray<ESpeakerShortNames> SrcOrder = InFromType.Order;
					for (int32 i = 0; i < NumChannelsPrivate; ++i)
					{
						const ESpeakerShortNames DstSpeaker = Order[i];
						if (const int32 SrcChannelIndex = SrcOrder.IndexOfByKey(DstSpeaker); SrcChannelIndex != INDEX_NONE)
						{
							FMemory::Memcpy(DstChannels[i], SrcChannels[SrcChannelIndex], NumFrames * sizeof(float));	
						}
					}
				};
			}
			
			case EChannelTranscodeMethod::MixUpOrDown:
			{
				// Make a mix matrix and call a mix/up down.
				if (TArray<float> Gains; Create2DChannelMap(
					{
						.NumInputChannels = InFromType.NumChannels(),
						.NumOutputChannels = NumChannels(),
						.Order = EChannelMapOrder::OutputMajorOrder,
						.MonoUpmixMethod = InParams.MixMethod,
						.bIsCenterChannelOnly = InFromType.NumChannels() == 1 && InFromType.HasSpeaker(ESpeakerShortNames::FC)
					}, Gains))
				{
					return [MixGains = MoveTemp(Gains)](TArrayView<const float*> InSrc, TArrayView<float*> InDst, const int32 NumFrames)
					{
						MultiMonoMixUpOrDown(InSrc, InDst, NumFrames, MixGains);
					};
				}
			}
		}

		// fail.
		return {};
	}

	TOptional<FChannelTypeFamily::FChannelName> FDiscreteChannelTypeFamily::GetChannelName(const int32 InChannelIndex) const
	{
		check(InChannelIndex >= 0 && InChannelIndex < NumChannels());
		if (Order.IsValidIndex(InChannelIndex))
		{
			const ESpeakerShortNames Speaker = Order[InChannelIndex];
			const FName SpeakerName = LexToString(Speaker);
			return
			{ // Optional.
					{ 
						.Name = SpeakerName,
						.FriendlyName = SpeakerName.ToString() 
					}
			};
		}
		return {};
	}

	void RegisterChannelLayouts(IChannelTypeRegistry& Registry)
	{
		/**
		 * Standard layouts.
		 * This should be defined in .ini ultimately, allowing new, custom formats to be added simply.
		 **/
		
		// Register root type.
		static FChannelTypeFamily BaseCat(TEXT("Cat"), TEXT("Cat"),  0, nullptr, TEXT("Base Cat"), false, true);
		Registry.RegisterType(BaseCat.GetName(), BaseCat);
	
#define REGISTER_CAT(NAME, PARENT_NAME, FRIENDLY_NAME, LAYOUT, DEFAULT,ABSTRACT)\
static FDiscreteChannelTypeFamily UE_JOIN(__Type,NAME)(UE_STRINGIZE(NAME), Registry.FindChannel(PARENT_NAME), FRIENDLY_NAME, LAYOUT, DEFAULT, ABSTRACT);\
Registry.RegisterType(TEXT(UE_STRINGIZE(NAME)),UE_JOIN(__Type,NAME))

		using enum ESpeakerShortNames;

		// Top level abstraction. "Discrete".
		REGISTER_CAT(Discrete,			TEXT("Cat"),		TEXT("Discrete"),						{},										false,					true);
	
		// Mono		(UniqueName)		(Parent Name)			(Friendly Name)										(Channel membership/order.)				(is parents default).  (abstract)
		REGISTER_CAT(Mono,				TEXT("Discrete"),TEXT("Mono"),							{},										false,					true);
		REGISTER_CAT(Mono1Dot0,			TEXT("Mono"),	TEXT("Mono (1.0)"),					(TArray{FC}),							true,					false);
		REGISTER_CAT(Mono1Dot1,			TEXT("Mono"),	TEXT("Mono (1.1)"),					(TArray{FC,LFE}),						false,					false);

		// Stereo																								
		REGISTER_CAT(Stereo,			TEXT("Discrete"), TEXT("Stereo"),						{},										false,					true);
		REGISTER_CAT(Stereo2Dot0,		TEXT("Stereo"),  TEXT("Stereo (2.0)"),					(TArray{FL,FR}),						true,					false);
		REGISTER_CAT(Stereo2Dot1,		TEXT("Stereo"),  TEXT("Stereo (2.1)"),					(TArray{FL,FR,LFE}),					false,					false);
		REGISTER_CAT(Stereo3Dot0,		TEXT("Stereo"),  TEXT("Stereo (3.0)"),					(TArray{FL,FR,FC}),						false,					false);
		REGISTER_CAT(Stereo3DOt1,		TEXT("Stereo"),  TEXT("Stereo (3.1)"),					(TArray{FL,FR,FC,LFE}),					false,					false);
	
		// Quad 
		REGISTER_CAT(Quad,				TEXT("Discrete"),	TEXT("Quad"),						{},										false,					true);
		REGISTER_CAT(Quad4Dot0Back,		TEXT("Quad"),		TEXT("Quad Back Speakers (4.0)"),	(TArray{FL,FR,BL,BR}),					true,					false);
		REGISTER_CAT(Quad4Dot0Side,		TEXT("Quad"),		TEXT("Quad Side Speakers (4.0)"),	(TArray{FL,FR,SL,SR}),					false,					false);
		REGISTER_CAT(Quad4Dot1,			TEXT("Quad"),		TEXT("Quad Back Centre LFE (4.1)"),(TArray{FL,FR,BL,BR,LFE}),				false,					false);

		// Surround
		REGISTER_CAT(Surround,			TEXT("Discrete"),	TEXT("Surround"),					{},										false,					true);
		
		REGISTER_CAT(Surround5,			TEXT("Surround"),	TEXT("Surround (5.X)"),			{},										true,					true);
		REGISTER_CAT(Surround5Dot0,		TEXT("Surround5"),  TEXT("Surround (5.0)"),			(TArray{FR,FR,BL,BR,FC}),				false,					false);
		REGISTER_CAT(Surround5_1,		TEXT("Surround5"),  TEXT("Surround (5.1)"),			(TArray{FL,FR,BL,BR,FC,LFE}),			true,					false);

		REGISTER_CAT(Surround7,			TEXT("Surround"),	TEXT("Surround (7.X)"),			{},										false,					true);
		REGISTER_CAT(Surround7Dot0,		TEXT("Surround7"),  TEXT("Surround (7.0)"),			(TArray{FL,FR,SL,SR,FC,BL,BR}),			false,					false);
		REGISTER_CAT(Surround7Dot1,		TEXT("Surround7"),  TEXT("Surround (7.1)"),			(TArray{FL,FR,SL,SR,FC,BL,BR,LFE}),		true,					false);
		
		// Atmos.
		REGISTER_CAT(Atmos,				TEXT("Surround7"),		TEXT("Dolby Atmos Bed"),		{},													false,		true);
		REGISTER_CAT(Atmos7Dot0Dot2,	TEXT("Atmos"),			TEXT("Dolby Atmos (7.0.2)"),	(TArray{FL,FR,SL,SR,FC,BL,BR,TFL,TFR}),				false,		false);
		REGISTER_CAT(Atmos7Dot0Dot4,	TEXT("Atmos"),			TEXT("Dolby Atmos (7.0.4)"),	(TArray{FL,FR,SL,SR,FC,BL,BR,TFL,TFR,TBL,TBR}),		false,		false);
		REGISTER_CAT(Atmos7Dot1Dot2,	TEXT("Atmos"),			TEXT("Dolby Atmos (7.1.2)"),	(TArray{FL,FR,SL,SR,FC,BL,BR,TFL,TFR,LFE}),			false,		false);
		REGISTER_CAT(Atmos7Dot1Dot4,	TEXT("Atmos"),			TEXT("Dolby Atmos (7.1.4)"),	(TArray{FL,FR,SL,SR,FC,BL,BR,TFL,TFR,TBL,TBR,LFE}),	true,		false);

#if 0
		// Channel packs.
		REGISTER_CAT(ChannelPack,		TEXT("Cat"),				TEXT("Channel Pack"),			{},													false,		true);
		
		// Packed mono example.
		// Might make a new type for this, but for now....
		static TArray<FDiscreteChannelTypeFamily> MultiMonos = []() 
		{
			TArray<FDiscreteChannelTypeFamily> Types;
			constexpr int32 NumPacks = 6;
			Types.Reserve(NumPacks);
			for (int32 i = 0; i < NumPacks; ++i)
			{
				constexpr ESpeakerShortNames MonoChannel = FC;
				TArray<ESpeakerShortNames> Order;
				Order.Init(ESpeakerShortNames::FC, i+2); 
				Types.Emplace(
					*FString::Printf(TEXT("MonoPack%d"), i+2),
					GetChannelRegistry().FindChannel(TEXT("ChannelPack")),
					FString::Printf(TEXT("Mono Pack %d Channels"), i+2),
					Order,	
					false,
					false
				);
				GetChannelRegistry().RegisterType(Types[i].GetName(),Types[i]);
			}
			return Types;
		}();
#endif 
					
#define REGISTER_AMBISONICS(NAME, PARENT_NAME, FRIENDLY_NAME, ORDER, DEFAULT,ABSTRACT)\
static FAmbisonicsChannelTypeFamily UE_JOIN(__Type,NAME)(UE_STRINGIZE(NAME), ORDER, Registry.FindChannel(PARENT_NAME), FRIENDLY_NAME, DEFAULT, ABSTRACT);\
ensure(Registry.RegisterType(UE_STRINGIZE(NAME),UE_JOIN(__Type,NAME)))

#if 0
		// Ambisonics 
		REGISTER_AMBISONICS(Ambisonics,				 TEXT("Cat"), 		TEXT("Ambisonics"), 							0, false, true);
		REGISTER_AMBISONICS(AmbisonicsFirstOrder,	 TEXT("Ambisonics"), TEXT("First Order Ambisonics (4 channels)"), 	1, false, false); 
		REGISTER_AMBISONICS(AmbisonicsSecondOrder,	 TEXT("Ambisonics"), TEXT("Second Ambisonics (9 channels)"), 		2, false, false); 
		REGISTER_AMBISONICS(AmbisonicsThirdOrder,	 TEXT("Ambisonics"), TEXT("Third Ambisonics (16 channels)"), 		3, false, false);
		REGISTER_AMBISONICS(AmbisonicsFourthOrder,	 TEXT("Ambisonics"), TEXT("Fourth Ambisonics (25 chhanels)"), 		4, false, false); 
		REGISTER_AMBISONICS(AmbisonicsFirthOrder,	 TEXT("Ambisonics"), TEXT("Firth Ambisonics (36 channels)"), 		5, false, false);  
#endif //
		// Channel packs.
		// FPackedChannelTypeFamily MonoPack( TEXT("PackedChannel")
		
#undef REGISTER_CAT
#undef REGISTER_AMBISONICS
	}
}

const TCHAR* LexToString(const ESpeakerShortNames InSpeaker)
{
	// No code gen support here, so role this manually.
#define CASE_AND_STRING(X) case ESpeakerShortNames::X: return TEXT(#X)

	using enum ESpeakerShortNames;
	switch (InSpeaker)
	{
	CASE_AND_STRING(FL);   // Front Left
	CASE_AND_STRING(FR);   // Front Right
	CASE_AND_STRING(FC);   // Front Center
	CASE_AND_STRING(LFE);  // Low-Frequency Effects (Subwoofer)
	CASE_AND_STRING(FLC);  // Front Left Center
	CASE_AND_STRING(FRC);  // Front Right Center
	CASE_AND_STRING(SL);   // Side Left
	CASE_AND_STRING(SR);   // Side Right
	CASE_AND_STRING(BL);   // Back Left
	CASE_AND_STRING(BR);   // Back Right
	CASE_AND_STRING(BC);   // Back Center
	CASE_AND_STRING(TFL);  // Top Front Left
	CASE_AND_STRING(TFR);  // Top Front Right
	CASE_AND_STRING(TBL);  // Top Back Left
	CASE_AND_STRING(TBR);  // Top Back Right
	default:
		break;
	}
	return nullptr;
}