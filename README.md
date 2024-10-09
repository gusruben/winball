# Winball

**Winball** is a little pinball game made for MS-DOS using [Allegro](https://liballeg.org/). Winball was made for [Retrospect](https://retrospect.hackclub.com).

## Running

Winball can be run on any DOS emulator. The easiest way to compile Winball with Allegro is likely [DJGPP](https://www.delorie.com/djgpp/), a C/C++ development system for DOS PCs.

1. Download DJGPP from https://www.delorie.com/djgpp/zip-picker.html - Check the box for 'Allegro' under toolkits. Select an HTTP mirror if you don't want to use FTP.
2. Download the listed zip files, and extract them all into the same folder. Clone Winball next to it.
3. Mount the parent folder in DOSBOX (or your DOS emulator of choice): `mount c C:\path\to\folder`
4. Set the environment variables:
    - `set PATH=C:\DJGPP\BIN;%PATH%` (Note: this is the path from inside the DOS emulator, not from your main system)
    - `set DJGPP=C:\DJGPP\DJGPP.ENV`
5. `cd` to the Winball folder, and compile it with `gcc -o winball.exe main.c -lalleg`
6. Run `winball.exe`!

## Demo

https://github.com/user-attachments/assets/76d1fb91-05be-4b06-a477-a258a71979e0
