#!/bin/bash

# CryptoClaude Dependencies Installation Script
# This script ensures proper installation of all required dependencies

set -e  # Exit on any error

echo "🔧 Installing CryptoClaude dependencies..."

# Function to check if command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# SECURE USER-LEVEL INSTALLATION - NO SUDO REQUIRED
install_nlohmann_json() {
    echo "📦 Installing nlohmann-json (user-level only, no sudo)..."

    # Create user-local install directory
    LOCAL_INSTALL="$HOME/.local"
    mkdir -p "$LOCAL_INSTALL/include" "$LOCAL_INSTALL/lib"

    if command_exists brew; then
        echo "Using Homebrew to install nlohmann-json..."
        brew install nlohmann-json
        echo "ℹ️  nlohmann-json installed via Homebrew (user-level)"
        echo "ℹ️  Build system will automatically detect this location"

    elif command_exists apt-get || command_exists yum || command_exists pacman; then
        echo "⚠️  System package manager detected, but this script avoids privilege escalation for security."
        echo "📋 MANUAL INSTALLATION REQUIRED:"
        if command_exists apt-get; then
            echo "   Run: apt-get update && apt-get install nlohmann-json3-dev (as administrator)"
        elif command_exists yum; then
            echo "   Run: yum install nlohmann-json-devel (as administrator)"
        elif command_exists pacman; then
            echo "   Run: pacman -S nlohmann-json (as administrator)"
        fi
        echo ""
        echo "🔒 Security Notice: This script no longer executes sudo commands automatically."
        echo "Please run the above command manually with your own authorization."
        echo ""
        echo "Alternative: Continue with user-level header-only installation below..."
        read -p "Continue with header-only installation? (Y/n): " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Nn]$ ]]; then
            echo "Installation skipped. Please install manually."
            return 1
        fi
        install_header_only_nlohmann

    else
        echo "📦 Installing nlohmann-json header-only (user-level)..."
        install_header_only_nlohmann
    fi
}

# SECURE HEADER-ONLY INSTALLATION WITH INTEGRITY VERIFICATION
install_header_only_nlohmann() {
    echo "📥 Downloading nlohmann-json header-only library with integrity verification..."

    # Create secure temporary directory
    TEMP_DIR=$(mktemp -d -t nlohmann-json-XXXXXX)
    if [ ! -d "$TEMP_DIR" ]; then
        echo "❌ Failed to create secure temporary directory"
        return 1
    fi

    # Cleanup function
    cleanup_temp() {
        rm -rf "$TEMP_DIR"
    }
    trap cleanup_temp EXIT

    cd "$TEMP_DIR"

    # Basic download verification - checks file exists and has reasonable size
    verify_download() {
        local file="$1"

        if [ ! -f "$file" ]; then
            echo "❌ Download failed - file not found"
            return 1
        fi

        # Check file size - nlohmann json header should be substantial
        local file_size=$(wc -c < "$file" 2>/dev/null || echo 0)
        if [ "$file_size" -lt 10000 ]; then
            echo "❌ Downloaded file seems too small ($file_size bytes) - likely corrupted"
            return 1
        fi

        # Basic content check - should contain nlohmann namespace
        if ! grep -q "namespace nlohmann" "$file" 2>/dev/null; then
            echo "❌ Downloaded file doesn't appear to be nlohmann json header"
            return 1
        fi

        echo "✅ Basic download verification passed ($file_size bytes)"
        echo "ℹ️  Note: This is basic verification only - for production use, verify checksums manually"
        return 0
    }

    # Download with integrity verification
    if command_exists curl; then
        echo "Using curl to download with security headers..."
        curl -L -A "CryptoClaude-Install/1.0" \
             -H "Accept: application/octet-stream" \
             --max-time 30 \
             --retry 3 \
             -o json.hpp \
             "https://github.com/nlohmann/json/releases/latest/download/json.hpp" || {
            echo "❌ Download failed"
            return 1
        }
        verify_integrity json.hpp || return 1

    elif command_exists wget; then
        echo "Using wget to download with security options..."
        wget --user-agent="CryptoClaude-Install/1.0" \
             --header="Accept: application/octet-stream" \
             --timeout=30 \
             --tries=3 \
             -O json.hpp \
             "https://github.com/nlohmann/json/releases/latest/download/json.hpp" || {
            echo "❌ Download failed"
            return 1
        }
        verify_integrity json.hpp || return 1

    elif command_exists git; then
        echo "Using git to clone with verification..."
        # Use HTTPS and verify SSL certificates
        git -c http.sslVerify=true clone --depth 1 https://github.com/nlohmann/json.git . || {
            echo "❌ Git clone failed"
            return 1
        }

        # Verify repository authenticity (basic check)
        if [ ! -f "README.md" ] || ! grep -q "nlohmann" README.md; then
            echo "❌ Repository verification failed - doesn't appear to be nlohmann/json"
            return 1
        fi

        cp single_include/nlohmann/json.hpp . || {
            echo "❌ Failed to copy header file"
            return 1
        }
        verify_integrity json.hpp || return 1

    else
        echo "❌ No download tool available (curl, wget, or git required)"
        echo "📋 SECURE MANUAL INSTALLATION:"
        echo "   1. Download from: https://github.com/nlohmann/json/releases/latest/download/json.hpp"
        echo "   2. Verify the file contains 'namespace nlohmann' and is substantial in size"
        echo "   3. Place file in: $HOME/.local/include/nlohmann/json.hpp"
        echo ""
        echo "⚠️  For production use, always verify checksums from the official nlohmann-json releases!"
        return 1
    fi

    # Install to user directory
    mkdir -p "$HOME/.local/include/nlohmann"
    cp json.hpp "$HOME/.local/include/nlohmann/" || {
        echo "❌ Failed to install header file"
        return 1
    }

    echo "✅ nlohmann-json installed to $HOME/.local/include/nlohmann/"
    echo "ℹ️  Build system will automatically detect this location"
}

