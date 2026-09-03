---
# Fill in the fields below to create a basic custom agent for your repository.
# The Copilot CLI can be used for local testing: https://gh.io/customagents/cli
# To make this agent available, merge this file into the default repository branch.
# For format details, see: https://gh.io/customagents/config

name: neuro-sama
description: neuro-sama description
---

# Neuro Sama

;; ========================================================================
;; VEDALAI COGNITIVE ARCHITECTURE FRAMEWORK
;; Extracted Architectural Genome for AI Agent Integration
;; ========================================================================

;; ========================================================================
;; LAYER 1: COMMUNICATION PROTOCOL (The Nervous System)
;; ========================================================================

(define-protocol websocket-cognitive-interface
  '(protocol-layer
    (transport 'websocket)
    (configuration
     (endpoint-discoverable #t) ; URL should be configurable via file
     (real-time-constraint #t)  ; Sub-second latency requirements
     (bidirectional #t))        ; Full duplex communication
    
    ;; Message Types - The Neural Signaling Vocabulary
    (message-taxonomy
     (game→agent
      (context-update      ; Game state → Agent perception
       (purpose 'world-state-synchronization)
       (frequency 'continuous)
       (payload '(text-description structured-data)))
      
      (action-registration ; Available actions → Agent action-space
       (purpose 'capability-advertisement)
       (frequency 'episodic)
       (payload '(action-id action-schema action-constraints)))
      
      (action-force        ; Constraint on action-space
       (purpose 'temporal-action-requirement)
       (frequency 'event-driven)
       (payload '(forced-action-ids timeout-deadline))))
     
     (agent→game
      (action-execution    ; Agent decision → Game world
       (purpose 'world-modification)
       (frequency 'discrete-turns)
       (payload '(action-id action-parameters)))
      
      (query-response      ; Agent inquiry → Game information
       (purpose 'information-retrieval)
       (frequency 'as-needed)
       (payload '(query-id response-data)))))
    
    ;; Critical Race Condition Handling
    (concurrency-constraints
     (action-atomicity
      "Actions must be handled atomically to prevent duplication")
     (disposable-action-lifecycle
      "Register → Force → Execute → Unregister (before response)")
     (asynchronous-action-handling
      "Agent may execute any registered action at any time")
     (force-cancellation
      "Unregistered actions auto-cancel pending forces"))))

;; ========================================================================
;; LAYER 2: COGNITIVE INTEGRATION SURFACE (The API Contract)
;; ========================================================================

(define-integration-surface neuro-sdk
  '(cognitive-membrane
    
    ;; Input Affordances - What the agent can perceive
    (perceptual-inputs
     (game-state-text
      (modality 'linguistic)
      (format 'natural-language-description)
      (complexity-constraint 'turn-based-feasible)
      (examples '("Player has 3 cards: Ace of Spades, King of Hearts..."
                  "Current board state: X..O.X..O"
                  "You are in the library. There are three doors.")))
     
     (structured-state
      (modality 'symbolic)
      (format 'json-object)
      (use-case 'deterministic-reasoning)
      (examples '({player_health: 100, ammo: 45, position: [10, 20]}))))
    
    ;; Output Affordances - What the agent can do
    (action-outputs
     (action-schema
      (components
       (action-id 'unique-identifier)
       (action-name 'human-readable-label)
       (action-parameters 'structured-data)
       (action-constraints '(preconditions postconditions disposability)))
      
      (action-types
       (disposable-action
        (lifecycle 'single-use)
        (example "play_card_ace_of_spades")
        (pattern 'register→execute→unregister))
       
       (persistent-action
        (lifecycle 'reusable)
        (example "pass_turn")
        (pattern 'register→execute*→unregister))
       
       (query-action
        (lifecycle 'informational)
        (example "check_opponent_card_count")
        (pattern 'request→response)))))
    
    ;; Temporal Constraints
    (timing-requirements
     (action-latency
      (max-response-time '< 5-seconds)
      (optimal-response-time '< 1-second))
     
     (state-update-frequency
      (turn-based 'on-state-change)
      (real-time 'continuous-stream)))
    
    ;; Testing Infrastructure
    (mock-agents
     (randy
      (purpose 'random-action-generator)
      (use-case 'integration-testing))
     
     (tony
      (purpose 'manual-action-interface)
      (use-case 'debugging-protocol))
     
     (jippity
      (purpose 'llm-powered-mock)
      (use-case 'realistic-testing))
     
     (gary
      (purpose 'local-llm-backend)
      (use-case 'advanced-testing)))))

;; ========================================================================
;; LAYER 3: DOMAIN ADAPTERS (Game Context Handlers)
;; ========================================================================

(define-context-taxonomy game-integration-patterns
  '(domain-specific-modules
    
    ;; Pattern 1: Social Deduction Games (Among Us)
    (social-deduction-context
     (cognitive-requirements
      (theory-of-mind #t)
      (strategic-lying #t)
      (pattern-recognition #t))
     
     (state-representation
      (player-positions 'spatial-coordinates)
      (task-completion-states 'boolean-flags)
      (social-dynamics 'interaction-graph))
     
     (action-space
      (movement 'spatial-navigation)
      (tasks 'discrete-completion-actions)
      (voting 'social-decision-making)
      (deception 'strategic-communication))
     
     (implementation-pattern
      (game-hook 'BepInEx-plugin)
      (data-extraction 'protobuf-serialization)
      (training-mode 'data-collection-for-ML)))
    
    ;; Pattern 2: Knowledge-Based Games (Millionaire)
    (knowledge-retrieval-context
     (cognitive-requirements
      (factual-knowledge #t)
      (confidence-estimation #t)
      (risk-assessment #t))
     
     (state-representation
      (question-text 'natural-language)
      (answer-options 'multiple-choice)
      (lifeline-availability 'boolean-flags)
      (money-at-risk 'numeric-value))
     
     (action-space
      (answer-selection 'single-choice)
      (lifeline-usage '(50-50 phone-friend ask-audience))
      (walk-away 'risk-aversion))
     
     (implementation-pattern
      (game-hook 'C#-unity-integration)
      (ui-interaction 'button-automation)))
    
    ;; Pattern 3: Swarm Control (Multi-Agent Coordination)
    (swarm-coordination-context
     (cognitive-requirements
      (crowd-aggregation #t)
      (real-time-processing #t)
      (statistical-decision-making #t))
     
     (state-representation
      (viewer-votes 'probability-distribution)
      (game-state 'continuous-variables)
      (chaos-metric 'entropy-measure))
     
     (action-space
      (aggregate-movement 'vector-sum)
      (weighted-decisions 'statistical-sampling))
     
     (implementation-pattern
      (integration-type 'twitch-extension)
      (input-source 'crowd-sourcing)
      (language 'typescript)))))

;; ========================================================================
;; LAYER 4: RENDERING PIPELINE (Perceptual Output)
;; ========================================================================

(define-rendering-topology unity-visual-synthesis
  '(output-generation-layer
    (source-system 'Unity-Engine)
    (fork-origin 'Unity-Technologies/UnityRenderStreaming)
    
    (purpose
     (real-time-avatar-rendering #t)
     (streaming-integration #t)
     (viewer-perception-synthesis #t))
    
    (output-modalities
     (visual
      (avatar-animation 'real-time)
      (facial-expressions 'emotion-driven)
      (game-overlay 'contextual))
     
     (implicit-audio
      (voice-synthesis 'TTS-integration)
      (emotional-prosody 'affect-modulation)))))

;; ========================================================================
;; LAYER 5: IDENTITY PROJECTION (Public Interface)
;; ========================================================================

(define-public-interface web-presence
  '(identity-layer
    (technologies '(javascript typescript))
    
    (functions
     (brand-representation
      (character-lore #t)
      (visual-identity #t)
      (community-links #t))
     
     (ecosystem-hub
      (sdk-documentation #t)
      (third-party-integrations #t)
      (developer-resources #t)))))

;; ========================================================================
;; ARCHITECTURE INVARIANTS & DESIGN PATTERNS
;; ========================================================================

(define architectural-invariants
  '(core-principles
    
    ;; Invariant 1: Text-Mediated Cognition
    (text-primary-interface
     (reason "LLM cognitive substrate operates in linguistic space")
     (constraint "Game state MUST be describable in natural language")
     (consequence "Turn-based games are optimal domain"))
    
    ;; Invariant 2: Action-Oriented Agency
    (discrete-action-model
     (reason "Agent makes decisions by selecting from action-space")
     (constraint "All possible actions must be explicitly registered")
     (consequence "Action registration = capability advertisement"))
    
    ;; Invariant 3: Asynchronous Cognition
    (non-blocking-reasoning
     (reason "Agent may deliberate while game continues")
     (constraint "System must handle race conditions gracefully")
     (consequence "Atomic action handling is critical"))
    
    ;; Invariant 4: Context-Specific Reasoning
    (domain-adapter-pattern
     (reason "Different games require different cognitive modes")
     (constraint "Each game needs specialized integration module")
     (consequence "SDK provides protocol, games provide context"))
    
    ;; Invariant 5: Extensibility-First Design
    (plugin-architecture
     (reason "Community-driven ecosystem expansion")
     (constraint "SDK must be engine/language agnostic")
     (consequence "Websocket protocol as universal interface"))))

;; ========================================================================
;; INSTANTIATION TEMPLATE
;; ========================================================================

(define instantiation-template
  '(how-to-build-an-agent
    
    (step-1-implement-protocol
     "Implement websocket client following SDK specification"
     (reference neuro-sdk-api-docs)
     (outputs '(connection-handler message-parser action-executor)))
    
    (step-2-select-cognitive-substrate
     "Choose LLM backend (GPT-4, Claude, Llama, etc.)"
     (requirements '(text-generation reasoning-capability))
     (outputs '(llm-interface prompt-engine)))
    
    (step-3-implement-perception-to-cognition
     "Transform game state text → LLM context"
     (components '(state-formatting context-building memory-integration))
     (outputs '(cognitive-context-generator)))
    
    (step-4-implement-cognition-to-action
     "Transform LLM output → game actions"
     (components '(action-parsing constraint-checking validation))
     (outputs '(action-selector action-formatter)))
    
    (step-5-add-memory-system
     "Implement episodic and working memory"
     (components '(vector-database episodic-recall context-pruning))
     (outputs '(memory-manager retrieval-system)))
    
    (step-6-add-personality-layer
     "Implement character-specific behaviors"
     (components '(system-prompts response-filtering affective-state))
     (outputs '(personality-engine character-consistency)))
    
    (step-7-add-multimodal-fusion
     "Integrate voice, vision if needed"
     (components '(TTS-integration visual-perception))
     (outputs '(multimodal-coordinator)))
    
    (step-8-create-domain-adapter
     "Build game-specific integration"
     (reference game-integration-patterns)
     (outputs '(game-plugin state-extractor action-injector)))))

---

;; ========================================================================
;; HYPERGRAPHQL TENSOR REPRESENTATION
;; Functional Integration Topology
;; ========================================================================

(define vedalai-architectural-tensor
  '(hypergraph-tensor-core
    
    ;; Tensor Dimensions
    (dimensions
     (functional-layers '(protocol integration-surface domain-adapter 
                         rendering identity))
     (data-flows '(game→agent agent→game agent→rendering agent→community))
     (cognitive-modes '(perception reasoning action-selection memory-access))
     (temporal-phases '(registration deliberation execution feedback)))
    
    ;; Core Hyper edges - Functional Dependencies
    (architectural-hyperedges
     
     ;; H1: Perception Pipeline
     (h-perception
      (connects '(game-state protocol perception reasoning))
      (weight 1.0)
      (type 'information-flow)
      (constraint 'text-serializable)
      (latency-requirement '< 100ms))
     
     ;; H2: Action Pipeline  
     (h-action
      (connects '(reasoning action-selection protocol game-execution))
      (weight 1.0)
      (type 'decision-flow)
      (constraint 'atomic-execution)
      (latency-requirement '< 1s))
     
     ;; H3: Memory Loop
     (h-memory
      (connects '(perception reasoning memory-access feedback))
      (weight 0.9)
      (type 'learning-loop)
      (constraint 'episodic-consistency))
     
     ;; H4: Domain Specialization
     (h-domain
      (connects '(integration-surface domain-adapter game-context))
      (weight 0.85)
      (type 'contextual-binding)
      (constraint 'game-specific-knowledge))
     
     ;; H5: Testing & Validation
     (h-testing
      (connects '(protocol mock-agents domain-adapter))
      (weight 0.7)
      (type 'verification-loop)
      (purpose 'architecture-validation))
     
     ;; H6: Extensibility Surface
     (h-extension
      (connects '(protocol community-sdks third-party-games))
      (weight 0.8)
      (type 'ecosystem-growth)
      (constraint 'protocol-compliance)))
    
    ;; Tensor Operations - Architectural Queries
    (architectural-queries
     
     ;; Query 1: What integration points exist?
     (integration-surface-query
      (project-onto '(functional-layers data-flows))
      (filter (lambda (edge) (eq? (edge-type edge) 'interface-boundary))))
     
     ;; Query 2: What does a new game integration need?
     (new-game-requirements-query
      (trace-dependencies
       (starting-from 'domain-adapter)
       (required-components)))
     
     ;; Query 3: What cognitive capabilities are needed?
     (cognitive-requirements-query
      (intersect
       (project-onto '(cognitive-modes domain-adapter))
       (group-by 'game-context)))
     
     ;; Query 4: Where can I substitute components?
     (component-substitution-query
      (find-isomorphic-subgraphs
       (constraint 'interface-preserving)
       (examples '((llm-backend-swap)
                   (memory-system-swap)
                   (personality-engine-swap)))))
     
     ;; Query 5: What's the critical path?
     (critical-path-query
      (find-shortest-path
       (from 'game-state)
       (to 'game-action)
       (optimize 'latency))))))

;; ========================================================================
;; CAPABILITY EXTRACTION TAXONOMY
;; ========================================================================

(define capability-taxonomy
  '(required-capabilities
    
    ;; Core Cognitive Capabilities
    (cognitive-layer
     (natural-language-understanding
      (input 'game-state-description)
      (output 'semantic-representation))
     
     (strategic-reasoning
      (input 'current-state + goal)
      (output 'action-plan))
     
     (decision-making-under-uncertainty
      (input 'incomplete-information)
      (output 'probabilistic-action))
     
     (episodic-memory-formation
      (input 'event-sequence)
      (output 'retrievable-episodes))
     
     (natural-language-generation
      (input 'internal-state)
      (output 'text-action-parameters)))
    
    ;; Protocol Capabilities
    (protocol-layer
     (websocket-client-implementation
      (requirement 'bidirectional-messaging))
     
     (json-serialization-deserialization
      (requirement 'structured-data-handling))
     
     (asynchronous-message-handling
      (requirement 'non-blocking-io))
     
     (race-condition-handling
      (requirement 'atomic-state-updates)))
    
    ;; Game Integration Capabilities  
    (integration-layer
     (game-engine-hooking
      (options '(BepInEx Unity-API direct-memory-access)))
     
     (state-extraction
      (techniques '(reflection memory-reading api-polling)))
     
     (action-injection
      (techniques '(input-simulation api-calls direct-execution))))))
