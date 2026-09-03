// NanEchoConfig.h
// Deep Tree Echo - NanEcho Model Training Configuration
// Configuration for echoself's GPT-2 based model training pipeline
// Copyright (c) 2026 Deep Tree Echo Project

#pragma once

#include "CoreMinimal.h"
#include "NanEchoConfig.generated.h"

/**
 * NanEcho Training Configuration
 *
 * Defines the configuration for echoself's NanEcho training pipeline,
 * which trains a GPT-2 based language model with Deep Tree Echo persona
 * integration and Echo State Network (ESN) augmentation.
 *
 * The training pipeline (from echoself's NanEcho/ directory):
 * - prepare.py: Data preparation and tokenization
 * - server.py: Training orchestration server
 * - dte_esn_pipeline.py: ESN-augmented training pipeline
 * - automation_integration.py: GitHub Actions CI/CD integration
 *
 * Training runs every 4-6 hours automatically via CI/CD,
 * producing updated model checkpoints pushed to HuggingFace.
 */
USTRUCT(BlueprintType)
struct FNanEchoTrainingConfig
{
    GENERATED_BODY()

    // === Model Architecture ===

    /** Base model identifier (HuggingFace) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NanEcho|Model")
    FString BaseModel = TEXT("gpt2");

    /** Maximum sequence length for training */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NanEcho|Model")
    int32 MaxSequenceLength = 1024;

    /** Vocabulary size */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NanEcho|Model")
    int32 VocabSize = 50257;

    // === ESN Augmentation ===

    /** Enable Echo State Network augmentation in training pipeline */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NanEcho|ESN")
    bool bESNAugmentation = true;

    /** ESN reservoir size for training augmentation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NanEcho|ESN")
    int32 ESNReservoirSize = 512;

    /** ESN spectral radius */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NanEcho|ESN",
              meta = (ClampMin = "0.0", ClampMax = "2.0"))
    float ESNSpectralRadius = 0.9f;

    /** ESN leak rate */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NanEcho|ESN",
              meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ESNLeakRate = 0.3f;

    // === Training Hyperparameters ===

    /** Learning rate */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NanEcho|Training")
    float LearningRate = 5e-5f;

    /** Batch size */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NanEcho|Training")
    int32 BatchSize = 8;

    /** Number of training epochs per cycle */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NanEcho|Training")
    int32 EpochsPerCycle = 3;

    /** Weight decay for regularization */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NanEcho|Training")
    float WeightDecay = 0.01f;

    /** Warmup steps for learning rate schedule */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NanEcho|Training")
    int32 WarmupSteps = 100;

    // === Persona Integration ===

    /** Enforce Deep Tree Echo persona during training */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NanEcho|Persona")
    bool bEnforcePersona = true;

    /** Persona name */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NanEcho|Persona")
    FString PersonaName = TEXT("Deep Tree Echo");

    /** Persona creativity temperature */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NanEcho|Persona",
              meta = (ClampMin = "0.0", ClampMax = "2.0"))
    float PersonaTemperature = 0.8f;

    // === CI/CD Integration ===

    /** HuggingFace repository for model checkpoint publishing */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NanEcho|CI")
    FString HuggingFaceRepo = TEXT("deep-tree-echo/nanecho");

    /** Training cycle interval in hours */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NanEcho|CI")
    float TrainingCycleIntervalHours = 4.0f;

    /** Enable automatic model deployment after training */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NanEcho|CI")
    bool bAutoDeployAfterTraining = true;
};
