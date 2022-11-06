# Run the Darkflame Server inside Docker

## What you need

- [Docker](https://docs.docker.com/get-docker/) (Docker Desktop or on Linux normal Docker)
- [Docker Compose](https://docs.docker.com/compose/install/) (Included in Docker Desktop)
- LEGO® Universe packed Client. Check the main [README](./README.md) for details on this.

## Run server inside Docker

1. Copy `.env.example` and save it as `.env` inside the root directory of this repository
2. Edit the `.env` file and add your path to the root directory of your LEGO® Universe Client after `CLIENT_PATH=`
3. Update other values in the `.env` file as needed (be sure to update passwords!)
4. Run `docker compose up -d --build`
5. Run `docker compose exec darkflame /app/MasterServer -a` and setup your admin account
6. Follow the directions [here](https://github.com/DarkflameUniverse/AccountManager) to setup regular user accounts. The server will be accessible at: `http://<EXTERNAL_IP>:5000`
7. Now you can see the output of the server with `docker compose logs -f --tail 100` or `docker compose logs -f --tail 100`. This can help you understand issues and there you can also see when the server finishes it's startup.
8. You're ready to connect your client!

**NOTE #1**: If you're running an older version of Docker, you may need to use the command `docker-compose` instead of `docker compose`.

**NOTE #2**: To stop the server simply run `docker compose down` and to restart it just run `docker compose up -d` again. No need to run all the steps above every time.

**NOTE #3**: Docker buildkit needs to be enabled. https://docs.docker.com/develop/develop-images/build_enhancements/#to-enable-buildkit-builds

**NOTE #4**: Make sure to run the following in the repo root directory after cloning so submodules are also downloaded.
```
git submodule update --init --recursive
```
**NOTE #5**: If DarkflameSetup fails due to not having cdclient.fdb, rename CDClient.fdb (in the same folder) to cdclient.fdb

## Disable brickbuildfix

If you don't need the http server running on port 80 do this:

1. Create a file with the name `docker-compose.override.yml` in the root of the repository
2. Paste this content:

```yml
services:
  brickbuildfix:
    profiles:
      - donotstart
```

3. Now run `docker compose up -d`
