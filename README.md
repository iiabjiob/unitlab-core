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

Проверка IP:

```bash
hostname -I
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

### 🛠️ Настройка Chrony для локальной сети

### 🛰️ Добавление локального NTP-сервера в chrony

Чтобы использовать локальный сервер времени (например, `192.168.10.1`), создайте отдельный файл в каталоге `/etc/chrony/sources.d/`.

> 📁 В этот каталог можно добавлять только источники времени: `server`, `pool`, `peer`.

```bash
echo 'server 192.168.10.1 iburst prefer' | sudo tee /etc/chrony/sources.d/unitlab.sources
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
mkdir -p unitlab/{backend,frontend}
touch unitlab/backend/.env
```

### 🔄 Перенос файлов проекта

Перенесите файлы backend и frontend в папку проекта через rsync:

```bash
rsync -avz --delete --exclude-from=.rsync-exclude ./ pi@unitlab.local:~/unitlab/

```

### 📦 Создание виртуального окружения и установка зависимостей

Перейдите в папку backend и создайте окружение Python:

```bash
cd unitlab/backend
python3 -m venv .venv
source .venv/bin/activate
pip install --upgrade pip
pip install -r requirements.txt
```

### Запуск в ручном режиме FastAPI

Для продакшин не использовать --reload

```bash
uvicorn app.main:app --host 0.0.0.0 --port 8000 --reload
```

Создайте файл сервиса:

```bash
sudo nano /etc/systemd/system/fastapi.service
```

### 🚀 Создание сервиса для автозапуска FastAPI

Создайте файл сервиса:

```bash
sudo nano /etc/systemd/system/fastapi.service
```

Добавьте конфиг из /config/fastapi.service

Примените изменения и запустите сервис:

```bash
sudo systemctl daemon-reload
sudo systemctl enable fastapi
sudo systemctl start fastapi
```

Проверьте статус работы:

```bash
sudo systemctl status fastapi
```

### 🛠️ Настройка Nginx для работы с FastAPI

Создайте конфигурационный файл:

```bash
sudo mkdir -p /etc/unitlab
sudo nano /etc/unitlab/nginx.conf
```

Скопируйте и вставьте конфигурацию из /config/nginx_prod.conf

Удалите стандартный конфиг и включите созданный:

```bash
sudo rm /etc/nginx/sites-enabled/default
sudo ln -s /etc/unitlab/nginx.conf /etc/nginx/sites-enabled/unitlab.conf
sudo chmod o+x /home/pi /home/pi/unitlab /home/pi/unitlab/frontend
sudo nginx -t
sudo systemctl restart nginx
```

---

## ✅ Проверка состояния всех сервисов

```bash
sudo systemctl status nginx postgresql chrony fastapi
```

Вы должны увидеть статус **active (running)** у каждой службы.

Варианты команд для логов:

```bash
journalctl -u fastapi -f
journalctl -u fastapi -f -n 50 --no-pager

```
---

✅ **Готово!** Компоненты успешно установлены и настроены на Raspberry Pi 5.
