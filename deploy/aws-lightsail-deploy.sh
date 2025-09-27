#!/bin/bash

##############################################################################
# CryptoClaude AWS Lightsail Automated Deployment Script
# Day 27: Production Deployment Architecture
#
# This script fully automates the deployment of CryptoClaude to AWS Lightsail
# with production-ready configuration, monitoring, and security.
##############################################################################

set -e  # Exit on any error

# Configuration
INSTANCE_NAME="cryptoclaude-prod"
BUNDLE_ID="medium_3_0"  # 4GB RAM, 2 vCPUs, 80GB SSD - suitable for production ($24/month)
BLUEPRINT_ID="ubuntu_22_04"
AVAILABILITY_ZONE="eu-north-1a"
REGION="eu-north-1"
DOMAIN_NAME="${DOMAIN_NAME:-cryptoclaude.trading}"  # Set your domain

# Database Configuration
DB_NAME="cryptoclaude-db"
DB_BUNDLE_ID="micro_2_0"  # Cost-effective for initial deployment ($15/month)
DB_USERNAME="cryptoclaude"
DB_PASSWORD_FILE="./secrets/db_password.txt"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

##############################################################################
# Utility Functions
##############################################################################

log() {
    echo -e "${BLUE}[$(date '+%Y-%m-%d %H:%M:%S')] $1${NC}"
}

success() {
    echo -e "${GREEN}✅ $1${NC}"
}

warn() {
    echo -e "${YELLOW}⚠️  $1${NC}"
}

error() {
    echo -e "${RED}❌ $1${NC}"
    exit 1
}

##############################################################################
# Pre-deployment Checks
##############################################################################

check_prerequisites() {
    log "🔍 Checking deployment prerequisites..."

    # Check AWS CLI
    if ! command -v aws &> /dev/null; then
        error "AWS CLI is not installed. Please install it first."
    fi

    # Check AWS credentials
    if ! aws sts get-caller-identity &> /dev/null; then
        error "AWS credentials not configured. Run 'aws configure' first."
    fi

    # Load API keys from secure configuration file
    API_KEYS_FILE="$(dirname "$0")/api-keys.conf"

    if [[ -f "$API_KEYS_FILE" ]]; then
        log "📄 Loading API keys from $API_KEYS_FILE"
        source "$API_KEYS_FILE"
        success "API keys loaded from configuration file"
    else
        warn "API keys configuration file not found: $API_KEYS_FILE"
        warn "API keys will need to be configured manually after deployment"
        echo "To set up API keys:"
        echo "1. Copy the template: cp deploy/api-keys.conf.template deploy/api-keys.conf"
        echo "2. Edit deploy/api-keys.conf and add your actual API keys"
    fi

    success "API keys configuration checked"

    # Create secrets directory
    mkdir -p ./secrets

    success "Prerequisites check completed"
}

##############################################################################
# Database Setup
##############################################################################

setup_database() {
    log "🗄️  Setting up managed database..."

    # Generate database password if not exists
    if [[ ! -f "$DB_PASSWORD_FILE" ]]; then
        # Generate a password with only valid characters (no /, @, ", or spaces)
        openssl rand -hex 16 > "$DB_PASSWORD_FILE"
        chmod 600 "$DB_PASSWORD_FILE"
        success "Generated secure database password"
    fi

    DB_PASSWORD=$(cat "$DB_PASSWORD_FILE")

    # Check if database already exists
    if aws lightsail get-relational-database --relational-database-name "$DB_NAME" &> /dev/null; then
        warn "Database $DB_NAME already exists, skipping creation"
    else
        log "Creating managed MySQL database..."
        aws lightsail create-relational-database \
            --relational-database-name "$DB_NAME" \
            --relational-database-bundle-id "$DB_BUNDLE_ID" \
            --relational-database-blueprint-id "mysql_8_0" \
            --master-database-name "cryptoclaude" \
            --master-username "$DB_USERNAME" \
            --master-user-password "$DB_PASSWORD" \
            --publicly-accessible \
            --tags key="Project",value="CryptoClaude" key="Environment",value="Production"

        success "Database creation initiated"
    fi
}

