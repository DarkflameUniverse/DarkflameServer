#!/bin/bash

mkdir -p docker/client/res/
cp -r client/res/BrickModels docker/client/res/BrickModels
cp -r client/res/macros docker/client/res/macros
cp -r client/res/maps docker/client/res/maps
cp -r client/res/names docker/client/res/names
cp client/res/chatplus_en_us.txt docker/client/res/chatplus_en_us.txt
cp client/res/cdclient.fdb docker/client/res/cdclient.fdb

# copy over database files
# mkdir -p mariadb/initdb.d
# cp migrations/dlu/0_initial.sql mariadb/initdb.d/

# # populate build/res/
# mkdir -p build/res/
# cp -r client/res/BrickModels build/res/BrickModels
# cp -r client/res/macros build/res/macros
# cp -r client/res/maps build/res/maps
# cp -r client/res/names build/res/names
# cp client/res/chatplus_en_us.txt build/res/chatplus_en_us.txt

# # unzip navmeshes into build/res/maps/
# unzip resources/navmeshes.zip -d build/res/maps/

# # convert and update sqlite database
# python3 thirdparty/utils/utils/fdb_to_sqlite.py client/res/cdclient.fdb --sqlite_path build/res/CDServer.sqlite
# sqlite3 build/res/CDServer.sqlite < migrations/cdserver/0_nt_footrace.sql
# sqlite3 build/res/CDServer.sqlite < migrations/cdserver/1_fix_overbuild_mission.sql
# sqlite3 build/res/CDServer.sqlite < migrations/cdserver/2_script_component.sql

# TODO: Configuration
# After the server has been built there should be four ini files in the build director:
# authconfig.ini, chatconfig.ini, masterconfig.ini, and worldconfig.ini.
# Go through them and fill in the database credentials and configure other settings if necessary.
