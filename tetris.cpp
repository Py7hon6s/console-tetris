/*
    Console Tetris:
    A simple, single-file, cross-platform (Windows / Linux / macOS) console Tetris
    written in standard C++17, no external libraries required.

    Controls:
        A / Left Arrow   - move left
        D / Right Arrow  - move right
        S / Down Arrow   - soft drop   (+1 point per cell)
        W / Up Arrow     - rotate clockwise
        Space            - hard drop   (+2 points per cell)
        P                - pause / resume
        R                - reset game
        Q                - quit

    Run:
        ./tetris     (Linux / macOS)
        tetris.exe   (Windows)
*/

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
    #include <conio.h>
    #include <windows.h>
#else
    #include <fcntl.h>
    #include <termios.h>
    #include <unistd.h>
#endif

using namespace std;

//  Constants
constexpr int BOARD_WIDTH  = 10;
constexpr int BOARD_HEIGHT = 20;
constexpr int NUM_PIECES   = 7;
const string HIGH_SCORE_FILE = "tetris_highscore.txt";

enum Key {
    KEY_NONE = 0, KEY_LEFT, KEY_RIGHT, KEY_DOWN,
    KEY_UP, KEY_DROP, KEY_PAUSE, KEY_RESET, KEY_QUIT
};

// Base shapes in their starting orientations
const int BASE_SHAPES[NUM_PIECES][4][4] = {
    { {0,0,0,0}, {1,1,1,1}, {0,0,0,0}, {0,0,0,0} },
    { {0,0,0,0}, {0,1,1,0}, {0,1,1,0}, {0,0,0,0} },
    { {0,1,0,0}, {1,1,1,0}, {0,0,0,0}, {0,0,0,0} },
    { {0,1,1,0}, {1,1,0,0}, {0,0,0,0}, {0,0,0,0} },
    { {1,1,0,0}, {0,1,1,0}, {0,0,0,0}, {0,0,0,0} },
    { {1,0,0,0}, {1,1,1,0}, {0,0,0,0}, {0,0,0,0} },
    { {0,0,1,0}, {1,1,1,0}, {0,0,0,0}, {0,0,0,0} },
};

// Stores each piece in its four possible rotations
int pieceRotations[NUM_PIECES][4][4][4];

void rotateGrid90CW(const int src[4][4], int dst[4][4]) {
    for (int row = 0; row < 4; row++)
        for (int col = 0; col < 4; col++)
            dst[row][col] = src[3 - col][row];
}

void initPieceRotations() {
    for (int p = 0; p < NUM_PIECES; p++) {
        for (int r = 0; r < 4; r++)
            for (int c = 0; c < 4; c++)
                pieceRotations[p][0][r][c] = BASE_SHAPES[p][r][c];
        for (int rot = 1; rot < 4; rot++)
            rotateGrid90CW(pieceRotations[p][rot - 1], pieceRotations[p][rot]);
    }
}

const string RESET = "\033[0m";
const string PIECE_BG[NUM_PIECES] = {
    "\033[46m",       
    "\033[43m",       
    "\033[45m",       
    "\033[42m",       
    "\033[41m",       
    "\033[44m",       
    "\033[48;5;208m", 
};

//  Terminal handling
#ifndef _WIN32
termios g_origTermios;

void disableRawMode() {
    tcsetattr(STDIN_FILENO, TCSANOW, &g_origTermios);
}

void enableRawMode() {
    tcgetattr(STDIN_FILENO, &g_origTermios);
    termios raw = g_origTermios;
    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
}

int readByte() {
    unsigned char ch;
    int n = read(STDIN_FILENO, &ch, 1);
    return (n == 1) ? ch : -1;
}
#endif

void setupTerminal() {
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    GetConsoleMode(hOut, &mode);
    SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#else
    enableRawMode();
#endif
    cout << "\033[?25l\033[2J";
}

void restoreTerminal() {
    cout << "\033[?25h" << flush;
#ifndef _WIN32
    disableRawMode();
#endif
}

int readInput() {
#ifdef _WIN32
    if (!_kbhit()) return KEY_NONE;
    int ch = _getch();
    if (ch == 0 || ch == 224) {
        switch (_getch()) {
            case 75: return KEY_LEFT;
            case 77: return KEY_RIGHT;
            case 72: return KEY_UP;
            case 80: return KEY_DOWN;
        }
        return KEY_NONE;
    }
#else
    int ch = readByte();
    if (ch == -1) return KEY_NONE;
    if (ch == 27) {
        int ch2 = readByte();
        int ch3 = readByte();
        if (ch2 == '[') {
            switch (ch3) {
                case 'A': return KEY_UP;
                case 'B': return KEY_DOWN;
                case 'C': return KEY_RIGHT;
                case 'D': return KEY_LEFT;
            }
        }
        return KEY_NONE;
    }
#endif
    switch (ch) {
        case 'a': case 'A': return KEY_LEFT;
        case 'd': case 'D': return KEY_RIGHT;
        case 's': case 'S': return KEY_DOWN;
        case 'w': case 'W': return KEY_UP;
        case ' ':            return KEY_DROP;
        case 'p': case 'P':  return KEY_PAUSE;
        case 'r': case 'R':  return KEY_RESET;
        case 'q': case 'Q':  return KEY_QUIT;
    }
    return KEY_NONE;
}

