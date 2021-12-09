# Run the Darkflame Server inside Docker

## What you need

- Docker (Docker Desktop or on Linux normal Docker)
- Docker-Compose (Included in Docker Desktop)
- LEGO® Universe Client

## Run server inside Docker

1. Copy `.env.example` and save it as `.env` inside the root directory of this repository
2. Edit the `.env` file and add your path to your LEGO® Universe Client after `CLIENT_PATH=`
3. Update other values in the `.env` file as need (be sure to update passwords!)
4. Run `docker-compose up --build setup`
5. Run `docker-compose up -d database`
6. Run `docker-compose up --build -d account-manager brickbuildfix`
7. Run `docker-compose build darkflame`
8. Run `docker-compose exec darkflame /app/MasterServer -a` and setup your admin account
9. Run `docker-compose up -d darkflame`
10. Now you can see the output of the server with `docker compose logs -f --tail 100` or `docker-compose logs -f --tail 100`. This can help you understand issues and there you can also see when the server finishes it's startup.
11. You're ready to connect your client!

## Disable brickbuildfix

If you don't need the http server running on port 80 do this:

1. Create a file with the name `docker-compose.override.yml`
2. Paste this content:

```yml
services:
  brickbuildfix:
    profiles:
      - donotstart
```

3. Now run `docker compose up -d --build` or `docker-compose up -d --build`