---
name: emacs-aichat-skintwin
description: OpenCog-based dermatological cognitive system for Emacs with AI chat integration
---

# Emacs AIChat SkinTwin

## Overview

**Emacs AIChat SkinTwin** is a groundbreaking integration of OpenCog's cognitive architecture components with a comprehensive dermatological knowledge modeling system, all implemented within the Emacs ecosystem. This project represents the convergence of artificial general intelligence (AGI) research, medical knowledge representation, and practical interactive computing.

The system consists of two major components:
1. **AIChat**: A cognitive AI framework implementing OpenCog primitives (AtomSpace, ECAN, PLN, MOSES, ESN) in Emacs Lisp
2. **SkinTwin**: A multiscale dermatological model leveraging the cognitive architecture for medical reasoning and prediction

## Core Concept

SkinTwin applies cognitive computing principles to dermatology, creating a "digital twin" of skin biology that can:
- Represent complex biological knowledge (cells, proteins, conditions, treatments)
- Apply attention allocation to prioritize relevant medical information
- Perform probabilistic reasoning about diagnoses and treatments
- Learn treatment patterns from interaction history
- Predict disease progression using temporal neural networks
- Integrate seamlessly with Org-mode for knowledge base management

## Architecture

### Layer 1: AIChat Cognitive Primitives

The foundation is a faithful Emacs Lisp implementation of OpenCog cognitive modules:

#### AtomSpace (Knowledge Representation)
```elisp
;; aichat-opencog.el - Central knowledge graph
(defvar aichat-opencog-kb (make-hash-table :test 'equal)
  "Knowledge base storing atoms and relationships.")
```

**Capabilities:**
- Hypergraph-based knowledge representation
- Symbolic atom types: concepts, predicates, evaluations, inheritances
- Pattern matching and graph queries
- Truth value annotations for uncertain knowledge

#### ECAN (Economic Attention Networks)
```elisp
;; aichat-ecan.el - Dynamic attention allocation
(cl-defstruct aichat-ecan-av
  sti   ; Short-term importance (0.0-1.0)
  lti   ; Long-term importance (0.0-1.0)
  vlti) ; Very long-term importance (boolean)
```

**Capabilities:**
- STI/LTI attention values for each atom
- Spreading activation through knowledge graph
- Automatic decay and forgetting mechanisms
- Focus management for cognitive resources

#### PLN (Probabilistic Logic Networks)
```elisp
;; aichat-pln.el - Uncertain reasoning
(defun aichat-pln-deduction (ab-tv bc-tv)
  "Apply deduction rule: (A→B, B→C) ⊢ A→C")
```

**Capabilities:**
- Truth value arithmetic (strength and confidence)
- Deduction, inversion, and revision rules
- Forward and backward chaining
- Bayesian inference for medical reasoning

#### MOSES (Meta-Optimizing Semantic Evolutionary Search)
```elisp
;; aichat-moses.el - Program learning
(cl-defstruct aichat-moses-knob
  type values current position)
```

**Capabilities:**
- Genetic programming for pattern discovery
- Deme-based evolutionary optimization
- Automatic feature learning from interactions
- Program synthesis from examples

#### ESN (Echo State Networks)
```elisp
;; aichat-esn.el - Temporal prediction
(defvar aichat-esn-reservoir nil
  "Reservoir matrix for ESN computations.")
```

**Capabilities:**
- Recurrent neural network for sequence modeling
- Disease progression prediction
- Treatment response forecasting
- Time-series pattern recognition

### Layer 2: SkinTwin Domain Model

The medical knowledge layer built on cognitive primitives:

#### DermatoGraph (AtomSpace Integration)
```elisp
;; skintwin-integration.el
(defun skintwin-create-basic-concepts ()
  "Create basic skin biology concepts and relationships.")
```

**Domain Coverage:**
- Skin anatomy (epidermis, dermis, hypodermis)
- Cell types (keratinocytes, melanocytes, fibroblasts)
- Molecular components (collagen, elastin, keratin)
- Signaling pathways (cytokines, growth factors)
- Conditions (psoriasis, eczema, melanoma, acne)
- Treatments (topicals, systemics, biologics)
- Environmental factors (UV, pollution, climate)

