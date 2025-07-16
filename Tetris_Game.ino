#include <MD_MAX72xx.h>
#include <SPI.h>

// Define MAX7219 settings
#define MAX_DEVICES 4  // 4-in-1 Display
#define CLK_PIN 11
#define DATA_PIN 12
#define CS_PIN 10

MD_MAX72XX matrix = MD_MAX72XX(MD_MAX72XX::FC16_HW, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);

// Button Pins
#define LEFT_BUTTON 2   // Move left
#define RIGHT_BUTTON 3  // Move right
#define ROTATE_BUTTON 4 // Rotate 90° clockwise (short press), reset (long press when game over)
#define DROP_BUTTON 5   // Instant drop (hard drop)

// Grid size
#define WIDTH 8    // 8 columns
#define HEIGHT 32  // 32 rows

// Tetromino definitions (4x4 grid, 7 shapes: I, O, T, S, Z, J, L)
const int TETROMINOES[7][4][4] = {
  // I
  {{0, 0, 0, 0}, {1, 1, 1, 1}, {0, 0, 0, 0}, {0, 0, 0, 0}},
  // O
  {{0, 0, 0, 0}, {0, 1, 1, 0}, {0, 1, 1, 0}, {0, 0, 0, 0}},
  // T
  {{0, 0, 0, 0}, {0, 1, 1, 1}, {0, 0, 1, 0}, {0, 0, 0, 0}},
  // S
  {{0, 0, 0, 0}, {0, 1, 1, 0}, {1, 1, 0, 0}, {0, 0, 0, 0}},
  // Z
  {{0, 0, 0, 0}, {1, 1, 0, 0}, {0, 1, 1, 0}, {0, 0, 0, 0}},
  // J
  {{0, 0, 0, 0}, {1, 1, 1, 0}, {0, 0, 1, 0}, {0, 0, 0, 0}},
  // L
  {{0, 0, 0, 0}, {1, 1, 1, 0}, {1, 0, 0, 0}, {0, 0, 0, 0}}
};

// Game variables
int board[HEIGHT][WIDTH];      // Game board (1 = occupied, 0 = empty)
int currentTetromino[4][4];    // Current tetromino shape
int tetrominoX = 2;            // X-position (leftmost column, 0-4)
int tetrominoY = 28;           // Y-position (top row of 4x4 box, starts near row 31)
int currentShape = 0;          // Current tetromino index (0-6)
bool gameOver = false;         // Game over flag
unsigned long lastFall = 0;    // Last tetromino fall time
int fallDelay = 1000;          // Fall delay (ms, starts at 1000ms)
int score = 0;                 // Player score
bool testMode = false;         // Test mode for display mapping
int testRow = 0;               // Current row for test mode
const bool ROTATE_DISPLAY = true; // Adjust for FC-16 rotation

void setup() {
  Serial.begin(9600);
  Serial.println("Setup started");

  matrix.begin(); // Initialize display
  matrix.clear();

  // Initialize buttons with internal pull-ups
  pinMode(LEFT_BUTTON, INPUT_PULLUP);
  pinMode(RIGHT_BUTTON, INPUT_PULLUP);
  pinMode(ROTATE_BUTTON, INPUT_PULLUP);
  pinMode(DROP_BUTTON, INPUT_PULLUP);

  randomSeed(analogRead(A0)); // Random seed
  resetGame();
  Serial.println("Tetris initialized. Send 't' in Serial Monitor to enter test mode.");
}

void resetGame() {
  // Clear board
  for (int row = 0; row < HEIGHT; row++) {
    for (int col = 0; col < WIDTH; col++) {
      board[row][col] = 0;
    }
  }
  // Initialize new tetromino
  currentShape = random(7);
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      currentTetromino[i][j] = TETROMINOES[currentShape][i][j];
    }
  }
  tetrominoX = 2; // Center (8 - 4 = 4 columns, start at 2)
  tetrominoY = 28; // Near top (4x4 box, top row at 31-3=28)
  gameOver = false;
  fallDelay = 1000;
  score = 0;
  testMode = false;
  testRow = 0;
  updateDisplay();
  Serial.println("Game reset");
}