//  Game logic
struct GameState {
    vector<vector<int>> board = vector<vector<int>>(BOARD_HEIGHT, vector<int>(BOARD_WIDTH, 0));
    int currentPiece = 0;
    int nextPiece = 0;
    int rotation = 0;
    int posX = 0, posY = 0;
    int score = 0;
    int level = 1;
    int linesCleared = 0;
    bool paused = false;
    bool gameOver = false;
};

mt19937 rng((unsigned)chrono::steady_clock::now().time_since_epoch().count());
int randomPiece() { return rng() % NUM_PIECES; }

int loadHighScore() {
    ifstream in(HIGH_SCORE_FILE);
    int value = 0;
    if (in) in >> value;
    return max(0, value);
}

void saveHighScore(int value) {
    ofstream out(HIGH_SCORE_FILE, ios::trunc);
    if (out) out << value;
}

int highScore = loadHighScore();

void updateHighScore(int score) {
    if (score > highScore) {
        highScore = score;
        saveHighScore(highScore);
    }
}

bool checkCollision(const GameState& gs, int piece, int rot, int px, int py) {
    for (int row = 0; row < 4; row++) {
        for (int col = 0; col < 4; col++) {
            if (!pieceRotations[piece][rot][row][col]) continue;
            int bx = px + col, by = py + row;
            if (bx < 0 || bx >= BOARD_WIDTH || by >= BOARD_HEIGHT) return true;
            if (by >= 0 && gs.board[by][bx] != 0) return true;
        }
    }
    return false;
}

void spawnPiece(GameState& gs) {
    gs.currentPiece = gs.nextPiece;
    gs.nextPiece = randomPiece();
    gs.rotation = 0;
    gs.posX = BOARD_WIDTH / 2 - 2;
    gs.posY = 0;
    if (checkCollision(gs, gs.currentPiece, gs.rotation, gs.posX, gs.posY))
        gs.gameOver = true;
}

void lockPiece(GameState& gs) {
    for (int row = 0; row < 4; row++)
        for (int col = 0; col < 4; col++)
            if (pieceRotations[gs.currentPiece][gs.rotation][row][col]) {
                int bx = gs.posX + col, by = gs.posY + row;
                if (by >= 0 && by < BOARD_HEIGHT && bx >= 0 && bx < BOARD_WIDTH)
                    gs.board[by][bx] = gs.currentPiece + 1;
            }
}

int clearLines(GameState& gs) {
    int cleared = 0;
    for (int row = BOARD_HEIGHT - 1; row >= 0; row--) {
        bool full = true;
        for (int col = 0; col < BOARD_WIDTH; col++)
            if (gs.board[row][col] == 0) { full = false; break; }
        if (full) {
            gs.board.erase(gs.board.begin() + row);
            gs.board.insert(gs.board.begin(), vector<int>(BOARD_WIDTH, 0));
            cleared++;
            row++;
        }
    }
    return cleared;
}

void applyLineScore(GameState& gs, int cleared) {
    static const int TABLE[5] = {0, 100, 300, 500, 800};
    if (cleared <= 0) return;
    gs.score += TABLE[cleared] * gs.level;
    updateHighScore(gs.score);
    gs.linesCleared += cleared;
    gs.level = 1 + gs.linesCleared / 10;
}

void tryRotate(GameState& gs) {
    int newRot = (gs.rotation + 1) % 4;
    const int kicks[3] = {0, -1, 1};
    for (int k : kicks) {
        if (!checkCollision(gs, gs.currentPiece, newRot, gs.posX + k, gs.posY)) {
            gs.rotation = newRot;
            gs.posX += k;
            return;
        }
    }
}

void resetGame(GameState& gs, chrono::steady_clock::time_point& lastDrop) {
    gs = GameState{};
    gs.nextPiece = randomPiece();
    spawnPiece(gs);
    lastDrop = chrono::steady_clock::now();
}

