#!/bin/bash
set -e

echo "📦 Starting full installation..."

for script in ./setup/*.sh; do
  echo -e "\n▶️ Executing $script..."
  
  if [[ "$(basename "$script")" == "70-wifi-hotspot.sh" ]]; then
    sudo bash "$script"
  else
    bash "$script"
  fi
done

echo -e "\n✅ System setup complete!"