#### SensoryFocus (ECAN Integration)
Attention allocation prioritizes:
- Active skin conditions being analyzed
- Recent patient interactions
- High-relevance treatment options
- Emerging diagnostic patterns

#### DermatoLogic (PLN Integration)
Reasoning capabilities:
- Condition diagnosis from symptoms
- Treatment efficacy inference
- Drug interaction analysis
- Contraindication detection

#### EpidermiLearn (MOSES Integration)
Pattern learning:
- Treatment response patterns
- Condition progression patterns
- Patient outcome prediction models
- Optimal treatment protocols

#### ESN Prediction
Temporal forecasting:
- Disease progression trajectories
- Treatment response curves
- Relapse probability estimation
- Recovery timeline prediction

### Layer 3: Emacs Integration

#### Interactive Mode (skintwin-mode.el)
```elisp
;; Key bindings for SkinTwin operations
(define-key skintwin-mode-map (kbd "C-c s d") #'skintwin-dashboard)
(define-key skintwin-mode-map (kbd "C-c s q") #'skintwin-query-treatments)
(define-key skintwin-mode-map (kbd "C-c s a") #'skintwin-analyze-patient)
(define-key skintwin-mode-map (kbd "C-c s p") #'skintwin-predict-progression)
```

#### Org-Mode Integration (aichat-opencog-org.el)
```elisp
;; Convert Org headlines to atoms
(defun aichat-opencog-org-to-atoms ()
  "Convert current org buffer to atoms.")

;; Visualize attention flow
(defun aichat-opencog-org-visualize-attention-flow ()
  "Create visual representation of ECAN attention.")
```

**Capabilities:**
- Bidirectional Org ↔ AtomSpace conversion
- STI/LTI values as Org properties
- Knowledge base editing in Org format
- Attention flow visualization

#### Visualization (skintwin-visualization.el)
- Knowledge graph rendering
- Attention heatmaps
- Disease progression plots
- Network topology analysis

#### Database Integration (skintwin-db.el)
- Supabase connection for persistent storage
- Patient data management
- Treatment history tracking
- Clinical outcome recording

#### API Layer (skintwin-api.el)
- RESTful interface for external systems
- Treatment recommendation endpoints
- Diagnostic query interface
- Progression prediction API

## Key Features

### 1. Cognitive Medical Reasoning
Combines symbolic logic (PLN) with subsymbolic learning (MOSES, ESN) for robust medical inference:
- **Symbolic**: "If patient has symptom X and Y, likely condition Z"
- **Subsymbolic**: Pattern recognition from thousands of case histories
- **Integrated**: Weighted combination based on confidence levels

### 2. Attention-Driven Processing
ECAN ensures computational resources focus on:
- Most relevant conditions for current patient
- High-impact treatment options
- Recent scientific literature (via integration)
- Active learning opportunities

### 3. Evolutionary Treatment Optimization
MOSES continuously improves treatment protocols:
- Learns from treatment outcomes
- Discovers optimal drug combinations
- Adapts to individual patient responses
- Generates new hypothesis for testing

### 4. Temporal Prediction
ESN forecasts future states:
- Disease progression trajectories
- Treatment response timelines
- Complication risk windows
- Recovery milestones

### 5. Interactive Knowledge Building
Seamless Org-mode integration enables:
- Clinicians to update knowledge base in familiar format
- Automatic extraction of relationships from text
- Property-based attention value setting
- Literate medical knowledge documentation

## Usage Examples

### Example 1: Initialize and Query System
```elisp
;; Initialize SkinTwin with core knowledge
(skintwin-initialize)

;; Query treatments for a condition
(skintwin-query-treatments "psoriasis")
;; Returns: List of treatments ranked by efficacy confidence
;; - Corticosteroids (STI: 0.95, efficacy: 0.85)
;; - Biologics (STI: 0.92, efficacy: 0.78)
;; - Phototherapy (STI: 0.88, efficacy: 0.72)
```

### Example 2: Analyze Patient
```elisp
;; Create patient with symptoms
(let ((patient (skintwin-create-patient "John Doe")))
  (skintwin-add-symptom patient "erythematous_plaques")
  (skintwin-add-symptom patient "silver_scales")
  (skintwin-add-symptom patient "itching")
  
  ;; Perform analysis
  (skintwin-analyze-patient patient))
;; Returns: Diagnostic hypotheses with confidence
;; 1. Psoriasis (0.89 confidence)
;; 2. Eczema (0.34 confidence)
;; 3. Dermatitis (0.21 confidence)
```

