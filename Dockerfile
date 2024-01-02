FROM gcc:13 as build

WORKDIR /app

RUN set -ex; \
    apt-get update; \
    apt-get install -y cmake

COPY . /app/

RUN sed -i 's/MARIADB_CONNECTOR_COMPILE_JOBS__=.*/MARIADB_CONNECTOR_COMPILE_JOBS__=2/' /app/CMakeVariables.txt 

RUN chmod +x build.sh
RUN ./build.sh

FROM debian:12 as runtime

WORKDIR /app

COPY --from=build /app/build/*Server /app/
COPY --from=build /app/build/*.ini /app/default-configs/
COPY --from=build /app/build/*.so /usr/lib/
COPY --from=build /app/build/mariadbcpp/src/mariadb_connector_cpp-build/*.so /usr/lib/
COPY --from=build /app/build/mariadbcpp/src/mariadb_connector_cpp-build/mariadbcpp/plugin/*.so /usr/lib/
COPY --from=build /app/build/vanity /app/default-vanity
COPY --from=build /app/build/navmeshes /app/navmeshes
COPY --from=build /app/build/migrations /app/migrations
COPY --from=build /app/build/*.dcf /app/
COPY ./entrypoint.sh /app/
RUN ldconfig
# needed as the container runs with the root user
# and therefore sudo doesn't exist
ENV USE_SUDO_AUTH=0
RUN chmod +x entrypoint.sh
CMD [ "/app/entrypoint.sh" ]
