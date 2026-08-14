// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#define INSTRUMENTATION_FUNCTION_ATTRIBUTES __declspec(safebuffers) __attribute__((disable_sanitizer_instrumentation))

// Reserves 12 NOPs before the function and 2 NOPs at function entry so that we can patch atomically
#define INSTRUMENTATION_HOTPATCH_TOTAL_NOPS  14
#define INSTRUMENTATION_HOTPATCH_PREFIX_NOPS 12

#define INSTRUMENTATION_FUNCTION_HOTPATCHABLE __attribute__((patchable_function_entry(INSTRUMENTATION_HOTPATCH_TOTAL_NOPS, INSTRUMENTATION_HOTPATCH_PREFIX_NOPS)))