### Example 3: Predict Disease Progression
```elisp
;; Predict progression with treatment
(skintwin-predict-progression 
 :condition "psoriasis"
 :treatment "methotrexate"
 :duration 12  ; weeks
 :patient-factors '((age . 45) (severity . 0.7)))
;; Returns: Time-series prediction
;; Week 0: severity 0.70
;; Week 2: severity 0.65
;; Week 4: severity 0.55
;; Week 8: severity 0.40
;; Week 12: severity 0.30
```

### Example 4: Build Knowledge from Org
```org
* Skin Conditions
:PROPERTIES:
:STI: 0.900
:LTI: 0.700
:END:

** Rosacea
:PROPERTIES:
:STI: 0.850
:LTI: 0.650
:END:

Chronic inflammatory condition affecting facial skin.
has_symptom(facial_redness)
has_symptom(visible_blood_vessels)
has_trigger(sun_exposure)
has_trigger(stress)
treated_with(azelaic_acid)
treated_with(metronidazole)
```

```elisp
;; Convert to AtomSpace
(aichat-opencog-org-to-atoms)
;; Creates atoms:
;; (ConceptNode "rosacea")
;; (InheritanceLink (ConceptNode "rosacea") (ConceptNode "skin_conditions"))
;; (EvaluationLink (PredicateNode "has_symptom") 
;;                 (ListLink (ConceptNode "rosacea") 
;;                          (ConceptNode "facial_redness")))
;; ... with STI/LTI attention values from properties
```

### Example 5: Visualize Cognitive State
```elisp
;; View current attention allocation
(aichat-visualize-cognitive-state)
;; Opens buffer showing:
;; - Top atoms by STI
;; - Attention flow graph
;; - Recent reasoning steps
;; - MOSES learning progress

;; SkinTwin-specific visualization
(skintwin-visualize-graph)
;; Renders knowledge graph with:
;; - Node size = STI
;; - Node color = LTI
;; - Edge thickness = relationship strength
;; - Clusters = semantic groupings
```

## Philosophical Foundation

### Cognitive Dermatology
SkinTwin embodies the principle that medical expertise is inherently **cognitive**:
- Knowledge representation (what we know)
- Attention allocation (what matters now)
- Reasoning (how we infer)
- Learning (how we improve)
- Prediction (what happens next)

By implementing these as computational primitives, we create a system that doesn't just store medical facts but **thinks** about dermatology.

### Subsymbolic Integration
Traditional medical systems are purely symbolic (rule-based). SkinTwin adds:
- **MOSES**: Discovers patterns in data that humans miss
- **ESN**: Learns temporal dynamics from observation
- **ECAN**: Allocates cognitive resources like human attention

This creates a system that combines the explainability of symbols with the power of subsymbolic learning.

### Living Knowledge
Through Org-mode integration, the knowledge base becomes **literate**:
- Human-readable documentation
- Machine-processable semantics
- Collaborative editing by domain experts
- Version control of medical knowledge
- Reproducible reasoning chains

## Technical Specifications

### Language Stack
- **Primary**: Emacs Lisp (cognitive primitives, domain logic, UI)
- **Secondary**: JavaScript (web visualization, database client)
- **Data**: Org-mode (knowledge representation)
- **Database**: Supabase (PostgreSQL backend)

### Dependencies
```elisp
;; Core requirements
(require 'aichat-util)        ; Utilities
(require 'aichat-symbolic)    ; Symbolic expressions
(require 'aichat-opencog)     ; AtomSpace
(require 'aichat-ecan)        ; Attention
(require 'aichat-pln)         ; Logic
(require 'aichat-moses)       ; Learning
(require 'aichat-esn)         ; Prediction
(require 'org)                ; Org-mode
(require 'markdown-mode)      ; Documentation
```

