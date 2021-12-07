# syntax=docker/dockerfile:1
FROM python:3.9.9-slim
WORKDIR /empty_dir
EXPOSE 80
CMD python -m http.server 80
