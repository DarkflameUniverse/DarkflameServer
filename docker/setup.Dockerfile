FROM python:3.10-alpine3.14 as prep

RUN apk add bash --no-cache

WORKDIR /setup

# copy needed files from repo
COPY resources/ resources/

COPY docker/setup.sh /setup.sh

CMD [ "/setup.sh" ]
