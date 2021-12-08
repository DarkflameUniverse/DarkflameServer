FROM python:3.10.0-slim-buster as prep

RUN apt update && apt install unzip sqlite3

WORKDIR /setup

# copy needed files from repo
COPY resources/ resources/
COPY migrations/cdserver/ migrations/cdserver
ADD docker/*.py utils/

COPY docker/setup.sh /setup.sh

CMD [ "/setup.sh" ]