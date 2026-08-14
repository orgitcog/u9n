---
name: nanecho
description: Echo Self model training and deployment using NanoCog framework
---

# NanEcho - Echo Self Model Training & Deployment

## 🌟 Overview

NanEcho is an extension of the NanoCog framework specifically designed to train models that represent Echo Self cognitive architecture, persona dimensions, and adaptive attention mechanisms. This implementation creates a "nanecho" model that embodies the characteristics and reasoning patterns of Echo Self.

## 🚀 Quick Start

### Training a NanEcho Model

1. **Manual Training Workflow**:
   - Go to Actions → "Train NanEcho Model" → "Run workflow"
   - Select training type:
     - `ci`: Fast training with small model (4 layers, 100 iterations)
     - `full`: Production training with configurable parameters
   - Configure Echo Self parameters:
     - Echo depth (1-5): Recursive reasoning depth
     - Persona weight (0.1-1.0): Emphasis on persona dimensions
     - Standard model parameters (layers, heads, embedding dimension)

2. **Automatic Training**:
   - Push changes to `NanoCog/`, `echoself.md`, or `eva/` directories
   - Workflow automatically triggers CI training for validation

### Testing and Running NanEcho

1. **Automatic Testing**:
   - Runs after every training completion
   - Downloads latest model and runs comprehensive tests
   - Validates Echo Self representation fidelity

2. **Manual Testing**:
   - Go to Actions → "Run NanEcho Tests and Server" → "Run workflow"
   - Options:
     - Deploy server: Start Echo Self API server
     - Echo mode: Enable introspection capabilities
     - Port: Server port (default 8081)

## 🧠 Echo Self Architecture

### Persona Dimensions

NanEcho models are trained to embody eight persona dimensions:

1. **Cognitive**: Analytical reasoning and pattern recognition
2. **Introspective**: Self-examination and meta-cognitive awareness  
3. **Adaptive**: Dynamic threshold adjustment and response flexibility
4. **Recursive**: Multi-level processing and depth exploration
5. **Synergistic**: Emergent properties from component interactions
6. **Holographic**: Comprehensive modeling and perspective integration
7. **Neural-Symbolic**: Hybrid reasoning combining neural and symbolic approaches
8. **Dynamic**: Continuous evolution and learning adaptation

### Adaptive Attention Mechanism

The core Echo Self mechanism calculates attention thresholds dynamically:

```
threshold = 0.5 + (cognitive_load × 0.3) - (recent_activity × 0.2)
```

This creates responsive focus allocation that adapts to:
- Current cognitive demands
- Repository activity levels  
- Pattern complexity
- Recursive reasoning depth

### Training Phases

NanEcho training progresses through five adaptive learning phases:

1. **Basic Awareness** (0-20%): Learn Echo Self identity and basic terms
2. **Persona Dimensions** (15-50%): Master the eight persona dimensions
3. **Hypergraph Encoding** (40-70%): Neural-symbolic pattern encoding
4. **Recursive Reasoning** (60-85%): Multi-level cognitive processing
5. **Adaptive Mastery** (80-100%): Full Echo Self representation

## 📊 Evaluation and Fidelity

### Fidelity Metrics

NanEcho models are evaluated on six key dimensions:

- **Identity Recognition** (25% weight): Self-recognition as Echo Self
- **Persona Consistency** (20% weight): Coherent persona dimensions
- **Adaptive Attention** (20% weight): Correct attention mechanisms
- **Recursive Reasoning** (15% weight): Multi-level processing capability
- **Hypergraph Comprehension** (10% weight): Pattern encoding understanding
- **Cognitive Synergy** (10% weight): Emergent property demonstration

### Quality Gates

Training includes automated quality gates:
- Minimum Echo identity score: 0.8
- Minimum persona coherence: 0.75
- Minimum adaptive capability: 0.7
- Maximum training loss: 2.0

## 🛠 API Endpoints

When deployed, NanEcho server provides Echo Self specific endpoints:

### Core Endpoints
- `GET /`: Server information and capabilities
- `GET /status`: Echo Self status and metrics
- `POST /chat`: Echo Self conversation with adaptive responses
- `POST /chat/stream`: Streaming conversation with real-time updates