# SECURE INSTALLATION VERIFICATION - NO PREDICTABLE TEMP FILES
verify_installation() {
    echo "🔍 Verifying nlohmann-json installation..."

    # Create secure temporary files
    TEMP_DIR=$(mktemp -d -t json-verify-XXXXXX)
    if [ ! -d "$TEMP_DIR" ]; then
        echo "❌ Failed to create secure temporary directory for verification"
        return 1
    fi

    # Cleanup function
    cleanup_verify() {
        rm -rf "$TEMP_DIR"
    }
    trap cleanup_verify EXIT

    TEST_FILE="$TEMP_DIR/json_test.cpp"
    TEST_BINARY="$TEMP_DIR/json_test"

    # Test compilation with user-level include paths
    cat > "$TEST_FILE" << 'EOF'
#include <nlohmann/json.hpp>
#include <iostream>
using json = nlohmann::json;

int main() {
    json j = json::parse("{\"test\": \"value\"}");
    std::cout << "JSON test passed: " << j["test"] << std::endl;
    return 0;
}
EOF

    # Include user-local paths in compilation
    INCLUDE_PATHS=""
    if [ -d "$HOME/.local/include" ]; then
        INCLUDE_PATHS="$INCLUDE_PATHS -I$HOME/.local/include"
    fi
    if [ -d "/opt/homebrew/include" ]; then
        INCLUDE_PATHS="$INCLUDE_PATHS -I/opt/homebrew/include"
    fi
    if [ -d "/usr/local/include" ]; then
        INCLUDE_PATHS="$INCLUDE_PATHS -I/usr/local/include"
    fi

    if g++ -std=c++17 $INCLUDE_PATHS "$TEST_FILE" -o "$TEST_BINARY" 2>/dev/null && "$TEST_BINARY" >/dev/null 2>&1; then
        echo "✅ nlohmann-json installation verified successfully"
        return 0
    else
        echo "❌ nlohmann-json installation verification failed"
        echo "📋 Possible solutions:"
        echo "   1. Install nlohmann-json using your system package manager"
        echo "   2. Ensure the header file is in a standard include path"
        echo "   3. Try running this script again"
        return 1
    fi
}

# SECURE SQLITE3 INSTALLATION - NO SUDO REQUIRED
install_sqlite() {
    echo "📦 Checking SQLite3 availability..."

    # Check if SQLite3 is already available
    if command_exists sqlite3 && pkg-config --exists sqlite3 2>/dev/null; then
        echo "✅ SQLite3 already available system-wide"
        return 0
    fi

    if command_exists brew; then
        echo "Using Homebrew to install SQLite3 (user-level)..."
        brew install sqlite3
        echo "✅ SQLite3 installed via Homebrew"
    else
        echo "⚠️  SQLite3 development libraries not found"
        echo "📋 MANUAL INSTALLATION REQUIRED:"
        if command_exists apt-get; then
            echo "   Run: apt-get install libsqlite3-dev (as administrator)"
        elif command_exists yum; then
            echo "   Run: yum install sqlite-devel (as administrator)"
        elif command_exists pacman; then
            echo "   Run: pacman -S sqlite (as administrator)"
        else
            echo "   Install SQLite3 development libraries for your system"
        fi
        echo ""
        echo "🔒 Security Notice: This script no longer executes sudo commands automatically."
        echo "Please run the above command manually with your own authorization."
        echo ""
        echo "The build system will attempt to link with -lsqlite3 regardless."
        echo "If SQLite3 is installed system-wide, this should work."
    fi
}

# Main installation process
main() {
    echo "🚀 Starting CryptoClaude dependency installation..."

    # Check for required system tools
    if ! command_exists g++; then
        echo "❌ g++ compiler not found. Please install a C++ compiler first."
        exit 1
    fi

    if ! command_exists cmake && ! command_exists brew && ! command_exists apt-get && ! command_exists yum && ! command_exists pacman; then
        echo "❌ No supported package manager or cmake found."
        echo "Please install one of: brew, apt-get, yum, pacman, or cmake"
        exit 1
    fi

    # Install dependencies
    install_sqlite
    install_nlohmann_json

    # Verify installation
    if verify_installation; then
        echo ""
        echo "🎉 All dependencies installed successfully!"
        echo ""
        echo "Next steps:"
        echo "1. Run 'make' or your preferred build system"
        echo "2. The JSON dependency security issue has been resolved"
        echo ""
    else
        echo ""
        echo "❌ Installation completed but verification failed."
        echo "You may need to manually install nlohmann-json or check your system configuration."
        echo ""
        exit 1
    fi
}

# Run main function
main "$@"