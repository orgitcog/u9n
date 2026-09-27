// Copyright Epic Games, Inc. All Rights Reserved.

#include "NamingTokensEngineSubsystem.h"

#include "NamingTokens.h"
#include "NamingTokensEvaluationData.h"
#include "NamingTokensLog.h"
#include "Utils/NamingTokenUtils.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetDataTagMap.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Blueprint.h"
#include "Internationalization/Regex.h"
#include "Misc/PackageName.h"

TArray<FString> UNamingTokensEngineSubsystem::GetAllNamespaces() const
{
	const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	const IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	// Find all assets whose native parent class is UNamingTokens, and add their namespaces
	TArray<FAssetData> NamingTokenAssets;
	const TMultiMap<FName, FString> TagValues = { { FBlueprintTags::NativeParentClassPath, FObjectPropertyBase::GetExportPath(UNamingTokens::StaticClass()) } };
	AssetRegistry.GetAssetsByTagValues(TagValues, NamingTokenAssets);

	TSet<FString> DiscoveredNamespaces;
	for (const FAssetData& NamingTokenAssetData : NamingTokenAssets)
	{
		FString FoundNamespace;
		NamingTokenAssetData.GetTagValue(UNamingTokens::GetNamespacePropertyName(), FoundNamespace);

		DiscoveredNamespaces.Add(FoundNamespace);
	}

	// Find all native classes derived from UNamingTokens, and add their namespaces
	TArray<UClass*> DerivedClasses;
	GetDerivedClasses(UNamingTokens::StaticClass(), DerivedClasses);
	for (const UClass* DerivedClass : DerivedClasses)
	{
		constexpr EClassFlags InvalidClassFlags = CLASS_Deprecated | CLASS_NewerVersionExists | CLASS_Abstract;
		if (DerivedClass->IsNative() && !DerivedClass->HasAnyClassFlags(InvalidClassFlags))
		{
			FString Namepsace = DerivedClass->GetDefaultObject<UNamingTokens>()->GetNamespace();
			DiscoveredNamespaces.Add(Namepsace);
		}
	}

	return DiscoveredNamespaces.Array();
}

UNamingTokensEngineSubsystem::UNamingTokensEngineSubsystem()
	: bIsCacheEnabled(true)
{
}

UNamingTokens* UNamingTokensEngineSubsystem::GetNamingTokens(const FString& InNamespace) const
{
	// Check cache.
	UNamingTokens* FoundTokens = GetNamingTokenFromCache(InNamespace, false);
	
	if (FoundTokens)
	{
		return FoundTokens;
	}

	FoundTokens = GetNamingTokensNative(InNamespace);

	const UClass* TargetClass = UNamingTokens::StaticClass();

	// Check blueprint classes.
	// NOTE: We may want to use OnAssetAdded and cache the asset or load the class then.
	{
		const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		const IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

		FARFilter Filter;
		Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
		Filter.bRecursiveClasses = true;
		Filter.bRecursivePaths = true;

		TArray<FAssetData> BlueprintAssets;
		AssetRegistry.GetAssets(Filter, BlueprintAssets);

		TSet<FTopLevelAssetPath> DerivedClassNames;
		AssetRegistry.GetDerivedClassNames({ TargetClass->GetClassPathName() }, {}, DerivedClassNames);

		for (const FAssetData& AssetData : BlueprintAssets)
		{
			// Narrow down to only our assets.
			const FAssetDataTagMapSharedView::FFindTagResult Result = AssetData.TagsAndValues.FindTag(TEXT("GeneratedClass"));
			if (Result.IsSet())
			{
				const FString& GeneratedClassPathPtr = Result.GetValue();
				const FTopLevelAssetPath ClassObjectPath(FPackageName::ExportTextPathToObjectPath(*GeneratedClassPathPtr));

				if (DerivedClassNames.Contains(ClassObjectPath))
				{
					// Now check for the namespace match.
					FString FoundNamespace;
					if (AssetData.GetTagValue(UNamingTokens::GetNamespacePropertyName(), FoundNamespace)
						&& FoundNamespace == InNamespace)
					{
						// Allow BP children to overwrite native classes.
						auto IsAssetChildOfNativeToken = [&AssetRegistry, &InNamespace](const FTopLevelAssetPath& InAssetPath, const UNamingTokens* InParentTokens) -> bool
						{
							check(InParentTokens);
							if (InParentTokens->GetClass()->IsNative())
							{
								// We need to make sure this BP is a child of the chosen native class.
								TSet<FTopLevelAssetPath> NativeDerivedClassNames;
								AssetRegistry.GetDerivedClassNames({ InParentTokens->GetClass()->GetClassPathName() },
									{}, NativeDerivedClassNames);

								if (NativeDerivedClassNames.Contains(InAssetPath))
								{
									UE_LOG(LogNamingTokens, Log, TEXT("Using namespace '%s' of BP child '%s' instead of native parent '%s'."),
										*InNamespace, *InAssetPath.ToString(), *InParentTokens->GetClass()->GetName());
									return true;
								}
							}
							return false;
						};
						
						if (!FoundTokens || IsAssetChildOfNativeToken(ClassObjectPath, FoundTokens))
						{
							const TSoftClassPtr<UNamingTokens> SoftClassPath(ClassObjectPath.ToString());
							FoundTokens = LoadNamingToken(SoftClassPath, InNamespace);
						}
						else
						{
							UE_LOG(LogNamingTokens, Warning, TEXT("Namespace '%s' exists more than once in class '%s' and BP asset '%s'."),
								*InNamespace, *FoundTokens->GetClass()->GetName(), *AssetData.PackageName.ToString());
						}
					}
				}
			}
		}
	}

	return FoundTokens;
}

