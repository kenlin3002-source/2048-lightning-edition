#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "rlutil.h"

#define SIZE 4
#define MAX_UNDO 50

typedef struct {
    int grid[SIZE][SIZE]; 
    int score;            
} GameState;

GameState game; 
GameState history[MAX_UNDO]; 
int historyCount = 0;        

int maxTime = 60;            
int timeLeft = 60;           
time_t lastTime;             
int globalPR = 0; 

void loadPR() {
    FILE *f = fopen("record.txt", "r");
    if (f) {
        fscanf(f, "%d", &globalPR);
        fclose(f);
    } else {
        globalPR = 0; 
    }
}

void savePR() {
    if (game.score > globalPR) {
        globalPR = game.score;
        FILE *f = fopen("record.txt", "w");
        if (f) {
            fprintf(f, "%d", globalPR);
            fclose(f);
        }
    }
}

// --- 難度選擇主選單 ---
void showMenu() {
    int choice = 1;
    while(1) {
        cls();
        setBackgroundColor(BLACK);
        
        locate(5, 3); setColor(LIGHTCYAN);
        printf("=== 2048 LIGHTNING EDITION ===");
        locate(5, 5); setColor(WHITE);
        printf("Select Mode (Use [W/S] to choose, [Enter] to start):");
        
        locate(8, 7);
        if (choice == 1) { setColor(LIGHTRED); printf("> 1. Blitz Mode (20 Seconds) - ULTRA FAST!"); }
        else { setColor(GREY); printf("  1. Blitz Mode (20 Seconds)"); }
        
        locate(8, 9);
        if (choice == 2) { setColor(YELLOW); printf("> 2. Normal Mode (45 Seconds)"); }
        else { setColor(GREY); printf("  2. Normal Mode (45 Seconds)"); }
        
        locate(8, 11);
        if (choice == 3) { setColor(LIGHTGREEN); printf("> 3. Casual Mode (60 Seconds)"); }
        else { setColor(GREY); printf("  3. Casual Mode (60 Seconds)"); }
        
        if (kbhit()) {
            int k = getkey();
            if (k == 'w' || k == 'W' || k == KEY_UP) {
                choice--;
                if (choice < 1) choice = 3;
            }
            else if (k == 's' || k == 'S' || k == KEY_DOWN) {
                choice++;
                if (choice > 3) choice = 1;
            }
            else if (k == 13 || k == 10 || k == KEY_ENTER) {
                if (choice == 1) maxTime = 20;
                else if (choice == 2) maxTime = 45;
                else maxTime = 60;
                break; 
            }
        }
        msleep(50);
    }
    cls();
}

void saveState() {
    if (historyCount < MAX_UNDO) {
        history[historyCount++] = game;
    } else {
        for (int i = 0; i < MAX_UNDO - 1; i++) history[i] = history[i+1];
        history[MAX_UNDO - 1] = game;
    }
}

void undoMove() {
    if (historyCount > 0) {
        game = history[--historyCount];
        game.score -= 50; 
        if (game.score < 0) game.score = 0;
    }
}

// --- 隨機生成方塊 (新增紫色百搭史萊姆) ---
void spawnTile() {
    int emptySpots[SIZE * SIZE][2]; 
    int count = 0;
    for (int i = 0; i < SIZE; i++) {
        for (int j = 0; j < SIZE; j++) {
            if (game.grid[i][j] == 0) {
                emptySpots[count][0] = i;
                emptySpots[count][1] = j;
                count++;
            }
        }
    }
    if (count > 0) {
        int randIndex = rand() % count;
        int r = emptySpots[randIndex][0];
        int c = emptySpots[randIndex][1];
        
        int chance = rand() % 100;
        if (chance < 5) {
            game.grid[r][c] = 1; // 5% 機率生成紫色萬能史萊姆 (用數值 1 代表)
        } else if (chance < 15) {
            int val = (rand() % 10 < 9) ? 2 : 4;
            game.grid[r][c] = -val; // 10% 機率生成黃金方塊 (負數)
        } else {
            game.grid[r][c] = (rand() % 10 < 9) ? 2 : 4; // 85% 生成普通方塊
        }
    }
}

