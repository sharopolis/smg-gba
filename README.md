# SMG GBA

Release of my GBA SMG Project. It uses the engine from Joshua Barettos https://github.com/zesterer as a base and adds a lot of new features. It also implements code from the Galaxy Decomp https://github.com/SMGCommunity/Petari 

This project contains only code. It contains no assets. Every model, animation, texture, sound, piece of music and line of text is constructed on your computer from your own copy of SMG while the game is built.

You need:

- YOUR OWN PERSONAL legal dump of **SMG (USA)** as an `.iso` file

## macOS

1. Open **Terminal**
2. Install Apple's developer tools:

       xcode-select --install

Click **Install** on the opened window

3. Install the devkitPro:
   - Download `devkitpro-pacman-installer.pkg` from https://github.com/devkitPro/pacman/releases/latest and open it. If macOS blocks it, go to **System Settings > Privacy & Security** and click **Open Anyway**.
   - Then run this command with sudo access:

         sudo dkp-pacman -S gba-dev

4. Open a new terminal to build following the steps below. 

## Windows

We use WSL.

1. Open **Windows PowerShell** and choose **Run as administrator**. Run:

       wsl --install -d Ubuntu

   When it says the changes take effect after a reboot, restart your PC. After the restart, open **Windows PowerShell** as administrator again and run the same command again. Now it should dowload Ubuntu and starts it in that window. Create a default Unix user account (with a password for sudo later).

2. In Ubuntu install the tools:

       sudo apt update
       sudo apt install -y build-essential python3 curl wget

3. Install devkitPro:

       curl -fsSLO -A "dkp apt" https://apt.devkitpro.org/install-devkitpro-pacman
       chmod +x install-devkitpro-pacman
       sudo ./install-devkitpro-pacman
       sudo dkp-pacman -S gba-dev

4. Close the Ubuntu window and open it again

5. Copy this project folder and your `.iso` into Ubuntu: in Windows Explorer, open `\\wsl$\Ubuntu\home\<your user name>` and copy them there. Maybe don't build from the `C:` drive: it works, but it's a bit slower...

Then build with the building steps below ! :)) 

## Linux (Ubuntu or Debian)

So actually like windows but easier: do steps 2 to 4 of the Windows section in a normal terminal.

## Building

1. Put your game dump into the project folder and rename it`baserom_us.iso`. Windows Explorer hides file extensions, so rename it to just `baserom_us` there (otherwise it becomes `baserom_us.iso.iso`). Or, in Ubuntu: `mv "<your dump>.iso" galaxy-gba/baserom_us.iso`.
2. In terminal go into the project folder. Type `cd ` drag the project folder into the terminal window and press Enter. On Windows/Ubuntu, if you copied the folder into your home folder as in step 5, type `cd galaxy-gba` instead, using your folder's name.
3. Start the build:

       make

The first build takes a few minutes. It downloads the disc tool `nodtool` and unpacks your disc, checks that every file it needs matches the original game. Also it constructs assets.

## Come her for help

- **`WSL2 is unable to start since virtualization is not enabled on this machine`** (Windows): turn on virtualization (Intel VT-x / AMD-V / SVM) in your PC's BIOS/UEFI settings, then run `wsl --install -d Ubuntu` again.
- **`curl: (22) The requested URL returned error: 403`** in the devkitPro step: the devkitPro server blocks plain `curl`. Copy the command exactly as written above, including `-A "dkp apt"`.
- **`arm-none-eabi-gcc: No such file or directory`**: the GBA compiler isn't installed. Repeat the devkitPro step, then open a new terminal window.




Hope it all works:)) 