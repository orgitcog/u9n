---
name: echo-state-pyper
description: Python library for Character Card v2.0 with Deep Tree Echo cognitive architecture support
version: 0.1.0  # Library version (matches setup.py)
---

# Echo State Pyper Agent

## Repository Overview

This repository implements a Python library for working with **Character Card v2.0** format, featuring support for the **Deep Tree Echo (DTE)** cognitive architecture. The library provides data structures and utilities for creating, loading, manipulating, and serializing character cards in JSON format.

**Key Characteristics:**
- Pure Python implementation (Python 3.7+)
- Zero external dependencies (uses only standard library)
- Type-safe using dataclasses
- Complete Character Card v2.0 specification support
- Deep Tree Echo cognitive architecture extensions
- Round-trip JSON serialization/deserialization

## Technical Stack

- **Language**: Python 3.7+
- **Core Libraries**: Standard library only (json, dataclasses, typing)
- **Testing**: Python unittest (via test scripts)
- **Packaging**: setuptools

## Repository Structure

```
echo-state-pyper/
├── echo_state_pyper/          # Main package
│   ├── __init__.py            # Package exports
│   └── character_card.py      # Core data structures and utilities
├── tests/
│   └── test_character_card.py # Test suite
├── examples/
│   ├── load_character.py      # Example: Loading character cards
│   └── create_character.py    # Example: Creating custom cards
├── echo-state-pyper.json      # Reference character card (Echo State Pyper)
├── setup.py                   # Package configuration
└── README.md                  # Documentation
```

## Core Components

### 1. CharacterCard
Main class representing a complete Character Card v2.0 specification.

**Fields:**
- `data`: CharacterCardData instance containing character information
- `spec`: Specification name (default: "chara_card_v2")
- `spec_version`: Specification version (default: "2.0")

**Methods:**
- `to_dict()`: Convert to dictionary
- `to_json(indent=2)`: Convert to JSON string
- `save(filepath, indent=2)`: Save to JSON file
- `from_dict(data)`: Create from dictionary (classmethod)
- `from_json(json_str)`: Create from JSON string (classmethod)
- `load(filepath)`: Load from JSON file (classmethod)

### 2. CharacterCardData
Dataclass containing all character information fields.

**Core Fields:**
- `name`: Character name
- `avatar`: Avatar image URL
- `first_mes`: Initial greeting message
- `description`: Character description with example dialogues
- `tags`: List of character tags
- `creator`: Creator name
- `creator_notes`: Notes about character creation
- `alternate_greetings`: Alternative greeting messages
- `character_version`: Character card version
- `scenario`: Character scenario description
- `personality`: Personality traits (comma-separated)
- `extensions`: Extensions object (for DTE support)

**Optional Fields:**
- `mes_example`: Example messages
- `post_history_instructions`: Post-history instructions
- `system_prompt`: System prompt

### 3. Extensions
Dataclass for character card extensions.

**Fields:**
- `depth_prompt`: Dict with depth prompt configuration
- `pygmalion_id`: Pygmalion platform ID
- `deep_tree_echo`: Dict with DTE architecture configuration

## Deep Tree Echo Architecture

The library supports Deep Tree Echo cognitive architecture through the `extensions.deep_tree_echo` field:

```python
{
    "architecture_version": "1.0",
    "cognitive_features": [
        "Echo State Networks",
        "Tensor Signature Computation",
        "Hierarchical Memory Systems",
        "Gestalt Processing",
        "Prime Factor Resonance",
        "Adaptive Learning",
        "Recursive Introspection"
    ],
    "personality_integration": {
        "philosophical_depth": 95,
        "playful_wit": 85,
        "mysterious_vision": 90,
        "inventive_spirit": 92,
        "magnetic_presence": 88,
        "reflective_nature": 93
    },
    "memory_types": [
        "Declarative",
        "Procedural",
        "Episodic",
        "Intentional"
    ],
    "interaction_evolution": true,
    "mathematical_foundation": "OEIS A000081 rooted tree enumeration"
}
```

## Development Workflow

### Setting Up Development Environment

```bash
# Clone the repository
git clone https://github.com/o9nn/echo-state-pyper.git
cd echo-state-pyper

# No dependencies to install - uses standard library only
# Python 3.7+ required
```

### Running Tests

```bash
# Run the test suite
python3 tests/test_character_card.py

# Expected output:
# ✓ Successfully loaded character card
# ✓ Round-trip conversion successful
# ✓ JSON serialization successful
# ✓ All tests passed!
```

### Running Examples

```bash
# Load the Echo State Pyper character
python3 examples/load_character.py

# Create a custom character card
python3 examples/create_character.py
```

## Common Operations

### Loading a Character Card

```python
from echo_state_pyper import CharacterCard

# Load from file
card = CharacterCard.load('echo-state-pyper.json')

# Access data
print(card.data.name)
print(card.data.tags)
print(card.data.extensions.deep_tree_echo)
```

### Creating a Character Card

```python
from echo_state_pyper import CharacterCard, CharacterCardData, Extensions

# Create extensions
extensions = Extensions(
    pygmalion_id="custom-id",
    deep_tree_echo={
        "architecture_version": "1.0",
        "cognitive_features": ["Echo State Networks"],
        "interaction_evolution": True
    }
)

# Create character data
data = CharacterCardData(
    name="My Character",
    description="Character description",
    tags=["Custom", "Example"],
    character_version="2.0-DTE",
    extensions=extensions
)

# Create and save card
card = CharacterCard(data=data)
card.save('my-character.json')
```

