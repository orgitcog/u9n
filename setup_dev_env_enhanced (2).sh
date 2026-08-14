#!/bin/bash
#===============================================================================
# Enhanced Development Environment Setup for UnrealEngineCog
# Deep Tree Echo AGI Avatar System
#
# This script sets up a complete development environment including:
# - Unreal Engine build tools
# - Live2D Cubism SDK
# - Python virtual environment with ML dependencies
# - Node.js environment
# - Testing frameworks
# - Docker support
#===============================================================================

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Logging functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Detect OS
detect_os() {
    if [[ "$OSTYPE" == "linux-gnu"* ]]; then
        OS="linux"
    elif [[ "$OSTYPE" == "darwin"* ]]; then
        OS="macos"
    elif [[ "$OSTYPE" == "msys" ]] || [[ "$OSTYPE" == "cygwin" ]]; then
        OS="windows"
    else
        log_error "Unsupported operating system: $OSTYPE"
        exit 1
    fi
    log_info "Detected OS: $OS"
}

# Check prerequisites
check_prerequisites() {
    log_info "Checking prerequisites..."
    
    # Check for Git
    if ! command -v git &> /dev/null; then
        log_error "Git is not installed. Please install Git first."
        exit 1
    fi
    log_success "Git found: $(git --version)"
    
    # Check for Python 3
    if ! command -v python3 &> /dev/null; then
        log_error "Python 3 is not installed. Please install Python 3.8 or higher."
        exit 1
    fi
    log_success "Python found: $(python3 --version)"
    
    # Check for CMake
    if ! command -v cmake &> /dev/null; then
        log_warning "CMake not found. Installing CMake..."
        if [[ "$OS" == "linux" ]]; then
            sudo apt-get update && sudo apt-get install -y cmake
        elif [[ "$OS" == "macos" ]]; then
            brew install cmake
        fi
    fi
    log_success "CMake found: $(cmake --version | head -n1)"
    
    # Check for Node.js
    if ! command -v node &> /dev/null; then
        log_warning "Node.js not found. Installing Node.js..."
        if [[ "$OS" == "linux" ]]; then
            curl -fsSL https://deb.nodesource.com/setup_18.x | sudo -E bash -
            sudo apt-get install -y nodejs
        elif [[ "$OS" == "macos" ]]; then
            brew install node
        fi
    fi
    log_success "Node.js found: $(node --version)"
}

# Setup Python virtual environment
setup_python_env() {
    log_info "Setting up Python virtual environment..."
    
    # Create virtual environment
    if [ ! -d "venv" ]; then
        python3 -m venv venv
        log_success "Virtual environment created"
    else
        log_info "Virtual environment already exists"
    fi
    
    # Activate virtual environment
    source venv/bin/activate
    
    # Upgrade pip
    pip install --upgrade pip
    
    # Install Python dependencies
    log_info "Installing Python dependencies..."
    pip install -r requirements.txt
    
    log_success "Python environment setup complete"
}

# Setup Node.js environment
setup_nodejs_env() {
    log_info "Setting up Node.js environment..."
    
    # Install npm dependencies if package.json exists
    if [ -f "package.json" ]; then
        npm install
        log_success "Node.js dependencies installed"
    else
        log_info "No package.json found, skipping Node.js setup"
    fi
}

# Setup Unreal Engine build tools
setup_unreal_build_tools() {
    log_info "Setting up Unreal Engine build tools..."
    
    if [[ "$OS" == "linux" ]]; then
        # Install required packages for Linux
        sudo apt-get update
        sudo apt-get install -y \
            build-essential \
            clang \
            mono-complete \
            mono-devel \
            mono-xbuild \
            mono-dmcs \
            mono-reference-assemblies-4.0 \
            libmono-system-data-datasetextensions4.0-cil \
            libmono-system-web-extensions4.0-cil \
            libmono-system-management4.0-cil \
            libmono-system-xml-linq4.0-cil \
            cmake \
            dos2unix \
            clang-format \
            libxcursor-dev \
            libxrandr-dev \
            libxinerama-dev \
            libxi-dev \
            libgl1-mesa-dev \
            libglu1-mesa-dev \
            libvulkan-dev
        
        log_success "Unreal Engine build tools installed (Linux)"
        
    elif [[ "$OS" == "macos" ]]; then
        # Install Xcode command line tools
        if ! xcode-select -p &> /dev/null; then
            log_info "Installing Xcode command line tools..."
            xcode-select --install
        fi
        
        log_success "Unreal Engine build tools installed (macOS)"
    fi
}

# Setup Live2D Cubism SDK
setup_live2d_sdk() {
    log_info "Setting up Live2D Cubism SDK..."
    
    SDK_DIR="ThirdParty/Live2DCubismSDK"
    
    if [ ! -d "$SDK_DIR" ]; then
        mkdir -p "$SDK_DIR"
        
        log_warning "Live2D Cubism SDK not found in $SDK_DIR"
        log_info "Please download the SDK from: https://www.live2d.com/en/download/cubism-sdk/"
        log_info "Extract the SDK to: $SDK_DIR"
        log_info "Expected structure:"
        log_info "  $SDK_DIR/Core/"
        log_info "  $SDK_DIR/Framework/"
        log_info "  $SDK_DIR/Samples/"
        
        read -p "Press Enter when SDK is installed, or Ctrl+C to skip..."
    else
        log_success "Live2D Cubism SDK directory found"
    fi
    
    # Check for required SDK components
    if [ -d "$SDK_DIR/Core" ] && [ -d "$SDK_DIR/Framework" ]; then
        log_success "Live2D Cubism SDK appears to be properly installed"
        
        # Create symbolic links for easier access
        ln -sf "$(pwd)/$SDK_DIR/Core" "Source/Live2DCubism/SDK/Core" 2>/dev/null || true
        ln -sf "$(pwd)/$SDK_DIR/Framework" "Source/Live2DCubism/SDK/Framework" 2>/dev/null || true
    else
        log_warning "Live2D Cubism SDK components not found. Build will use simulation mode."
    fi
}

