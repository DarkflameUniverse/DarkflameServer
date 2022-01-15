#!/bin/bash

function symlink_client_files() {
    echo "Creating symlinks for client files"
    ln -s /client/client/res/macros/ /app/res/macros
    ln -s /client/client/res/BrickModels/ /app/res/BrickModels
    ln -s /client/client/res/chatplus_en_us.txt /app/res/chatplus_en_us.txt
    ln -s /client/client/res/names/ /app/res/names
    ln -s /client/client/res/CDServer.sqlite /app/res/CDServer.sqlite
    ln -s /client/client/locale/locale.xml /app/locale/locale.xml
    # need to iterate over entries in maps due to maps already being a directory with navmeshes/ in it
    (
        cd /client/client/res/maps
        readarray -d '' entries < <(printf '%s\0' * | sort -zV)
        for entry in "${entries[@]}"; do
            ln -s /client/client/res/maps/$entry /app/res/maps/
        done
    )
}

function symlink_config_files() {
    echo "Creating symlinks for config files"
    rm /app/*.ini
    ln -s /shared_configs/configs/authconfig.ini /app/authconfig.ini
    ln -s /shared_configs/configs/chatconfig.ini /app/chatconfig.ini
    ln -s /shared_configs/configs/masterconfig.ini /app/masterconfig.ini
    ln -s /shared_configs/configs/worldconfig.ini /app/worldconfig.ini
}

# check to make sure the setup has completed
while [ ! -f "/client/extracted" ] || [ ! -f "/client/migrated" ]; do
    echo "Client setup not finished. Waiting for setup container to complete..."
    sleep 5
done

if [[ ! -f "/app/initialized" ]]; then
    # setup symlinks for volume files
    symlink_client_files
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