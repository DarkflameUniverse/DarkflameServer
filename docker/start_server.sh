#!/bin/bash

function set_defaults() {
    DATABASE_PORT=${DATABASE_PORT:-3306}
    DATABASE=${DATABASE:-darkflame}

    if [[ -z $DATABASE_PASSWORD ]]; then
        USE_DATABASE_PASSWORD="No"
    else
        USE_DATABASE_PASSWORD="Yes"
    fi

    CHAT_SERVER_PORT=${CHAT_SERVER_PORT:-2005}
    MAX_CLIENTS=${MAX_CLIENTS:-999}

    echo "Start server with configuration:"
    echo "===== Database Config ========="
    echo "Database: $DATABASE"
    echo "Database host: $DATABASE_HOST"
    echo "Database port: $DATABASE_PORT"
    echo "Database user: $DATABASE_USER"
    echo "Database password set: $USE_DATABASE_PASSWORD"
    echo "===== Other settings =========="
    echo "Chat server port: $CHAT_SERVER_PORT"
    echo "Max clients: $MAX_CLIENTS"
}

function check_sql_connection() {
    until echo '\q' | mysql -h"$DATABASE_HOST" -P"$DATABASE_PORT" -u"$DATABASE_USER" -p"$DATABASE_PASSWORD" $DATABASE; do
        >&2 echo "MySQL/MariaDB is unavailable - sleeping"
        sleep 1
    done
}

function update_ini() {
    INI_FILE=$1
    KEY=$2
    NEW_VALUE=$3
    sed -i "/^$KEY=/s/=.*/=$NEW_VALUE/" $INI_FILE
}

function update_ini_values() {
    update_ini worldconfig.ini mysql_host $DATABASE_HOST
    update_ini worldconfig.ini mysql_database $DATABASE
    update_ini worldconfig.ini mysql_username $DATABASE_USER
    update_ini worldconfig.ini mysql_password $USE_DATABASE_PASSWORD
    update_ini worldconfig.ini chat_server_port $CHAT_SERVER_PORT
    update_ini worldconfig.ini max_clients $MAX_CLIENTS
}

set_defaults

check_sql_connection

update_ini_values

if [[ ! -d "/client" ]]; then
    echo "Client not found."
    echo "Did you forgot to mount the client into the \"/client\" directory?"
    exit 1
fi

if [[ ! -f "/client/extracted" ]]; then
    echo "Start client resource extraction"

    python3 /app/utils/pkextractor.py /client/ /client/
    
    touch /client/extracted
else
    echo "Client already extracted. Skip this step"
    echo "If you want to force re-extract, just delete the file called \"extracted\" in the client directory"
fi

while [[ 1 ]]; do
    sleep 1
    echo "Hello"
done