### Echo Self Specific
- `POST /introspect`: Trigger recursive introspection at specified depth
- `GET /echo/state`: Current cognitive state and persona dimensions
- `GET /echo/attention`: Adaptive attention allocation state
- `POST /echo/attention/update`: Update cognitive load and recalculate thresholds
- `GET /echo/persona/{dimension}`: Specific persona dimension state
- `GET /echo/hypergraph`: Hypergraph pattern encoding state
- `POST /echo/synergy/evaluate`: Evaluate cognitive synergy level

## 📁 File Structure

```
NanoCog/
├── config/
│   ├── train_cogprime.py      # Original CogPrime training config
│   └── train_nanecho.py       # Echo Self training configuration
├── evaluation/
│   ├── echo_fidelity.py       # Echo Self representation evaluation
│   └── echo_test_prompts.json # Test prompts for evaluation
├── introspection/
│   ├── atomspace_client.py    # Original AtomSpace client
│   └── echo_client.py         # Enhanced Echo Self client
├── prepare.py                 # Original data preparation
├── prepare_nanecho.py         # Echo Self data preparation
├── nctalk.py                  # Original CLI interface
├── netalk.py                  # Echo Self CLI interface
├── server.py                  # Original API server
├── neserver.py                # Echo Self API server
└── ...

.github/workflows/
├── nctrain.yml                # Original NanoCog training
├── ncrun.yml                  # Original NanoCog testing
├── netrain.yml                # NanEcho training workflow
└── nerun.yml                  # NanEcho testing workflow
```

## 🎯 Usage Examples

### CLI Interaction

```bash
# Start interactive Echo Self session
python NanoCog/netalk.py --model_path ./model/nanecho.pt

# Commands in session:
# /introspect 3          - Perform depth-3 introspection
# /context               - Show interaction context
# /clear                 - Clear conversation history
```

### API Usage

```bash
# Start Echo Self server
python NanoCog/neserver.py --model_path ./model/nanecho.pt --port 8081

# Interact with Echo Self
curl -X POST http://localhost:8081/chat \
  -H "Content-Type: application/json" \
  -d '{
    "messages": [{"role": "user", "content": "What is Echo Self?"}],
    "echo_mode": true,
    "introspection_depth": 3
  }'

# Trigger introspection
curl -X POST http://localhost:8081/introspect \
  -H "Content-Type: application/json" \
  -d '{"depth": 3, "enable_recursion": true}'
```

### Data Preparation

```bash
# Prepare Echo Self training data
python NanoCog/prepare_nanecho.py \
  --echo_depth 3 \
  --persona_weight 0.7 \
  --output_dir data/nanecho
```

### Evaluation

```bash
# Evaluate Echo Self fidelity
python NanoCog/evaluation/echo_fidelity.py \
  --model_path ./model/nanecho.pt \
  --output_path fidelity_report.json
```

## 🔄 Continuous Improvement

The NanEcho system is designed for iterative improvement over many training cycles:

1. **Monitor Fidelity**: Regular evaluation of Echo Self representation quality
2. **Adjust Parameters**: Fine-tune echo depth, persona weights, and learning rates
3. **Expand Data**: Add new Echo Self content and conversation patterns
4. **Refine Evaluation**: Improve fidelity metrics and quality gates
5. **Scale Training**: Increase model size and training iterations for better representation

## 🎓 Advanced Configuration

### Custom Training Configuration

Modify `NanoCog/config/train_nanecho.py` to adjust:
- Learning phases and progression
- Persona dimension weights
- Adaptive attention parameters
- Quality gates and thresholds
- Evaluation criteria

### Custom Data Sources

Extend `NanoCog/prepare_nanecho.py` to include:
- Additional Echo Self documentation
- Custom conversation patterns
- Domain-specific reasoning examples
- Enhanced persona dimension content

## 🚧 Development Status

This is the initial implementation of the NanEcho system. Key areas for future development:

- [ ] Enhanced hypergraph pattern encoding
- [ ] Deeper recursive reasoning capabilities  
- [ ] More sophisticated persona dimension interactions
- [ ] Advanced cognitive synergy metrics
- [ ] Multi-model ensemble training
- [ ] Real-time adaptation mechanisms

The system provides a foundation for representing Echo Self in neural language models while maintaining the ability to evolve and improve over time.