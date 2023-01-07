#!/bin/bash

function symlink_config_files() {
    echo "Creating symlinks for config files"
    rm /app/*.ini
    ln -s /shared_configs/configs/authconfig.ini /app/authconfig.ini
    ln -s /shared_configs/configs/chatconfig.ini /app/chatconfig.ini
    ln -s /shared_configs/configs/masterconfig.ini /app/masterconfig.ini
    ln -s /shared_configs/configs/worldconfig.ini /app/worldconfig.ini
    ln -s /shared_configs/configs/sharedconfig.ini /app/sharedconfig.ini
}

if [[ ! -f "/app/initialized" ]]; then
    # setup symlinks for volume files
    symlink_config_files
    # do not run symlinks more than once
    touch /app/initialized
else
    echo "Server already initialized"
fi

# start the server
echo "Starting MasterServer"
./MasterServer
tail -f /dev/null
