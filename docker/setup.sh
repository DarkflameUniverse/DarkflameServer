#!/bin/bash

if [[ $SCRIPT_DEBUG == "1" || ${SCRIPT_DEBUG@L} =~ "true" ]]; then
    set -x
    # trap read debug
fi

if [[ -f /docker/error ]]; then
    echo "Clear /docker/error"
    rm /docker/error
fi

function set_error() {
    exit_code=$?
    if [[ $exit_code > 0 ]]; then
        tree /client
        touch /docker/error
    fi
}

# Notify darkflame container that something went wrong
trap "set_error" EXIT

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

    if [[ ! -z $CHAT_SERVER_PORT ]]; then
        update_ini worldconfig.ini chat_server_port $CHAT_SERVER_PORT
        update_ini chatconfig.ini port $CHAT_SERVER_PORT
    fi

    if [[ ! -z $MAX_CLIENTS ]]; then
        update_ini worldconfig.ini max_clients $MAX_CLIENTS
        update_ini authconfig.ini max_clients $MAX_CLIENTS
        update_ini chatconfig.ini max_clients $MAX_CLIENTS
    fi

    # always use the internal docker hostname
    update_ini masterconfig.ini master_ip "darkflame"

    update_database_ini_values_for masterconfig.ini
    update_database_ini_values_for authconfig.ini
    update_database_ini_values_for chatconfig.ini
    update_database_ini_values_for worldconfig.ini
}

function fdb_to_sqlite() {
    echo "Run fdb_to_sqlite"
    python3 utils/fdb_to_sqlite.py $CLIENT_ROOT_DIR/res/cdclient.fdb --sqlite_path $CLIENT_ROOT_DIR/res/CDServer.sqlite

    (
        cd migrations/cdserver
        readarray -d '' entries < <(printf '%s\0' *.sql | sort -zV)
        for entry in "${entries[@]}"; do
            echo "Execute $entry"
            sqlite3 $CLIENT_ROOT_DIR/res/CDServer.sqlite < $entry
        done
    )
}

update_ini_values

if [[ ! -d "/client" ]]; then
    echo "[ERROR] Client not found."
    echo "[ERROR] Did you forget to mount the client into the \"/client\" directory?"
    exit 12
fi

if [[ "${CLIENT_TYPE@L}" != "packed" && "${CLIENT_TYPE@L}" != "unpacked" && "${CLIENT_TYPE@L}" != "auto" ]]; then
    echo "[ERROR] Unknown CLIENT_TYPE"
    exit 16
fi

# Try to auto detect client type
if [[ "${CLIENT_TYPE@L}" == "auto" ]]; then
    if [[ ! -z $CLIENT_ROOT_DIR ]]; then
        echo "[WARNING] Your CLIENT_ROOT_DIR variable gets overwritten by auto mode"
    fi

    if [[ -f "/client/legouniverse.exe" ]]; then
        # Look for a unpacked file. If it doesn't exist, then we can assume the user provided a packed client
        # with a client structure like a unpacked client. At this point we can't continue the process. 
        # Without the versions directory lunpack can't extract the client files
        if [[ ! -f "/client/res/CDClient.fdb" ]]; then
            echo "[ERROR] You provided a packed client without the versions directory"
            echo "[ERROR] Without the versions directory you need to unpack the client by yourself"
            echo "[ERROR] The server can't boot without an unpacked client"
            exit 13
        fi
        CLIENT_TYPE=unpacked
        CLIENT_ROOT_DIR=/client
        touch /docker/extracted
    elif [[ -f "/client/client/legouniverse.exe" ]]; then
        # If this file exist, we can assume the user provided an unpacked client but with a different client root structure
        if [[ -f "/client/client/res/CDClient.fdb" ]]; then
            CLIENT_TYPE=unpacked
            CLIENT_ROOT_DIR=/client/client
            touch /docker/extracted
        elif [[ -f "/client/versions/trunk.txt" ]]; then
            CLIENT_TYPE=packed
            CLIENT_ROOT_DIR=/client/client
        else
            echo "[ERROR] Can't detect client type. You need to define the client type by yourself"
            exit 14
        fi
    fi
else
    if [[ -z $CLIENT_ROOT_DIR ]]; then
        if [[ "${CLIENT_TYPE@L}" == "packed" ]]; then
            CLIENT_ROOT_DIR=/client/client
        elif [[ "${CLIENT_TYPE@L}" == "unpacked" ]]; then
            CLIENT_ROOT_DIR=/client
        fi
    fi
fi

if [[ -z $CLIENT_ROOT_DIR ]]; then
    echo "[ERROR] Client root path wasn't auto detected. You need to provide it by yourself"
    echo "[ERROR] You can use CLIENT_ROOT_DIR for this"
    exit 15
fi

echo "Client type: $CLIENT_TYPE"
echo "Client root: $CLIENT_ROOT_DIR"

echo "$CLIENT_ROOT_DIR" > /docker/root_dir

if [[ ! -f "/docker/extracted" ]]; then
    echo "Start client resource extraction"

    touch globs.txt

    echo "client/res/macros/**" >> globs.txt
    echo "client/res/BrickModels/**" >> globs.txt
    echo "client/res/maps/**" >> globs.txt
    echo "*.fdb" >> globs.txt

    lunpack -g ./globs.txt $CLIENT_ROOT_DIR/..

    touch /docker/extracted
else
    echo "Client already extracted. Skip this step..."
    echo "If you want to force a re-extract, just delete the file called \"extracted\" in the client directory"
fi

if [[ ! -f "/docker/migrated" ]]; then
    echo "Start client db migration"

    fdb_to_sqlite

    touch /docker/migrated
else
    echo "Client db already migrated. Skip this step..."
    echo "If you want to force a re-migrate, just delete the file called \"migrated\" in the client directory"
fi
