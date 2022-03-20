FROM gcc:11 as build

WORKDIR /build

RUN --mount=type=cache,id=build-apt-cache,target=/var/cache/apt \
    echo "Install build dependencies" && \
    apt update && \
    apt remove -y libmysqlcppconn7v5 libmysqlcppconn-dev && \
    apt install cmake zlib1g zlib1g-dev unzip -yqq --no-install-recommends && \
    rm -rf /var/lib/apt/lists/*

COPY dAuthServer/ /build/dAuthServer
COPY dChatServer/ /build/dChatServer
COPY dCommon/ /build/dCommon
COPY dChatFilter/ /build/dChatFilter
COPY dDatabase/ /build/dDatabase
COPY dGame/ /build/dGame
COPY dMasterServer/ /build/dMasterServer
COPY dNet/ /build/dNet
COPY dPhysics/ /build/dPhysics
COPY dScripts/ /build/dScripts
COPY dWorldServer/ /build/dWorldServer
COPY dZoneManager/ /build/dZoneManager
COPY migrations/ /build/migrations
COPY resources/ /build/resources
COPY thirdparty/ /build/thirdparty
COPY vanity /build/vanity
COPY tests/ /build/tests
COPY .clang-* CMake* LICENSE /build/

ARG BUILD_THREADS=1
ARG BUILD_VERSION=171022

RUN echo "Build server" && \
    mkdir -p cmake_build && \
    cd cmake_build && \
    sed -i -e "s/171022/${BUILD_VERSION}/g" ../CMakeVariables.txt && \
    cmake .. && \
    make -j $BUILD_THREADS

RUN unzip /build/resources/navmeshes.zip -d /build/cmake_build/res/maps

FROM gcc:11 as runtime

WORKDIR /app

# With this utility we can wait for the database to be ready
ADD https://github.com/ufoscout/docker-compose-wait/releases/download/2.9.0/wait ./wait-for-db

ADD docker/wait-for-db.sha512 ./wait-for-db.sha512

ADD docker/wait-for-setup.sh .

RUN set -e && echo "$(cat ./wait-for-db.sha512) wait-for-db" | sha512sum --check --status

RUN --mount=type=cache,id=runtime-apt-cache,target=/var/cache/apt \
    apt update && \
    apt install sudo -yqq --no-install-recommends && \
    apt remove -y libmysqlcppconn7v5 libmysqlcppconn-dev && \
    rm -rf /var/lib/apt/lists/* && \
    chmod +x ./wait-for-db

COPY --from=build /build/cmake_build /app

RUN mkdir -p /build/cmake_build && ln -s /app/_deps /build/cmake_build/_deps

COPY docker/start_server.sh /start_server.sh

COPY docker/master-server-wrapper.sh /app/MasterServer.wrapper

RUN mv ./MasterServer /app/MasterServer.org && \
    mv /app/MasterServer.wrapper /app/MasterServer && \
    chmod +x /app/MasterServer

CMD [ "/start_server.sh" ]