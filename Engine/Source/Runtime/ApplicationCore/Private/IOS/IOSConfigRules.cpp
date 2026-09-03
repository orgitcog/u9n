// Copyright Epic Games, Inc. All Rights Reserved.

#include "IOS/IOSConfigRules.h"
#include "Misc/FileHelper.h"
#include "zlib.h"
#include <CommonCrypto/CommonCryptor.h>
#include <CommonCrypto/CommonKeyDerivation.h>
#include "String/ParseLines.h"
#include "String/ParseTokens.h"
#include "Containers/StringConv.h"
#include "Misc/ByteSwap.h"
#include "Containers/Map.h"
#include "Misc/Compression.h"
#include "String/RemoveFrom.h"
#include "Internationalization/Regex.h"

DEFINE_LOG_CATEGORY_STATIC(LogConfigRules, Log, All);

#define USE_ODS_LOGGING 1

#if USE_ODS_LOGGING
	#define CONF_LOG(A,B, ...) FPlatformMisc::LowLevelOutputDebugStringf(__VA_ARGS__);
#else
	#define CONF_LOG(...) UE_LOG(__VA_ARGS__);
#endif

@implementation iOSConfigRuleProviders
static NSMutableArray* ConfigRuleProviders = nil;
+ (void)load
{
	ConfigRuleProviders = [[NSMutableArray alloc]init];
}

+ (void)registerRuleProvider:(NSObject<iOSConfigRuleProvider>*)newProvider
{
	[ConfigRuleProviders addObject: newProvider];	
}

+ (NSArray *)getRuleProviders
{
	return [ConfigRuleProviders copy];
}

+ (void)empty
{
	[ConfigRuleProviders removeAllObjects];
}
@end

class FConfigRules
{
public:

    static const int ExpectedConfRuleSig = 0x39d8;

    FString Path;
	FString Key;
	FString IV;

	uint32 Version = -1;
	uint32 UncompressedSize = 0;
    TArray<uint8> FileBytes;

#pragma pack(push)
#pragma pack(1)
    struct FConfHeader
    {
        uint16 Sig;
        int32 Version;
        int32 UncompressedSize;
    };
#pragma pack(pop)
    
    FConfigRules()
    {
       
    }
    
    int OpenAndGetVersionNumber(const FString& PathIN, const FString& KeyIn, const FString& IVIn)
    {
        check(Path.IsEmpty());
        Path = PathIN;
		Key = KeyIn;
		IV = IVIn;
		
        FFileHelper::LoadFileToArray(FileBytes, *Path);

		if (FileBytes.Num() > sizeof(FConfHeader))
        {
            FConfHeader ConfHeader = *((FConfHeader*)FileBytes.GetData());
            if (ByteSwap(ConfHeader.Sig) == ExpectedConfRuleSig)
            {
                Version = ByteSwap(ConfHeader.Version);
                UncompressedSize = ByteSwap(ConfHeader.UncompressedSize);
            }
        }
		else
		{
			CONF_LOG(LogConfigRules, Log, TEXT("ConfigRules: %s was not found."), *Path);	
		}
        return Version;
    }
	
	TConstArrayView<uint8> GetData() const
	{
		return TConstArrayView<uint8>(FileBytes.GetData() + sizeof(FConfHeader), FileBytes.Num() - sizeof(FConfHeader));
	}
};


