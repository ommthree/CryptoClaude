#!/bin/bash

##############################################################################
# CryptoClaude Quick Redeployment Script
#
# This script performs a fast redeployment of the CryptoClaude application
# to an existing AWS Lightsail instance with all API keys preconfigured.
##############################################################################

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log() {
    echo -e "${BLUE}[$(date '+%Y-%m-%d %H:%M:%S')] $1${NC}"
}

success() {
    echo -e "${GREEN}✅ $1${NC}"
}

error() {
    echo -e "${RED}❌ $1${NC}"
    exit 1
}

main() {
    log "🚀 CryptoClaude Quick Redeployment"
    log "================================="

    # Check if instance exists
    if ! aws lightsail get-instance --instance-name "cryptoclaude-prod" &> /dev/null; then
        error "Instance 'cryptoclaude-prod' not found. Run full deployment first."
    fi

    log "📦 Building and deploying application..."
    ./deploy/deploy-application.sh

    success "🎊 Quick redeployment complete!"

    echo
    echo "🔗 Access your CryptoClaude instance:"
    STATIC_IP=$(aws lightsail get-static-ip --static-ip-name "cryptoclaude-static-ip" --query 'staticIp.ipAddress' --output text 2>/dev/null)
    echo "  • Console: http://${STATIC_IP}:3000"
    echo "  • Health: http://${STATIC_IP}/health"
    echo "  • SSH: ssh -i /Users/Owen/Downloads/LightsailDefaultKey-eu-north-1.pem ubuntu@${STATIC_IP}"
}

# Load API keys from secure configuration file
API_KEYS_FILE="$(dirname "$0")/api-keys.conf"

if [[ -f "$API_KEYS_FILE" ]]; then
    log "📄 Loading API keys from $API_KEYS_FILE"
    source "$API_KEYS_FILE"
else
    error "API keys configuration file not found: $API_KEYS_FILE

Please create the API keys configuration file:
1. Copy the template: cp deploy/api-keys.conf.template deploy/api-keys.conf
2. Edit the file and add your actual API keys
3. Run the deployment script again"
fi

if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    main "$@"
fi