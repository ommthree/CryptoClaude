#!/bin/bash

##############################################################################
# CryptoClaude Lightsail Launch Script
# Executed automatically when the instance starts
#
# This script installs all dependencies, sets up the application,
# and starts the CryptoClaude console service
##############################################################################

set -e

# Logging setup
exec > >(tee /var/log/cryptoclaude-setup.log)
exec 2>&1

log() {
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] $1"
}

log "🚀 Starting CryptoClaude production setup..."

##############################################################################
# System Updates and Dependencies
##############################################################################

log "📦 Updating system packages..."
apt-get update -y
apt-get upgrade -y

log "🔧 Installing essential build tools..."
apt-get install -y \
    build-essential \
    cmake \
    git \
    curl \
    wget \
    unzip \
    software-properties-common \
    apt-transport-https \
    ca-certificates \
    gnupg \
    lsb-release \
    jq \
    htop \
    nginx \
    supervisor \
    sqlite3 \
    libsqlite3-dev \
    libssl-dev \
    libcurl4-openssl-dev \
    pkg-config

##############################################################################
# Install Modern C++ Compiler
##############################################################################

log "🔨 Installing modern C++ development tools..."

# Install GCC 11 for C++17/C++20 support
apt-get install -y gcc-11 g++-11
update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-11 100
update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-11 100

##############################################################################
# Install Node.js and npm (for potential web interface)
##############################################################################

log "🟢 Installing Node.js..."
curl -fsSL https://deb.nodesource.com/setup_18.x | sudo -E bash -
apt-get install -y nodejs

##############################################################################
# MySQL Client Installation
##############################################################################

log "🗄️  Installing MySQL client..."
apt-get install -y mysql-client-core-8.0

##############################################################################
# Create Application User and Directories
##############################################################################

log "👤 Creating application user and directories..."

# Create cryptoclaude user
useradd -m -s /bin/bash cryptoclaude
usermod -aG sudo cryptoclaude

# Create application directories
mkdir -p /opt/cryptoclaude/{bin,config,data,logs}
mkdir -p /var/log/cryptoclaude
chown -R cryptoclaude:cryptoclaude /opt/cryptoclaude
chown -R cryptoclaude:cryptoclaude /var/log/cryptoclaude

##############################################################################
# Download and Build CryptoClaude
##############################################################################

log "📥 Downloading CryptoClaude source code..."

# Change to cryptoclaude user
sudo -u cryptoclaude bash << 'EOF'
cd /opt/cryptoclaude

# Clone the repository (assuming it will be available via git)
# For now, we'll prepare the directory structure
mkdir -p src/Core/{Console,Database,Trading,Portfolio,Risk,DataPipeline,AI,Backtesting,Config}
mkdir -p src/Core/Console/Commands

# Create placeholder for source code deployment
echo "Source code will be deployed here" > src/README.md

log "📁 Application directory structure created"
EOF

##############################################################################
# Configure Application Environment
##############################################################################

log "⚙️  Configuring application environment..."

# Create environment configuration
cat > /opt/cryptoclaude/config/production.env << EOF
# CryptoClaude Production Configuration
NODE_ENV=production
APP_ENV=production
LOG_LEVEL=info

# Database Configuration
DATABASE_URL=sqlite:///opt/cryptoclaude/data/cryptoclaude.db
DATABASE_POOL_SIZE=10

# API Configuration
API_PORT=3000
API_HOST=0.0.0.0

# External API Keys (will be set via environment variables)
# CRYPTOCOMPARE_API_KEY=your_key_here
# CLAUDE_API_KEY=your_key_here

# Security
SESSION_SECRET=$(openssl rand -hex 32)
JWT_SECRET=$(openssl rand -hex 64)

# Trading Configuration
DEFAULT_TRADING_MODE=paper
PAPER_TRADING_INITIAL_BALANCE=100000
MAX_POSITION_SIZE=0.1
RISK_MANAGEMENT_ENABLED=true

# Data Pipeline
DATA_CACHE_TTL=300
HISTORICAL_DATA_RETENTION_DAYS=365
AUTO_DATA_REFRESH=true

# AI Configuration
AI_SCORE_ADJUSTMENT_MAX=0.2
AI_MODEL_TEMPERATURE=0.7

# Monitoring
HEALTH_CHECK_INTERVAL=30
METRICS_EXPORT_ENABLED=true
EOF

chown cryptoclaude:cryptoclaude /opt/cryptoclaude/config/production.env

##############################################################################
# Database Setup
##############################################################################

log "🗄️  Setting up application database..."