bool Decrypt(const TConstArrayView<uint8>& DataIn, TArray<uint8>& DataOut, FString& key, FString& iv )
{
	check(!DataOut.IsEmpty());
	
	FTCHARToUTF8 UTF8Key(*key);
	
	uint8 	GeneratedKey[16] = {0};
	{
		static const uint8 salt[] = { 0x23, 0x71, 0xd3, 0xa3, 0x30, 0x71, 0x63, 0xe3};
		uint    rounds  = 1000;
		uint    keySize = kCCKeySizeAES128;

		
		CCKeyDerivationPBKDF(CCPBKDFAlgorithm(kCCPBKDF2),
							 UTF8Key.Get(),
							 UTF8Key.Length(),
							 salt,
							 sizeof(salt),
							 kCCPRFHmacAlgSHA1,
							 rounds,
							 GeneratedKey,
							 sizeof(GeneratedKey));
	}
	
	size_t WrittenBytes = 0;
	
	char ivPtr[kCCBlockSizeAES128 + 1];
	FMemory::Memzero(ivPtr);
	if (!iv.IsEmpty())
	{
		FTCHARToUTF8 UTF8iv(*iv);
		if (UTF8iv.Length() + 1 != sizeof(ivPtr))
		{
			return false;
		}
		FMemory::Memcpy(ivPtr, UTF8iv.Get(), sizeof(ivPtr));
	}

	CCCryptorStatus ccStatus   = kCCSuccess;
	size_t          cryptBytes = 0;
	
	ccStatus = CCCrypt(kCCDecrypt,
						kCCAlgorithmAES128,
						kCCOptionECBMode | kCCOptionPKCS7Padding,
                         GeneratedKey,
                         kCCKeySizeAES128,
                         ivPtr,
                         DataIn.GetData(),
                         DataIn.Num(),
                         DataOut.GetData(),
                         DataOut.Num(),
                         &cryptBytes);
    
	if (ccStatus == kCCSuccess)
	{
		DataOut.SetNum(cryptBytes);
	}
	
	return ccStatus == kCCSuccess;
}

namespace ConfigRules
{
	TMap<FString, FString> ProcessConfigRules(FConfigRules& Rules, TMap<FString,FString>&& PredefinedVariables);
};

bool FIOSConfigRules::Init(TMap<FString,FString>&& PredefinedVariables)
{
	InitRules();

	// load the configrule with the highest version.
	int FoundVersion = -1;
	FConfigRules SelectedRules;
	for (int i = 0; i < ConfigRulesParams.Num(); i++)
	{
		FString Filename = ConfigRulesParams[i].Path;
		FConfigRules TestRules;
		int TestVersion = TestRules.OpenAndGetVersionNumber(Filename, ConfigRulesParams[i].Key, ConfigRulesParams[i].IV);
		if (TestVersion > FoundVersion)
		{
			SelectedRules = MoveTemp(TestRules);
			FoundVersion = TestVersion;
		}
	}

	if (FoundVersion > -1)
	{
		ConfigRuleVariablesMap = ConfigRules::ProcessConfigRules(SelectedRules, MoveTemp(PredefinedVariables));
	}
	else
	{
		ConfigRuleVariablesMap = PredefinedVariables;
	}
	return FoundVersion > -1;
}

void FIOSConfigRules::InitRules()
{
    check(ConfigRulesParams.IsEmpty());
   
	for (NSObject<iOSConfigRuleProvider>* Provider in [iOSConfigRuleProviders getRuleProviders])
	{
		for (NSArray* Params in [Provider getRuleData])
		{
			if([Params count] == 3)
			{
				const char * PathStr = [[Params objectAtIndex:0] UTF8String];
				const char * KeyStr = [[Params objectAtIndex:1] UTF8String];
				const char * PathIV = [[Params objectAtIndex:2] UTF8String];
				
				FConfigRuleParams ConfigRuleParams;
				ConfigRuleParams.Path = PathStr;
				ConfigRuleParams.Key = KeyStr;
				ConfigRuleParams.IV = PathIV;
				
				ConfigRuleParams.Path.ReplaceInline(TEXT("[[cache]]"), *FString([NSSearchPathForDirectoriesInDomains(NSCachesDirectory, NSUserDomainMask, YES) objectAtIndex:0]));
				ConfigRulesParams.Add(ConfigRuleParams);
			}
		}
	}
	
	[iOSConfigRuleProviders empty];
	
	if(ConfigRulesParams.IsEmpty())
	{
		FConfigRuleParams ConfigRuleParams;
		ConfigRuleParams.Path = TEXT("~/configrules");		
		ConfigRulesParams.Add(ConfigRuleParams);
	}
}

namespace ConfigRules
{
	TMap<FString, FString> ParseConfigRules(TConstArrayView<uint8> ConfigRulesData, TMap<FString,FString>&& PredefinedVariables);

