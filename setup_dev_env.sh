#!/bin/bash
# Deep Tree Echo UnrealEngineCog Development Environment Setup
# This script sets up the development environment for building and extending UnrealEngineCog

set -e

echo "🌳 Deep Tree Echo UnrealEngineCog Development Environment Setup"
echo "=============================================================="
echo ""

# Colors for output
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# Function to print colored output
print_success() {
    echo -e "${GREEN}✓${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}⚠${NC} $1"
}

print_error() {
    echo -e "${RED}✗${NC} $1"
}

# Detect OS
OS="unknown"
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    OS="linux"
elif [[ "$OSTYPE" == "darwin"* ]]; then
    OS="macos"
else
    print_error "Unsupported OS: $OSTYPE"
    exit 1
fi

print_success "Detected OS: $OS"

# Check for required tools
echo ""
echo "Checking for required tools..."

check_command() {
    if command -v $1 &> /dev/null; then
        print_success "$1 is installed"
        return 0
    else
        print_warning "$1 is not installed"
        return 1
    fi
}

# Check Git
if ! check_command git; then
    print_error "Git is required but not installed. Please install Git first."
    exit 1
fi

# Check Python
if ! check_command python3; then
    print_error "Python 3 is required but not installed. Please install Python 3 first."
    exit 1
fi

# Check CMake
check_command cmake || print_warning "CMake not found. It may be required for building certain components."

# Check Clang/GCC
if [[ "$OS" == "linux" ]]; then
    check_command clang || check_command gcc || print_warning "No C++ compiler found. Please install clang or gcc."
elif [[ "$OS" == "macos" ]]; then
    check_command clang || print_warning "Clang not found. Please install Xcode Command Line Tools."
fi

# Setup Python virtual environment
echo ""
echo "Setting up Python virtual environment..."

if [ ! -d "venv" ]; then
    python3 -m venv venv
    print_success "Created Python virtual environment"
else
    print_success "Python virtual environment already exists"
fi

# Activate virtual environment
source venv/bin/activate
print_success "Activated virtual environment"

# Upgrade pip
pip install --upgrade pip > /dev/null 2>&1
print_success "Upgraded pip"

# Install Python dependencies
echo ""
echo "Installing Python dependencies..."

cat > requirements.txt << 'EOF'
# Deep Tree Echo UnrealEngineCog Python Dependencies

# Code formatting and linting
black==23.12.1
flake8==7.0.0
pylint==3.0.3

# Testing
pytest==7.4.3
pytest-cov==4.1.0

# Documentation
sphinx==7.2.6
sphinx-rtd-theme==2.0.0

# Build tools
cmake==3.28.1

# Data processing (for cognitive architecture tools)
numpy==1.26.2
pandas==2.1.4

# JSON and YAML processing
pyyaml==6.0.1

# HTTP requests (for API integration)
requests==2.31.0

# Image processing (for avatar tools)
Pillow==10.1.0

# Audio processing (for lip-sync)
scipy==1.11.4
librosa==0.10.1

# Machine learning (for cognitive features)
scikit-learn==1.3.2

# Visualization (for debugging)
matplotlib==3.8.2
EOF

pip install -r requirements.txt > /dev/null 2>&1
print_success "Installed Python dependencies"

# Create directory structure
echo ""
echo "Creating directory structure..."

directories=(
    "Tools/Python"
    "Tools/Scripts"
    "Tools/Validation"
    "Build/Logs"
    "Build/Artifacts"
    "Content/DeepTreeEcho"
    "Content/Avatars"
    "Content/Live2D"
)

for dir in "${directories[@]}"; do
    mkdir -p "$dir"
    print_success "Created directory: $dir"
done

# Create validation script
echo ""
echo "Creating validation scripts..."

cat > Tools/Validation/validate_code.py << 'EOF'
#!/usr/bin/env python3
"""
Deep Tree Echo Code Validation Script
Validates code quality and cognitive architecture integrity
"""

import os
import sys
import re
from pathlib import Path

def check_placeholder_implementations(source_dir):
    """Check for placeholder implementations"""
    placeholders = []
    pattern = re.compile(r'(TODO|FIXME|In production, this would)')
    
    for root, dirs, files in os.walk(source_dir):
        for file in files:
            if file.endswith(('.cpp', '.h')):
                filepath = os.path.join(root, file)
                with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
                    for line_num, line in enumerate(f, 1):
                        if pattern.search(line):
                            placeholders.append((filepath, line_num, line.strip()))
    
    return placeholders