void handleInput(GameState& gs, int key, int dropIntervalMs, chrono::steady_clock::time_point& lastDrop) {
    if (key == KEY_RESET) {
        resetGame(gs, lastDrop);
        return;
    }
    if (key == KEY_PAUSE) { gs.paused = !gs.paused; return; }
    if (gs.paused) return;

    switch (key) {
        case KEY_LEFT:
            if (!checkCollision(gs, gs.currentPiece, gs.rotation, gs.posX - 1, gs.posY)) gs.posX--;
            break;
        case KEY_RIGHT:
            if (!checkCollision(gs, gs.currentPiece, gs.rotation, gs.posX + 1, gs.posY)) gs.posX++;
            break;
        case KEY_UP:
            tryRotate(gs);
            break;
        case KEY_DOWN:
            if (!checkCollision(gs, gs.currentPiece, gs.rotation, gs.posX, gs.posY + 1)) {
                gs.posY++;
                gs.score += 1;
                updateHighScore(gs.score);
            }
            lastDrop = chrono::steady_clock::now();
            break;
        case KEY_DROP:
            while (!checkCollision(gs, gs.currentPiece, gs.rotation, gs.posX, gs.posY + 1)) {
                gs.posY++;
                gs.score += 2;
                updateHighScore(gs.score);
            }
            lastDrop = chrono::steady_clock::now() - chrono::milliseconds(dropIntervalMs);
            break;
    }
}

void update(GameState& gs, int dropIntervalMs, chrono::steady_clock::time_point& lastDrop) {
    if (gs.paused || gs.gameOver) return;

    auto now = chrono::steady_clock::now();
    auto elapsed = chrono::duration_cast<chrono::milliseconds>(now - lastDrop).count();
    if (elapsed < dropIntervalMs) return;

    if (!checkCollision(gs, gs.currentPiece, gs.rotation, gs.posX, gs.posY + 1)) {
        gs.posY++;
    } else {
        lockPiece(gs);
        applyLineScore(gs, clearLines(gs));
        spawnPiece(gs);
    }
    lastDrop = now;
}

//  Rendering
void render(const GameState& gs) {
    vector<vector<int>> view = gs.board;
    for (int row = 0; row < 4; row++)
        for (int col = 0; col < 4; col++)
            if (pieceRotations[gs.currentPiece][gs.rotation][row][col]) {
                int bx = gs.posX + col, by = gs.posY + row;
                if (by >= 0 && by < BOARD_HEIGHT && bx >= 0 && bx < BOARD_WIDTH)
                    view[by][bx] = gs.currentPiece + 1;
            }

    const string EOL = "\033[K";
    string out = "\033[H";
    out += "  T E T R I S" + EOL + "\n" + EOL + "\n";

    out += " +";
    for (int i = 0; i < BOARD_WIDTH; i++) out += "--";
    out += "+" + EOL + "\n";

    for (int row = 0; row < BOARD_HEIGHT; row++) {
        out += " |";
        for (int col = 0; col < BOARD_WIDTH; col++) {
            int cell = view[row][col];
            out += cell == 0 ? "  " : (PIECE_BG[cell - 1] + "  " + RESET);
        }
        out += "|";

        if (row == 0) {
            out += "  Next:";
        } else if (row >= 1 && row <= 4) {
            out += "  ";
            for (int c = 0; c < 4; c++) {
                bool filled = pieceRotations[gs.nextPiece][0][row - 1][c];
                out += filled ? (PIECE_BG[gs.nextPiece] + "  " + RESET) : "  ";
            }
        } else if (row == 6) {
            out += "  Score: " + to_string(gs.score);
        } else if (row == 7) {
            out += "  Level: " + to_string(gs.level);
        } else if (row == 8) {
            out += "  Lines: " + to_string(gs.linesCleared);
        } else if (row == 9) {
            out += "  High Score: " + to_string(highScore);
        } else if (row == 10) {
            out += gs.paused ? "  ** PAUSED **" : "";
        } else if (row == 11) {
            out += gs.gameOver ? "  ** GAME OVER - Press R to restart **" : "";
        }
        out += EOL + "\n";
    }

    out += " +";
    for (int i = 0; i < BOARD_WIDTH; i++) out += "--";
    out += "+" + EOL + "\n";
    out += EOL + "\n";
    out += "  A/D move   W rotate   S soft-drop   Space hard-drop   P pause   R reset   Q quit" + EOL + "\n";

    cout << out << flush;
}

int main() {
    initPieceRotations();
    setupTerminal();

    GameState gs;
    gs.nextPiece = randomPiece();
    spawnPiece(gs);
    updateHighScore(gs.score);

    auto lastDrop = chrono::steady_clock::now();
    bool running = true;

    while (running) {
        int dropIntervalMs = max(100, 800 - (gs.level - 1) * 60);

        int key = readInput();
        if (key == KEY_QUIT) break;
        if (key == KEY_RESET) {
            resetGame(gs, lastDrop);
        } else {
            handleInput(gs, key, dropIntervalMs, lastDrop);
        }

        if (!gs.gameOver) {
            update(gs, dropIntervalMs, lastDrop);
        }
        render(gs);

        this_thread::sleep_for(chrono::milliseconds(16));
    }

    restoreTerminal();
    saveHighScore(highScore);
    if (gs.gameOver) {
        cout << "\n  GAME OVER - Final score: " << gs.score
             << "   High score: " << highScore
             << "   Lines: " << gs.linesCleared
             << "   Level: " << gs.level << "\n\n";
    }
    return 0;
}