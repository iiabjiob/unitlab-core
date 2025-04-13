#!/bin/bash
set -e

echo "🔧 Installing Nginx..."
sudo apt install nginx -y
sudo systemctl enable nginx
sudo systemctl start nginx