void initGame() {
    loadPR(); 
    game.score = 0;
    historyCount = 0;
    timeLeft = maxTime; 
    for (int i = 0; i < SIZE; i++) {
        for (int j = 0; j < SIZE; j++) game.grid[i][j] = 0;
    }
    srand(time(NULL)); 
    spawnTile();       
    spawnTile();  
    lastTime = time(NULL); 
}

// --- 死局判定 (萬能史萊姆 1 不會死局) ---
int checkGameOver() {
    for(int i = 0; i < SIZE; i++) {
        for(int j = 0; j < SIZE; j++) {
            if(game.grid[i][j] == 0 || abs(game.grid[i][j]) == 1) return 0; 
        }
    }
    for(int i = 0; i < SIZE; i++) {
        for(int j = 0; j < SIZE - 1; j++) {
            if(abs(game.grid[i][j]) == abs(game.grid[i][j+1])) return 0;
        }
    }
    for(int j = 0; j < SIZE; j++) {
        for(int i = 0; i < SIZE - 1; i++) {
            if(abs(game.grid[i][j]) == abs(game.grid[i+1][j])) return 0;
        }
    }
    return 1; 
}

// --- 合併引擎 (支援萬能牌邏輯) ---
int slideArray(int array[SIZE]) {
    int moved = 0;
    int writePos = 0;
    for (int i = 0; i < SIZE; i++) {
        if (array[i] != 0) {
            if (writePos != i) {
                array[writePos] = array[i];
                array[i] = 0;
                moved = 1;
            }
            writePos++;
        }
    }
    for (int i = 0; i < SIZE - 1; i++) {
        if (array[i] != 0 && array[i + 1] != 0) {
            int v1 = abs(array[i]);
            int v2 = abs(array[i + 1]);
            int isW1 = (v1 == 1); // 是否為萬能牌
            int isW2 = (v2 == 1);

            // 如果數字相同，或是其中一個是萬能牌，就可以合併！
            if (v1 == v2 || isW1 || isW2) {
                int mergedVal = 0;
                if (isW1 && isW2) mergedVal = 4; // 兩張萬能牌相撞變成 4
                else if (isW1) mergedVal = v2 * 2;
                else if (isW2) mergedVal = v1 * 2;
                else mergedVal = v1 * 2;

                int isGolden = (array[i] < 0 || array[i + 1] < 0);

                array[i] = mergedVal; // 合併後變回普通正數
                array[i + 1] = 0;
                moved = 1;

                if (isGolden) {
                    game.score += mergedVal * 3; 
                    timeLeft += 5;               
                } else {
                    game.score += mergedVal;
                    timeLeft += 1;               
                }
                if (timeLeft > maxTime) timeLeft = maxTime; 
            }
        }
    }
    writePos = 0;
    for (int i = 0; i < SIZE; i++) {
        if (array[i] != 0) {
            if (writePos != i) {
                array[writePos] = array[i];
                array[i] = 0;
            }
            writePos++;
        }
    }
    return moved;
}

int processMove(int key) {
    int moved = 0;
    int line[SIZE];
    if (key == 'a' || key == 'A' || key == KEY_LEFT) {
        for (int r = 0; r < SIZE; r++) {
            for (int c = 0; c < SIZE; c++) line[c] = game.grid[r][c]; 
            if (slideArray(line)) moved = 1;
            for (int c = 0; c < SIZE; c++) game.grid[r][c] = line[c]; 
        }
    } else if (key == 'd' || key == 'D' || key == KEY_RIGHT) {
        for (int r = 0; r < SIZE; r++) {
            for (int c = 0; c < SIZE; c++) line[c] = game.grid[r][SIZE - 1 - c]; 
            if (slideArray(line)) moved = 1;
            for (int c = 0; c < SIZE; c++) game.grid[r][SIZE - 1 - c] = line[c]; 
        }
    } else if (key == 'w' || key == 'W' || key == KEY_UP) {
        for (int c = 0; c < SIZE; c++) {
            for (int r = 0; r < SIZE; r++) line[r] = game.grid[r][c]; 
            if (slideArray(line)) moved = 1;
            for (int r = 0; r < SIZE; r++) game.grid[r][c] = line[r]; 
        }
    } else if (key == 's' || key == 'S' || key == KEY_DOWN) {
        for (int c = 0; c < SIZE; c++) {
            for (int r = 0; r < SIZE; r++) line[r] = game.grid[SIZE - 1 - r][c]; 
            if (slideArray(line)) moved = 1;
            for (int r = 0; r < SIZE; r++) game.grid[SIZE - 1 - r][c] = line[r]; 
        }
    }
    return moved;
}