UNamingTokens* UNamingTokensEngineSubsystem::GetNamingTokensNative(const FString& InNamespace) const
{
	// Check cache.
	UNamingTokens* FoundTokens = GetNamingTokenFromCache(InNamespace, true);
	if (FoundTokens)
	{
		return FoundTokens;
	}

	const UClass* TargetClass = UNamingTokens::StaticClass();

	// Check native classes.
	constexpr EClassFlags InvalidClassFlags = CLASS_Deprecated | CLASS_NewerVersionExists | CLASS_Abstract;

	TArray<UClass*> DerivedClasses;
	GetDerivedClasses(TargetClass, DerivedClasses);
	for (const UClass* DerivedClass : DerivedClasses)
	{
		if (DerivedClass->IsNative()
			&& DerivedClass->IsChildOf(TargetClass)
			&& !DerivedClass->HasAnyClassFlags(InvalidClassFlags)
			&& !DerivedClass->GetName().StartsWith(TEXT("SKEL_"))
			&& !DerivedClass->GetName().StartsWith(TEXT("REINST_"))
			&& DerivedClass->GetDefaultObject<UNamingTokens>()->GetNamespace() == InNamespace)
		{
			if (!FoundTokens)
			{
				FoundTokens = LoadNamingToken(DerivedClass, InNamespace);
			}
			else
			{
				UE_LOG(LogNamingTokens, Warning, TEXT("Namespace '%s' exists more than once in native class '%s' and '%s'."),
						*InNamespace, *FoundTokens->GetClass()->GetName(), *DerivedClass->GetName());
			}
		}
	}

	return FoundTokens;
}

TArray<UNamingTokens*> UNamingTokensEngineSubsystem::GetMultipleNamingTokens(const TArray<FString>& InNamespaces) const
{
	TArray<UNamingTokens*> Result;
	const TSet<FString> UniqueNamespaces(InNamespaces);
	for (const FString& Namespace : UniqueNamespaces)
	{
		if (UNamingTokens* NamingTokens = GetNamingTokens(Namespace))
		{
			Result.Add(NamingTokens);
		}
	}

	return Result;
}

