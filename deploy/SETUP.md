# CryptoClaude Deployment Setup Guide

## 🔐 Security-First API Key Configuration

### Step 1: Create Your API Keys Configuration

1. **Copy the template file:**
   ```bash
   cp deploy/api-keys.conf.template deploy/api-keys.conf
   ```

2. **Edit the configuration file:**
   ```bash
   nano deploy/api-keys.conf
   # or use your preferred editor
   ```

3. **Add your actual API keys:**
   Replace the placeholder values with your real API keys from each provider:

   - **CryptoCompare**: Get from https://cryptocompare.com/
   - **Binance**: Get from https://binance.com/
   - **Alpha Vantage**: Get from https://alphavantage.co/
   - **News API**: Get from https://newsapi.org/
   - **CryptoNews API**: Get from https://cryptonews-api.com/
   - **Claude API**: Get from https://console.anthropic.com/

### Step 2: Verify Configuration

Test that your API keys configuration loads correctly:

```bash
cd deploy
source api-keys.conf
echo "Keys configured: $([ -n "$CRYPTOCOMPARE_API_KEY" ] && echo "✅" || echo "❌") CryptoCompare"
```

### Step 3: Deploy to AWS

Run the deployment scripts in order:

```bash
# 1. Infrastructure setup
./deploy/aws-lightsail-deploy.sh

# 2. Application deployment
./deploy/deploy-application.sh
```

Or use the quick redeployment script:

```bash
# For existing infrastructure
./deploy/redeploy-quick.sh
```

## 🛡️ Security Features

- ✅ **No secrets in git**: API keys are never committed to version control
- ✅ **Template system**: Safe templates provide guidance without exposing keys
- ✅ **Automatic validation**: Scripts check for missing or placeholder keys
- ✅ **Environment fallback**: Can use environment variables as backup

## 📋 File Structure

```
deploy/
├── api-keys.conf.template    # Safe template (committed to git)
├── api-keys.conf            # Your actual keys (gitignored)
├── aws-lightsail-deploy.sh  # Infrastructure deployment
├── deploy-application.sh    # Application deployment
├── redeploy-quick.sh       # Quick redeployment
└── SETUP.md                # This guide
```

## ⚠️ Important Notes

- **Never commit api-keys.conf** - It's automatically ignored by git
- **Rotate keys regularly** - Update your configuration file periodically
- **Backup safely** - Store your keys securely outside of git
- **Use environment variables** - For CI/CD, set keys as environment variables

## 🚀 Quick Start

```bash
# One-time setup
cp deploy/api-keys.conf.template deploy/api-keys.conf
nano deploy/api-keys.conf  # Add your API keys

# Deploy
./deploy/aws-lightsail-deploy.sh
./deploy/deploy-application.sh

# Future updates
./deploy/redeploy-quick.sh
```

Your CryptoClaude platform is now secure and ready for production!