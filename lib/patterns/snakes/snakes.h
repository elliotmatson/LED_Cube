#ifndef SNAKES_H
#define SNAKES_H

#include <Arduino.h>
#include <utility>
#include "cube_utils.h"
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

const uint8_t FOOD_ID = 254;
const uint8_t SPACE_ID = 255;
const uint8_t N_SNAKE_TYPES = 14;

inline std::pair<uint8_t, uint8_t> check_move(uint8_t row, uint8_t col, uint8_t dir){
  // Check if the move is valid, and if so, return the new position
  // Movement space is 3 sides of a cube (top, left, right) 64 x 64 pixels for each face. 
  // Cols 0-63 are the left face, 64-127 are the right face, and 128-191 are the top face
  
  switch (col / PANEL_WIDTH){
    case 0: // Top face
      switch (dir){
        case 0: // Up
          if(row == 0){ // Top border (invalid move)
            return std::make_pair(255, 255);
          } else {
            return std::make_pair(row - 1, col);
          }
          break;
        case 1: // Right
          if(col == 63){ // Move to the right face 
            return std::make_pair(63, 64 + row);
          } else {
            return std::make_pair(row, col + 1);
          }
          break;
        case 2: // Down
          if(row == 63){ // Move to the left face
            return std::make_pair(63, 191 - col);
          } else {
            return std::make_pair(row + 1, col);
          }
          break;
        case 3: // Left
          if(col == 0){ // Left border (invalid move)
            return std::make_pair(255, 255);
          } else {
            return std::make_pair(row, col - 1);
          }
          break;
      }
      break;
    case 1: // Right face
      switch (dir){
        case 0: // Up
          if(row == 0){ // Bottom border (invalid move)
            return std::make_pair(255, 255);
          } else {
            return std::make_pair(row - 1, col);
          }
          break;
        case 1: // Right
          return std::make_pair(row, col + 1);
          break;
        case 2: // Down
          if(row == 63){ // Move to the top face
            return std::make_pair(col - 64, 63);
          } else {
            return std::make_pair(row + 1, col);
          }
          break;
        case 3:
          if(col == 64){ // Right border (invalid move)
            return std::make_pair(255, 255);
          } else {
            return std::make_pair(row, col-1);
          }
      }
      break;
    case 2: // Left face
      switch (dir){
        case 0: // Up
          if(row == 0){ // Bottom border (invalid move)
            return std::make_pair(255, 255);
          } else {
            return std::make_pair(row - 1, col);
          }
          break;
        case 1: // Right
          if(col == 191){ // Left border (invalid move)
            return std::make_pair(255, 255);
          } else {
            return std::make_pair(row, col + 1);
          }
          break;
        case 2: // Down
          if(row == 63){ // Move to the top face
            return std::make_pair(63, 191 - col);
          } else {
            return std::make_pair(row + 1, col);
          }
          break;
        case 3: // Left
          return std::make_pair(row, col - 1);
          break;
      }
      break;
  }
  return std::make_pair(255, 255);
}

// struct representing a snake. Each snake has a position, direction, color, head
// direction is 0-3, 0 is up, 1 is right, 2 is down, 3 is left
struct Snake{
  uint8_t r1, r2, g1, g2, b1, b2, dir, col, row, t, len, id, respawn_delay, type, slow, segment_len;
  bool alive;
  void move(std::pair<uint8_t, uint8_t> ** board, Snake * snakes){

    bool valid_dirs[4] = {true, true, true, true};
    uint8_t n_dirs = 4;
    // valid_dirs[(this->dir + 2) % 4] = false;
    for(uint8_t i = 0 ; i < 4; i++){
      std::pair<uint8_t, uint8_t> new_pos = check_move(this->row, this->col, i);
      // if(new_pos.first == 255 || (board[new_pos.first][new_pos.second].second != 0 && board[new_pos.first][new_pos.second].first != this->id)){
      if(new_pos.first == 255){ 
        valid_dirs[i] = false;
        n_dirs--;
      } else if(board[new_pos.first][new_pos.second].second != 0){
        if(this->type != 13 || snakes[board[new_pos.first][new_pos.second].first].type == 13){
          valid_dirs[i] = false;
          n_dirs--;
        }
      } 
    }
    
    
    // If the snake has no valid moves, the snake is dead :(
    if(n_dirs == 0){
      this->die();
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

    std::pair<uint8_t, uint8_t> new_pos = check_move(this->row, this->col, this->dir);
    std::pair<uint8_t, uint8_t> board_vals = board[new_pos.first][new_pos.second];
    if(board_vals.second != 0){
      if(this->type == 13 && snakes[board_vals.first].type != 13 && snakes[board_vals.first].alive){ // Eater of worlds
        this->len += board_vals.second;
        snakes[board_vals.first].die();
      }
    }
    this->row = new_pos.first;
    this->col = new_pos.second;
    
    if(board[this->row][this->col].first == FOOD_ID){
      this->len+=2;
    }
    board[this->row][this->col].second = this->len * this->slow;
    board[this->row][this->col].first = this->id;
  }
  void die(){
    this->alive = false;
    this->respawn_delay = this->len * this->slow;
  }
};

class SnakeGame: public Pattern{
    public:
        SnakeGame();
        void init(PatternServices *pattern);
        void start();
        void stop();
        ~SnakeGame();

      private:
        unsigned long frameCount;
        uint8_t len; // Starting length of all snakes
        Snake * snakes; // Array of all snakes in the game
        std::pair<uint8_t,uint8_t> ** board; // 2D array representing the board, each element is a pair of uint8_t, the first is the snake id, the second is the length of the snake
        uint8_t n_snakes;
        uint16_t n_food;

        void reset();
        void update();
        void draw();
        void show();
        void place_food();
        void spawn_snake(uint8_t i);
        enum SnakeType {
          REGULAR = 0, 
          GRADIENT = 1, 
          ALTERNATING = 2, 
          GHOST = 3, 
          SPARKLE = 4, 
          PULSING = 5, 
          STROBE = 6, 
          FADE = 7,
          STATIC_ALTERNATING = 8,
          SLOW = 9,
          FAST = 10,
          TECHNICOLOR = 11,
          DASHED = 12,
          EATER_OF_WORLDS = 13,
        };
        long snake_type_to_rarity[N_SNAKE_TYPES] = {
          100000, // Regular
          3000, // Gradient
          1500, // Alternating
          100, // Ghost
          50, // Sparkle
          100, // Pulsing
          10, // Strobe
          0, // Fade
          1500, // Static Alternating
          500, // Slow
          500, // Fast
          100, // Technicolor
          200, // Dashed
          1, // Eater of Worlds
        };
};

#endif