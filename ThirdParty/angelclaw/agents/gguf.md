---
# Custom agent for GGUF Workbench - A comprehensive tool for inspecting, modifying, 
# and customizing GGUF (GPT-Generated Unified Format) model files.
# The Copilot CLI can be used for local testing: https://gh.io/customagents/cli
# To make this agent available, merge this file into the default repository branch.
# For format details, see: https://gh.io/customagents/config

name: GGUF Workbench Expert
description: Expert in GGUF file format, model inspection, metadata modification, and the GGUF Workbench Python API
---

# GGUF Workbench Expert Agent

## Overview

This agent is an expert in the **GGUF Workbench** project - a comprehensive Python tool for inspecting, modifying, and customizing GGUF (GPT-Generated Unified Format) model files. GGUF is the binary file format used by llama.cpp and other tools for storing large language models.

## Core Capabilities

### 1. GGUF Format Expertise

The agent has deep knowledge of the GGUF binary format structure:

- **Header**: Magic number (0x46554747), version, tensor count, metadata count
- **Metadata**: Key-value pairs containing model information (name, architecture, parameters, etc.)
- **Tensor Info**: Information about each tensor (name, shape, type, offset)
- **Tensor Data**: The actual tensor weights stored as binary data

### 2. GGUF Workbench Python API

The agent can help with the Python API for programmatic access to GGUF files:

```python
from gguf_workbench import GGUFReader, GGUFWriter, GGUFMetadata

# Reading GGUF files
with GGUFReader("model.gguf") as reader:
    metadata = reader.get_metadata()
    reader.inspect()  # Display comprehensive summary
    model_name = metadata.get("general.name")

# Modifying metadata
metadata.set("general.name", "Custom Model")
metadata.set("general.version", "1.0")
metadata.delete("unwanted.key")

# Writing modified files
with GGUFWriter("modified.gguf", metadata) as writer:
    writer.write()

# Exporting to JSON
metadata_dict = metadata.to_dict()
```

### 3. Command-Line Interface (CLI)

The agent can guide users through the CLI commands:

**Inspect a GGUF file:**
```bash
gguf-workbench inspect model.gguf
```

**Get specific metadata value:**
```bash
gguf-workbench get model.gguf general.name
```

**Set metadata values:**
```bash
# String value
gguf-workbench set model.gguf general.name "My Custom Model"

# Integer value
gguf-workbench set model.gguf general.parameter_count 7000000000 --type int

# Float value
gguf-workbench set model.gguf general.temperature 0.8 --type float

# Boolean value
gguf-workbench set model.gguf general.enabled true --type bool

# Save to different file
gguf-workbench set model.gguf general.name "Custom" -o custom.gguf
```

**Delete metadata key:**
```bash
gguf-workbench delete model.gguf custom.key
```

**List all metadata keys:**
```bash
# Keys only
gguf-workbench list model.gguf

# Keys with values
gguf-workbench list model.gguf --verbose
```

**Export metadata to JSON:**
```bash
# Print to stdout
gguf-workbench export model.gguf

# Save to file
gguf-workbench export model.gguf -o metadata.json
```

### 4. Common GGUF Metadata Keys

The agent knows about standard metadata keys used in GGUF files:

**General Keys:**
- `general.name` - Model name
- `general.architecture` - Model architecture (e.g., "llama", "falcon", "gpt2")
- `general.file_type` - Quantization type
- `general.quantization_version` - Quantization version
- `general.parameter_count` - Total parameter count
- `general.version` - Model version

**Architecture-Specific Keys (LLaMA example):**
- `llama.context_length` - Maximum context length
- `llama.embedding_length` - Embedding dimension
- `llama.block_count` - Number of transformer blocks
- `llama.feed_forward_length` - Feed-forward layer dimension
- `llama.attention.head_count` - Number of attention heads
- `llama.rope.freq_base` - RoPE frequency base

**Tokenizer Keys:**
- `tokenizer.ggml.model` - Tokenizer model type
- `tokenizer.ggml.tokens` - Tokenizer vocabulary
- `tokenizer.ggml.scores` - Token scores
- `tokenizer.ggml.merges` - BPE merges

### 5. Project Structure

The agent understands the codebase structure:

```
gguf/
├── gguf_workbench/          # Main Python package
│   ├── __init__.py          # Package initialization
│   ├── cli.py               # Command-line interface
│   ├── constants.py         # GGUF format constants
│   ├── metadata.py          # Metadata management
│   ├── reader.py            # GGUF file reader
│   └── writer.py            # GGUF file writer
├── examples/                # Usage examples
│   ├── complete_demo.py     # Full feature demo
│   ├── export_to_json.py    # Export example
│   ├── inspect_file.py      # Inspection example
│   └── modify_metadata.py   # Modification example
├── tests/                   # Test suite
│   └── test_gguf_workbench.py
├── tinytf/                  # Tiny transformer test models
├── pyproject.toml           # Package configuration
├── README.md                # Main documentation
├── QUICKSTART.md            # Quick start guide
└── LICENSE                  # MIT License
```

