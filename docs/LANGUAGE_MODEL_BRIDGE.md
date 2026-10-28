# Language Model Bridge

Deep Tree Echo obtains every word it says, writes, thinks aloud or publishes from a
language model. No component in this repository may stand in for the model with fixed,
templated, randomised or fallback text, and no component may take an action on the
character's behalf (send a message, publish a post, record a training example, play
speech) unless the model produced the text that action carries.

The single code path that enforces this is `UDTELanguageModelBridge`, declared in
`DeepTreeEcho/Core/DTELanguageModelBridge.h` and compiled into the `DeepTreeEcho` module.

## What the bridge talks to

Any OpenAI-compatible chat completion server. The default is a locally served model:

| Variable | Default | Meaning |
|----------|---------|---------|
| `LOCAL_LLM_BASE_URL` | `http://localhost:11434/v1` | Base URL ending in `/v1` (Ollama, llama.cpp `llama-server`, vLLM, ...) |
| `LOCAL_LLM_MODEL` | `llama3.2` | Model name as the server lists it under `GET /models` |
| `LOCAL_LLM_API_KEY` or `OPENAI_API_KEY` | empty | Optional bearer token for hosted endpoints |

Component properties (`BaseUrl`, `Model`, `ApiKey`, `PersonaPrompt`, `Temperature`,
`MaxTokens`, `TimeoutSeconds`) override the environment for one actor.

A model counts as available only when the server answers `GET /models` and lists the
configured model, or when a completion request succeeds. Availability is reported through
`OnModelAvailabilityChanged` and `GetModelStatus()` so operators can see it. It is never
reported through anything the character says.

## Using it from C++

Components:

```cpp
#include "DTELanguageModelBridge.h"

// In BeginPlay
ModelBridge = UDTELanguageModelBridge::FindOrAdd(GetOwner());

// Later, wherever a template used to be filled in
TWeakObjectPtr<ThisClass> WeakThis(this);
ModelBridge->RequestText(
    TEXT("Greet the visitor by name in one or two sentences."),
    FString::Printf(TEXT("Visitor: %s\nMood: %.2f\nFormality: %.2f"), *Name, Mood, Formality),
    [WeakThis](bool bOk, const FString& TextOrError)
    {
        if (!WeakThis.IsValid()) { return; }
        if (!bOk)
        {
            UE_LOG(LogDTEModel, Warning, TEXT("Greeting skipped: %s"), *TextOrError);
            return;            // say nothing; never substitute a line
        }
        WeakThis->OnResponseGenerated.Broadcast(TextOrError);
    });
```

Plain C++ classes (no `UObject`) use the shared client directly:

```cpp
FDTELanguageModelClient::Get().Complete(SystemPrompt, UserPrompt,
    [Token](bool bOk, const FString& Text) { /* guard lifetime with Token */ });
```

Every request is asynchronous. Callbacks run on the game thread when the HTTP response
arrives. Capture a `TWeakObjectPtr` (or a shared lifetime token) and check it first.

## Rules for new code

1. A former template becomes a prompt: its intent is the instruction, its slot values are
   the context. The template text itself is deleted.
2. Side effects that depend on the text happen only inside the success callback.
3. On failure, log an operator-facing warning and do nothing else. No apology, no
   placeholder, no random pick, no "offline mode".
4. Numeric and state computations that are not text (scores, weights, emotion
   intensities, timing) are unaffected by this rule unless they are presented as the
   character's reaction to specific content.
