# CryptoClaude AWS Lightsail Deployment

## 🚀 Automated Production Deployment

This directory contains the complete automated deployment system for CryptoClaude on AWS Lightsail.

## 📋 Prerequisites

### Required API Keys
Before deployment, ensure you have:

1. **AWS CLI Configured**
   ```bash
   aws configure
   # Enter your AWS Access Key ID, Secret Access Key, and Region
   ```

2. **CryptoCompare API Key**
   ```bash
   export CRYPTOCOMPARE_API_KEY="your_cryptocompare_api_key_here"
   ```

3. **Claude API Key**
   ```bash
   export CLAUDE_API_KEY="your_claude_api_key_here"
   ```

### Optional Configuration
```bash
export DOMAIN_NAME="your-domain.com"  # Optional custom domain
export ALERT_EMAIL="admin@your-domain.com"  # Optional monitoring alerts
```

## 🎯 Deployment Process

### Step 1: Infrastructure Deployment
```bash
chmod +x deploy/aws-lightsail-deploy.sh
./deploy/aws-lightsail-deploy.sh
```

**What this does:**
- ✅ Creates AWS Lightsail instance (4GB RAM, 2 vCPUs)
- ✅ Sets up managed MySQL database
- ✅ Allocates static IP address
- ✅ Configures SSL certificate (if domain provided)
- ✅ Sets up load balancer
- ✅ Enables automated backups
- ✅ Configures monitoring

**Duration:** ~5-10 minutes

### Step 2: Application Deployment
```bash
chmod +x deploy/deploy-application.sh
./deploy/deploy-application.sh
```

**What this does:**
- ✅ Builds CryptoClaude application locally
- ✅ Deploys to Lightsail instance
- ✅ Configures environment variables
- ✅ Starts all services
- ✅ Verifies deployment

**Duration:** ~3-5 minutes

## 📊 Deployment Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    AWS Lightsail Instance                   │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────────────┐  ┌─────────────────┐  ┌──────────────┐ │
│  │      Nginx      │  │  CryptoClaude   │  │    SQLite    │ │
│  │  Reverse Proxy  │  │   Application   │  │   Database   │ │
│  │   Port 80/443   │  │    Port 3000    │  │              │ │
│  └─────────────────┘  └─────────────────┘  └──────────────┘ │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────────────┐  ┌─────────────────┐  ┌──────────────┐ │
│  │   Monitoring    │  │  Health Checks  │  │   Backups    │ │
│  │   & Logging     │  │   & Alerts      │  │ & Snapshots  │ │
│  └─────────────────┘  └─────────────────┘  └──────────────┘ │
└─────────────────────────────────────────────────────────────┘
                              │
                    ┌─────────┴─────────┐
                    │   Static IP &     │
                    │  Load Balancer    │
                    └───────────────────┘
```

## 🔧 Manual Configuration (If Needed)

### Set API Keys on Instance
If environment variables weren't set during deployment:

```bash
# SSH into instance
ssh -i ~/.ssh/lightsail-key.pem ubuntu@YOUR_INSTANCE_IP

# Add API keys
sudo -u cryptoclaude bash
cat >> /opt/cryptoclaude/config/production.env << EOF
CRYPTOCOMPARE_API_KEY=your_key_here
CLAUDE_API_KEY=your_key_here
EOF

# Restart service
sudo systemctl restart cryptoclaude
```

### Check Service Status
```bash
# Check all services
sudo systemctl status cryptoclaude nginx

# View logs
sudo journalctl -u cryptoclaude -f

# Health check
curl http://localhost/health
```

## 📋 Post-Deployment Checklist

### ✅ Verification Steps
1. **SSH Access**: `ssh -i ~/.ssh/lightsail-key.pem ubuntu@INSTANCE_IP`
2. **Service Status**: `sudo systemctl status cryptoclaude`
3. **Health Check**: `curl http://INSTANCE_IP/health`
4. **Console Access**: Connect to application console
5. **Paper Trading**: Verify paper trading mode is active
6. **API Connectivity**: Test CryptoCompare and Claude API connections

### ⚙️ Configuration Verification
- [ ] API keys configured and working
- [ ] Database initialized with schema
- [ ] Console commands responsive
- [ ] Monitoring and alerts active
- [ ] Backups scheduled
- [ ] SSL certificate valid (if using domain)

## 🔍 Troubleshooting

### Common Issues

#### Service Won't Start
```bash
# Check service status and logs
sudo systemctl status cryptoclaude
sudo journalctl -u cryptoclaude --no-pager -l

# Check configuration
cat /opt/cryptoclaude/config/production.env
```

#### Health Check Fails
```bash
# Check nginx status
sudo systemctl status nginx
sudo nginx -t

# Test local connectivity
curl -v http://localhost:3000/api/health
```

#### API Key Issues
```bash
# Verify environment variables
sudo -u cryptoclaude printenv | grep API

# Test API connectivity manually
curl "https://min-api.cryptocompare.com/data/price?fsym=BTC&tsyms=USD&api_key=YOUR_KEY"
```

## 📊 Monitoring & Maintenance

### Health Monitoring
- **Automated Health Checks**: Every 5 minutes
- **Service Monitoring**: Systemd with auto-restart
- **Log Rotation**: Daily with 30-day retention
- **Database Backups**: Daily at 2:00 AM

### Log Files
```bash
# Application logs
tail -f /var/log/cryptoclaude/application.log

# System logs
sudo journalctl -u cryptoclaude -f

# Nginx logs
tail -f /var/log/nginx/access.log
tail -f /var/log/nginx/error.log
```

### Performance Monitoring
```bash
# System resources
htop

# Application metrics
curl http://localhost:3000/api/metrics

# Database size
du -sh /opt/cryptoclaude/data/
```

## 🛠️ Advanced Configuration

### Custom Domain Setup
1. Point your domain to the static IP
2. Update SSL certificate configuration
3. Configure DNS records

### Load Balancer Configuration
```bash
# Add additional instances to load balancer
aws lightsail attach-instances-to-load-balancer \
  --load-balancer-name "cryptoclaude-lb" \
  --instance-names "cryptoclaude-prod-2"
```

### Database Migration to RDS
For production scaling, consider migrating to managed RDS:
1. Create RDS instance
2. Migrate data using standard MySQL tools
3. Update application configuration

## 🚨 Security Considerations

### Production Security Checklist
- [ ] SSH key-based authentication only
- [ ] UFW firewall configured and active
- [ ] Regular security updates scheduled
- [ ] API keys stored securely
- [ ] Database access restricted
- [ ] SSL/TLS encryption enabled
- [ ] Log monitoring for security events

## 🎯 Success Metrics

### Day 27 Objectives Met
- ✅ 24/7 AWS Lightsail deployment
- ✅ Secure remote access capability
- ✅ Database backup and recovery
- ✅ API key security management
- ✅ 99.5% uptime monitoring target

### Performance Targets
- **Response Time**: < 2 seconds for all commands
- **Uptime**: > 99.5%
- **API Success Rate**: > 95%
- **Deployment Time**: < 30 minutes clean deployment

## 🎉 Next Steps

After successful deployment:

1. **Day 28**: AI-Enhanced Trading Integration
2. **Day 29**: Advanced Backtesting Complete
3. **Day 30**: Production Trading Platform Ready

---

## 📞 Support

For deployment issues or questions:
1. Check troubleshooting section above
2. Review AWS Lightsail console for instance status
3. Examine application logs for specific errors
4. Verify all prerequisites are met

**🎊 Ready for Production Trading!**