#!/bin/bash
set -e

echo "📦 Starting full installation..."

INSTALLER_DIR="$(pwd)"

# Выполняем setup-скрипты в нужном порядке
for script in "$INSTALLER_DIR"/setup/[0-9][0-9]-*.sh; do
  echo -e "\n▶️ Executing $script..."
  bash "$script"
done

# После завершения всех — удаляем installer
cd ~
rm -rf "$INSTALLER_DIR"

echo -e "\n✅ System setup complete and installer removed."
