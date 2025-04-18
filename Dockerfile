FROM gcc:12 as build

WORKDIR /app

RUN set -ex; \
    apt-get update; \
    apt-get install -y cmake

COPY . /app/
COPY --chmod=0500 ./build.sh /app/

RUN sed -i 's/MARIADB_CONNECTOR_COMPILE_JOBS__=.*/MARIADB_CONNECTOR_COMPILE_JOBS__=2/' /app/CMakeVariables.txt 

RUN --mount=type=cache,target=/app/build,id=build-cache \
    mkdir -p /app/build /tmp/persisted-build && \
    cd /app/build && \
    cmake .. && \
    make -j$(nproc --ignore 1) && \
    cp -r /app/build/* /tmp/persisted-build/

FROM debian:12 as runtime

WORKDIR /app

RUN --mount=type=cache,id=build-apt-cache,target=/var/cache/apt \
    apt update && \
    apt install -y libssl3 libcurl4 && \
    rm -rf /var/lib/apt/lists/*

# Grab libraries and load them
COPY --from=build /tmp/persisted-build/mariadbcpp/libmariadbcpp.so /usr/local/lib/
RUN ldconfig

# Server bins
COPY --from=build /tmp/persisted-build/*Server /app/

# Necessary suplimentary files
COPY --from=build /tmp/persisted-build/*.ini /app/configs/
COPY --from=build /tmp/persisted-build/vanity/*.* /app/vanity/
COPY --from=build /tmp/persisted-build/navmeshes /app/navmeshes
COPY --from=build /tmp/persisted-build/migrations /app/migrations
COPY --from=build /tmp/persisted-build/*.dcf /app/

# backup of config and vanity files to copy to the host incase 
# of a mount clobbering the copy from above
COPY --from=build /tmp/persisted-build/*.ini /app/default-configs/ 
COPY --from=build /tmp/persisted-build/vanity/*.* /app/default-vanity/

# needed as the container runs with the root user
# and therefore sudo doesn't exist
ENV USE_SUDO_AUTH=0
ENV DLU_CONFIG_DIR=/app/configs/

COPY --chmod=0500 ./entrypoint.sh /app/
ENTRYPOINT [ "/app/entrypoint.sh" ]
