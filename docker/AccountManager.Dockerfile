# syntax=docker/dockerfile:1
FROM python:3.10-alpine3.14

WORKDIR /usr/local/share/AccountManager

COPY ./thirdparty/AccountManager .

ADD docker/credentials_example.py credentials.py
ADD docker/resources_example.py resources.py

RUN apk add libffi-dev build-base --no-cache && pip3 install -r requirements.txt

EXPOSE 5000
CMD python3 app.py