# Setup testing framework
setup_testing_framework() {
    log_info "Setting up testing framework..."
    
    # Install Google Test for C++ testing
    if [[ "$OS" == "linux" ]]; then
        sudo apt-get install -y libgtest-dev
        
        # Build Google Test
        cd /usr/src/gtest
        sudo cmake CMakeLists.txt
        sudo make
        sudo cp lib/*.a /usr/lib
        cd - > /dev/null
        
        log_success "Google Test installed"
    elif [[ "$OS" == "macos" ]]; then
        brew install googletest
        log_success "Google Test installed"
    fi
    
    # Install Python testing tools
    if [ -f "venv/bin/activate" ]; then
        source venv/bin/activate
        pip install pytest pytest-cov pytest-asyncio
        log_success "Python testing tools installed"
    fi
}

# Setup Docker support
setup_docker_support() {
    log_info "Setting up Docker support..."
    
    if command -v docker &> /dev/null; then
        log_success "Docker found: $(docker --version)"
        
        # Check if docker-compose is installed
        if ! command -v docker-compose &> /dev/null; then
            log_info "Installing docker-compose..."
            if [[ "$OS" == "linux" ]]; then
                sudo curl -L "https://github.com/docker/compose/releases/latest/download/docker-compose-$(uname -s)-$(uname -m)" -o /usr/local/bin/docker-compose
                sudo chmod +x /usr/local/bin/docker-compose
            elif [[ "$OS" == "macos" ]]; then
                brew install docker-compose
            fi
        fi
        log_success "docker-compose found: $(docker-compose --version)"
    else
        log_warning "Docker not found. Skipping Docker setup."
        log_info "To install Docker, visit: https://docs.docker.com/get-docker/"
    fi
}

# Create development configuration files
create_dev_configs() {
    log_info "Creating development configuration files..."
    
    # Create .editorconfig if it doesn't exist
    if [ ! -f ".editorconfig" ]; then
        cat > .editorconfig << 'EOF'
root = true

[*]
charset = utf-8
end_of_line = lf
insert_final_newline = true
trim_trailing_whitespace = true

[*.{cpp,h,cs}]
indent_style = space
indent_size = 4

[*.{json,yml,yaml}]
indent_style = space
indent_size = 2

[*.md]
trim_trailing_whitespace = false
EOF
        log_success "Created .editorconfig"
    fi
    
    # Create .clang-format if it doesn't exist
    if [ ! -f ".clang-format" ]; then
        cat > .clang-format << 'EOF'
BasedOnStyle: LLVM
IndentWidth: 4
TabWidth: 4
UseTab: Never
ColumnLimit: 120
BreakBeforeBraces: Allman
AllowShortFunctionsOnASingleLine: Empty
AllowShortIfStatementsOnASingleLine: false
IndentCaseLabels: true
AccessModifierOffset: -4
EOF
        log_success "Created .clang-format"
    fi
}

# Generate project files
generate_project_files() {
    log_info "Generating Unreal Engine project files..."
    
    if [ -f "GenerateProjectFiles.sh" ]; then
        chmod +x GenerateProjectFiles.sh
        ./GenerateProjectFiles.sh
        log_success "Project files generated"
    else
        log_warning "GenerateProjectFiles.sh not found, skipping"
    fi
}

# Create virtual environment activation helper
create_activation_helper() {
    log_info "Creating environment activation helper..."
    
    cat > activate_dev_env.sh << 'EOF'
#!/bin/bash
# Activate Deep Tree Echo development environment

# Activate Python virtual environment
if [ -f "venv/bin/activate" ]; then
    source venv/bin/activate
    echo "Python virtual environment activated"
fi

# Set environment variables
export UE_ROOT="$(pwd)"
export LIVE2D_SDK_ROOT="$UE_ROOT/ThirdParty/Live2DCubismSDK"
export PATH="$UE_ROOT/Engine/Binaries/Linux:$PATH"

echo "Deep Tree Echo development environment activated"
echo "UE_ROOT: $UE_ROOT"
echo "LIVE2D_SDK_ROOT: $LIVE2D_SDK_ROOT"
EOF
    
    chmod +x activate_dev_env.sh
    log_success "Created activate_dev_env.sh"
}

# Print summary
print_summary() {
    echo ""
    log_success "=========================================="
    log_success "Development Environment Setup Complete!"
    log_success "=========================================="
    echo ""
    log_info "To activate the development environment, run:"
    echo "  source activate_dev_env.sh"
    echo ""
    log_info "To build the project:"
    echo "  make"
    echo ""
    log_info "To run tests:"
    echo "  make test"
    echo ""
    log_info "For more information, see README.md"
    echo ""
}

# Main execution
main() {
    log_info "Starting Deep Tree Echo development environment setup..."
    echo ""
    
    detect_os
    check_prerequisites
    setup_python_env
    setup_nodejs_env
    setup_unreal_build_tools
    setup_live2d_sdk
    setup_testing_framework
    setup_docker_support
    create_dev_configs
    create_activation_helper
    generate_project_files
    
    print_summary
}

# Run main function
main "$@"
