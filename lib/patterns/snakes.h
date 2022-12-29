#ifndef SNAKES_H
#define SNAKES_H

#include <Arduino.h>
#include "config.h"
#include "pattern.h"
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

const uint8_t FOOD_ID = 255;

// struct representing a snake. Each snake has a position, direction, color, head
// direction is 0-3, 0 is up, 1 is right, 2 is down, 3 is left
struct Snake{
  uint8_t r1, r2, g1, g2, b1, b2, dir, col, row, t, len, id, respawn_delay, type;
  bool alive;
  void move(std::pair<uint8_t, uint8_t> ** board){
    bool valid_dirs[4] = {true, true, true, true};
    uint8_t n_dirs = 4;
    if(this->row == 0 || board[this->row - 1][this->col].second != 0){
      valid_dirs[0] = false;
      n_dirs--;
    }
    if(this->col == PANEL_WIDTH * PANELS_NUMBER - 1 || board[this->row][this->col + 1].second != 0){
      valid_dirs[1] = false;
      n_dirs--;
    }
    if(this->row == PANEL_HEIGHT - 1 || board[this->row + 1][this->col].second != 0){
      valid_dirs[2] = false;
      n_dirs--;
    }
    if(this->col == 0 || board[this->row][this->col - 1].second != 0){
      valid_dirs[3] = false;
      n_dirs--;
    }
    
    // If the snake has no valid moves, the snake is dead :(
    if(n_dirs == 0){
      this->alive = false;
      this->respawn_delay = this->len;
      return;
    }

    // Randomly change direction, increasing the chance of changing direction the longer the snake has been going in the same direction
    if(random(1000) < 30 * (1.0/sqrt(len)) * t || !valid_dirs[this->dir]){
      do{
        this->dir = random(4);
      } while(!valid_dirs[this->dir]);
      t=0;
    }
    t++;

    // Move the snake
    switch(this->dir){
      case 0:
        this->row--;
        break;
      case 1:
        this->col++;
        break;
      case 2:
        this->row++;
        break;
      case 3:
        this->col--;
        break;
    }
    if(board[this->row][this->col].first == FOOD_ID){
      this->len+=2;
    }
    board[this->row][this->col].second = this->len;
    board[this->row][this->col].first = this->id;
  }
};

class SnakeGame: public Pattern{
    public:
        SnakeGame(MatrixPanel_I2S_DMA *display, uint8_t, uint8_t, uint16_t);
        void init();
        void update();
        void draw();
        void show();
        ~SnakeGame();
    private:
        uint8_t len; // Starting length of all snakes
        Snake * snakes; // Array of all snakes in the game
        std::pair<uint8_t,uint8_t> ** board; // 2D array representing the board, each element is a pair of uint8_t, the first is the snake id, the second is the length of the snake
        uint8_t n_snakes;
        uint16_t n_food;
        MatrixPanel_I2S_DMA *display;

        void place_food();
        void spawn_snake(uint8_t i);
        enum SnakeType {REGULAR = 0, GRADIENT = 1, ALTERNATING = 2, GHOST = 3};
};

#endif