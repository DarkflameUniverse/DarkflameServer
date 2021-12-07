# syntax=docker/dockerfile:1
FROM python:3.9.9-slim
WORKDIR /empty_dir
EXPOSE 80
HEALTHCHECK --interval=2m --timeout=3s \
        CMD curl -f http://localhost:80 || exit 1
CMD python -m http.server 80
