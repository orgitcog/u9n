// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ProfileTree/GenericProfileTreeBuilder.h"

namespace ProjectLauncher
{
	class PROJECTLAUNCHER_API FCustomProfileTreeBuilder : public FGenericProfileTreeBuilder
	{
	public:
		FCustomProfileTreeBuilder( const ILauncherProfileRef& Profile, const TSharedRef<FModel>& InModel );
		virtual ~FCustomProfileTreeBuilder() = default;

		virtual void Construct() override;

		virtual FString GetName() const override
		{
			return TEXT("CustomProfile");
		}
	};


	class PROJECTLAUNCHER_API FCustomProfileTreeBuilderFactory : public ILaunchProfileTreeBuilderFactory
	{
	public:
		virtual TSharedPtr<ILaunchProfileTreeBuilder> TryCreateTreeBuilder( const ILauncherProfileRef& Profile, const TSharedRef<FModel>& InModel ) override;

		virtual bool IsProfileTypeSupported(EProfileType ProfileType ) const override { return ProfileType == EProfileType::Custom; }

	};

}