sudo -u cryptoclaude bash << 'EOF'
cd /opt/cryptoclaude

# Create SQLite database with proper permissions
sqlite3 data/cryptoclaude.db << 'SQL'
-- Initial database schema will be created by the application
-- This just ensures the database file exists
PRAGMA journal_mode=WAL;
PRAGMA synchronous=NORMAL;
PRAGMA cache_size=10000;
PRAGMA temp_store=memory;

-- Create a simple health check table
CREATE TABLE IF NOT EXISTS system_health (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
    status TEXT NOT NULL,
    details TEXT
);

INSERT INTO system_health (status, details) VALUES ('initialized', 'Database created during deployment');
SQL

chmod 644 data/cryptoclaude.db
log "✅ Database initialized"
EOF

##############################################################################
# Create Systemd Service
##############################################################################

log "🔧 Creating systemd service..."

cat > /etc/systemd/system/cryptoclaude.service << EOF
[Unit]
Description=CryptoClaude Trading Platform
Documentation=https://github.com/your-org/cryptoclaude
After=network.target

[Service]
Type=simple
User=cryptoclaude
Group=cryptoclaude
WorkingDirectory=/opt/cryptoclaude
Environment=NODE_ENV=production
EnvironmentFile=/opt/cryptoclaude/config/production.env
ExecStart=/opt/cryptoclaude/bin/cryptoclaude-console
ExecReload=/bin/kill -HUP \$MAINPID
KillMode=mixed
KillSignal=SIGINT
TimeoutStopSec=10
Restart=always
RestartSec=5

# Security settings
NoNewPrivileges=yes
PrivateTmp=yes
ProtectSystem=strict
ProtectHome=yes
ReadWritePaths=/opt/cryptoclaude/data /opt/cryptoclaude/logs /var/log/cryptoclaude

# Logging
StandardOutput=journal
StandardError=journal
SyslogIdentifier=cryptoclaude

[Install]
WantedBy=multi-user.target
EOF

##############################################################################
# Nginx Configuration for Reverse Proxy
##############################################################################

log "🌐 Configuring Nginx reverse proxy..."

cat > /etc/nginx/sites-available/cryptoclaude << EOF
server {
    listen 80;
    server_name _;

    # Security headers
    add_header X-Frame-Options DENY;
    add_header X-Content-Type-Options nosniff;
    add_header X-XSS-Protection "1; mode=block";
    add_header Referrer-Policy "strict-origin-when-cross-origin";

    # Health check endpoint
    location /health {
        access_log off;
        return 200 "healthy\n";
        add_header Content-Type text/plain;
    }

    # API endpoints
    location /api/ {
        proxy_pass http://127.0.0.1:3000;
        proxy_http_version 1.1;
        proxy_set_header Upgrade \$http_upgrade;
        proxy_set_header Connection 'upgrade';
        proxy_set_header Host \$host;
        proxy_set_header X-Real-IP \$remote_addr;
        proxy_set_header X-Forwarded-For \$proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto \$scheme;
        proxy_cache_bypass \$http_upgrade;
    }

    # Console interface (if web-based)
    location / {
        proxy_pass http://127.0.0.1:3000;
        proxy_http_version 1.1;
        proxy_set_header Upgrade \$http_upgrade;
        proxy_set_header Connection 'upgrade';
        proxy_set_header Host \$host;
        proxy_set_header X-Real-IP \$remote_addr;
        proxy_set_header X-Forwarded-For \$proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto \$scheme;
        proxy_cache_bypass \$http_upgrade;
    }
}
EOF

# Enable the site
ln -sf /etc/nginx/sites-available/cryptoclaude /etc/nginx/sites-enabled/
rm -f /etc/nginx/sites-enabled/default

# Test nginx configuration
nginx -t

##############################################################################
# Configure Log Rotation
##############################################################################

log "📋 Setting up log rotation..."

