#!/bin/bash

##############################################################################
# CryptoClaude Dashboard Launcher
# Simple script to launch the control dashboard locally
##############################################################################

set -e

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${BLUE}"
echo "🚀 CryptoClaude Control Dashboard Launcher"
echo "==========================================="
echo -e "${NC}"

# Check if Python is available
if ! command -v python3 &> /dev/null; then
    echo -e "${RED}❌ Python 3 is required but not installed${NC}"
    exit 1
fi

# Check if we're in the right directory
if [[ ! -f "src/Web/dashboard_server.py" ]]; then
    echo -e "${RED}❌ Please run this script from the CryptoClaude root directory${NC}"
    exit 1
fi

# Default port
PORT=8080

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -p|--port)
            PORT="$2"
            shift 2
            ;;
        -h|--help)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  -p, --port PORT    Port to run the dashboard on (default: 8080)"
            echo "  -h, --help         Show this help message"
            exit 0
            ;;
        *)
            echo -e "${RED}❌ Unknown option: $1${NC}"
            exit 1
            ;;
    esac
done

echo -e "${YELLOW}⚙️  Configuration:${NC}"
echo "   Port: $PORT"
echo "   Dashboard: http://localhost:$PORT"
echo ""

echo -e "${GREEN}✅ Starting dashboard server...${NC}"
echo -e "${BLUE}   Press Ctrl+C to stop the server${NC}"
echo ""

# Change to Web directory and start server
cd src/Web
python3 dashboard_server.py --port "$PORT"