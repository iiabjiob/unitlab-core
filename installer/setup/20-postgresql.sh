#!/bin/bash
set -e

echo "🐘 Installing PostgreSQL..."
sudo apt install postgresql postgresql-contrib -y
sudo systemctl enable postgresql
sudo systemctl start postgresql

# === Настройки по умолчанию ===
DB_NAME="unitlab_db"
DB_USER="admin"
DB_PASS="Welcome!1"

echo "📚 Creating user '$DB_USER' and database '$DB_NAME'..."

# Создание пользователя (если не существует)
sudo -u postgres psql -tc "SELECT 1 FROM pg_roles WHERE rolname = '$DB_USER'" | grep -q 1 || \
sudo -u postgres psql -c "CREATE USER $DB_USER WITH PASSWORD '$DB_PASS';"

# Создание базы данных (если не существует)
sudo -u postgres psql -tc "SELECT 1 FROM pg_database WHERE datname = '$DB_NAME'" | grep -q 1 || \
sudo -u postgres psql -c "CREATE DATABASE $DB_NAME OWNER $DB_USER;"

# Выдача прав и опций
sudo -u postgres psql -c "GRANT ALL PRIVILEGES ON DATABASE $DB_NAME TO $DB_USER;"
sudo -u postgres psql -c "ALTER USER $DB_USER CREATEDB;"

echo "✅ Database and user setup complete."