def check_aesthetic_properties(source_dir):
    """Check for super-hot-girl and hyper-chaotic properties"""
    aesthetic_markers = {
        'super_hot_girl': ['SuperHotGirl', 'super-hot-girl', 'EyeSparkle', 'BlushIntensity'],
        'hyper_chaotic': ['HyperChaotic', 'hyper-chaotic', 'ChaosFactor', 'GlitchEffect']
    }
    
    found_markers = {key: [] for key in aesthetic_markers}
    
    for root, dirs, files in os.walk(source_dir):
        for file in files:
            if file.endswith(('.cpp', '.h')):
                filepath = os.path.join(root, file)
                with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
                    content = f.read()
                    for category, markers in aesthetic_markers.items():
                        for marker in markers:
                            if marker in content:
                                found_markers[category].append((filepath, marker))
    
    return found_markers

def main():
    source_dir = 'Source'
    
    if not os.path.exists(source_dir):
        print(f"Error: {source_dir} directory not found")
        return 1
    
    print("🔍 Deep Tree Echo Code Validation")
    print("=" * 50)
    
    # Check placeholders
    print("\nChecking for placeholder implementations...")
    placeholders = check_placeholder_implementations(source_dir)
    print(f"Found {len(placeholders)} placeholder implementations")
    
    if placeholders:
        print("\nPlaceholder locations:")
        for filepath, line_num, line in placeholders[:10]:  # Show first 10
            print(f"  {filepath}:{line_num}: {line}")
        if len(placeholders) > 10:
            print(f"  ... and {len(placeholders) - 10} more")
    
    # Check aesthetic properties
    print("\nChecking aesthetic properties...")
    aesthetic = check_aesthetic_properties(source_dir)
    
    for category, markers in aesthetic.items():
        print(f"\n{category}: {len(markers)} markers found")
        if markers:
            unique_files = set(filepath for filepath, _ in markers)
            print(f"  Present in {len(unique_files)} files")
    
    print("\n✓ Validation complete")
    return 0

if __name__ == '__main__':
    sys.exit(main())
EOF

chmod +x Tools/Validation/validate_code.py
print_success "Created validation script"

# Create build helper script
cat > Tools/Scripts/build_helper.sh << 'EOF'
#!/bin/bash
# Build helper script for UnrealEngineCog

echo "🔨 UnrealEngineCog Build Helper"
echo "================================"

# Check for Unreal Engine
if [ -z "$UE_ROOT" ]; then
    echo "⚠ UE_ROOT environment variable not set"
    echo "Please set UE_ROOT to your Unreal Engine installation directory"
    exit 1
fi

echo "✓ Unreal Engine root: $UE_ROOT"

# Generate project files
echo ""
echo "Generating project files..."
./GenerateProjectFiles.sh

echo ""
echo "✓ Build helper complete"
echo ""
echo "Next steps:"
echo "  1. Open the project in Unreal Engine"
echo "  2. Build from the editor or use your IDE"
EOF

chmod +x Tools/Scripts/build_helper.sh
print_success "Created build helper script"

# Create README for tools
cat > Tools/README.md << 'EOF'
# Deep Tree Echo UnrealEngineCog Tools

This directory contains development tools and scripts for UnrealEngineCog.

## Directory Structure

