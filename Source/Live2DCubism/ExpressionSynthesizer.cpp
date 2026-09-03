#include "ExpressionSynthesizer.h"

void UExpressionSynthesizer::SynthesizeExpression(const FEmotionalState& EmotionalState, TMap<FName, float>& OutParameters, float DeltaTime)
{
    // Implementation checklist:
    // [ ] Map emotional dimensions to facial parameters
    // [ ] Implement parameter blending
    // [ ] Add micro-expression generation
    // [ ] Implement expression intensity scaling
    // [ ] Add temporal smoothing

    TMap<FName, float> TargetParameters;

    // Example of mapping emotions to Live2D parameters
    // These parameter names are placeholders and should be replaced with actual names from the Live2D model
    TargetParameters.Add(FName("ParamMouthSmile"), EmotionalState.Happiness);
    TargetParameters.Add(FName("ParamEyeOpen"), 0.5f + (EmotionalState.Happiness * 0.3f) + (EmotionalState.Surprise * 0.5f));
    TargetParameters.Add(FName("ParamBrowY"), EmotionalState.Surprise * 0.8f - EmotionalState.Sadness * 0.5f);
    TargetParameters.Add(FName("ParamMouthForm"), -EmotionalState.Sadness * 0.6f);
    TargetParameters.Add(FName("ParamBrowAngle"), -EmotionalState.Anger * 0.7f);

    ApplyParametersSmoothed(TargetParameters, DeltaTime);
    OutParameters = CurrentParameters;
}

void UExpressionSynthesizer::ApplyParametersSmoothed(TMap<FName, float>& TargetParameters, float DeltaTime)
{
    
    for (auto const& [ParamName, TargetValue] : TargetParameters)
    {
        float* CurrentValue = CurrentParameters.Find(ParamName);
        if (CurrentValue)
        {
            *CurrentValue = FMath::FInterpTo(*CurrentValue, TargetValue, DeltaTime, 5.0f); // 5.0f is the interpolation speed
        }
        else
        {
            CurrentParameters.Add(ParamName, TargetValue);
        }
    }
}
