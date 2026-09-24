/**
 * @file DeepTreeEchoFacade.h
 * @brief Unified API facade — nest-4 (OEIS A000081) entry point
 *
 * Nest-1: DeepTreeEcho/   Nest-2: Core + Self
 * Nest-3: + Reservoir + Cognition4E
 * Nest-4: nine terms below (see Self/COGNITIVE_GRIP.md)
 */
#pragma once

// ============================================================================
// 1 Core
// ============================================================================
#include "Core/DeepTreeEchoCore.h"
#include "Core/CognitiveCycleManager.h"
#include "Core/CognitiveMemoryManager.h"
#include "Core/AutonomyPipeline.h"
#include "Core/Sys6CognitiveBridge.h"
#include "Core/Sys6OperadEngine.h"
#include "Core/Types/CognitiveTypes.h"
#include "Core/Sys6/Sys6LCMClockSynchronizer.h"
#include "Core/System5/System5CognitiveIntegration.h"
#include "Core/Taskflow/TaskflowCognitiveScheduler.h"
#include "Core/Taskflow/TaskflowProfiler.h"

// ============================================================================
// 2 Self
// ============================================================================
#include "Self/CoreSelfEngine.h"
#include "Self/Introspection/AutognosisSystem.h"
#include "Self/EchoSelf/EchoSelfIntegration.h"
#include "Self/EchoSelf/ToroidalCognitiveAdapter.h"
#include "Self/EchoSelf/HypergraphBridgeAdapter.h"
#include "Self/EchoSelf/EchoSpaceMemoryBridge.h"

// ============================================================================
// 3 Reservoir
// ============================================================================
#include "Reservoir/DeepTreeEchoReservoir.h"
#include "Reservoir/ReservoirCognitiveIntegration.h"
#include "Reservoir/DeepCognitiveBridge.h"
#include "Reservoir/EchobeatsReservoirDynamics.h"
#include "Reservoir/TetradicReservoirIntegration.h"
#include "Cognition4E/Embodied4ECognition.h"

// ============================================================================
// 5 Memory
// ============================================================================
#include "Memory/MemorySystems.h"
#include "Memory/HypergraphMemorySystem.h"
#include "Memory/EpisodicMemorySystem.h"
#include "Memory/ReservoirMemoryIntegration.h"
#include "Memory/MemoryConsciousnessIntegration.h"

// ============================================================================
// 4 Cognition4E
// ============================================================================
#include "Cognition4E/4ECognition/EmbodiedCognitionComponent.h"
#include "Cognition4E/4ECognition/DNABodySchemaBinding.h"
#include "Cognition4E/Sensorimotor/SensorimotorIntegration.h"
#include "Cognition4E/Sensory/SensoryInputIntegration.h"
#include "Cognition4E/Avatar/EmbodiedAvatarComponent.h"
#include "Cognition4E/Avatar/UnrealAvatarCognition.h"
#include "Cognition4E/Avatar/UnrealAvatarEmbodiment.h"
#include "Cognition4E/Avatar/ExpressiveAnimationSystem.h"
#include "Cognition4E/Avatar/AdvancedEmotionBlending.h"
#include "Cognition4E/Avatar/AvatarEvolutionSystem.h"
#include "Cognition4E/Avatar/AvatarQualityAssurance.h"
#include "Cognition4E/Avatar/Sys6AvatarIntegration.h"
#include "Cognition4E/Avatar/UnrealAvatar/DeepTreeEchoAvatarComponent.h"
#include "Cognition4E/Avatar/UnrealAvatar/AGICoreCommunication.h"
#include "Cognition4E/Avatar/UnrealAvatar/AGIPCGManager.h"
#include "Cognition4E/Emotion/EmotionalSystem.h"
#include "Cognition4E/Evolution/Enhanced4ECognitionEvolution.h"

// ============================================================================
// 9 Wisdom
// ============================================================================
#include "Wisdom/WisdomCultivation.h"
#include "Wisdom/RelevanceRealizationEnnead.h"
#include "Wisdom/InsightFacilitator.h"
#include "Wisdom/ContemplativeAffordanceDetector.h"
#include "Wisdom/DialogicalPracticeFramework.h"
#include "Wisdom/SelfDeceptionDetector.h"
#include "Wisdom/MeaningIntegrator.h"
#include "Wisdom/Metamodel/HolisticMetamodel.h"

// ============================================================================
// 8 Enaction
// ============================================================================
#include "Enaction/ActiveInference/ActiveInferenceEngine.h"
#include "Enaction/ActiveInference/AXIOMActiveInference.h"
#include "Enaction/ActiveInference/ActiveInferenceCognitiveIntegration.h"
#include "Enaction/ActiveInference/NicheConstructionSystem.h"
#include "Enaction/Learning/OnlineLearningSystem.h"
#include "Enaction/Learning/PredictiveAdaptationEngine.h"
#include "Enaction/Planning/PlanningSystem.h"
#include "Enaction/Goals/HierarchicalGoalManager.h"
#include "Enaction/Cosmos/CosmosStateMachine.h"
#include "Enaction/Entelechy/EntelechyFramework.h"
#include "Enaction/Social/SocialCognitionSystem.h"
#include "Enaction/Language/LanguageSystem.h"
#include "Enaction/Neural/NeuralNetworkBackend.h"
#include "Enaction/UnrealBridge/DeepTreeEchoUnrealBridge.h"
#include "Enaction/UnrealBridge/DeepTreeEchoUnrealIntegration.h"
#include "Enaction/Blueprint/DeepTreeEchoBlueprintLibrary.h"
#include "Enaction/GameTraining/GameTrainingEnvironment.h"
#include "Enaction/GameTraining/GameSkillTrainingSystem.h"
#include "Enaction/GameTraining/GameControllerInterface.h"
#include "Enaction/GameTraining/ReinforcementLearningBridge.h"

// ============================================================================
// 6 Attention
// ============================================================================
#include "Attention/AttentionSystem.h"

// ============================================================================
// 7 Streams
// ============================================================================
#include "Streams/Echobeats/EchobeatsStreamEngine.h"
#include "Streams/Echobeats/NestedShellStructure.h"
#include "Streams/Echobeats/TensionalCouplingDynamics.h"

/**
 * @namespace DeepTreeEcho
 */
namespace DeepTreeEcho
{
    inline const char* GetVersion() { return "1.1.0"; }
    inline const char* GetConsolidationDate() { return "2026-08-13"; }
    inline const char* GetArchitectureDescription()
    {
        return "Deep Tree Echo nest-4 (OEIS A000081): Core, Self, Reservoir, "
               "Cognition4E, Memory, Attention, Streams, Enaction, Wisdom. "
               "EchoSelf identity ticks through AutonomyPipeline.";
    }
}
