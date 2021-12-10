# Installation under Windows
## First Run
1. Navigate to the [Docker download page](https://hub.docker.com/editions/community/docker-ce-desktop-mac/) and download docker. ![Docker Download Page](docker/images/Docker Download Page.png)
2. Once the file has finished downloading, run it and proceed through the installation. Make sure, "Install required Windows components for WSL 2" is checked. ![Docker Desktop Installer Configuration](docker/images/Docker Desktop Installer Configuration.png)
3. If necessary, restart your computer.
4. After the restart, Docker Desktop will automatically open. If it does not, simply start it like any other program.
5. If a window "WSL 2 Installation is incomplete." pops up, follow the link and click "WSL2 Linux kernel update package for x64 machines". Run the downloaded file and once that finishes, click "Restart" in the Docker Desktop window. ![WSL 2 download](docker/images/WSL 2 download.png)
6. Wait until Docker Desktop has started. You may skip the tutorial.
7. You may want to disable "OpenDocker Dashboard at startup" in _Settings_ -> _General_ ![Disable Autostart](docker/images/DD General Settings.png)
8. Download the DarkflameServer repository and extract it to a location of your choice. TODO submodules were not downloaded for me...
9. Inside the extracted folder, copy `.env.example` and name the copy `.env`
10. Open `.env` with notepad by right-clicking it and selecting _Open With_ -> _More apps_ -> _Notepad_.
11. Change the text after `CLIENT_PATH=` to the location of your client. The folder you are pointing to must contain a folder called `client` which should contain the client files. Use `/` instead of `\`
12. Optionally, you can change the number after `BUILD_THREADS=` to the number of cores / threads your processor has. If your computer crashes while building, you can try to reduce this value.
13. After `ACCOUNT_MANAGER_SECRET=` paste a "SHA 256-bit Key" from https://keygen.io/
14. If you are not only hosting a local server, change the value after `EXTERNAL_IP=` to the external IP address of your computer.
15. Change the two values `SECRET_VALUE_CHANGE_ME` to passwords only you know. Save and close the file.
16. In the extracted folder hit Shift+Right Click and select "Open PowerShell window here". ![Open PowerShell](docker/images/Open Powershell.png)
17. In the new window, paste (with right click) or type `docker-compose up -d --build` and confirm with enter.
18. Once you see the blinking cursor and the path again, setup has finished and the server is already running. ![setup done](docker/images/setup finished.png)
19. Create an admin account by pasting `docker-compose exec darkflame /app/MasterServer -a` and following the prompts. ![admin account creation](docker/images/Account Creation.png)
20. You can now login with these credentials at `http://your_ip:5000` (replace your_ip with your external IP). There you can create your account for playing as well as generate keys for other people to join; use these at `http://your_ip:5000/activate`

## Normal Use
1. In Docker Desktop you should now see an entry `darkflameserver-main` and when you click on it all containers but `DarkflameSetup` should eventually be green. That means the server is running. ![server running](docker/images/Docker Compose Finished.png)
2. For troubleshooting, you can check the logs of the various parts by clicking their entry.
3. You can start and stop the server with the corresponding buttons. Once all containers are grey, the server has shut down, and when all containers but `DarkflameSetup` are green, the server is running. Note that starting and stopping takes some time, please be patient. ![start stop buttons](docker/images/DD Server Startstop.png)