int getBgColor(int val) {
    if (val < 0) return YELLOW; 
    if (val == 1) return MAGENTA; // 紫色萬能牌
    switch(val) {
        case 0: return DARKGREY;
        case 2: return GREY;
        case 4: return CYAN;
        case 8: return LIGHTBLUE;
        case 16: return BLUE;
        case 32: return LIGHTGREEN;
        case 64: return GREEN;
        case 128: return LIGHTRED;
        case 256: return RED;
        case 512: return MAGENTA;
        case 1024: return LIGHTMAGENTA;
        case 2048: return WHITE;
        default: return WHITE;
    }
}

int getFgColor(int val) {
    if (val < 0) return BLACK; 
    switch(val) {
        case 2: case 4: case 2048: case 0: return BLACK;
        case 1: return WHITE; // 萬能牌白色字
        default: return WHITE;
    }
}

void drawBoard() {
    setBackgroundColor(BLACK); 
    
    locate(2, 2); setColor(LIGHTCYAN);
    printf("=== 2048 LIGHTNING EDITION ===");
    
    locate(38, 2); setColor(YELLOW);
    printf("Score: %-6d", game.score); 

    locate(38, 3);
    if (timeLeft > (maxTime / 4)) setColor(LIGHTGREEN); else setColor(LIGHTRED);
    printf("Time : %-2d / %-2d sec ", timeLeft, maxTime);

    locate(38, 4);
    printf("[");
    int totalBars = 15; 
    int filledBars = (timeLeft * totalBars) / maxTime;
    for(int b = 0; b < totalBars; b++) {
        if (b < filledBars) {
            if (timeLeft > (maxTime / 4)) setColor(LIGHTGREEN); else setColor(LIGHTRED);
            printf("="); 
        } else {
            setColor(DARKGREY);
            printf(" "); 
        }
    }
    setColor(WHITE); printf("]");

    locate(38, 6); setColor(CYAN);
    int currentHighest = (game.score > globalPR) ? game.score : globalPR;
    printf("PR   : %-6d", currentHighest);

    // --- 右側說明更新：加入兩種特殊牌 ---
    locate(38, 8); setColor(WHITE);  printf("--- Special Tiles ---");
    locate(38, 9); setBackgroundColor(YELLOW); setColor(BLACK); printf(" Golden "); setBackgroundColor(BLACK); setColor(YELLOW); printf(" [*N*]");
    locate(38, 10); setColor(WHITE); printf("  -> 3x Score & +5 Sec!");
    locate(38, 11); setBackgroundColor(MAGENTA); setColor(WHITE); printf(" Slime  "); setBackgroundColor(BLACK); setColor(MAGENTA); printf(" [ ? ]");
    locate(38, 12); setColor(WHITE); printf("  -> Merge w/ ANYTHING!");

    for (int i = 0; i < SIZE; i++) {
        for (int j = 0; j < SIZE; j++) {
            int val = game.grid[i][j];
            int bg = getBgColor(val);
            int fg = getFgColor(val);
            int startX = 2 + j * 8; 
            int startY = 4 + i * 4; 
            
            setBackgroundColor(bg);
            setColor(fg);
            locate(startX, startY);     printf("       ");
            locate(startX, startY + 1);
            if (val == 0) printf("       ");
            else if (val == 1) printf("   ?   "); // 紫色史萊姆印問號
            else if (val < 0) printf(" *%3d* ", abs(val));
            else printf(" %5d ", val);
            locate(startX, startY + 2); printf("       ");
        }
    }

    setBackgroundColor(BLACK);
    setColor(WHITE);
    locate(2, 20); printf("===================================");
    locate(2, 21); printf("Controls: [W A S D] Move | [U] Undo");
    locate(2, 22); printf("          [Q] Quit");
}

