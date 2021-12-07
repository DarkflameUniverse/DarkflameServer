#!/bin/bash

# Initialise variables
SERVER_DELAY=${SERVER_DELAY:=0}

# Go to the server directory
cd /opt/darkflame-universe/

# Start the server
sleep "${SERVER_DELAY}"
case "${SERVER_TYPE}" in
    auth) 
        ./AuthServer
        ;;
    master) 
        ./MasterServer
        ;;
    debug) 
        tail -f /dev/null
        ;;
    *) 
        echo "Unknown server type ${SERVER_TYPE}"; 
        exit 1
        ;;
esac