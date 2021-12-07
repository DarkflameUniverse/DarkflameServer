FROM debian:11-slim

WORKDIR /build

RUN --mount=type=cache,target=/var/cache/apt \
    echo "Install build dependencies" && \
    apt update && \
    apt install gcc cmake zlib1g-dev make build-essential g++ mariadb-client git python3 -yqq --no-install-recommends && \
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
COPY .clang-* CMake* LICENSE /build/

ARG BUILD_THREADS=1

RUN echo "Build server" && \
    mkdir -p build && \
    cd build && \
    ls -lah && ls -lah ../ && cmake .. && \
    make -j $BUILD_THREADS && \
    mkdir -p /app && \
    cp -R ./* /app && \
    rm -rf /build

WORKDIR /app

ADD docker/*.py /app/utils/

COPY docker/start_server.sh /start_server.sh

RUN chmod +x /start_server.sh

CMD [ "/start_server.sh" ]