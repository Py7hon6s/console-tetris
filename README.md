# Console Tetris

A Tetris game that runs in the terminal, written in C++17 in a single file with no external libraries.

## About the project

This is a version of Tetris you play in the terminal. The pieces fall, you move and rotate them, and full rows get cleared for points. I made it to get more practice with C++ on something bigger than the usual small exercises, and a game seemed like a fun way to do that. I wanted to try a game loop, reading keyboard input without waiting for Enter, and saving data to a file, and Tetris covers all of those.

I also wanted it to work on Windows, Linux and macOS, which turned out to be the trickiest part because each system handles keyboard input differently.

## Features

- All 7 standard pieces (I, O, T, S, Z, J, L), each with its own colour
- Piece rotation, with a small "kick" so a piece near a wall can still rotate by shifting one cell left or right
- Line clearing, including multiple lines at once
- Scoring: 1 point per cell for a soft drop, 2 points per cell for a hard drop, and bonus points for clearing lines (100 / 300 / 500 / 800 for 1 / 2 / 3 / 4 lines, multiplied by the current level)
- Levels: you go up a level every 10 lines, and the pieces fall faster each time
- "Next piece" preview
- Pause and reset
- High score saved to `tetris_highscore.txt`, so it is still there next time you run the game

## Requirements and installation

You need:

- A C++ compiler that supports C++17 (GCC, Clang or MSVC)
- A terminal that supports ANSI colour codes. This is true for most Linux and macOS terminals. On Windows, use Windows 10 or later (Windows Terminal works well).

**Linux / macOS**

```bash
g++ -std=c++17 -O2 tetris.cpp -o tetris
./tetris
```

**Windows (MinGW / g++)**

```bash
g++ -std=c++17 -O2 tetris.cpp -o tetris.exe
tetris.exe
```

**Windows (Visual Studio Developer Command Prompt)**

```bash
cl /std:c++17 /EHsc tetris.cpp
tetris.exe
```

## Controls

| Key | Action |
|-----|--------|
| `A` or Left Arrow | Move left |
| `D` or Right Arrow | Move right |
| `S` or Down Arrow | Soft drop |
| `W` or Up Arrow | Rotate clockwise |
| `Space` | Hard drop |
| `P` | Pause / resume |
| `R` | Reset the game |
| `Q` | Quit |

## Project structure

```
.
├── tetris.cpp            # all of the source code
├── README.md
└── tetris_highscore.txt  # created automatically the first time you play
```

Everything is in `tetris.cpp`, split into sections: constants and piece shapes, terminal handling, game logic, and rendering. The executable (`tetris` or `tetris.exe`) appears after you compile.

## What I learned

- **2D arrays and `vector`:** the board is a `vector<vector<int>>`, and each piece is stored as a 4x4 grid. Each piece is rotated once at startup and all four rotations are stored.
- **Structs and enums:** I put the whole game state in one `GameState` struct, which made resetting the game easy. I used an `enum` for the key presses so the code is easier to read than raw character codes.
- **Platform differences with `#ifdef`:** Windows uses `conio.h`, while Linux and macOS need `termios` to turn off line buffering and echo. I had to learn how raw mode works and that it must be switched back when the program exits, otherwise the terminal is left in a broken state.
- **Timing with `<chrono>`:** I used timestamps to decide when the piece should fall, instead of just sleeping, so key presses still feel responsive.
- **File I/O:** reading and writing the high score with `ifstream` and `ofstream`.
- **Random numbers:** using `mt19937` seeded with the clock instead of `rand()`.
- **ANSI escape codes:** for colours, hiding the cursor, and redrawing the screen without flickering.

Things I would improve if I come back to it:

- The rotation happens inside a fixed 4x4 grid, so some pieces shift position slightly when they rotate. A proper rotation system (like SRS) would fix this.
- Pieces are picked completely at random, so you can get long runs of the same piece. A "7-bag" randomiser would be fairer.
- The high score file is written on every point gained once you beat the record. It would be better to save it only when the game ends.
- `using namespace std;` is fine for a small project like this, but I would avoid it in bigger ones.
