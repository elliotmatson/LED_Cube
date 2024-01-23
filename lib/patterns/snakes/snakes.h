#ifndef SNAKES_H
#define SNAKES_H

#include <Arduino.h>
#include <utility>
#include "cube_utils.h"
#include <random>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

const uint8_t FOOD_ID = 254;
const uint8_t SPACE_ID = 255;
const uint8_t N_SNAKE_TYPES = 18;
const uint8_t STASIS_DURATION_MIN = 10;

inline std::pair<std::pair<uint8_t,uint8_t>, uint8_t> check_move(uint8_t row, uint8_t col, uint8_t dir){
  // Check if the move is valid, and if so, return the new position and new direction.
  // Movement space is 3 sides of a cube (top, left, right) 64 x 64 pixels for each face. 
  // Cols 0-63 are the top face, 64-127 are the right face, and 128-191 are the left face
  
  switch (col / PANEL_WIDTH){
    case 0: // Top face
      switch (dir){
        case 0: // Up
          if(row <= 0){ // Top right border (invalid move)
            return std::make_pair(std::make_pair(255, 255), 255);
          } else {
            return std::make_pair(std::make_pair(row - 1, col), dir);
          }
          break;
        case 1: // Right
          if(col == 63){ // Move to the right face 
            return std::make_pair(std::make_pair(63, 64 + row), 0);
          } else {
            return std::make_pair(std::make_pair(row, col + 1), dir);
          }
          break;
        case 2: // Down
          if(row == 63){ // Move to the left face
            return std::make_pair(std::make_pair(63, 191 - col), 0);
          } else {
            return std::make_pair(std::make_pair(row + 1, col), dir);
          }
          break;
        case 3: // Left
          if(col <= 0){ // Top left border (invalid move)
            return std::make_pair(std::make_pair(255, 255), dir);
          } else {
            return std::make_pair(std::make_pair(row, col - 1), dir);
          }
          break;
      }
      break;
    case 1: // Right face
      switch (dir){
        case 0: // Up
          if(row == 0){ // Bottom border (invalid move)
            return std::make_pair(std::make_pair(255, 255), dir);
          } else {
            return std::make_pair(std::make_pair(row - 1, col), dir);
          }
          break;
        case 1: // Right
          return std::make_pair(std::make_pair(row, col + 1), dir);
          break;
        case 2: // Down
          if(row == 63){ // Move to the top face
            return std::make_pair(std::make_pair(col - 64, 63), 3);
          } else {
            return std::make_pair(std::make_pair(row + 1, col), dir);
          }
          break;
        case 3:
          if(col <= 64){ // Right border (invalid move)
            return std::make_pair(std::make_pair(255, 255), dir);
          } else {
            return std::make_pair(std::make_pair(row, col-1), dir);
          }
      }
      break;
    case 2: // Left face
      switch (dir){
        case 0: // Up
          if(row <= 0){ // Bottom border (invalid move)
            return std::make_pair(std::make_pair(255, 255), dir);
          } else {
            return std::make_pair(std::make_pair(row - 1, col), dir);
          }
          break;
        case 1: // Right
          if(col >= 191){ // Left border (invalid move)
            return std::make_pair(std::make_pair(255, 255), dir);
          } else {
            return std::make_pair(std::make_pair(row, col + 1), dir);
          }
          break;
        case 2: // Down
          if(row == 63){ // Move to the top face
            return std::make_pair(std::make_pair(63, 191 - col), 0);
          } else {
            return std::make_pair(std::make_pair(row + 1, col), dir);
          }
          break;
        case 3: // Left
          return std::make_pair(std::make_pair(row, col - 1), dir);
          break;
      }
      break;
  }
  return std::make_pair(std::make_pair(255, 255), 255);
}

