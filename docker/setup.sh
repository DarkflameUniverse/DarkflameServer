#!/bin/bash

# fail on first error
set -e

function update_ini() {
    FILE="/docker/configs/$1"
    KEY=$2
    NEW_VALUE=$3
    sed -i "s~$2=.*~$2=$3~" $FILE
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
    cp resources/sharedconfig.ini /docker/configs/

    # always use the internal docker hostname
    update_ini masterconfig.ini master_ip "darkflame"
    update_ini sharedconfig.ini client_location "/client"

    update_database_ini_values_for sharedconfig.ini
}

update_ini_values
