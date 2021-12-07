# Run the Darkflame Server inside Docker

### What you need

- Docker (Docker Desktop or on Linux normal Docker)
- Docker-Compose (Included in Docker Desktop)
- LEGO® Universe Client

### Run server inside Docker

1. Copy `.env.example` and save it as `.env` inside the root directory of this repository
2. Edit the `.env` file and add your path to your LEGO® Universe Client after `CLIENT_PATH=`
3. Add some random long string after `ACCOUNT_MANAGER_SECRET=` in the `.env` file
4. (Optional) You can decrease the build time if you change number behind `BUILD_THREADS=` in the `.env` file. You should change it to the number of threads your system have.
5. Run `docker compose up -d --build` or `docker-compose up -d --build` and wait for it to complete
6. Now you can see the output of the server with `docker compose logs -f --tail 100` or `docker-compose logs -f --tail 100`. This can help you understand issues and there you can also see when the server finishes it's startup.


### Disable brickbuildfix

If you don't need the http server running on port 80 do this:

1. Create a file with the name `docker-compose.override.yml`
2. Paste this content:

```yml
services:
  brickbuildfix:
    profiles:
      - donotstart
```

4. Now run `docker compose up -d --build` or `docker-compose up -d --build`