// struct representing a snake. Each snake has a position, direction, color, head
// direction is 0-3, 0 is up, 1 is right, 2 is down, 3 is left
struct Snake{
  uint8_t r1, r2, g1, g2, b1, b2, dir, col, row, t, id, type, slow, misc1, misc2;
  uint16_t len, respawn_delay;
  bool alive;
  void move(std::pair<uint8_t, uint16_t> ** board, Snake * snakes){

    bool valid_dirs[4] = {true, true, true, true};
    uint8_t n_dirs = 4;
    // valid_dirs[(this->dir + 2) % 4] = false;
    for(uint8_t i = 0 ; i < 4; i++){
      std::pair<uint8_t, uint8_t> new_pos;
      uint8_t new_dir;
      std::tie(new_pos, new_dir) = check_move(this->row, this->col, i);
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

    // DEATH LOGIC
    if(this->type == 17){ // Stasis Snake
      if(n_dirs == 0){
        if(this->misc2 == 0){ // If going to die, enter stasis
          this->misc2 = misc1;
        } else {
          this->misc2--; // Decrement stasis timer
          if(this->misc2 == 0){ // If last frame of stasis
            this->die();
          }
        }
        return;
      }
      else { // We have a move, exit stasis if applicable
        this->misc2 = 0;
      }
    } else if(n_dirs == 0){ // All other snakes
      // If the snake has no valid moves, the snake is dead :(
      this->die();
      return;
    }
    uint8_t old_dir = this->dir;
    // If raycaster, use raycasting to move
    if(this->type == 15){
      this->dir = raycast(board, valid_dirs);
    } else if(this->type == 13){
      this->dir = evil_raycast(board, valid_dirs, snakes);
    } else {
      // Randomly change direction, increasing the chance of changing direction the longer the snake has been going in the same direction
      if(random(100) < 3 * (1.0/sqrt(len)) * t || !valid_dirs[this->dir]){
        do{
          this->dir = random(4);
        } while(!valid_dirs[this->dir]);
        t=0;
      }
      t++;
    }
    if(this->type == 16 && this->dir != old_dir){
      this->r1 = random(255);
      this->g1 = random(255);
      this->b1 = random(255);
    }


    // Move the snake
    std::pair<uint8_t, uint8_t> new_pos;
    uint8_t new_dir;
    std::tie(new_pos, new_dir) = check_move(this->row, this->col, this->dir);
    std::pair<uint8_t, uint16_t> board_vals = board[new_pos.first][new_pos.second];
    if(board_vals.second != 0){
      if(this->type == 13 && snakes[board_vals.first].type != 13 && snakes[board_vals.first].alive){ // Eater of worlds
        this->len += board_vals.second;
        snakes[board_vals.first].die();
      }
    }
    this->row = new_pos.first;
    this->col = new_pos.second;
    this->dir = new_dir;
    
    if(board[this->row][this->col].first == FOOD_ID){
      this->len+=2;
    } else if(this->type == 14){ // Infinite
      this->len++;
    }
    board[this->row][this->col].second = this->len * this->slow;
    board[this->row][this->col].first = this->id;
  }
  void die(){
    this->alive = false;
    this->respawn_delay = this->len * this->slow;
  }
  
  uint8_t raycast(std::pair<uint8_t, uint16_t> ** board, bool * valid_dirs){
    // u_int8_t distances[4] = {0,0,0,0};
    uint8_t maxD = 0;
    uint8_t maxI = 0;
    for(uint8_t i = 0; i < 4; i++){
      // if(!valid_dirs[i]){
      //   continue;
      // }
      std::pair<uint8_t, uint8_t> pos = {this->row, this->col};
      uint8_t dir = i;
      uint8_t d = 0;
      while(d < 200){
        std::pair<uint8_t, uint8_t> new_pos;
        uint8_t new_dir;
        std::tie(new_pos, new_dir) = check_move(pos.first, pos.second, dir);
        if(new_pos.first == 255){
          break;
        }
        if(board[new_pos.first][new_pos.second].second != 0){
          break;
        }
        pos = new_pos;
        dir = new_dir;
        d++;
      }
      if(d > maxD){
        maxD = d;
        maxI = i;
      }
    }
    return maxI;
  }

  uint8_t evil_raycast(std::pair<uint8_t, uint16_t> ** board, bool * valid_dirs, Snake * snakes){
    // u_int8_t distances[4] = {0,0,0,0};
    uint8_t minD = 255;
    uint8_t minI = 255;
    uint8_t maxD = 0;
    uint8_t maxI = 0;
    for(uint8_t i = 0; i < 4; i++){
      std::pair<uint8_t, uint8_t> pos = {this->row, this->col};
      uint8_t d = 0;
      uint8_t dir = i;
      while(d < 200){
        std::pair<uint8_t, uint8_t> new_pos;
        uint8_t new_dir;
        std::tie(new_pos, new_dir) = check_move(pos.first, pos.second, dir);
        if(new_pos.first == 255){
          break;
        }
        if(board[new_pos.first][new_pos.second].second != 0){
          if(snakes[board[new_pos.first][new_pos.second].first].type != 13 && snakes[board[new_pos.first][new_pos.second].first].alive && d < minD){
            minD = d;
            minI = i;
            break;
          } else if(snakes[board[new_pos.first][new_pos.second].first].type == 13){
            break;
          }
        }
        pos = new_pos;
        dir = new_dir;
        d++;
      }
      if(d > maxD){
        maxD = d;
        maxI = i;
      }
    }
    if(minI != 255){
      this->misc2 = 1;
      return minI;
    } else {
      this->misc2 = 0;
      return maxI;
    }
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
        std::pair<uint8_t,uint16_t> ** board; // 2D array representing the board, each element is a pair of uint8_t, the first is the snake id, the second is the length of the snake
        uint8_t n_snakes;
        uint16_t n_food;
        double infinite_vals[20] = {1.05,1.1,1.15,1.2,1.25,1.3,1.35,1.4,1.45,1.5,1.55,1.6,1.65,1.7,1.75,1.8,1.85,1.9,1.95,2};
        std::default_random_engine generator;
        std::lognormal_distribution<double> stasis_distribution = std::lognormal_distribution<double>(3.68,0.672);
        std::lognormal_distribution<double> slow_distribution = std::lognormal_distribution<double>(0,0.613);
        std::lognormal_distribution<double> segment_distribution = std::lognormal_distribution<double>(0.693,0.672);

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
          INFINITE = 14,
          RAYCASTER = 15,
          DISCO_TURN = 16,
          STASIS_SNAKE = 17
        };
        long snake_type_to_rarity[N_SNAKE_TYPES] = {
          100000, // Regular
          3000, // Gradient
          1500, // Alternating
          50, // Ghost
          40, // Sparkle
          70, // Pulsing
          10, // Strobe
          100, // Fade
          1500, // Static Alternating
          500, // Slow
          500, // Fast
          10, // Technicolor
          200, // Dashed
          1, // Eater of Worlds
          1, // Infinite
          100, // Raycaster
          50, // Disco Turn
          100 // Stasis Snake
        };
};

#endif