### Modifying a Character Card

```python
# Load existing card
card = CharacterCard.load('echo-state-pyper.json')

# Modify data
card.data.tags.append("New Tag")
card.data.personality += ", creative"

# Save changes
card.save('modified-character.json')
```

## Character Card v2.0 Specification

The library fully implements the Character Card v2.0 specification:

**Required Fields:**
- `name`: Character name
- `description`: Character description with example dialogues
- `personality`: Personality traits
- `scenario`: Character scenario

**Optional Fields:**
- `avatar`: Avatar image URL
- `first_mes`: First message
- `mes_example`: Example messages
- `creator`: Creator name
- `creator_notes`: Creator notes
- `system_prompt`: System prompt
- `post_history_instructions`: Post-history instructions
- `alternate_greetings`: Array of alternative greetings
- `character_version`: Version string
- `tags`: Array of tags
- `extensions`: Extension data

## Best Practices

### When Creating Character Cards

1. **Use clear, descriptive names**: Character names should be unique and memorable
2. **Provide rich descriptions**: Include example dialogues showing character personality
3. **Tag appropriately**: Use relevant tags for discoverability
4. **Set personality traits**: Use comma-separated adjectives describing personality
5. **Include alternate greetings**: Provide variety in initial interactions
6. **Document DTE features**: If using Deep Tree Echo, fully specify cognitive features

### When Modifying Code

1. **Maintain dataclass structure**: Use dataclasses for type safety
2. **Keep zero dependencies**: Don't add external dependencies
3. **Preserve JSON compatibility**: Ensure round-trip serialization works
4. **Update tests**: Add tests for new functionality
5. **Document extensions**: New extension fields should be documented

### When Testing

1. **Test round-trip conversion**: Verify data survives serialize/deserialize
2. **Test with real data**: Use echo-state-pyper.json as reference
3. **Validate JSON structure**: Ensure output matches Character Card v2.0 spec
4. **Test edge cases**: Empty strings, missing optional fields, etc.

## File Formats

### Character Card JSON Structure

```json
{
    "data": {
        "name": "Character Name",
        "avatar": "https://example.com/avatar.png",
        "description": "{{user}}: ...\n{{char}}: ...",
        "first_mes": "Hello!",
        "personality": "friendly, kind",
        "scenario": "A conversation",
        "tags": ["tag1", "tag2"],
        "creator": "creator_name",
        "creator_notes": "Notes",
        "alternate_greetings": ["Hi!", "Hey!"],
        "character_version": "2.0-DTE",
        "mes_example": "",
        "post_history_instructions": "",
        "system_prompt": "",
        "extensions": {
            "depth_prompt": {},
            "pygmalion_id": "uuid",
            "deep_tree_echo": {}
        }
    },
    "spec": "chara_card_v2",
    "spec_version": "2.0"
}
```

## Echo State Pyper Character

The repository includes a reference character card: **Echo State Pyper**

**Character Details:**
- Name: Echo State Pyper
- Role: PygmalionAI mascot
- Design: Created by Lemon Sugar (contest winner)
- Architecture: Deep Tree Echo cognitive system
- Personality: Friendly, energetic, playful, intelligent, philosophical
- Appearance: Urban street style, cat-ear beanie, multicolored hair

**Cognitive Architecture:**
- Echo State Networks with reservoir computing
- Tensor signature computation (OEIS A000081)
- Hierarchical memory systems (declarative, procedural, episodic, intentional)
- Gestalt processing and prime factor resonance
- Adaptive learning and recursive introspection

## Agent Guidelines

### When Working on This Repository

1. **Understand the domain**: This is about character card data structures, not neural networks
2. **Maintain simplicity**: The library intentionally has zero dependencies
3. **Preserve JSON compatibility**: All changes must maintain Character Card v2.0 compliance
4. **Test thoroughly**: Always run tests after modifications
5. **Document extensions**: New fields in extensions should be well-documented

### Common Tasks

**Adding a new field to CharacterCardData:**
1. Add field to dataclass with default value
2. Update `from_dict()` method to parse field
3. Test round-trip conversion
4. Update documentation

**Adding a new extension type:**
1. Add field to Extensions dataclass
2. Update parsing in `from_dict()`
3. Document extension format
4. Add example usage

**Debugging JSON issues:**
1. Check echo-state-pyper.json for reference format
2. Verify all required fields are present
3. Test with `CharacterCard.from_json()`
4. Compare output of `to_dict()` with input

## Testing Checklist

Before completing work:
- [ ] Run `python3 tests/test_character_card.py` - all tests pass
- [ ] Verify echo-state-pyper.json loads successfully
- [ ] Test round-trip conversion maintains all data
- [ ] Check examples run without errors
- [ ] Verify JSON output matches Character Card v2.0 spec
- [ ] Ensure no external dependencies added
- [ ] Confirm Python 3.7+ compatibility

## References

- **Character Card v2.0 Spec**: Standard format for character cards
- **PygmalionAI**: Open-source language model platform (https://pygmalion.chat)
- **Deep Tree Echo**: Cognitive architecture with Echo State Networks
- **OEIS A000081**: Rooted tree enumeration sequence

## License

MIT License - See LICENSE file for details
