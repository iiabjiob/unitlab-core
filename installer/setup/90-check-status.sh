#!/bin/bash

echo "📋 Checking status of key services..."

for svc in nginx postgresql chrony fastapi; do
  echo -e "\n🔍 Status: $svc"
  systemctl status "$svc" --no-pager || true
done
