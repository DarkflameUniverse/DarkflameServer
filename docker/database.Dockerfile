FROM mariadb:10.6

COPY ./migrations/dlu /docker-entrypoint-initdb.d
COPY ./migrations/mariadb /docker-entrypoint-initdb.d