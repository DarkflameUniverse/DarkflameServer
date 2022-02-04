#!/bin/bash

function symlink_client_files() {
    echo "Creating symlinks for client files"
    ln -s $CLIENT_ROOT/res/macros/ /app/res/macros
    ln -s $CLIENT_ROOT/res/BrickModels/ /app/res/BrickModels
    ln -s $CLIENT_ROOT/res/chatplus_en_us.txt /app/res/chatplus_en_us.txt
    ln -s $CLIENT_ROOT/res/names/ /app/res/names
    ln -s $CLIENT_ROOT/res/CDServer.sqlite /app/res/CDServer.sqlite
    ln -s $CLIENT_ROOT/locale/locale.xml /app/locale/locale.xml
    # need to iterate over entries in maps due to maps already being a directory with navmeshes/ in it
    (
        cd $CLIENT_ROOT/res/maps
        readarray -d '' entries < <(printf '%s\0' * | sort -zV)
        for entry in "${entries[@]}"; do
            ln -s $CLIENT_ROOT/res/maps/$entry /app/res/maps/
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
while [ ! -f "/shared_configs/extracted" ] || [ ! -f "/shared_configs/migrated" ] || [ ! -f "/shared_configs/root_dir" ]; do
    echo "Client setup not finished. Waiting for setup container to complete..."
    sleep 5
    if [ -f /shared_configs/error ]; then
        echo "[ERROR] It looks like the setup container exited with an error"
        echo "[ERROR] Look at the logs of the setup container to find out what happened"
        exit 1
    fi
done

CLIENT_ROOT=`cat /shared_configs/root_dir`

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
echo "MasterServer exited with $?"
tail -f /dev/null