void updateDisplay() {
  matrix.clear();

  if (testMode) {
    // Test mode: Light up one row at a time
    for (int col = 0; col < WIDTH; col++) {
      if (ROTATE_DISPLAY) {
        matrix.setPoint(col, testRow, true); // Rotate 90°
      } else {
        matrix.setPoint(testRow, col, true);
      }
    }
    Serial.print("Test mode: Lighting row ");
    Serial.println(testRow);
    return;
  }

  // Draw board
  for (int row = 0; row < HEIGHT; row++) {
    for (int col = 0; col < WIDTH; col++) {
      if (board[row][col]) {
        if (ROTATE_DISPLAY) {
          matrix.setPoint(col, row, true);
        } else {
          matrix.setPoint(row, col, true);
        }
      }
    }
  }

  // Draw current tetromino
  if (!gameOver) {
    for (int i = 0; i < 4; i++) {
      for (int j = 0; j < 4; j++) {
        if (currentTetromino[i][j]) {
          int boardRow = tetrominoY + i;
          int boardCol = tetrominoX + j;
          if (boardRow >= 0 && boardRow < HEIGHT && boardCol >= 0 && boardCol < WIDTH) {
            if (ROTATE_DISPLAY) {
              matrix.setPoint(boardCol, boardRow, true);
            } else {
              matrix.setPoint(boardRow, boardCol, true);
            }
          }
        }
      }
    }
    Serial.print("Tetromino at (");
    Serial.print(tetrominoX);
    Serial.print(", ");
    Serial.print(tetrominoY);
    Serial.print("), shape ");
    Serial.println(currentShape);
  }
}

bool canMoveTetromino(int newX, int newY, int shape[4][4]) {
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      if (shape[i][j]) {
        int boardRow = newY + i;
        int boardCol = newX + j;
        if (boardRow < 0 || boardRow >= HEIGHT || boardCol < 0 || boardCol >= WIDTH || board[boardRow][boardCol]) {
          return false;
        }
      }
    }
  }
  return true;
}

void rotateTetromino() {
  int temp[4][4];
  // Rotate 90° clockwise
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      temp[j][3 - i] = currentTetromino[i][j];
    }
  }
  // Check if rotation is valid
  if (canMoveTetromino(tetrominoX, tetrominoY, temp)) {
    for (int i = 0; i < 4; i++) {
      for (int j = 0; j < 4; j++) {
        currentTetromino[i][j] = temp[i][j];
      }
    }
    Serial.println("Tetromino rotated");
  }
}

void hardDrop() {
  // Move tetromino down until it cannot move further
  while (canMoveTetromino(tetrominoX, tetrominoY - 1, currentTetromino)) {
    tetrominoY--;
  }
  Serial.println("Hard drop");
  lockTetromino();
}

void lockTetromino() {
  // Add tetromino to board
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      if (currentTetromino[i][j]) {
        int boardRow = tetrominoY + i;
        int boardCol = tetrominoX + j;
        if (boardRow >= 0 && boardRow < HEIGHT && boardCol >= 0 && boardCol < WIDTH) {
          board[boardRow][boardCol] = 1;
        }
      }
    }
  }
  // Check for completed lines
  int linesCleared = 0;
  for (int row = HEIGHT - 1; row >= 0; row--) {
    bool full = true;
    for (int col = 0; col < WIDTH; col++) {
      if (!board[row][col]) {
        full = false;
        break;
      }
    }
    if (full) {
      linesCleared++;
      // Shift rows down
      for (int r = row; r < HEIGHT - 1; r++) {
        for (int col = 0; col < WIDTH; col++) {
          board[r][col] = board[r + 1][col];
        }
      }
      // Clear top row
      for (int col = 0; col < WIDTH; col++) {
        board[HEIGHT - 1][col] = 0;
      }
      row++; // Recheck this row after shift
    }
  }
  if (linesCleared > 0) {
    score += linesCleared * 100;
    fallDelay = max(200, fallDelay - 50); // Speed up
    Serial.print("Lines cleared: ");
    Serial.print(linesCleared);
    Serial.print(", Score: ");
    Serial.println(score);
  }
  // Spawn new tetromino
  currentShape = random(7);
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      currentTetromino[i][j] = TETROMINOES[currentShape][i][j];
    }
  }
  tetrominoX = 2;
  tetrominoY = 28;
  if (!canMoveTetromino(tetrominoX, tetrominoY, currentTetromino)) {
    gameOver = true;
    Serial.println("Game over: Cannot spawn tetromino");
  }
}

