#!/bin/bash

# fail on first error
set -e

function update_ini() {
    FILE="/docker/configs/$1"
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
    echo "Copying and updating config files"

    mkdir -p /docker/configs
    cp resources/masterconfig.ini /docker/configs/
    cp resources/authconfig.ini /docker/configs/
    cp resources/chatconfig.ini /docker/configs/
    cp resources/worldconfig.ini /docker/configs/

    update_ini worldconfig.ini chat_server_port $CHAT_SERVER_PORT
    update_ini worldconfig.ini max_clients $MAX_CLIENTS

    # always use the internal docker hostname
    update_ini masterconfig.ini master_ip "darkflame"

    update_database_ini_values_for masterconfig.ini
    update_database_ini_values_for authconfig.ini
    update_database_ini_values_for chatconfig.ini
    update_database_ini_values_for worldconfig.ini
}

function fdb_to_sqlite() {
    echo "Run fdb_to_sqlite"
    python3 utils/fdb_to_sqlite.py /client/client/res/cdclient.fdb --sqlite_path /client/client/res/CDServer.sqlite

    (
        cd migrations/cdserver
        readarray -d '' entries < <(printf '%s\0' *.sql | sort -zV)
        for entry in "${entries[@]}"; do
            echo "Execute $entry"
            sqlite3 /client/client/res/CDServer.sqlite < $entry
        done
    )
}

update_ini_values

if [[ ! -d "/client" ]]; then
    echo "Client not found."
    echo "Did you forget to mount the client into the \"/client\" directory?"
    exit 1
fi

if [[ ! -f "/client/extracted" ]]; then
    echo "Start client resource extraction"

    touch globs.txt

    echo "client/res/macros/**" >> globs.txt
    echo "client/res/BrickModels/**" >> globs.txt
    echo "client/res/maps/**" >> globs.txt
    echo "*.fdb" >> globs.txt

    lunpack -g ./globs.txt /client/

    touch /client/extracted
else
    echo "Client already extracted. Skip this step..."
    echo "If you want to force a re-extract, just delete the file called \"extracted\" in the client directory"
fi

if [[ ! -f "/client/migrated" ]]; then
    echo "Start client db migration"

    fdb_to_sqlite

    touch /client/migrated
else
    echo "Client db already migrated. Skip this step..."
    echo "If you want to force a re-migrate, just delete the file called \"migrated\" in the client directory"
fi
