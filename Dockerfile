# syntax=docker/dockerfile:1

# GCC Base
FROM gcc:11.2.0 as build

# Set Build Arguments
#
# 171022 = Unmodded Client
# 171023 = DLU Client
ARG BUILD_VERSION=171022
ARG BUILD_ARGS=-j12

# Working Directory
WORKDIR /opt/darkflame-universe

# Copy Source
COPY ./ /tmp/darkflame-universe/
COPY docker-entrypoint.sh /docker-entrypoint.sh

# Install Dependencies
RUN apt update && \
    apt remove -y libmysqlcppconn7v5 libmysqlcppconn-dev && \
    apt install -y cmake sudo zlib1g zlib1g-dev

# Install MySQL C++ Connector
RUN wget https://dev.mysql.com/get/Downloads/Connector-C++/libmysqlcppconn9_8.0.27-1debian11_amd64.deb -O /tmp/libmysqlcppconn.deb && \
    dpkg -i /tmp/libmysqlcppconn.deb && \
    rm /tmp/libmysqlcppconn.deb

# Try to build
RUN cd /tmp/darkflame-universe/ && \
    sed -i -e "s/171023/${BUILD_VERSION}/g" CMakeVariables.txt && \
    cat CMakeVariables.txt | grep VERSION && \
    mkdir -p build && \
    cd build && \
    cmake .. && \
    make ${BUILD_ARGS}

# Copy back to working directory
RUN mv /tmp/darkflame-universe/build/AuthServer ./AuthServer && \
    mv /tmp/darkflame-universe/build/ChatServer ./ChatServer && \
    mv /tmp/darkflame-universe/build/MasterServer ./MasterServer && \
    mv /tmp/darkflame-universe/build/WorldServer ./WorldServer

# Expose our ports
EXPOSE 1001/udp
EXPOSE 2000/udp
EXPOSE 2005/udp
EXPOSE 3000-3200/udp

# Set default environment variables
ENV SERVER_TYPE=debug

# Start the server
ENTRYPOINT [ "/docker-entrypoint.sh" ]