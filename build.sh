# Create the build directory, preserving it if it already exists
mkdir -p build
cd build

# Run cmake to generate make files
cmake ..

# Run make to build the project. To build utilizing multiple cores, append `-j` and the amount of cores to utilize, for example `make -j8`
make

# Run mysql migrations

#Check we have sqlite db
if [ -f res/CDServer.sqlite ]; then

        #Check connection to database

	#Get database credentials
        dbName=$(cat masterconfig.ini | grep mysql_database | sed -e "s/mysql_database=//")
        dbUname=$(cat masterconfig.ini | grep mysql_username | sed -e "s/mysql_username=//")
        dbPass=$(cat masterconfig.ini | grep mysql_password | sed -e "s/mysql_password=//")

	if [ -z "$dbPass" ]; then
                dbPass="-"
        fi

        conn=$(mysql -u ${dbUname} -D ${dbName} -p${dbPass} -e '\q' 2>&1 >/dev/null)  #Only capture stderr
        if [ -z "$conn" ]; then #If there wasn't an error
		./MasterServer -m
        else
                exit
        fi
fi
