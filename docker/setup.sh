#!/bin/bash

# fail on first error
set -e

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
