/**
 * Engine/Source annotated path index — nest-4 grip projection.
 *
 * Comment-only grouping of catalog hubs. Does not #include Unreal public
 * headers. Lookup lives on DeepTreeEcho/Enaction/EngineGripMap.h.
 *
 * Nest-1: Engine/Source
 * Nest-2: Runtime + Editor
 * Nest-3: + Developer + Programs
 * Nest-4: nine terms below (see COGNITIVE_GRIP.md)
 *
 * Do not git-mv Runtime, Editor, Developer, Programs, or ThirdParty.
 */
#pragma once

#include "DeepTreeEcho/Enaction/EngineGripMap.h"

// ============================================================================
// 1 CoreRuntime — Independent Region Runtime (APL 1, 95)
// ============================================================================
// Engine/Source/Runtime/Core
// Engine/Source/Runtime/CoreUObject
// Engine/Source/Runtime/Launch
// Engine/Source/Runtime/Projects

// ============================================================================
// 2 EngineWorld — not DeepTreeEcho/Self (APL 30)
// ============================================================================
// Engine/Source/Runtime/Engine
// Engine/Source/Runtime/EngineSettings

// ============================================================================
// 3 RenderReservoir (APL 30)
// ============================================================================
// Engine/Source/Runtime/RenderCore
// Engine/Source/Runtime/RHI
// Engine/Source/Runtime/Renderer
// Engine/Source/Runtime/StateStream

// ============================================================================
// 4 BodyRuntime (APL 8)
// ============================================================================
// Engine/Source/Runtime/InputCore
// Engine/Source/Runtime/Slate
// Engine/Source/Runtime/SlateCore
// Engine/Source/Runtime/UMG
// Engine/Source/Runtime/ApplicationCore

// ============================================================================
// 5 MemoryRuntime (APL 95)
// ============================================================================
// Engine/Source/Runtime/Serialization
// Engine/Source/Runtime/PakFile
// Engine/Source/Runtime/AssetRegistry

// ============================================================================
// 6 EditorAttention (APL 107)
// ============================================================================
// Engine/Source/Editor
// Engine/Source/Runtime/TypedElementFramework
// Engine/Source/Runtime/GameplayDebugger

// ============================================================================
// 7 NetStreams (APL 52, 100)
// ============================================================================
// Engine/Source/Runtime/Net
// Engine/Source/Runtime/Networking
// Engine/Source/Runtime/Sockets
// Engine/Source/Runtime/Online
// Engine/Source/Runtime/AudioMixer

// ============================================================================
// 8 ActionRuntime (APL 30, 52)
// ============================================================================
// Engine/Source/Runtime/AIModule
// Engine/Source/Runtime/GameplayTasks
// Engine/Source/Runtime/NavigationSystem

// ============================================================================
// 9 DevWisdom (APL 95, 205)
// ============================================================================
// Engine/Source/Developer
// Engine/Source/Programs
// Engine/Source/UnrealEditor.Target.cs
// Engine/Source/UnrealGame.Target.cs
// Engine/Source/UnrealServer.Target.cs
// Engine/Source/UnrealClient.Target.cs

// Mosaic / countryside (not a tenth nest-4 core): Engine/Source/ThirdParty, Engine/Plugins
