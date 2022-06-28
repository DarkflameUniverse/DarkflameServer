#!/bin/bash

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

echo "Wait for database to be ready"

./wait-for-db

echo "Database is ready"