FNamingTokenResultData UNamingTokensEngineSubsystem::EvaluateTokenText(const FText& InTokenText, const FNamingTokenFilterArgs& InFilter, const TArray<UObject*>& InContexts)
{
	FNamingTokenResultData Result;

	FText ProcessedTokenText = InTokenText;
	Result.EvaluatedText = ProcessedTokenText; // Set so we always have something in case no tokens were evaluated.

	TSet<FString> Namespaces = GetNamingTokenNamespacesFromString(InTokenText.ToString(), InFilter);

	TArray<FNamingTokenValueData> CompletedNamingTokenValueData;
	
	// These external filters are intentionally executed before adding any namespaces from the input FNamingTokenFilterArgs filter. 
	// This allows the caller of this function to guarantee that the namespaces they want included will always be allowed, regardless of other filters that may execute.
	for (const TPair<FName, FFilterNamespace>& DelegatePair : FilterNamespaceDelegates)
	{
		DelegatePair.Value.Execute(Namespaces);
	}

	Namespaces.Append(InFilter.AdditionalNamespacesToInclude); // Tokens could have been written without the namespace but should still be scoped to the filter.

	FNamingTokensEvaluationData EvaluationData;
	EvaluationData.Contexts = InContexts;
	EvaluationData.bForceCaseSensitive = InFilter.bForceCaseSensitive;
	for (const FString& Namespace : Namespaces)
	{
		DECLARE_DELEGATE_RetVal_OneParam(UNamingTokens*, FGetNamingTokensFunction, const FString&);
		
		FGetNamingTokensFunction GetNamingTokensFunction;
		if (InFilter.bNativeOnly)
		{
			GetNamingTokensFunction.BindUObject(this, &UNamingTokensEngineSubsystem::GetNamingTokensNative);
		}
		else
		{
			GetNamingTokensFunction.BindUObject(this, &UNamingTokensEngineSubsystem::GetNamingTokens);
		}

		if (UNamingTokens* NamingTokens = GetNamingTokensFunction.Execute(Namespace))
		{
			Result = NamingTokens->EvaluateTokenText(ProcessedTokenText, EvaluationData);

			// Properly record individual token evaluation data. The order should be consistent with the order
			// a key was processed in the string. A key may be processed multiple times if it is referenced multiple times, or is not
			// identified in one NamingTokens class but is in another. In the event a future class identifies it, it will update the original
			// key.
			for (const FNamingTokenValueData& NewTokenValue : Result.TokenValues)
			{
				bool bReplacedOldValue = false;
				// Find previously recorded values that were undefined and update them indicating they are no longer undefined.
				for (FNamingTokenValueData& OldTokenValue : CompletedNamingTokenValueData)
				{
					if (OldTokenValue.TokenKey.Equals(NewTokenValue.TokenKey, ESearchCase::CaseSensitive))
					{
						OldTokenValue = NewTokenValue;
						bReplacedOldValue = true;
						break;
					}
				}
				if (!bReplacedOldValue)
				{
					// Add to the completed naming token value.
					CompletedNamingTokenValueData.Add(NewTokenValue);
				}
			}
			
			ProcessedTokenText = Result.EvaluatedText;
		}
	}

	Result.OriginalText = InTokenText;

	Result.TokenValues = MoveTemp(CompletedNamingTokenValueData);
	
	return Result;
}

FNamingTokenResultData UNamingTokensEngineSubsystem::EvaluateTokenText(const FText& InTokenText, const FNamingTokenFilterArgs& InFilter)
{
	return EvaluateTokenText(InTokenText, InFilter, {});
}

FNamingTokenResultData UNamingTokensEngineSubsystem::EvaluateTokenString(const FString& InTokenString, const FNamingTokenFilterArgs& InFilter, const TArray<UObject*>& InContexts)
{
	return EvaluateTokenText(FText::FromString(InTokenString), InFilter, InContexts);
}

FNamingTokenResultData UNamingTokensEngineSubsystem::EvaluateTokenString(const FString& InTokenString, const FNamingTokenFilterArgs& InFilter)
{
	return EvaluateTokenString(InTokenString, InFilter, {});
}

void UNamingTokensEngineSubsystem::RegisterGlobalNamespace(const FString& InNamespace)
{
	if (IsGlobalNamespaceRegistered(InNamespace))
	{
		const FString UnregisterFunctionName = GET_FUNCTION_NAME_CHECKED(UNamingTokensEngineSubsystem, UnregisterGlobalNamespace).ToString();
		UE_LOG(LogNamingTokens, Error, TEXT("NamingTokens Namespace '%s' is already registered as a global namespace. Call '%s' first if you wish to overwrite the namespace."),
			 *InNamespace, *UnregisterFunctionName);
		return;
	}
	FText ErrorMessage;
	if (UE::NamingTokens::Utils::ValidateName(InNamespace, ErrorMessage))
	{
		GlobalNamespaces.Add(InNamespace);
	}
	else
	{
		UE_LOG(LogNamingTokens, Error, TEXT("NamingTokens Namespace '%s' cannot be registered. Error: %s"),
			*InNamespace, *ErrorMessage.ToString());
	}
}

void UNamingTokensEngineSubsystem::UnregisterGlobalNamespace(const FString& InNamespace)
{
	GlobalNamespaces.Remove(InNamespace);
}

