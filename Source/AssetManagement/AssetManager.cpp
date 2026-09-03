#include "AssetManager.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/Texture2D.h"

UAssetManager::UAssetManager()
{
	LoadAllAssets();
}

void UAssetManager::LoadAllAssets()
{
	// Load Live2D Models
	{
		FString ModelsPath = FPaths::ProjectContentDir() + TEXT("Live2DModels/");
		TArray<FString> ModelNames = { "miara_pro_en", "miku", "miku_pro_jp", "rice_pro_en" };

		for (const FString& ModelName : ModelNames)
		{
			FString ModelJsonPath = ModelsPath + ModelName + "/runtime/" + ModelName + ".model3.json";
			UObject* Model = StaticLoadObject(UObject::StaticClass(), nullptr, *ModelJsonPath);
			if (Model)
			{
				Live2DModels.Add(ModelName, Model);
			}
		}
	}

	// Load Profile Pictures
	{
		FString TexturesPath = FPaths::ProjectContentDir() + TEXT("UI/Textures/");
		TArray<FString> VariantNames = { "4K", "2K", "1K", "512", "256", "128" };

		for (const FString& VariantName : VariantNames)
		{
			FString TexturePath = TexturesPath + "DeepTreeEcho_Profile_" + VariantName + ".DeepTreeEcho_Profile_" + VariantName;
			UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *TexturePath);
			if (Texture)
			{
				ProfilePictures.Add(VariantName, Texture);
			}
		}
	}
}

void UAssetManager::LoadLive2DModel(const FString& ModelName)
{
	// Placeholder for model loading logic
}

UTexture* UAssetManager::GetProfilePicture(const FString& VariantName)
{
	if (ProfilePictures.Contains(VariantName))
	{
		return ProfilePictures[VariantName];
	}
	return nullptr;
}
