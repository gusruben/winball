<h1 align="center">
    <img height="300" src="https://raw.githubusercontent.com/gusruben/winball/master/.github/title.gif" alt="Winball">
</h1>

**Winball** is a little pinball game made for MS-DOS using [Allegro](https://liballeg.org/). Winball was made for [Retrospect](https://retrospect.hackclub.com). 

> **Messy code alert!** This was my first time using C, beware spaghetti code and bad practices!

## Running

Unless you're planning on running the game on a 35-year-old computer, Winball can be run on any DOS emulator. During development, I used [DOSBox Staging](https://www.dosbox-staging.org/).

### From Releases

1. Download & extract [the latest ZIP file from releases](https://github.com/gusruben/winball/releases/latest).
2. Mount the folder in DOSBox (or an equivalent) using `mount C C:\path\to\folder`
3. Start the game with `run.bat`!

### From Source

The easiest way to compile Winball with Allegro is likely [DJGPP](https://www.delorie.com/djgpp/), a C/C++ development system for DOS PCs.

1. Download DJGPP from https://www.delorie.com/djgpp/zip-picker.html - Check the box for 'Allegro' under toolkits. Select an HTTP mirror if you don't want to use FTP.
2. Download the listed zip files, and extract them all into the same folder. Clone Winball next to it.
3. Mount the parent folder in DOSBOX (or your DOS emulator of choice): `mount C C:\path\to\folder`
4. Set the environment variables:
    - `set PATH=C:\DJGPP\BIN;%PATH%` (Note: this is the path from inside the DOS emulator, not from your main system)
    - `set DJGPP=C:\DJGPP\DJGPP.ENV`
5. `cd` to the Winball folder, and compile it with `gcc -o winball.exe main.c -lalleg`
6. Run `winball.exe`!

## Demo

> Video not working? Try watching it [here](https://github.com/user-attachments/assets/4fc3fa43-2a16-4f3e-a1d3-24e993795fd0).

https://github.com/user-attachments/assets/4fc3fa43-2a16-4f3e-a1d3-24e993795fd0
