# MinecraftClone
## What is this project?
This project is an attempt at cloning Minecraft in C++ with optimizations and so on.
The difference is, it aims to clone the old Minecraft Legacy Console Edition instead of any other edition.
## Why do this project?
Originally I started this project out as a small hobby project but has started turning into a project I really want to work on in order to add more features to the game with alot more capabilities and be able
to play The old Minecraft Legacy Console Edition again but on a PC Port which hopefully runs and works alot better.
## What does this project currently include?
Currently this project includes these features:
- Work in progress UI system
- Working Terrain Generation (which will still be changed) with Biomes
- 2 Lighting systems, the normal Lighting system and a Dynamic Lighting system
and much more...
## How do I play this?
You currently have to compile the game yourself.
The game is currently made only for Linux, so you may have some trouble doing this on Windows.
There will be Windows support in the future.
### How to compile
#### Dependencies
- GCC
- SDL2
- freetype2
- OpenGL
- OpenAL
#### Compiling
Compiling is quite simple if you have everything you need installed.
Simply run `make clean` to make sure there is no leftovers that may have accidently been left in the repo and then run `make` to compile the game.
The executable file will be in the `bin` folder.
To compile and run the game right after (added for testing features easier) run `make start` instead.
You can also add ` -j` at the end of the command to compile the game faster by allowing the Makefile to compile multiple files.