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

function update_ini() {
    FILE="/app/$1"
    KEY=$2
    NEW_VALUE=$3
    sed -i "/^$KEY=/s/=.*/=$NEW_VALUE/" $FILE
}

function update_database_ini_values_for() {
    INI_FILE=$1

    update_ini $INI_FILE mysql_host $DATABASE_HOST
    update_ini $INI_FILE mysql_database $DATABASE
    update_ini $INI_FILE mysql_username $DATABASE_USER
    update_ini $INI_FILE mysql_password $DATABASE_PASSWORD
    if [[ "$INI_FILE" != "worldconfig.ini" ]]; then
        update_ini $INI_FILE external_ip $EXTERNAL_IP
    fi
}

function update_ini_values() {
    echo "Updating config files"

    update_ini worldconfig.ini chat_server_port $CHAT_SERVER_PORT
    update_ini worldconfig.ini max_clients $MAX_CLIENTS

    # always use the internal docker hostname
    update_ini masterconfig.ini master_ip "darkflame"

    update_database_ini_values_for masterconfig.ini
    update_database_ini_values_for authconfig.ini
    update_database_ini_values_for chatconfig.ini
    update_database_ini_values_for worldconfig.ini
}

update_ini_values

# check to make sure the setup has completed
while [ ! -f "/client/extracted" ] || [ ! -f "/client/migrated" ]; do
    echo "Client setup not finished. Waiting for setup container to complete..."
    sleep 5
done

if [[ ! -f "/app/initialized" ]]; then
    # setup symlinks for volume files
    symlink_client_files
    # do not run symlinks more than once
    touch /app/initialized
else
    echo "Server already initialized"
fi

# start the server
echo "Starting MasterServer"
./MasterServer
tail -f /dev/null
