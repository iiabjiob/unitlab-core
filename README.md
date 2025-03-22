# 📌 Инструкция по установке компонентов на Raspberry Pi 5

Данный документ содержит команды и базовую настройку для установки следующих компонентов:

- **Nginx** (веб-сервер)
- **PostgreSQL** (база данных)
- **Chrony** (синхронизация времени по NTP)
- **FastAPI** (Python-приложение)

Тестировалось на: `Raspberry Pi OS 64-bit Lite`

---

## 🚀 Подготовка

Перед началом установки рекомендуется обновить систему:

```bash
sudo apt update && sudo apt upgrade -y
```

---

## 🔧 Установка Nginx

Nginx — быстрый и легковесный веб-сервер.

```bash
sudo apt install nginx -y
```

Проверка состояния службы:

```bash
sudo systemctl status nginx
```

Добавление в автозагрузку:

```bash
sudo systemctl enable nginx
```

После установки откройте в браузере адрес устройства:

```
http://<IP-адрес вашего устройства>
```

Вы должны увидеть страницу приветствия Nginx.

---

## 🐘 Установка PostgreSQL

PostgreSQL — мощная и надёжная система управления базами данных.

```bash
sudo apt install postgresql postgresql-contrib -y
```

Проверка состояния PostgreSQL:

```bash
sudo systemctl status postgresql
```

Включение автозагрузки:

```bash
sudo systemctl enable postgresql
```

### 📚 Создание базы данных и пользователя (опционально)

Перейдите в пользователя postgres:

```bash
sudo -i -u postgres
```

Запустите оболочку PostgreSQL:

```bash
psql
```

Создайте базу данных и нового пользователя:

```sql
CREATE DATABASE mydb;
CREATE USER myuser WITH PASSWORD 'mypassword';
GRANT ALL PRIVILEGES ON DATABASE mydb TO myuser;
ALTER USER myuser CREATEDB;
\q
```

Вернитесь в основной терминал:

```bash
exit
```

---

## ⏱️ Установка и настройка Chrony

Chrony — эффективный и точный NTP-клиент для синхронизации времени.

```bash
sudo apt install chrony -y
```

Проверка состояния службы:

```bash
sudo systemctl status chrony
```

Включение автозагрузки Chrony:

```bash
sudo systemctl enable chrony
```

Проверка текущей синхронизации:

```bash
chronyc tracking
```

### 🛠️ Настройка Chrony для локальной сети (опционально)

Если вы используете локальный сервер NTP, отредактируйте конфигурацию:

```bash
sudo nano /etc/chrony/chrony.conf
```

Добавьте адрес локального сервера NTP:

```
server 192.168.1.1 iburst prefer
```

Перезапуск Chrony для применения настроек:

```bash
sudo systemctl restart chrony
```

Проверка источников синхронизации:

```bash
chronyc sources -v
```

---

## 🐍 Настройка и запуск FastAPI

Создайте структуру проекта:

```bash
mkdir -p fat-simulator/{backend,frontend}
touch fat-simulator/backend/.env
```

### 🔄 Перенос файлов проекта

Перенесите файлы backend и frontend в папку проекта через rsync:

```bash
rsync -avz --delete --exclude-from=.rsync-exclude backend/ pi@fat-simulator.local:~/fat-simulator/backend/
rsync -avz --delete --exclude-from=.rsync-exclude frontend/ pi@fat-simulator.local:~/fat-simulator/frontend/
```

### 📦 Создание виртуального окружения и установка зависимостей

Перейдите в папку backend и создайте окружение Python:

```bash
cd fat-simulator/backend
python3 -m venv .venv
source .venv/bin/activate
pip install --upgrade pip
pip install -r requirements.txt
```

### 🚀 Создание сервиса для автозапуска FastAPI

Создайте файл сервиса:

```bash
sudo nano /etc/systemd/system/fat-simulator.service
```

Добавьте конфиг из /config/fat-simulator.service

Примените изменения и запустите сервис:

```bash
sudo systemctl daemon-reload
sudo systemctl enable fat-simulator
sudo systemctl start fat-simulator
```

Проверьте статус работы:

```bash
sudo systemctl status fat-simulator
```

### 🛠️ Настройка Nginx для работы с FastAPI

Создайте конфигурационный файл:

```bash
sudo nano /etc/fat-simulator/nginx.conf
```

Скопируйте и вставьте конфигурацию из /config/nginx_prod.conf

Удалите стандартный конфиг и включите созданный:

```bash
sudo rm /etc/nginx/sites-enabled/default
sudo ln -s /etc/fat-simulator/nginx.conf /etc/nginx/sites-enabled/fat-simulator.conf
sudo chmod o+x /home/pi /home/pi/fat-simulator /home/pi/fat-simulator/frontend
sudo nginx -t
sudo systemctl restart nginx
```

---

## ✅ Проверка состояния всех сервисов

```bash
sudo systemctl status nginx postgresql chrony fat-simulator
```

Вы должны увидеть статус **active (running)** у каждой службы.

---

✅ **Готово!** Компоненты успешно установлены и настроены на Raspberry Pi 5.
