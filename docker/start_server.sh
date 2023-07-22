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
	touch /app/darkflame_1.1.1
else
    echo "Server already initialized"
fi

if [[ ! -f "/app/darkflame_1.1.1" ]]; then
    # setup symlinks for volume files
    echo "Adding default client_net_version 171022 sharedconfig.ini"
	echo "\nclient_net_version=171022" >> /app/sharedconfig.ini
    # do not run symlinks more than once
    touch /app/darkflame_1.1.1
else
    echo "darkflame_1.1.1 patch already applied"
fi

# start the server
echo "Starting MasterServer"
./MasterServer
tail -f /dev/null
