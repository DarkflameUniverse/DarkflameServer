FROM gcc:12 AS build

RUN --mount=type=cache,id=build-apt-cache,target=/var/cache/apt \
    apt update && \
    apt install -y libssl-dev libcurl4-openssl-dev uncrustify git cmake && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /work
RUN git clone https://libwebsockets.org/repo/libwebsockets --depth 1 --branch v4.2-stable
RUN git clone https://github.com/yaml/libyaml --depth 1 --branch release/0.2.5

WORKDIR /work/libwebsockets/build
RUN cmake -DLWS_WITHOUT_TESTAPPS=ON -DLWS_WITHOUT_TEST_SERVER=ON -DLWS_WITHOUT_TEST_SERVER_EXTPOLL=ON \
    -DLWS_WITHOUT_TEST_PING=ON -DLWS_WITHOUT_TEST_CLIENT=ON -DCMAKE_C_FLAGS="-fpic" -DCMAKE_INSTALL_PREFIX=/usr/local ..
RUN make install

WORKDIR /work/libyaml/build
RUN cmake -DCMAKE_INSTALL_PREFIX=/usr/local -DBUILD_TESTING=OFF  -DBUILD_SHARED_LIBS=ON ..
RUN make install

COPY ../thirdparty/kubernetes-client-c/kubernetes /work/kubernetes-client-c/
WORKDIR /work/kubernetes-client-c/build
RUN cmake -DCMAKE_INSTALL_PREFIX=/usr/local ..
RUN make install

FROM debian:12

RUN --mount=type=cache,id=build-apt-cache,target=/var/cache/apt \
    apt update && \
    apt install -y libssl3 libcurl4 && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY build/mariadbcpp/src/mariadb_connector_cpp-build/libmariadb/libmariadb/libmariadb.so.3 /usr/local/lib
COPY --from=build /usr/local/lib/libkubernetes.so /usr/local/lib
COPY --from=build /usr/local/lib/libyaml.so /usr/local/lib
COPY --from=build /usr/local/lib/libwebsockets.so.18 /usr/local/lib
COPY --from=build /usr/local/lib/libwebsockets.so /usr/local/lib
COPY build/libmariadbcpp.so /usr/local/lib
RUN ldconfig
COPY build/MasterServerK8s /app/MasterServerK8s
COPY build/MasterServer /app/MasterServer
COPY build/AuthServer /app/AuthServer
COPY build/ChatServer /app/ChatServer
COPY build/WorldServer /app/WorldServer

COPY build/migrations /app/migrations
COPY build/navmeshes /app/navmeshes

ENTRYPOINT ["/app/MasterServer"]