bool UNamingTokensEngineSubsystem::IsGlobalNamespaceRegistered(const FString& InNamespace) const
{
	return GlobalNamespaces.Contains(InNamespace);
}

TArray<FString> UNamingTokensEngineSubsystem::GetGlobalNamespaces() const
{
	return GlobalNamespaces.Array();
}

void UNamingTokensEngineSubsystem::RegisterNamespaceFilter(const FName OwnerName, FFilterNamespace Delegate)
{
	FilterNamespaceDelegates.Add(OwnerName, MoveTemp(Delegate));
}

void UNamingTokensEngineSubsystem::UnregisterNamespaceFilter(const FName OwnerName)
{
	FilterNamespaceDelegates.Remove(OwnerName);
}

TSet<FString> UNamingTokensEngineSubsystem::GetNamingTokenNamespacesFromString(const FString& InTokenString, const FNamingTokenFilterArgs& InFilter) const
{
	TSet<FString> Namespaces;

	const FRegexPattern Pattern(FString::Printf(TEXT(R"(\{\s*([a-zA-Z0-9_]+)%s[a-zA-Z0-9_]+\s*\})"), *UE::NamingTokens::Utils::GetNamespaceDelimiter()));
	FRegexMatcher Matcher(Pattern, InTokenString);
	
	while (Matcher.FindNext())
	{
		const FString Namespace = Matcher.GetCaptureGroup(1);
		Namespaces.Add(Namespace);
	}

	// Add any global namespaces.
	if (InFilter.bIncludeGlobal)
	{
		Namespaces.Append(GlobalNamespaces);
	}
	
	return Namespaces;	
}

FString UNamingTokensEngineSubsystem::GetFormattedTokensStringForDisplay(const FNamingTokenFilterArgs& InFilter) const
{
	TSet<FString> Namespaces = TSet<FString>(InFilter.AdditionalNamespacesToInclude);

	if (InFilter.bIncludeGlobal)
	{
		Namespaces.Append(GlobalNamespaces.Array());
	}

	FString Result;

	for (const FString& Namespace : Namespaces)
	{
		if (const UNamingTokens* NamingTokens = GetNamingTokens(Namespace))
		{
			Result += NamingTokens->GetFormattedTokensStringForDisplay();
		}
	}

	return Result;
}

void UNamingTokensEngineSubsystem::ClearCachedNamingTokens()
{
	FScopeLock Lock(&CachedNamingTokensMutex);
	CachedNamingTokens.Empty();
}

bool UNamingTokensEngineSubsystem::IsCacheEnabled() const
{
	return bIsCacheEnabled;
}

void UNamingTokensEngineSubsystem::SetCacheEnabled(bool bEnabled)
{
	bIsCacheEnabled = bEnabled;
	if (!bIsCacheEnabled)
	{
		ClearCachedNamingTokens();
	}
}

UNamingTokens* UNamingTokensEngineSubsystem::LoadNamingToken(const TSoftClassPtr<UNamingTokens>& InTokensClass, const FString& InNamespace) const
{
	if (const UClass* Class = InTokensClass.LoadSynchronous())
	{
		UNamingTokens* NamingTokens = NewObject<UNamingTokens>(GetTransientPackage(), Class);
		// Initial validation on load. This will log any errors.
		NamingTokens->Validate();
		if (ensure(NamingTokens->GetNamespace() == InNamespace))
		{
			FScopeLock Lock(&CachedNamingTokensMutex);
			if (IsCacheEnabled())
			{
				CachedNamingTokens.Add(InNamespace, NamingTokens);
			}
			return NamingTokens;
		}
	}

	return nullptr;
}

UNamingTokens* UNamingTokensEngineSubsystem::GetNamingTokenFromCache(const FString& InNamespace, bool bNativeOnly) const
{
	FScopeLock Lock(&CachedNamingTokensMutex);
	if (IsCacheEnabled())
	{
		if (const TObjectPtr<UNamingTokens>* Tokens = CachedNamingTokens.Find(InNamespace))
		{
			if ((*Tokens) == nullptr)
			{
				// NamingTokens may have been deleted, with a new asset using the same namespace then added afterward.
				CachedNamingTokens.Remove(InNamespace);
				return nullptr;
			}
			
			if (!bNativeOnly)
			{
				return *Tokens;
			}

			if (bNativeOnly && (*Tokens)->GetClass()->IsNative())
			{
				return *Tokens;
			}
		}
	}

	return nullptr;
}
