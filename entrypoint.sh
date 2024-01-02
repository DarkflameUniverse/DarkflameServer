#!/usr/bin/env bash
mkdir -p /app/configs/
cp -n /app/default-configs/* /app/configs/
mkdir -p /app/vanity/
cp -n /app/default-vanity/* /app/vanity/
cd /app/ && ./MasterServer