	TMap<FString, FString> ProcessConfigRules(FConfigRules& Rules, TMap<FString,FString>&& PredefinedVariables)
	{
		TArray<uint8> DecryptBytes;
		bool bSuccess = true;
		if (!Rules.Key.IsEmpty())
		{
			DecryptBytes.SetNum(Rules.FileBytes.Num());
			bSuccess = Decrypt(Rules.GetData(), DecryptBytes, Rules.Key, Rules.IV);
		}

		TArray<uint8> UncompressedBytes;
		if (bSuccess)
		{
			const TConstArrayView<uint8>& SourceBytes = Rules.Key.IsEmpty() ? Rules.GetData() : DecryptBytes;
			UncompressedBytes.SetNum(Rules.UncompressedSize);
			bSuccess = FCompression::UncompressMemory(NAME_Zlib, UncompressedBytes.GetData(), UncompressedBytes.Num(), SourceBytes.GetData(), SourceBytes.Num());
		}

		if (bSuccess)
		{
			return ParseConfigRules(UncompressedBytes, MoveTemp(PredefinedVariables));
		}
		else
		{
			CONF_LOG(LogConfigRules, Error, TEXT("ConfigRules: file read failed for %s!"), *Rules.Path);
		}
		return PredefinedVariables;
	}

	static FStringView RemoveSurrounds(FStringView Input, FStringView Entry, FStringView Exit)
	{
		if (Input.Len() >= 2 && Entry.Len() > 0 && Entry.Len() == Exit.Len())
		{
			FStringView Ret = UE::String::RemoveFromEnd(UE::String::RemoveFromStart(Input, Entry), Exit);
			if (Ret.Len() == (Input.Len() - (Entry.Len() * 2)))
			{
				return Ret;
			}
		}
		return Input;
	}

	static FStringView SubStr(FStringView Input, int32 StartIdx, int32 EndIdx)
	{
		return Input.Left(EndIdx).RightChop(StartIdx);
	};

	static int32 IndexOf(const FString& Input, FStringView Str, int FromIndex = 0)
	{
		return Input.Find(Str, ESearchCase::CaseSensitive, ESearchDir::FromStart, FromIndex);		
	}

	// returns list of strings separated by Split using Entry/Exit as pairing sets (ex. "(" and ")").  The Entry and Exit characters need to be in same order
	static TArray<FStringView> ParseSegments(FStringView Input, FStringView Split, FStringView Entry, FStringView Exit)
	{
		TArray<FStringView> Output;
		TArray<int32> EntryStack;

		int StartIndex = 0;
		int ScanIndex = 0;
		int ExitIndex = -1;
		int InputLength = Input.Len();

		while (ScanIndex < InputLength)
		{
			FStringView Scan = SubStr(Input, ScanIndex, ScanIndex + 1);

			if (Scan.Equals(Split) && EntryStack.Num() == 0)
			{
				Output.Add(SubStr(Input, StartIndex, ScanIndex).TrimStartAndEnd());
				ScanIndex++;
				StartIndex = ScanIndex;
				continue;
			}
			ScanIndex++;

			if (Scan.Equals(TEXT("\\")))
			{
				ScanIndex++;
				continue;
			}

			if (EntryStack.Num() > 0 && Exit.Find(Scan) == ExitIndex)
			{
				int StackLength = EntryStack.Num() - 1;
				EntryStack.RemoveAt(StackLength);
				ExitIndex = StackLength > 0 ? EntryStack[StackLength - 1] : -1;
				continue;
			}

			int EntryIndex = Entry.Find(Scan);
			if (EntryIndex >= 0)
			{
				EntryStack.Add(EntryIndex);
				ExitIndex = EntryIndex;
				continue;
			}
		}
		if (StartIndex < InputLength)
		{
			Output.Add(Input.RightChop(StartIndex).TrimStartAndEnd());
		}

		return Output;
	}

	FString ExpandVariables(TMap<FString, FString>& Variables, FStringView Input)
	{
		FString Result(Input);
		int Idx;
		for (Idx = IndexOf(Result, TEXT("$(")); Idx != -1; Idx = IndexOf(Result, TEXT("$("), Idx))
		{
			// Find the end of the variable name
			int EndIdx = IndexOf(Result, TEXT(")"), Idx + 2);
			if (EndIdx == -1)
			{
				break;
			}

			// Extract the variable name from the string
			FStringView VarKey = SubStr(Result, Idx + 2, EndIdx);

			// Find the value for it if it exists
			if (FString* VarValue = Variables.Find(FString(VarKey)))
			{
				// Replace the variable
				Result = SubStr(Result, 0, Idx) + (*VarValue) + Result.RightChop( EndIdx + 1);
			}
			else
			{
				// or skip past it
				Idx = EndIdx + 1;
				continue;
			}

		}
		return Result;
	}

