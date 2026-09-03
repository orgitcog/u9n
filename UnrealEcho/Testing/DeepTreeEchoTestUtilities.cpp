#include "Testing/DeepTreeEchoTestUtilities.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "HighResScreenshot.h"
#include "ImageUtils.h"
#include "IImageWrapperModule.h"
#include "IImageWrapper.h"
#include "Misc/FileHelper.h"
#include "Modules/ModuleManager.h"

// Include all avatar components
#include "Personality/PersonalityTraitSystem.h"
#include "Cognitive/CognitiveSystem.h"
#include "Avatar/AvatarMaterialManager.h"
#include "Audio/AudioManagerComponent.h"
#include "Avatar/Avatar3DComponent.h"

AActor* FDeepTreeEchoTestUtilities::SpawnCompleteAvatar(UWorld* World)
{
    if (!World)
    {
        return nullptr;
    }

    // Spawn a base actor
    AActor* Avatar = World->SpawnActor<AActor>();
    if (!Avatar)
    {
        return nullptr;
    }

    // Add all necessary components
    UPersonalityTraitSystem* Personality = NewObject<UPersonalityTraitSystem>(Avatar);
    Personality->RegisterComponent();

    UCognitiveSystem* Cognitive = NewObject<UCognitiveSystem>(Avatar);
    Cognitive->RegisterComponent();

    UAvatarMaterialManager* Materials = NewObject<UAvatarMaterialManager>(Avatar);
    Materials->RegisterComponent();

    UAudioManagerComponent* Audio = NewObject<UAudioManagerComponent>(Avatar);
    Audio->RegisterComponent();

    UAvatar3DComponent* Avatar3D = NewObject<UAvatar3DComponent>(Avatar);
    Avatar3D->RegisterComponent();

    return Avatar;
}

void FDeepTreeEchoTestUtilities::SetEmotionalState(AActor* Avatar, EAvatarEmotionalState Emotion, float Intensity)
{
    if (!Avatar)
    {
        return;
    }

    UCognitiveSystem* Cognitive = Avatar->FindComponentByClass<UCognitiveSystem>();
    if (Cognitive)
    {
        Cognitive->SetEmotionalState(Emotion, Intensity);
    }
}

void FDeepTreeEchoTestUtilities::SetCognitiveLoad(AActor* Avatar, float Load)
{
    if (!Avatar)
    {
        return;
    }

    UCognitiveSystem* Cognitive = Avatar->FindComponentByClass<UCognitiveSystem>();
    if (Cognitive)
    {
        Cognitive->SetCognitiveLoad(Load);
    }
}

void FDeepTreeEchoTestUtilities::CaptureScreenshot(const FString& Name)
{
    FString ScreenshotPath = FPaths::AutomationDir() / TEXT("Screenshots") / Name;
    FScreenshotRequest::RequestScreenshot(ScreenshotPath, false, false);
}

bool FDeepTreeEchoTestUtilities::CompareImages(const FString& ImageA, const FString& ImageB, float Tolerance)
{
    TArray<uint8> ImageDataA, ImageDataB;
    if (!FFileHelper::LoadFileToArray(ImageDataA, *ImageA) || !FFileHelper::LoadFileToArray(ImageDataB, *ImageB))
    {
        return false;
    }

    IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
    TSharedPtr<IImageWrapper> ImageWrapperA = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
    TSharedPtr<IImageWrapper> ImageWrapperB = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);

    if (!ImageWrapperA.IsValid() || !ImageWrapperB.IsValid() || !ImageWrapperA->SetCompressed(ImageDataA.GetData(), ImageDataA.Num()) || !ImageWrapperB->SetCompressed(ImageDataB.GetData(), ImageDataB.Num()))
    {
        return false;
    }

    const TArray<uint8>* RawDataA = nullptr;
    const TArray<uint8>* RawDataB = nullptr;
    if (!ImageWrapperA->GetRaw(ERGBFormat::BGRA, 8, RawDataA) || !ImageWrapperB->GetRaw(ERGBFormat::BGRA, 8, RawDataB))
    {
        return false;
    }

    if (RawDataA->Num() != RawDataB->Num())
    {
        return false;
    }

    int64 MismatchedPixels = 0;
    for (int32 i = 0; i < RawDataA->Num(); ++i)
    {
        if ((*RawDataA)[i] != (*RawDataB)[i])
        {
            MismatchedPixels++;
        }
    }

    float Similarity = 1.0f - (float)MismatchedPixels / (float)RawDataA->Num();
    return Similarity >= Tolerance;
}

FString FDeepTreeEchoTestUtilities::GetComponentStats(UActorComponent* Component)
{
    if (!Component)
    {
        return TEXT("Invalid Component");
    }

    // This is a placeholder. A full implementation would require more detailed profiling.
    return FString::Printf(TEXT("Component: %s, IsActive: %d"), *Component->GetName(), Component->IsActive());
}