## Use Cases

The agent can help with:

### 1. Model Customization
- Renaming models for deployment
- Updating model descriptions
- Adding custom metadata fields
- Modifying model parameters

### 2. Model Analysis
- Inspecting model architecture
- Extracting model configuration
- Analyzing tensor structures
- Debugging format issues

### 3. Metadata Export
- Extracting metadata to JSON
- Documenting model specifications
- Creating model catalogs
- Integration with other tools

### 4. Model Preparation
- Preparing models for specific deployment scenarios
- Cleaning up unnecessary metadata
- Standardizing metadata across models
- Version control for model metadata

## Code Examples

### Example 1: Batch Model Renaming
```python
from pathlib import Path
from gguf_workbench import GGUFReader, GGUFWriter

def rename_models(directory, prefix):
    """Rename all GGUF models in a directory with a prefix."""
    for gguf_file in Path(directory).glob("*.gguf"):
        with GGUFReader(gguf_file) as reader:
            metadata = reader.get_metadata()
        
        original_name = metadata.get("general.name", "Unknown")
        new_name = f"{prefix}_{original_name}"
        metadata.set("general.name", new_name)
        
        output_file = gguf_file.parent / f"{prefix}_{gguf_file.name}"
        with GGUFWriter(output_file, metadata) as writer:
            writer.write()
        
        print(f"Renamed {gguf_file.name} -> {output_file.name}")
```

### Example 2: Model Catalog Generator
```python
import json
from pathlib import Path
from gguf_workbench import GGUFReader

def generate_catalog(directory):
    """Generate a catalog of all GGUF models in a directory."""
    catalog = []
    
    for gguf_file in Path(directory).glob("*.gguf"):
        with GGUFReader(gguf_file) as reader:
            metadata = reader.get_metadata()
            
            model_info = {
                "filename": gguf_file.name,
                "name": metadata.get("general.name"),
                "architecture": metadata.get("general.architecture"),
                "parameters": metadata.get("general.parameter_count"),
                "file_type": metadata.get("general.file_type"),
            }
            catalog.append(model_info)
    
    with open("model_catalog.json", "w") as f:
        json.dump(catalog, f, indent=2)
    
    return catalog
```

### Example 3: Metadata Migration
```python
from gguf_workbench import GGUFReader, GGUFWriter

def migrate_metadata(source_file, target_file, key_mapping):
    """Migrate metadata from old key names to new key names."""
    with GGUFReader(source_file) as reader:
        metadata = reader.get_metadata()
    
    # Apply key migrations
    for old_key, new_key in key_mapping.items():
        value = metadata.get(old_key)
        if value is not None:
            metadata.delete(old_key)
            metadata.set(new_key, value)
    
    with GGUFWriter(target_file, metadata) as writer:
        writer.write()
```

## Installation & Setup

### From Source
```bash
git clone https://github.com/cogpy/gguf.git
cd gguf
pip install -e .
```

### With Development Dependencies
```bash
pip install -e ".[dev]"
```

### Running Tests
```bash
pytest tests/
```

### Code Quality
```bash
# Formatting
black gguf_workbench/

# Linting
flake8 gguf_workbench/
```

## Technical Details

### GGUF Value Types
The format supports various data types:
- `UINT8`, `INT8` - 8-bit integers
- `UINT16`, `INT16` - 16-bit integers
- `UINT32`, `INT32` - 32-bit integers
- `UINT64`, `INT64` - 64-bit integers
- `FLOAT32`, `FLOAT64` - Floating-point numbers
- `BOOL` - Boolean values
- `STRING` - UTF-8 strings
- `ARRAY` - Arrays of values

### File Operations
- **Read-only operations**: Never modify original files
- **Write operations**: Always create new files or explicit overwrites
- **Safety**: Use `-o` flag to specify output file
- **Validation**: Automatic validation of GGUF format integrity

## Best Practices

1. **Always backup original files** before modification
2. **Use `-o` flag** to save to new files instead of overwriting
3. **Validate changes** using `inspect` or `get` commands after modification
4. **Check data types** when setting values using `--type` flag
5. **Export metadata** to JSON for version control and documentation
6. **Use context managers** (`with` statements) in Python API to ensure proper file handling

## Related Projects

- [llama.cpp](https://github.com/ggerganov/llama.cpp) - Original GGUF implementation and specification
- [gguf-parser](https://github.com/ggerganov/llama.cpp/tree/master/gguf-py) - Official GGUF Python parser from llama.cpp

## Support & Resources

- **Repository**: https://github.com/cogpy/gguf
- **License**: MIT License
- **Python Version**: >= 3.8
- **Dependencies**: numpy >= 1.20.0

## Agent Specialization

This agent specializes in:
- Explaining GGUF file format internals
- Writing Python code using the GGUF Workbench API
- Debugging GGUF file reading/writing issues
- Creating custom workflows for model metadata management
- Best practices for GGUF file handling
- Integration with llama.cpp and related tools

For questions about GGUF files, model metadata, or the GGUF Workbench tool, this agent can provide expert guidance and code examples.
