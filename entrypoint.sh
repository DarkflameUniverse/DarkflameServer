#!/usr/bin/env bash
set -e

mkdir -p /app/configs/
cp -n /app/default-configs/* /app/configs/
mkdir -p /app/vanity/
cp -n /app/default-vanity/* /app/vanity/
cd /app/ && ./MasterServer
