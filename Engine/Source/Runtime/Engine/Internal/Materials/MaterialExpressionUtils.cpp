// Copyright Epic Games, Inc. All Rights Reserved.

#include "Materials/MaterialExpressionUtils.h"

#include "Containers/EnumAsByte.h"
#include "MaterialDomain.h"
#include "Materials/MaterialExternalCodeRegistry.h"


namespace MaterialExpressionUtils
{

	FString FormatUnsupportedMaterialDomainError(const FMaterialExternalCodeDeclaration& InExternalCode, const FName& AssetPathName)
	{
		TStringBuilder<1024> SupportedMaterialDomainsString;
		for (EMaterialDomain Domain : InExternalCode.Domains)
		{
			if (SupportedMaterialDomainsString.Len() > 0)
			{
				SupportedMaterialDomainsString.Append(TEXT(", "));
			}
			SupportedMaterialDomainsString.Append(*MaterialDomainString(Domain));
		}
		return FString::Printf(TEXT("External code '%s' (asset: %s) is only available in the following material domains: %s"), *InExternalCode.Name.ToString(), *AssetPathName.ToString(), *SupportedMaterialDomainsString);
	}

} // MaterialExpressionUtils
