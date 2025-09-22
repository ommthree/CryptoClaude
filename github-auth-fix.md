# GitHub Authentication Quick Fix

## The Error You're Seeing
```
remote: {"auth_status":"auth_error","body":"Invalid username or token. Password authentication is not supported for Git operations."}
fatal: Authentication failed for 'https://github.com/ommthree/CryptoClaude.git/'
```

This happens because GitHub no longer accepts password authentication for Git operations.

---

## **🚀 Quick Fix: Personal Access Token**

### Step 1: Create Personal Access Token
1. Go to [GitHub Personal Access Tokens](https://github.com/settings/tokens)
2. Click "Generate new token" → "Generate new token (classic)"
3. **Token description**: "CryptoClaude Development"
4. **Scopes**: Select `repo` (Full control of private repositories)
5. Click "Generate token"
6. **COPY THE TOKEN** (you won't see it again!)

### Step 2: Use Token for Authentication
```bash
# Try pushing again
git push -u origin master

# When prompted:
# Username: ommthree
# Password: [paste your token here, NOT your GitHub password]
```

---

## **🔐 Better Long-term Solution: SSH Keys**

### Generate SSH Key
```bash
# Generate new SSH key
ssh-keygen -t ed25519 -C "your_email@github.com"
# Press Enter for all prompts (use defaults)

# Start SSH agent
eval "$(ssh-agent -s)"

# Add key to agent
ssh-add ~/.ssh/id_ed25519

# Copy public key to clipboard
cat ~/.ssh/id_ed25519.pub
# Copy this output
```

### Add to GitHub
1. Go to [GitHub SSH Settings](https://github.com/settings/keys)
2. Click "New SSH key"
3. Title: "MacBook Air - CryptoClaude"
4. Paste the key content
5. Click "Add SSH key"

### Update Repository URL
```bash
# Change from HTTPS to SSH
git remote set-url origin git@github.com:ommthree/CryptoClaude.git

# Test connection
ssh -T git@github.com

# Push should now work without password prompts
git push -u origin master
```

---

## **🎯 GitKraken Method (Your Preferred Tool)**

If you want to use GitKraken exclusively:

1. **Open GitKraken**
2. **Authentication**:
   - File → Preferences → Authentication
   - Sign in to GitHub.com (this handles tokens automatically)
3. **Open Repository**:
   - File → Open Repo → Navigate to `/Users/Owen/CryptoClaude`
4. **Push**:
   - Click "Push" in toolbar
   - GitKraken handles authentication automatically

---

## **Troubleshooting**

### If SSH still doesn't work:
```bash
# Test SSH connection
ssh -T git@github.com
# Should show: "Hi ommthree! You've successfully authenticated..."
```

### If PAT doesn't work:
- Make sure you copied the token correctly
- Check that `repo` scope is selected when creating PAT
- Token should be pasted as password, not username

### GitKraken Issues:
- Make sure you're signed in to GitHub in GitKraken
- Try disconnecting and reconnecting GitHub in Preferences
- Use "Clone" instead of "Open" if issues persist

---

## **Security Notes**

- **Never commit your PAT** to the repository
- **SSH keys are more secure** than PATs for long-term use
- **GitKraken handles authentication** securely in the background

Choose whichever method you're most comfortable with!