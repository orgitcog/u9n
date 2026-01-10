/**
 * @file DeepTreeEchoFacade.h
 * @brief Unified API facade for the Deep Tree Echo cognitive framework
 * 
 * This header provides a single entry point to access all Deep Tree Echo
 * subsystems, implementing the neuro-symbolic AI architecture where:
 * - Deep Tree Echo = Neural perception / intuitive vision
 * - Unreal Engine = Symbolic differential solver / game physics
 * 
 * The facade consolidates previously fragmented components into a coherent
 * membrane-based cognitive architecture following the o9c specification.
 */
#pragma once

// ============================================================================
// Core Systems
// ============================================================================
#include "Core/DeepTreeEchoCore.h"
#include "Core/CognitiveCycleManager.h"
#include "Core/CognitiveMemoryManager.h"
#include "Core/Sys6CognitiveBridge.h"
#include "Core/Sys6OperadEngine.h"
#include "Core/Types/CognitiveTypes.h"

// ============================================================================
// Reservoir Computing (Echo State Networks)
// ============================================================================
#include "Reservoir/DeepTreeEchoReservoir.h"
#include "Reservoir/ReservoirCognitiveIntegration.h"
#include "Reservoir/DeepCognitiveBridge.h"
#include "Reservoir/EchobeatsReservoirDynamics.h"
#include "Reservoir/Embodied4ECognition.h"
#include "Reservoir/TetradicReservoirIntegration.h"

// ============================================================================
// Memory Systems (Hypergraph Knowledge)
// ============================================================================
#include "Memory/MemorySystems.h"
#include "Memory/HypergraphMemorySystem.h"
#include "Memory/EpisodicMemorySystem.h"
#include "Memory/ReservoirMemoryIntegration.h"
#include "Memory/MemoryConsciousnessIntegration.h"

// ============================================================================
// 4E Embodied Cognition
// ============================================================================
#include "4ECognition/EmbodiedCognitionComponent.h"
#include "4ECognition/DNABodySchemaBinding.h"
#include "Sensorimotor/SensorimotorIntegration.h"
#include "Sensory/SensoryInputIntegration.h"

// ============================================================================
// Avatar Systems (Embodiment Interface)
// ============================================================================
#include "Avatar/EmbodiedAvatarComponent.h"
#include "Avatar/UnrealAvatarCognition.h"
#include "Avatar/UnrealAvatarEmbodiment.h"
#include "Avatar/ExpressiveAnimationSystem.h"
#include "Avatar/AdvancedEmotionBlending.h"
#include "Avatar/AvatarEvolutionSystem.h"
#include "Avatar/AvatarQualityAssurance.h"
#include "Avatar/Sys6AvatarIntegration.h"
#include "Avatar/UnrealAvatar/DeepTreeEchoAvatarComponent.h"
#include "Avatar/UnrealAvatar/AGICoreCommunication.h"
#include "Avatar/UnrealAvatar/AGIPCGManager.h"

// ============================================================================
// Emotion & Wisdom Systems
// ============================================================================
#include "Emotion/EmotionalSystem.h"
#include "Wisdom/WisdomCultivation.h"
#include "Wisdom/RelevanceRealizationEnnead.h"

// ============================================================================
// Active Inference & Learning
// ============================================================================
#include "ActiveInference/ActiveInferenceEngine.h"
#include "ActiveInference/AXIOMActiveInference.h"
#include "ActiveInference/ActiveInferenceCognitiveIntegration.h"
#include "ActiveInference/NicheConstructionSystem.h"
#include "Learning/OnlineLearningSystem.h"
#include "Learning/PredictiveAdaptationEngine.h"

// ============================================================================
// Attention & Planning
// ============================================================================
#include "Attention/AttentionSystem.h"
#include "Planning/PlanningSystem.h"
#include "Goals/HierarchicalGoalManager.h"

// ============================================================================
// Introspection & Metamodel
// ============================================================================
#include "Introspection/AutognosisSystem.h"
#include "Metamodel/HolisticMetamodel.h"

// ============================================================================
// Echobeats Integration
// ============================================================================
#include "Echobeats/EchobeatsStreamEngine.h"
#include "Echobeats/NestedShellStructure.h"
#include "Echobeats/TensionalCouplingDynamics.h"

// ============================================================================
// System Integration
// ============================================================================
#include "Cosmos/CosmosStateMachine.h"
#include "Entelechy/EntelechyFramework.h"
#include "Evolution/Enhanced4ECognitionEvolution.h"
#include "Social/SocialCognitionSystem.h"
#include "Language/LanguageSystem.h"
#include "Neural/NeuralNetworkBackend.h"

// ============================================================================
// Unreal Engine Bridge
// ============================================================================
#include "UnrealBridge/DeepTreeEchoUnrealBridge.h"
#include "UnrealBridge/DeepTreeEchoUnrealIntegration.h"
#include "Blueprint/DeepTreeEchoBlueprintLibrary.h"

// ============================================================================
// Sys6 Triality (LCM Clock Synchronization)
// ============================================================================
#include "Sys6/Sys6LCMClockSynchronizer.h"

// ============================================================================
// System5 Cognitive Integration
// ============================================================================
#include "System5/System5CognitiveIntegration.h"

// ============================================================================
// Taskflow Scheduling
// ============================================================================
#include "Taskflow/TaskflowCognitiveScheduler.h"
#include "Taskflow/TaskflowProfiler.h"

// ============================================================================
// Game Training Systems
// ============================================================================
#include "GameTraining/GameTrainingEnvironment.h"
#include "GameTraining/GameSkillTrainingSystem.h"
#include "GameTraining/GameControllerInterface.h"
#include "GameTraining/ReinforcementLearningBridge.h"

/**
 * @namespace DeepTreeEcho
 * @brief Namespace containing all Deep Tree Echo cognitive framework components
 * 
 * The Deep Tree Echo framework implements a neuro-symbolic cognitive architecture
 * based on:
 * - Echo State Networks for temporal pattern processing
 * - P-System membranes for hierarchical boundary management
 * - Butcher B-Series for differential temporal integration
 * - Hypergraph memory for multi-relational knowledge
 * - 4E Cognition (Embodied, Embedded, Enacted, Extended)
 * - Relevance Realization for wisdom cultivation
 */
namespace DeepTreeEcho
{
    /**
     * @brief Get the Deep Tree Echo version string
     * @return Version string in format "major.minor.patch"
     */
    inline const char* GetVersion() { return "1.0.0"; }
    
    /**
     * @brief Get the consolidation date
     * @return Date string when fragmentation was resolved
     */
    inline const char* GetConsolidationDate() { return "2026-01-10"; }
    
    /**
     * @brief Architecture description
     * @return Description of the neuro-symbolic integration
     */
    inline const char* GetArchitectureDescription()
    {
        return "Deep Tree Echo: Neural perception layer for Unreal Engine symbolic solver. "
               "Implements 4E embodied cognition with echo state reservoir computing, "
               "P-system membrane filtering, and relevance realization optimization.";
    }
}
