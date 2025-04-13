#!/bin/bash
set -e

echo "⏱️ Installing Chrony..."
sudo apt install chrony -y
sudo systemctl enable chrony
sudo systemctl start chrony

echo "📡 Configuring chrony local source..."
echo 'server 192.168.10.1 iburst prefer' | sudo tee /etc/chrony/sources.d/unitlab.sources
sudo systemctl restart chrony
