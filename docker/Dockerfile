FROM gcc:12 as build

WORKDIR /build

RUN --mount=type=cache,id=build-apt-cache,target=/var/cache/apt \
    echo "Install build dependencies" && \
    apt update && \
    apt remove -y libmysqlcppconn7v5 libmysqlcppconn-dev && \
    apt install cmake zlib1g zlib1g-dev -yqq --no-install-recommends && \
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
COPY dNavigation/ /build/dNavigation
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
    sed -i -e "s/NET_VERSION=.*/NET_VERSION=${BUILD_VERSION}/g" ../CMakeVariables.txt && \
    sed -i -e "s/__maria_db_connector_compile_jobs__=.*/__maria_db_connector_compile_jobs__=${BUILD_THREADS}/g" ../CMakeVariables.txt && \
    cmake .. -DCMAKE_BUILD_RPATH_USE_ORIGIN=TRUE && \
    make -j $BUILD_THREADS

FROM gcc:12 as runtime

RUN --mount=type=cache,id=runtime-apt-cache,target=/var/cache/apt \
    apt update && \
    apt install sudo -yqq --no-install-recommends && \
    apt remove -y libmysqlcppconn7v5 libmysqlcppconn-dev && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY --from=build /build/cmake_build /app

RUN mkdir -p /build/cmake_build && ln -s /app/_deps /build/cmake_build/_deps

COPY docker/start_server.sh /start_server.sh

CMD [ "/start_server.sh" ]