// --- 巨大 ASCII 結算畫面 ---
void handleGameOver(const char* reason) {
    int isNewPR = 0;
    if (game.score > globalPR) {
        isNewPR = 1;
        savePR(); 
    }
    
    // 遊戲結束，清空畫面準備震撼演出
    setBackgroundColor(BLACK);
    cls();
    
    if (isNewPR) {
        setColor(LIGHTGREEN);
        locate(5, 4); printf("  _   _ _______        __  ____  ____  ");
        locate(5, 5); printf(" | \\ | |  ___\\ \\      / / |  _ \\|  _ \\ ");
        locate(5, 6); printf(" |  \\| | |_   \\ \\ /\\ / /  | |_) | |_) |");
        locate(5, 7); printf(" | |\\  |  _|   \\ V  V /   |  __/|  _ < ");
        locate(5, 8); printf(" |_| \\_|_|      \\_/\\_/    |_|   |_| \\_\\");
    } else {
        setColor(LIGHTRED);
        locate(5, 4); printf("   ____    _    __  __ _____    _____     _______ ____  ");
        locate(5, 5); printf("  / ___|  / \\  |  \\/  | ____|  / _ \\ \\   / / ____|  _ \\ ");
        locate(5, 6); printf(" | |  _  / _ \\ | |\\/| |  _|   | | | \\ \\ / /|  _| | |_) |");
        locate(5, 7); printf(" | |_| |/ ___ \\| |  | | |___  | |_| |\\ V / | |___|  _ < ");
        locate(5, 8); printf("  \\____/_/   \\_\\_|  |_|_____|  \\___/  \\_/  |_____|_| \\_\\");
    }

    locate(15, 11); setColor(WHITE);  printf("Reason      : %s", reason);
    locate(15, 13); setColor(YELLOW); printf("Final Score : %d", game.score);
    
    locate(15, 14);
    if (isNewPR) {
        setColor(LIGHTGREEN);
        printf("Record      : NEW PERSONAL RECORD !!!");
    } else {
        setColor(CYAN);
        printf("Record      : PR is %d", globalPR);
    }

    locate(15, 17); setColor(WHITE);
    printf("Press [Space] or [Enter] to exit...");
    
    while(kbhit()) getkey();
    while(1) {
        if(kbhit()) {
            int k = getkey();
            if(k == ' ' || k == 13 || k == 10 || k == KEY_ENTER) break;
        }
        msleep(50);
    }
}

int main() {
    saveDefaultColor(); 
    hidecursor();       
    
    showMenu();
    
    initGame();
    drawBoard();

    while (1) {
        time_t currentTime = time(NULL);
        if (currentTime - lastTime >= 1) {
            timeLeft--;
            lastTime = currentTime;
            drawBoard();
            
            if (timeLeft <= 0) {
                msleep(500); 
                handleGameOver("Time's up!");
                break;
            }
        }

        if (kbhit()) { 
            int key = getkey(); 
            if (key == 'q' || key == 'Q') {
                handleGameOver("Player Quit");
                break; 
            }
            if (key == 'u' || key == 'U') {
                undoMove();
                drawBoard();
            } 
            else if (key == 'w' || key == 'W' || key == 'a' || key == 'A' || 
                     key == 's' || key == 'S' || key == 'd' || key == 'D' ||
                     key == KEY_UP || key == KEY_DOWN || key == KEY_LEFT || key == KEY_RIGHT) {
                
                saveState(); 
                if (processMove(key)) {
                    spawnTile(); 
                    drawBoard(); 
                    if (checkGameOver()) {
                        msleep(500);
                        handleGameOver("No valid moves left!");
                        break;
                    }
                } else {
                    if (historyCount > 0) historyCount--; 
                }
            }
        }
        msleep(30); 
    }

    showcursor(); 
    setBackgroundColor(BLACK);
    resetColor(); 
    locate(1, 24); 
    return 0;
} 