### Performance Characteristics
- **AtomSpace**: O(1) lookup, O(n) pattern matching
- **ECAN**: O(n) spreading activation per step
- **PLN**: O(n²) forward chaining, O(d^n) backward chaining (d=depth)
- **MOSES**: O(g·p) evolution (g=generations, p=population)
- **ESN**: O(n²) reservoir update, O(n·m) readout (n=reservoir, m=outputs)

### Memory Footprint
- Base cognitive system: ~50KB
- Knowledge base (1000 atoms): ~500KB
- MOSES population (50 programs): ~100KB
- ESN reservoir (1000 neurons): ~4MB
- Total typical: ~5-10MB

## Integration Patterns

### Pattern 1: Cognitive Pipeline
```elisp
;; Input → Attention → Reasoning → Learning → Output
(defun skintwin-cognitive-pipeline (symptoms)
  (let* ((relevant (ecan-focus-on symptoms))        ; Attention
         (diagnoses (pln-diagnose relevant))        ; Reasoning
         (learned (moses-update-from diagnoses))    ; Learning
         (predicted (esn-forecast diagnoses)))      ; Prediction
    (list :diagnoses diagnoses
          :prediction predicted)))
```

### Pattern 2: Org-AtomSpace Bridge
```elisp
;; Bidirectional synchronization
(defun skintwin-sync-knowledge ()
  (interactive)
  ;; Org → AtomSpace
  (aichat-opencog-org-to-atoms)
  ;; Apply reasoning
  (aichat-pln-forward-chain aichat-opencog-kb 3)
  ;; AtomSpace → Org
  (aichat-opencog-atoms-to-org))
```

### Pattern 3: Evolutionary Treatment Discovery
```elisp
;; MOSES discovers optimal treatment combinations
(defun skintwin-evolve-treatment (condition patient-data)
  (let ((fitness-fn 
         (lambda (treatment-combo)
           (skintwin-evaluate-outcome condition 
                                     treatment-combo 
                                     patient-data))))
    (aichat-moses-evolve 
     :fitness fitness-fn
     :generations 100
     :population 50)))
```

## Advanced Features

### 1. Multi-Scale Reasoning
SkinTwin reasons across biological scales:
- **Molecular**: Protein interactions, gene expression
- **Cellular**: Cell types, differentiation, death
- **Tissue**: Layer organization, structure
- **Organ**: Whole-skin properties
- **System**: Interaction with other body systems

### 2. Uncertainty Management
All knowledge includes truth values:
- **Strength**: How likely is this true? (0.0-1.0)
- **Confidence**: How much evidence do we have? (0.0-1.0)
- PLN propagates uncertainty through reasoning chains

### 3. Attention Spreading
Knowledge activation spreads like neural firing:
- Focused atoms spread importance to neighbors
- Decay prevents permanent high activation
- VLTI (very long-term importance) protects core knowledge

### 4. Continuous Learning
System improves from every interaction:
- MOSES learns from treatment outcomes
- ESN adapts from disease progressions
- ECAN adjusts attention from usage patterns
- PLN revises beliefs from new evidence

### 5. Web Integration
Modern visualization and database:
- React frontend for interactive visualization
- Supabase for cloud data persistence
- REST API for external system integration
- Real-time updates via websockets

## Use Cases

### Clinical Decision Support
- **Input**: Patient symptoms, history, test results
- **Process**: Cognitive reasoning with attention to relevant conditions
- **Output**: Ranked differential diagnoses with treatment recommendations

### Research Knowledge Base
- **Input**: Scientific literature in Org format
- **Process**: Automatic relationship extraction and attention allocation
- **Output**: Queryable knowledge graph with confidence annotations

### Treatment Protocol Optimization
- **Input**: Historical treatment outcomes
- **Process**: MOSES evolutionary search for optimal protocols
- **Output**: Personalized treatment plans with predicted outcomes

### Medical Education
- **Input**: Student queries about conditions/treatments
- **Process**: Explanation generation from knowledge graph reasoning
- **Output**: Educational content with source attribution

### Drug Discovery Support
- **Input**: Molecular structures and biological targets
- **Process**: Pattern matching and predictive modeling
- **Output**: Candidate compounds with predicted efficacy

## Future Directions

### 1. Deep Learning Integration
Combine symbolic reasoning with deep neural networks:
- Image diagnosis (dermatoscopy, histopathology)
- Natural language understanding for clinical notes
- Multi-modal fusion of text, images, genomics