cat > /etc/logrotate.d/cryptoclaude << EOF
/var/log/cryptoclaude/*.log {
    daily
    missingok
    rotate 30
    compress
    delaycompress
    notifempty
    create 644 cryptoclaude cryptoclaude
    postrotate
        systemctl reload cryptoclaude || true
    endscript
}
EOF

##############################################################################
# Create Health Check Script
##############################################################################

log "🏥 Creating health check script..."

cat > /opt/cryptoclaude/bin/health-check.sh << 'EOF'
#!/bin/bash

# CryptoClaude Health Check Script

HEALTH_URL="http://localhost:3000/api/health"
LOG_FILE="/var/log/cryptoclaude/health-check.log"

check_health() {
    timestamp=$(date '+%Y-%m-%d %H:%M:%S')

    if curl -s -o /dev/null -w "%{http_code}" "$HEALTH_URL" | grep -q "200"; then
        echo "[$timestamp] ✅ CryptoClaude is healthy" >> "$LOG_FILE"
        return 0
    else
        echo "[$timestamp] ❌ CryptoClaude health check failed" >> "$LOG_FILE"
        return 1
    fi
}

# Run health check
check_health
EOF

chmod +x /opt/cryptoclaude/bin/health-check.sh
chown cryptoclaude:cryptoclaude /opt/cryptoclaude/bin/health-check.sh

##############################################################################
# Create Cron Jobs for Maintenance
##############################################################################

log "⏰ Setting up cron jobs..."

# Add cron job for health checks
crontab -u cryptoclaude << EOF
# CryptoClaude maintenance jobs
# Health check every 5 minutes
*/5 * * * * /opt/cryptoclaude/bin/health-check.sh

# Daily backup at 2 AM
0 2 * * * cp /opt/cryptoclaude/data/cryptoclaude.db /opt/cryptoclaude/data/backup-\$(date +\%Y\%m\%d).db

# Weekly cleanup of old backups (keep 14 days)
0 3 * * 0 find /opt/cryptoclaude/data/backup-*.db -mtime +14 -delete

# Daily log cleanup
0 4 * * * find /var/log/cryptoclaude -name "*.log" -mtime +7 -delete
EOF

##############################################################################
# Firewall Configuration
##############################################################################

log "🔒 Configuring UFW firewall..."

ufw --force reset
ufw default deny incoming
ufw default allow outgoing

# Allow SSH (port 22)
ufw allow 22/tcp

# Allow HTTP (port 80)
ufw allow 80/tcp

# Allow HTTPS (port 443)
ufw allow 443/tcp

# Allow application port (port 3000) - can be removed once nginx proxy is working
ufw allow 3000/tcp

# Enable firewall
ufw --force enable

##############################################################################
# Final System Configuration
##############################################################################

log "🔧 Final system configuration..."

# Enable and start services
systemctl daemon-reload
systemctl enable nginx
systemctl start nginx

# Note: CryptoClaude service will be started after the binary is deployed

# Set up swap file for better memory management
fallocate -l 2G /swapfile
chmod 600 /swapfile
mkswap /swapfile
swapon /swapfile
echo '/swapfile none swap sw 0 0' >> /etc/fstab

# Optimize system for trading application
cat >> /etc/sysctl.conf << EOF
# CryptoClaude optimizations
vm.swappiness=10
net.core.rmem_max=134217728
net.core.wmem_max=134217728
net.ipv4.tcp_rmem=4096 87380 134217728
net.ipv4.tcp_wmem=4096 65536 134217728
EOF

sysctl -p

##############################################################################
# Create Deployment Status File
##############################################################################

log "📝 Creating deployment status file..."

cat > /opt/cryptoclaude/deployment-status.json << EOF
{
    "deployment_time": "$(date -u +"%Y-%m-%dT%H:%M:%SZ")",
    "status": "infrastructure_ready",
    "version": "1.0.0",
    "environment": "production",
    "next_steps": [
        "Deploy application binary",
        "Configure environment variables",
        "Start CryptoClaude service",
        "Verify health checks"
    ],
    "services": {
        "nginx": "configured",
        "ufw": "enabled",
        "database": "initialized",
        "logging": "configured",
        "monitoring": "ready"
    }
}
EOF

chown cryptoclaude:cryptoclaude /opt/cryptoclaude/deployment-status.json

##############################################################################
# Installation Complete
##############################################################################

log "🎉 CryptoClaude infrastructure setup complete!"
log "📊 Installation Summary:"
log "  ✅ System packages updated and dependencies installed"
log "  ✅ Application user and directories created"
log "  ✅ Database initialized (SQLite)"
log "  ✅ Nginx reverse proxy configured"
log "  ✅ Systemd service configured"
log "  ✅ Firewall rules applied"
log "  ✅ Health check and monitoring setup"
log "  ✅ Log rotation configured"
log "  ✅ Cron jobs for maintenance"
log ""
log "🔄 Next: Deploy application binary and start services"
log "📋 Status: $(cat /opt/cryptoclaude/deployment-status.json | jq -r '.status')"
log ""
log "🎯 Ready for application deployment!"

# Create a flag file to indicate setup completion
touch /opt/cryptoclaude/infrastructure-ready
chown cryptoclaude:cryptoclaude /opt/cryptoclaude/infrastructure-ready

log "✨ Launch script execution completed successfully!"