#!/bin/bash
set -e

echo "📦 Starting full installation..."

for script in ./setup/*.sh; do
  echo -e "\n▶️ Executing $script..."
  bash "$script"
done

echo -e "\n✅ System setup complete!"