void loop() {
  // Handle test mode
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 't') {
      testMode = !testMode;
      if (testMode) {
        testRow = 0;
        Serial.println("Entered test mode. Send 'n' or press any button to advance row, 't' to exit.");
      } else {
        Serial.println("Exited test mode");
        resetGame();
      }
    } else if (testMode && c == 'n') {
      testRow = (testRow + 1) % HEIGHT;
      Serial.print("Test mode: Advanced to row ");
      Serial.println(testRow);
    }
    updateDisplay();
  }

  if (testMode) {
    // Advance row with any button press
    if (digitalRead(LEFT_BUTTON) == LOW || digitalRead(RIGHT_BUTTON) == LOW || 
        digitalRead(ROTATE_BUTTON) == LOW || digitalRead(DROP_BUTTON) == LOW) {
      delay(50); // Debounce
      if (digitalRead(LEFT_BUTTON) == LOW || digitalRead(RIGHT_BUTTON) == LOW || 
          digitalRead(ROTATE_BUTTON) == LOW || digitalRead(DROP_BUTTON) == LOW) {
        testRow = (testRow + 1) % HEIGHT;
        Serial.print("Test mode: Advanced to row ");
        Serial.println(testRow);
        updateDisplay();
        while (digitalRead(LEFT_BUTTON) == LOW || digitalRead(RIGHT_BUTTON) == LOW || 
               digitalRead(ROTATE_BUTTON) == LOW || digitalRead(DROP_BUTTON) == LOW); // Wait for release
      }
    }
    return;
  }

  if (gameOver) {
    // Flash display and wait for long press on rotate button to reset
    matrix.clear();
    delay(500);
    updateDisplay();
    delay(500);
    if (digitalRead(ROTATE_BUTTON) == LOW) {
      delay(50); // Debounce
      if (digitalRead(ROTATE_BUTTON) == LOW) {
        unsigned long pressStart = millis();
        while (digitalRead(ROTATE_BUTTON) == LOW && millis() - pressStart < 1000); // Wait up to 1000ms
        if (digitalRead(ROTATE_BUTTON) == LOW) {
          resetGame();
          Serial.println("Game restarted (long press on rotate button)");
          while (digitalRead(ROTATE_BUTTON) == LOW); // Wait for release
        }
      }
    }
    return;
  }

  // Handle button inputs
  if (digitalRead(LEFT_BUTTON) == LOW) {
    delay(50); // Debounce
    if (digitalRead(LEFT_BUTTON) == LOW) {
      int newX = tetrominoX - 1;
      if (canMoveTetromino(newX, tetrominoY, currentTetromino)) {
        tetrominoX = newX;
        Serial.println("Move left");
      }
      updateDisplay();
      while (digitalRead(LEFT_BUTTON) == LOW); // Wait for release
    }
  }
  if (digitalRead(RIGHT_BUTTON) == LOW) {
    delay(50); // Debounce
    if (digitalRead(RIGHT_BUTTON) == LOW) {
      int newX = tetrominoX + 1;
      if (canMoveTetromino(newX, tetrominoY, currentTetromino)) {
        tetrominoX = newX;
        Serial.println("Move right");
      }
      updateDisplay();
      while (digitalRead(RIGHT_BUTTON) == LOW); // Wait for release
    }
  }
  if (digitalRead(ROTATE_BUTTON) == LOW) {
    delay(50); // Debounce
    if (digitalRead(ROTATE_BUTTON) == LOW) {
      rotateTetromino();
      updateDisplay();
      while (digitalRead(ROTATE_BUTTON) == LOW); // Wait for release
    }
  }
  if (digitalRead(DROP_BUTTON) == LOW) {
    delay(50); // Debounce
    if (digitalRead(DROP_BUTTON) == LOW) {
      hardDrop();
      updateDisplay();
      while (digitalRead(DROP_BUTTON) == LOW); // Wait for release
    }
  }

  // Move tetromino down
  if (millis() - lastFall >= fallDelay) {
    if (canMoveTetromino(tetrominoX, tetrominoY - 1, currentTetromino)) {
      tetrominoY--;
    } else {
      lockTetromino();
    }
    lastFall = millis();
    updateDisplay();
  }
}