#!/bin/bash
# ==============================================
# Visual Homing - Local Interface Server
# Локальний сервер з повним інтерфейсом
# ==============================================

echo "╔══════════════════════════════════════════════╗"
echo "║   Visual Homing - Local Interface Setup      ║"
echo "╚══════════════════════════════════════════════╝"
echo ""

# Check if running on Pi
if [[ $(uname -m) == "aarch64" ]] || [[ $(uname -m) == "armv7l" ]]; then
    PI_MODE=true
    echo "✓ Running on Raspberry Pi"
else
    PI_MODE=false
    echo "✓ Running on PC/Server"
fi

# Check Node.js
if ! command -v node &> /dev/null; then
    echo ""
    echo "⚠ Node.js not found. Installing..."
    
    if [ "$PI_MODE" = true ]; then
        # Pi: Install via apt
        curl -fsSL https://deb.nodesource.com/setup_20.x | sudo -E bash -
        sudo apt-get install -y nodejs
    else
        echo "Please install Node.js 20 LTS from https://nodejs.org/"
        exit 1
    fi
fi

echo "✓ Node.js $(node -v)"
echo "✓ npm $(npm -v)"
echo ""

# Create local interface directory
INTERFACE_DIR="$HOME/visual_homing_interface"
mkdir -p "$INTERFACE_DIR"
cd "$INTERFACE_DIR"

# Download frontend if not exists
if [ ! -f "package.json" ]; then
    echo "📥 Downloading interface..."
    
    # Download from preview URL
    wget https://github.com/iigar/Visual_Homing_System_Claude/archive/refs/heads/main.zip -O visual_homing.zip
    unzip -o visual_homing.zip
    
    # Copy frontend if exists
    if [ -d "frontend" ]; then
        cp -r frontend/* .
        rm -rf frontend
    fi
    rm -f visual_homing.zip
    
    echo "✓ Interface downloaded"
fi

# Create .env file
echo "REACT_APP_BACKEND_URL=http://localhost:5000" > .env
echo "REACT_APP_PI_URL=http://visual-homing.local:5000" >> .env

# Install dependencies
echo ""
echo "📦 Installing dependencies..."
npm install --legacy-peer-deps 2>/dev/null || npm install --force

# Create local backend proxy
cat > local_proxy.js << 'PROXY_EOF'
const http = require('http');
const httpProxy = require('http-proxy');
const express = require('express');
const cors = require('cors');
const path = require('path');

const app = express();
app.use(cors());

// Serve static files (React build)
app.use(express.static(path.join(__dirname, 'build')));

// Proxy API requests to Pi
const PI_URL = process.env.PI_URL || 'http://visual-homing.local:5000';
const proxy = httpProxy.createProxyServer({});

app.all('/api/*', (req, res) => {
    proxy.web(req, res, { target: PI_URL }, (err) => {
        console.error('Proxy error:', err.message);
        res.status(502).json({ error: 'Pi not reachable', details: err.message });
    });
});

// Serve React app for all other routes
app.get('*', (req, res) => {
    res.sendFile(path.join(__dirname, 'build', 'index.html'));
});

const PORT = process.env.PORT || 3000;
app.listen(PORT, () => {
    console.log(`\n╔══════════════════════════════════════════════╗`);
    console.log(`║   Visual Homing Local Interface              ║`);
    console.log(`╠══════════════════════════════════════════════╣`);
    console.log(`║   Local:  http://localhost:${PORT}              ║`);
    console.log(`║   Pi:     ${PI_URL}        ║`);
    console.log(`╚══════════════════════════════════════════════╝\n`);
});
PROXY_EOF

# Install proxy dependencies
npm install http-proxy express cors --save 2>/dev/null

echo ""
echo "╔══════════════════════════════════════════════╗"
echo "║   Setup Complete!                            ║"
echo "╠══════════════════════════════════════════════╣"
echo "║                                              ║"
echo "║   To start development server:               ║"
echo "║   npm start                                  ║"
echo "║                                              ║"
echo "║   To build & run production:                 ║"
echo "║   npm run build                              ║"
echo "║   node local_proxy.js                        ║"
echo "║                                              ║"
echo "╚══════════════════════════════════════════════╝"