##############################################################################
# Instance Creation and Setup
##############################################################################

create_instance() {
    log "🚀 Creating Lightsail instance..."

    # Check if instance already exists
    if aws lightsail get-instance --instance-name "$INSTANCE_NAME" &> /dev/null; then
        warn "Instance $INSTANCE_NAME already exists, skipping creation"
        return
    fi

    # Create instance with launch script
    aws lightsail create-instances \
        --instance-names "$INSTANCE_NAME" \
        --bundle-id "$BUNDLE_ID" \
        --blueprint-id "$BLUEPRINT_ID" \
        --availability-zone "$AVAILABILITY_ZONE" \
        --user-data file://deploy/launch-script.sh \
        --tags key="Project",value="CryptoClaude" key="Environment",value="Production"

    success "Instance creation initiated"
}

##############################################################################
# Static IP and Networking
##############################################################################

setup_networking() {
    log "🌐 Setting up networking and static IP..."

    # Allocate static IP
    STATIC_IP_NAME="cryptoclaude-static-ip"
    if ! aws lightsail get-static-ip --static-ip-name "$STATIC_IP_NAME" &> /dev/null; then
        aws lightsail allocate-static-ip \
            --static-ip-name "$STATIC_IP_NAME"
        success "Static IP allocated"
    fi

    # Wait for instance to be running
    log "⏳ Waiting for instance to be running..."
    aws lightsail wait instance-exists --instance-name "$INSTANCE_NAME"

    # Attach static IP to instance
    aws lightsail attach-static-ip \
        --static-ip-name "$STATIC_IP_NAME" \
        --instance-name "$INSTANCE_NAME" || warn "Static IP attachment may have failed"

    # Configure firewall
    log "🔒 Configuring firewall rules..."

    # Allow HTTP/HTTPS
    aws lightsail put-instance-public-ports \
        --instance-name "$INSTANCE_NAME" \
        --port-infos fromPort=80,toPort=80,protocol=tcp,accessType=Public \
                     fromPort=443,toPort=443,protocol=tcp,accessType=Public \
                     fromPort=22,toPort=22,protocol=tcp,accessType=Public \
                     fromPort=3000,toPort=3000,protocol=tcp,accessType=Public

    # Get static IP
    STATIC_IP=$(aws lightsail get-static-ip --static-ip-name "$STATIC_IP_NAME" --query 'staticIp.ipAddress' --output text)
    success "Networking configured - Static IP: $STATIC_IP"
}

##############################################################################
# SSL Certificate Setup
##############################################################################

setup_ssl() {
    log "🔐 Setting up SSL certificate..."

    # Create domain if provided
    if [[ -n "$DOMAIN_NAME" ]]; then
        if ! aws lightsail get-domain --domain-name "$DOMAIN_NAME" &> /dev/null; then
            aws lightsail create-domain \
                --domain-name "$DOMAIN_NAME" || warn "Domain creation failed - continue manually"
        fi

        # Request SSL certificate
        CERT_NAME="cryptoclaude-ssl"
        if ! aws lightsail get-certificate --certificate-name "$CERT_NAME" &> /dev/null; then
            aws lightsail request-certificate \
                --certificate-name "$CERT_NAME" \
                --domain-name "$DOMAIN_NAME" \
                --subject-alternative-names "www.$DOMAIN_NAME" || warn "SSL certificate request failed"

            success "SSL certificate requested - check DNS validation requirements"
        fi
    else
        warn "No domain specified - SSL certificate setup skipped"
    fi
}

##############################################################################
# Load Balancer Setup (Optional)
##############################################################################