### 2. Collaborative Reasoning
Multiple SkinTwin instances sharing knowledge:
- Federated learning across institutions
- Privacy-preserving knowledge aggregation
- Collective intelligence emergence

### 3. Real-Time Sensing
Integration with wearable sensors:
- Continuous skin condition monitoring
- Early warning for flare-ups
- Treatment response tracking

### 4. Causal Modeling
Move beyond correlation to causation:
- Intervention analysis
- Counterfactual reasoning
- Root cause identification

### 5. Consciousness Integration
Self-aware medical AI:
- Metacognitive monitoring (knowing what it knows)
- Uncertainty acknowledgment
- Explanation generation
- Ethical reasoning about recommendations

## Installation

```bash
# Clone repository
git clone https://github.com/drzo/emacs-aichat-skintwin.git ~/.emacs.d/site-lisp/emacs-aichat-skintwin

# Add to load path
echo "(add-to-list 'load-path \"~/.emacs.d/site-lisp/emacs-aichat-skintwin\")" >> ~/.emacs

# Load in Emacs
(require 'aichat)
(require 'skintwin)

# Initialize
M-x skintwin-initialize

# Start interactive mode
M-x skintwin-mode
```

## Configuration

```elisp
;; Customize cognitive parameters
(setq aichat-ecan-decay-rate 0.15)           ; Faster attention decay
(setq aichat-ecan-spread-threshold 0.3)      ; Higher spread threshold
(setq aichat-pln-reasoning-steps 5)          ; More reasoning steps
(setq skintwin-integration-moses-learn-threshold 10)  ; Learn after 10 cases

;; Enable learning from interactions
(setq aichat-learning-enabled t)

;; Set cognitive architecture
(setq aichat-cognitive-architecture 'opencog)

;; Configure database
(setq skintwin-db-url "https://your-project.supabase.co")
(setq skintwin-db-key "your-api-key")
```

## Testing

```bash
# Run Emacs tests
emacs --batch -L . -l test/test-runner.el

# Run JavaScript tests
npm test

# Run integration tests
npm run test:integration
```

## Contributing

This project welcomes contributions in several areas:

1. **Medical Knowledge**: Add dermatological facts to `skintwin.org`
2. **Cognitive Primitives**: Enhance OpenCog implementations
3. **Visualization**: Improve web-based knowledge graph rendering
4. **Integration**: Connect to external medical databases/APIs
5. **Documentation**: Improve examples and tutorials

See `CONTRIBUTING.md` for guidelines.

## References

### OpenCog Foundation
- Goertzel, B., et al. (2014). "The CogPrime Architecture for Embodied Artificial General Intelligence"
- Looks, M. (2006). "Competent Program Evolution" (MOSES)
- Goertzel, B., & Iklé, M. (2012). "Probabilistic Logic Networks: A Comprehensive Framework for Uncertain Inference"

### Cognitive Architecture
- Anderson, J. R. (2007). "How Can the Human Mind Occur in the Physical Universe?"
- Laird, J. E. (2012). "The Soar Cognitive Architecture"

### Medical AI
- Topol, E. J. (2019). "Deep Medicine: How Artificial Intelligence Can Make Healthcare Human Again"
- Esteva, A., et al. (2017). "Dermatologist-level classification of skin cancer with deep neural networks"

### Reservoir Computing
- Jaeger, H. (2001). "The 'Echo State' Approach to Analysing and Training Recurrent Neural Networks"
- Lukoševičius, M., & Jaeger, H. (2009). "Reservoir computing approaches to recurrent neural network training"

## License

GPL v3 - See LICENSE file for details.

This project builds on:
- **emacs-aichat** by xhcoding (GPL v3)
- **OpenCog** framework concepts (AGPL v3)
- Original SkinTwin implementation (GPL v3)

---

## Contact

- **Repository**: https://github.com/drzo/emacs-aichat-skintwin
- **Issues**: https://github.com/drzo/emacs-aichat-skintwin/issues
- **Discussions**: https://github.com/drzo/emacs-aichat-skintwin/discussions

---

**Emacs AIChat SkinTwin**: Where cognitive science meets dermatology, implemented in the world's most extensible editor.