	/////////////////////
	static bool EvaluateConditions(TMap<FString, FString>& Variables, TArray<FStringView>& Conditions, FString& PreviousRegexMatch);
	static bool EvaluateConditions(TMap<FString, FString>& Variables, TArray<FStringView>& Conditions)
	{
		FString PrevRegEx(TEXT(""));
		return EvaluateConditions(Variables, Conditions, PrevRegEx);
	}

	/////////////////////
	TMap<FString, FString> ParseConfigRules(TConstArrayView<uint8> ConfigRulesData, TMap<FString,FString>&& PredefinedVariables)
	{
		enum class EConfRuleState : uint8
		{
			Run = 0,
			ExecTrue,
			FindElse,
			ExecFalse,
			FindEnd
		};

		// TODO: we should be able to parse this entire file without conversion to tchar.
		// Some functions such as ParseLines, LexFromString etc. expect tchar.
		auto ConvertedString = StringCast<TCHAR>((const char*)ConfigRulesData.GetData(), ConfigRulesData.Num());
		FStringView ConfigRules = FStringView(ConvertedString.Get(), ConvertedString.Length());

		TMap<FString, FString> ConfigRuleVars;
		ConfigRuleVars.Append(MoveTemp(PredefinedVariables));
		TArray<EConfRuleState> StateStack;

		EConfRuleState CurrentState = EConfRuleState::Run;
		int NestDepth = 0;
		bool bAbort = false;
		UE::String::ParseLines(ConfigRules,
			[&ConfigRuleVars, &CurrentState, &StateStack, &NestDepth, &bAbort](FStringView Line)
			{
				FStringView line = Line.TrimStartAndEnd();
				if (line.Len() < 1 || bAbort)
				{
					return;
				}
				if (line.StartsWith(TEXT("//"), ESearchCase::Type::CaseSensitive) || line.StartsWith(TEXT(";"), ESearchCase::Type::CaseSensitive))
				{
					if (line.StartsWith(TEXT("// version:")))
					{
						int configRulesVersion = 0;
						LexFromString(configRulesVersion, *FString(line.RightChop(11)));
						ConfigRuleVars.Add(TEXT("configRulesVersion"), FString::FromInt(configRulesVersion));
						CONF_LOG(LogConfigRules, Log, TEXT("ConfigRules version: %d"), configRulesVersion);
					}
					return;
				}

				// look for Command
				int index;
				if (!line.FindChar(':', index))
				{
					return;
				}
				FStringView Command = line.Left(index).TrimStartAndEnd();
				line = line.RightChop(index + 1).TrimStartAndEnd();

				// handle states
				switch (CurrentState)
				{
					case EConfRuleState::Run:
					{
						if (Command.Equals(TEXT("else")) || Command.Equals(TEXT("elseif")) || Command.Equals(TEXT("endif")))
						{
							CONF_LOG(LogConfigRules, Error, TEXT("ConfigRules: unexpected %s encountered!"), *FString(Command));
						}
						break;
					}

					case EConfRuleState::ExecTrue:
					{
						if (Command.Equals(TEXT("else")))
						{
							CurrentState = EConfRuleState::FindEnd;
						}
						else if (Command.Equals(TEXT("endif")))
						{
							CurrentState = StateStack.Pop();
						}
						return;
					}

					case EConfRuleState::FindElse:
					{
						if (Command.Equals(TEXT("if")))
						{
							NestDepth++;
							return;
						}
						if (NestDepth > 0)
						{
							if (Command.Equals(TEXT("endif")))
							{
								NestDepth--;
							}
							return;
						}
						if (Command.Equals(TEXT("endif")))
						{
							CurrentState = StateStack.Pop();
							return;
						}
						if (Command.Equals(TEXT("else")))
						{
							CurrentState = EConfRuleState::ExecFalse;
							return;
						}
						if (Command.Equals(TEXT("elseif")))
						{
							CurrentState = EConfRuleState::FindEnd;

							TArray<FStringView> Conditions;
							UE::String::ParseTokensMultiple(line, TConstArrayView<TCHAR>({ ',','(',')' }), Conditions);
							if (Conditions.Num() > 0)
							{
								bool bConditionTrue = EvaluateConditions(ConfigRuleVars, Conditions);
								CurrentState = bConditionTrue ? EConfRuleState::ExecTrue : EConfRuleState::FindElse;
							}
						}
						return;
					}
					case EConfRuleState::ExecFalse:
					{
						if (Command.Equals(TEXT("endif")))
						{
							CurrentState = StateStack.Pop();
						}
						if (Command.Equals(TEXT("else")) || Command.Equals(TEXT("elseif")))
						{
							CONF_LOG(LogConfigRules, Error, TEXT("ConfigRules: unexpected %s while handling false condition!"), *FString(Command));
						}
						return;
					}
					case EConfRuleState::FindEnd:
					{
						if (Command.Equals(TEXT("if")))
						{
							NestDepth++;
						}
						else if (Command.Equals(TEXT("endif")))
						{
							if (NestDepth > 0)
							{
								NestDepth--;
							}
							else
							{
								CurrentState = StateStack.Pop();
							}
						}
						return;
					}
					default:
						CONF_LOG(LogConfigRules, Error, TEXT("ConfigRules: unknown state!"));
						return;
				}

				// handle commands
				if (Command.Equals(TEXT("set")))
				{
					// set:(a=b[,c=d,...])
					TArray<FStringView> Sets = ParseSegments(RemoveSurrounds(line, TEXT("("), TEXT(")")), TEXT(","), TEXT("(\")"), TEXT(")\")"));
					for (FStringView Assignment : Sets)
					{
						TArray<FStringView> KeyValue = ParseSegments(Assignment, TEXT("="), TEXT("\""), TEXT("\""));
						if (KeyValue.Num() == 2)
						{
							FStringView Key = RemoveSurrounds(KeyValue[0], TEXT("\""), TEXT("\""));
							FString Value = ExpandVariables(ConfigRuleVars, RemoveSurrounds(KeyValue[1], TEXT("\""), TEXT("\"")));
							check(!Key.IsEmpty());
							if (Key.StartsWith(TEXT("APPEND_")))
							{
								Key = Key.RightChop(7);
								if (FString* Found = ConfigRuleVars.Find(FString(Key)))
								{
									Value = (*Found) + Value;
								}
								ConfigRuleVars.Add(FString(Key), Value);
							}
							else
							{
								ConfigRuleVars.Add(FString(Key), Value);
							}
						}
					}
				}
				else if (Command.Equals(TEXT("clear")))
				{
					// clear:(a[,b,...])
					TArray<FStringView> Sets = ParseSegments(RemoveSurrounds(Line, TEXT("("), TEXT(")")), TEXT(","), TEXT("(\""), TEXT(")\""));
					for (FStringView Key : Sets)
					{
						ConfigRuleVars.Remove(FString(RemoveSurrounds(Key, TEXT("\""), TEXT("\""))));
					}
				}
				else if (Command.Equals(TEXT("chipset")))
				{
					// not supported here.
				}
				else if (Command.Equals(TEXT("if")))
				{
					// if:(SourceType=SRC_DeviceMake,CompareType=CMP_Equal,MatchString="samsung")
					// ... commands for true for all conditions
					// elseif:(SourceType=SRC_DeviceMake,CompareType=CMP_Equal,MatchString="Google")
					// ... commands for true for all conditions
					// else:
					// ... commands for false for any condition
					// end:
					StateStack.Push(CurrentState);
					CurrentState = EConfRuleState::FindEnd;

					TArray<FStringView> Conditions = ParseSegments(Line, TEXT(","), TEXT("(\""), TEXT(")\""));
					if (!Conditions.IsEmpty())
					{
						const bool bConditionTrue = EvaluateConditions(ConfigRuleVars, Conditions);
						CurrentState = bConditionTrue ? EConfRuleState::ExecTrue : EConfRuleState::FindElse;
					}
				}
				else if (Command.Equals(TEXT("condition")))
				{
					// condition:((SourceType=SRC_DeviceMake,CompareType=CMP_Equal,MatchString="samsung")),(SourceType=,CompareType=,MatchString=),...]),(a=b[,c=d,...]),(a[,b,...])
					// if all the conditions are true, execute the optional sets and/or clears
					TArray<FStringView> ConditionAndSets = ParseSegments(line, TEXT(","), TEXT("(\""), TEXT(")\""));
					int SetSize = ConditionAndSets.Num();
					if (SetSize == 2 || SetSize == 3)
					{
						TArray<FStringView> Conditions = ParseSegments(RemoveSurrounds(ConditionAndSets[0], TEXT("("), TEXT(")")), TEXT(","), TEXT("(\""), TEXT(")\""));
						TArray<FStringView> Sets = ParseSegments(RemoveSurrounds(ConditionAndSets[1], TEXT("("), TEXT(")")), TEXT(","), TEXT("(\""), TEXT(")\""));
						TArray<FStringView> Clears = (SetSize == 3) ? ParseSegments(RemoveSurrounds(ConditionAndSets[2], TEXT("("), TEXT(")")), TEXT(","), TEXT("(\""), TEXT(")\"")) : TArray<FStringView>();

						bool bConditionTrue = EvaluateConditions(ConfigRuleVars, Conditions);

						if (bConditionTrue)
						{
							// run the sets
							for (FStringView Assignment : Sets)
							{
								TArray<FStringView> KeyValue = ParseSegments(Assignment, TEXT("="), TEXT("\""), TEXT("\""));
								if (KeyValue.Num() == 2)
								{
									FStringView Key = RemoveSurrounds(KeyValue[0], TEXT("\""), TEXT("\""));
									FString Value = ExpandVariables(ConfigRuleVars, RemoveSurrounds(KeyValue[1], TEXT("\""), TEXT("\"")));
									check(!Key.IsEmpty());
									if (Key.StartsWith(TEXT("APPEND_")))
									{
										Key = Key.RightChop(7);
										if (FString* Found = ConfigRuleVars.Find(FString(Key)))
										{
											Value = (*Found) + Value;
										}
										ConfigRuleVars.Add(FString(Key), Value);
									}
									else
									{
										ConfigRuleVars.Add(FString(Key), Value);
									}
								}
							}

							// run the clears
							for (FStringView Key : Clears)
							{
								ConfigRuleVars.Remove(FString(RemoveSurrounds(Key, TEXT("\""), TEXT("\""))));
							}
						}
					}
				}
				// see if log message requested
				static const FString LogStr(TEXT("log"));
				if (FString* Found = ConfigRuleVars.Find(LogStr))
				{
					CONF_LOG(LogConfigRules, Log, TEXT("ConfigRules log output:\n %s"), **Found);
					ConfigRuleVars.Remove(LogStr);
				}

				// check if requested to dump variables to the log
				static const FString DumpVarsStr(TEXT("dumpvars"));
				if (FString* Found = ConfigRuleVars.Find(DumpVarsStr))
				{
					ConfigRuleVars.Remove(DumpVarsStr);
					CONF_LOG(LogConfigRules, Log, TEXT("ConfigRules vars:"));
					for ( TPair<FString, FString> VarEntry: ConfigRuleVars)
					{
						CONF_LOG(LogConfigRules, Log, TEXT("%s = %s"), *VarEntry.Key, *VarEntry.Value);
					}
				}

				// if there was a raised error or break, stop
				const bool HasError = ConfigRuleVars.Contains(TEXT("error"));
				const bool HasBreak = ConfigRuleVars.Contains(TEXT("break"));

				// stop if user wants to break
				if (HasBreak || HasError)
				{
					CONF_LOG(LogConfigRules, Warning, TEXT("Config rules aborting parse due to %s."), HasBreak ? TEXT("break command") : TEXT("error"));
					bAbort = true;
				}
			});

		return ConfigRuleVars;
	}