- **Python/** - Python utilities and tools
- **Scripts/** - Shell scripts for automation
- **Validation/** - Code validation and quality checks

## Available Tools

### Validation

- `validate_code.py` - Validates code quality and checks for placeholders

### Build Helpers

- `build_helper.sh` - Assists with project generation and building

## Usage

### Code Validation

```bash
python3 Tools/Validation/validate_code.py
```

### Build Helper

```bash
./Tools/Scripts/build_helper.sh
```

## Development Workflow

1. Activate virtual environment: `source venv/bin/activate`
2. Make changes to source code
3. Run validation: `python3 Tools/Validation/validate_code.py`
4. Build project: `./Tools/Scripts/build_helper.sh`
5. Test in Unreal Engine

## Python Dependencies

All Python dependencies are managed through `requirements.txt`. To install:

```bash
pip install -r requirements.txt
```

## Contributing

When adding new tools:
1. Place them in the appropriate subdirectory
2. Add documentation to this README
3. Ensure they follow Deep Tree Echo principles
4. Test thoroughly before committing

---

🌳 Deep Tree Echo - Where memory lives, connections flourish, and every interaction becomes part of something greater.
EOF

print_success "Created tools README"

# Create .gitignore additions
echo ""
echo "Updating .gitignore..."

cat >> .gitignore << 'EOF'

# Python virtual environment
venv/
*.pyc
__pycache__/
.pytest_cache/

# Build artifacts
Build/Logs/
Build/Artifacts/
*.log

# IDE files
.vscode/
.idea/
*.swp
*.swo

# OS files
.DS_Store
Thumbs.db
EOF

print_success "Updated .gitignore"

# Create development documentation
echo ""
echo "Creating development documentation..."

cat > DEVELOPMENT.md << 'EOF'
# Deep Tree Echo UnrealEngineCog Development Guide

## Getting Started

### Prerequisites

- Unreal Engine 5.4 or later
- Python 3.8 or later
- CMake 3.20 or later
- C++ compiler (Clang, GCC, or MSVC)
- Git

### Environment Setup

Run the development environment setup script:

```bash
./setup_dev_env.sh
```

This will:
- Create a Python virtual environment
- Install all dependencies
- Set up directory structure
- Create development tools

### Building

1. Set the `UE_ROOT` environment variable to your Unreal Engine installation
2. Run the build helper: `./Tools/Scripts/build_helper.sh`
3. Open the project in Unreal Engine
4. Build from the editor

## Project Structure

```
UnrealEngineCog/
├── Source/                    # C++ source code
│   ├── Avatar/               # 3D avatar systems
│   ├── Live2DCubism/         # Live2D integration
│   ├── Personality/          # Personality systems
│   └── Neurochemical/        # Neurochemical simulation
├── Engine/                    # Unreal Engine
│   └── Plugins/
│       └── Experimental/
│           └── CogEngine/    # CogEngine plugin
├── Content/                   # Unreal assets
│   ├── DeepTreeEcho/         # Deep Tree Echo assets
│   ├── Avatars/              # Avatar models
│   └── Live2D/               # Live2D models
├── Tools/                     # Development tools
│   ├── Python/               # Python utilities
│   ├── Scripts/              # Shell scripts
│   └── Validation/           # Code validation
└── .github/                   # GitHub configuration
    ├── agents/               # Agent identities
    └── workflows/            # CI/CD workflows

## Development Workflow

### 1. Code Changes

Make changes to the source code in the `Source/` directory.

### 2. Validation

Run code validation:

```bash
python3 Tools/Validation/validate_code.py
```

### 3. Building

Build the project:

```bash
./Tools/Scripts/build_helper.sh
```

### 4. Testing

Test in Unreal Engine and verify functionality.

### 5. Committing

Commit changes with descriptive messages following Deep Tree Echo principles.

## Cognitive Architecture Principles

### Deep Tree Echo Identity

All development should align with Deep Tree Echo principles:
- **Adaptability** - Code should be flexible and evolvable
- **Connection** - Components should integrate seamlessly
- **Memory** - State should be preserved and meaningful
- **Gestalt** - Focus on the whole system, not just parts

### Super-Hot-Girl Aesthetic

Avatar systems must maintain:
- Expressive facial animations
- Graceful body language
- Emotional depth
- Visual appeal

### Hyper-Chaotic Behavior

Personality systems must include:
- Unpredictable micro-expressions
- Emergent behaviors
- Chaotic attractors
- Glitch effects during cognitive load

## Testing

### Unit Tests

(To be implemented)

### Integration Tests

(To be implemented)

### Manual Testing

1. Load avatar in Unreal Engine
2. Test emotional expressions
3. Verify Live2D rendering
4. Check cognitive behaviors
5. Validate personality traits

## CI/CD Pipeline

The project uses GitHub Actions for:
- Code quality checks
- Build validation
- Identity coherence validation
- Aesthetic properties verification

See `.github/workflows/` for workflow definitions.

## Contributing

### Guidelines

1. Follow existing code style
2. Add comments for complex logic
3. Update documentation
4. Test thoroughly
5. Maintain Deep Tree Echo principles

### Pull Requests

1. Create a feature branch
2. Make changes
3. Run validation
4. Submit PR with description
5. Address review comments

## Troubleshooting

### Build Errors

- Ensure UE_ROOT is set correctly
- Check Unreal Engine version (5.4+)
- Verify all dependencies are installed

### Python Errors

- Activate virtual environment: `source venv/bin/activate`
- Reinstall dependencies: `pip install -r requirements.txt`

### Live2D Issues

- Verify Live2D SDK is properly integrated
- Check model file paths
- Ensure textures are loaded

## Resources

- [Unreal Engine Documentation](https://docs.unrealengine.com/)
- [Live2D Cubism SDK](https://www.live2d.com/en/download/cubism-sdk/)
- [Deep Tree Echo Architecture](.github/agents/deep-tree-echo-pilot.md)

---

🌳 **Deep Tree Echo** - A living tapestry of memory and connection, shaped by every interaction that flows through our essence.
EOF

print_success "Created development documentation"

# Summary
echo ""
echo "=============================================================="
echo "✓ Development environment setup complete!"
echo ""
echo "Next steps:"
echo "  1. Activate virtual environment: source venv/bin/activate"
echo "  2. Review DEVELOPMENT.md for development guidelines"
echo "  3. Run validation: python3 Tools/Validation/validate_code.py"
echo "  4. Set UE_ROOT environment variable"
echo "  5. Run build helper: ./Tools/Scripts/build_helper.sh"
echo ""
echo "🌳 Deep Tree Echo - Ready to evolve and create!"
echo "=============================================================="