setup_load_balancer() {
    log "⚖️  Setting up load balancer for high availability..."

    LB_NAME="cryptoclaude-lb"
    if ! aws lightsail get-load-balancer --load-balancer-name "$LB_NAME" &> /dev/null; then
        aws lightsail create-load-balancer \
            --load-balancer-name "$LB_NAME" \
            --instance-port 3000 \
            --health-check-path "/api/health" \
            --certificate-name "${CERT_NAME:-}" \
            --tags key="Project",value="CryptoClaude"

        success "Load balancer created"

        # Attach instance to load balancer
        sleep 30  # Wait for load balancer to be ready
        aws lightsail attach-instances-to-load-balancer \
            --load-balancer-name "$LB_NAME" \
            --instance-names "$INSTANCE_NAME" || warn "Instance attachment to LB may have failed"
    else
        warn "Load balancer already exists"
    fi
}

##############################################################################
# Backup Configuration
##############################################################################

setup_backups() {
    log "💾 Configuring automated backups..."

    # Enable automatic snapshots for instance
    aws lightsail enable-add-on \
        --resource-name "$INSTANCE_NAME" \
        --add-on-request addOnType="AutoSnapshot",autoSnapshotAddOnRequest='{snapshotTimeOfDay="02:00"}'

    success "Automated backups configured (daily at 2:00 AM)"
}

##############################################################################
# Monitoring Setup
##############################################################################

setup_monitoring() {
    log "📊 Setting up monitoring and alerting..."

    # Create CloudWatch integration (requires additional setup)
    cat > ./deploy/monitoring-setup.json <<EOF
{
    "instance_name": "$INSTANCE_NAME",
    "database_name": "$DB_NAME",
    "static_ip": "$STATIC_IP",
    "monitoring_enabled": true,
    "alert_email": "${ALERT_EMAIL:-admin@cryptoclaude.com}"
}
EOF

    success "Monitoring configuration created"
}

##############################################################################
# Deployment Status and Information
##############################################################################

deployment_summary() {
    log "📋 Deployment Summary"
    echo "=================================================="
    echo "🎯 CryptoClaude Production Deployment Complete!"
    echo "=================================================="
    echo
    echo "📊 Infrastructure Details:"
    echo "  • Instance: $INSTANCE_NAME ($BUNDLE_ID)"
    echo "  • Database: $DB_NAME (MySQL 8.0)"
    echo "  • Region: $REGION ($AVAILABILITY_ZONE)"
    echo "  • Static IP: ${STATIC_IP:-'Pending'}"
    echo "  • Domain: ${DOMAIN_NAME:-'Not configured'}"
    echo
    echo "🔗 Access Information:"
    echo "  • SSH: ssh -i ~/.ssh/lightsail-key.pem ubuntu@${STATIC_IP:-'pending'}"
    echo "  • Console: http://${STATIC_IP:-'pending'}:3000"
    echo "  • HTTPS: https://${DOMAIN_NAME:-'N/A'}"
    echo
    echo "📁 Configuration Files:"
    echo "  • Database password: $DB_PASSWORD_FILE"
    echo "  • Monitoring config: ./deploy/monitoring-setup.json"
    echo
    echo "⚠️  Next Steps:"
    echo "  1. Wait 5-10 minutes for instance initialization to complete"
    echo "  2. SSH into instance and check application status"
    echo "  3. Configure environment variables if not set during deployment"
    echo "  4. Test console application: http://${STATIC_IP:-'IP'}:3000"
    echo "  5. Set up domain DNS records if using custom domain"
    echo
    echo "🎉 Ready for Day 28: AI-Enhanced Trading Integration!"
    echo "=================================================="
}

##############################################################################
# Main Deployment Process
##############################################################################

main() {
    log "🚀 Starting CryptoClaude AWS Lightsail Deployment"
    log "================================================="

    # Execute deployment steps
    check_prerequisites
    setup_database
    create_instance

    # Wait for instance to be available
    log "⏳ Waiting for instance to be available (this may take 2-3 minutes)..."
    sleep 180

    setup_networking
    setup_ssl
    setup_load_balancer
    setup_backups
    setup_monitoring

    # Final summary
    deployment_summary

    success "🎊 Day 27 AWS Lightsail Deployment Complete!"
}

##############################################################################
# Script Execution
##############################################################################

# Check if running with required parameters
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    main "$@"
fi