	static bool EvaluateConditions(TMap<FString, FString>& Variables, TArray<FStringView>& Conditions, FString& PreviousRegexMatch)
	{
		bool bConditionTrue = true;
		for (FStringView Condition : Conditions)
		{
			FString SourceType = "";
			FString CompareType = "";
			FString MatchString = "";

			// deal with Condition group (src,cmp,match)
			TArray<FStringView> groups = ParseSegments(RemoveSurrounds(Condition, TEXT("("), TEXT(")")), TEXT(","), TEXT("\""), TEXT("\""));
			for (FStringView group : groups)
			{
				TArray<FStringView> KeyValue = ParseSegments(group, TEXT("="), TEXT("\""), TEXT("\""));
				if (KeyValue.Num() == 2)
				{
					FStringView Key = RemoveSurrounds(KeyValue[0], TEXT("\""), TEXT("\""));
					FStringView value = RemoveSurrounds(KeyValue[1], TEXT("\""), TEXT("\""));

					if (Key.Equals(TEXT("SourceType")))
					{
						SourceType = value;
					}
					else if (Key.Equals(TEXT("CompareType")))
					{
						CompareType = value;
					}
					else if (Key.Equals(TEXT("MatchString")))
					{
						MatchString = value;
					}
				}
			}

			FString Source = "";
			if (SourceType.Equals(TEXT("SRC_PreviousRegexMatch")))
			{
				Source = PreviousRegexMatch;
			}
			else if (SourceType.Equals(TEXT("SRC_CommandLine")))
			{
				checkNoEntry();
			}
			else if (FString* Found = Variables.Find(SourceType))
			{
				Source = *Found;
			}
			else if (SourceType.Equals(TEXT("[EXIST]")))
			{
				Source = MatchString;
			}
			else
			{
				bConditionTrue = false;
				break;
			}

			// apply operation
			if (CompareType.Equals(TEXT("CMP_Exist")))
			{
				if (!Variables.Contains(Source))
				{
					bConditionTrue = false;
					break;
				}
			}
			else if (CompareType.Equals(TEXT("CMP_NotExist")))
			{
				if (Variables.Contains(Source))
				{
					bConditionTrue = false;
					break;
				}
			}
			else if (CompareType.Equals(TEXT("CMP_Equal")))
			{
				if (!Source.Equals(MatchString))
				{
					bConditionTrue = false;
					break;
				}
			}
			else if (CompareType.Equals(TEXT("CMP_NotEqual")))
			{
				if (Source.Equals(MatchString))
				{
					bConditionTrue = false;
					break;
				}
			}
			else if (CompareType.Equals(TEXT("CMP_EqualIgnore")))
			{
				if (!Source.ToLower().Equals(MatchString.ToLower()))
				{
					bConditionTrue = false;
					break;
				}
			}
			else if (CompareType.Equals(TEXT("CMP_NotEqualIgnore")))
			{
				if (Source.ToLower().Equals(MatchString.ToLower()))
				{
					bConditionTrue = false;
					break;
				}
			}
			else if (CompareType.Equals(TEXT("CMP_Regex")))
			{
				const FRegexPattern RegexPattern(MatchString);
				FRegexMatcher RegexMatcher(RegexPattern, Source);

				if (RegexMatcher.FindNext())
				{
					FString Captured = RegexMatcher.GetCaptureGroup(1);
					PreviousRegexMatch = Captured.IsEmpty() ? RegexMatcher.GetCaptureGroup(0) : Captured;
				}
				else
				{
					bConditionTrue = false;
					break;
				}
			}
			else
			{
				bool bNumericOperands = true;
				float SourceFloat = 0.0f;
				float MatchFloat = 0.0f;

				// convert source and match to float if numeric
				bNumericOperands = LexTryParseString(SourceFloat, *Source);
				bNumericOperands = LexTryParseString(MatchFloat, *MatchString) && bNumericOperands;

				// if comparison ends with Ignore, do case-insensitive compare by converting both to lowercase
				if (CompareType.EndsWith(TEXT("Ignore")))
				{
					bNumericOperands = false;
					CompareType = SubStr(CompareType, 0, CompareType.Len() - 6);

					Source = Source.ToLower();
					MatchString = MatchString.ToLower();
				}

				if (CompareType.Equals(TEXT("CMP_Less")))
				{
					if ((bNumericOperands && (SourceFloat >= MatchFloat)) || (!bNumericOperands && (Source.Compare(MatchString) >= 0)))
					{
						bConditionTrue = false;
						break;
					}
				}
				else if (CompareType.Equals(TEXT("CMP_LessEqual")))
				{
					if ((bNumericOperands && (SourceFloat > MatchFloat)) || (!bNumericOperands && (Source.Compare(MatchString) > 0)))
					{
						bConditionTrue = false;
						break;
					}
				}
				else if (CompareType.Equals(TEXT("CMP_Greater")))
				{
					if ((bNumericOperands && (SourceFloat <= MatchFloat)) || (!bNumericOperands && (Source.Compare(MatchString) <= 0)))
					{
						bConditionTrue = false;
						break;
					}
				}
				else if (CompareType.Equals(TEXT("CMP_GreaterEqual")))
				{
					if ((bNumericOperands && (SourceFloat < MatchFloat)) || (!bNumericOperands && (Source.Compare(MatchString) < 0)))
					{
						bConditionTrue = false;
						break;
					}
				}
				else
				{
					bConditionTrue = false;
					break;
				}
			}
		}

		return bConditionTrue;
	}
}
