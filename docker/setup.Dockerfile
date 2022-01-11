FROM rust:alpine3.14 as LUnpack

WORKDIR /build_LUnpack

COPY ./thirdparty/LUnpack .

RUN apk add musl-dev --no-cache && cargo build --release

FROM python:3.10-alpine3.14 as prep

RUN apk add sqlite bash --no-cache

WORKDIR /setup

# copy needed files from repo
COPY resources/ resources/
COPY migrations/cdserver/ migrations/cdserver
COPY --from=LUnpack /build_LUnpack/target/release/lunpack /usr/local/bin/lunpack
ADD thirdparty/docker-utils/utils/*.py utils/

COPY docker/setup.sh /setup.sh

CMD [ "/setup.sh" ]