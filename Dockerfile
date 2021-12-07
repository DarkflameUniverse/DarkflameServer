FROM gcc:11.2.0 as build

# Install Dependencies
RUN apt update && \
    apt remove -y libmysqlcppconn7v5 libmysqlcppconn-dev && \
    apt install -y cmake sudo zlib1g zlib1g-dev

# Install MySQL C++ Connector
RUN wget https://dev.mysql.com/get/Downloads/Connector-C++/libmysqlcppconn9_8.0.27-1debian11_amd64.deb -O /tmp/libmysqlcppconn.deb && \
    dpkg -i /tmp/libmysqlcppconn.deb && \
    rm /tmp/libmysqlcppconn.deb

COPY . /app

# Create the build directory
WORKDIR /app/build

# Run cmake to generate make files
RUN cmake ..

# Run make to build the project. To build utilizing multiple cores, append `-j` and the amount of cores to utilize, for example `make -j8`
RUN make -j4

FROM python:3.10.0-slim-buster as prep

RUN apt update && apt install unzip sqlite3
COPY --from=build /app/build /app/build
WORKDIR /app

# populate build/res/
COPY ./docker/client/res /app/client/res
RUN mkdir -p build/res/
RUN cp -r client/res/BrickModels build/res/BrickModels
RUN cp -r client/res/macros build/res/macros
RUN cp -r client/res/maps build/res/maps
RUN cp -r client/res/names build/res/names
RUN cp client/res/chatplus_en_us.txt build/res/chatplus_en_us.txt

# unzip navmeshes into build/res/maps/
COPY ./resources /app/resources
RUN unzip resources/navmeshes.zip -d build/res/maps/

# convert and update sqlite database
COPY ./migrations /app/migrations
COPY ./thirdparty/utils /app/thirdparty/utils
RUN python3 thirdparty/utils/utils/fdb_to_sqlite.py client/res/cdclient.fdb --sqlite_path build/res/CDServer.sqlite
RUN sqlite3 build/res/CDServer.sqlite < migrations/cdserver/0_nt_footrace.sql
RUN sqlite3 build/res/CDServer.sqlite < migrations/cdserver/1_fix_overbuild_mission.sql
RUN sqlite3 build/res/CDServer.sqlite < migrations/cdserver/2_script_component.sql

FROM debian:buster-20211201-slim as deploy

# Install Dependencies
RUN apt update && \
    apt remove -y libmysqlcppconn7v5 libmysqlcppconn-dev
# Install MySQL C++ Connector
RUN wget https://dev.mysql.com/get/Downloads/Connector-C++/libmysqlcppconn9_8.0.27-1debian11_amd64.deb -O /tmp/libmysqlcppconn.deb && \
    dpkg -i /tmp/libmysqlcppconn.deb && \
    rm /tmp/libmysqlcppconn.deb

COPY --from=prep /app/build /app/build

CMD ["/app/build/MasterServer"]
