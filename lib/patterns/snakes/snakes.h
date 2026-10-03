#ifndef SNAKES_H
#define SNAKES_H

#include <Arduino.h>
#include <utility>
#include "cube_utils.h"

const uint8_t FOOD_ID = 254;
const uint8_t SPACE_ID = 255;
const uint8_t N_SNAKE_TYPES = 15;

// One step across the cube surface for a snake at (row, col) heading dir,
// as {row, col}, or {255, 255} at the cube's outer edge. `dir` is updated to
// the heading on arrival: crossing a seam between faces rotates it, and
// keeping the old heading is what made snakes bounce back at every seam.
inline std::pair<uint8_t, uint8_t> check_move(uint8_t row, uint8_t col, uint8_t &dir)
{
  cube::Step s = cube::step(cube::Point{int16_t(col), int16_t(row)}, cube::Dir(dir & 3));
  if (!s.valid())
  {
    return std::make_pair(255, 255);
  }
  dir = s.dir;
  return std::make_pair(uint8_t(s.to.y), uint8_t(s.to.x));
}

// struct representing a snake. Each snake has a position, direction, color, head
// direction is 0-3, 0 is up, 1 is right, 2 is down, 3 is left
struct Snake{
  uint8_t r1, r2, g1, g2, b1, b2, dir, col, row, t, id, type, slow, segment_len;
  uint16_t len, respawn_delay;
  bool alive;
  void move(std::pair<uint8_t, uint16_t> ** board, Snake * snakes){

    bool valid_dirs[4] = {true, true, true, true};
    uint8_t n_dirs = 4;
    // valid_dirs[(this->dir + 2) % 4] = false;
    for(uint8_t i = 0 ; i < 4; i++){
      uint8_t heading = i;
      std::pair<uint8_t, uint8_t> new_pos = check_move(this->row, this->col, heading);
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
    if(random(1000) < 30 * (1.0f/sqrtf(len)) * t || !valid_dirs[this->dir]){
      do{
        this->dir = random(4);
      } while(!valid_dirs[this->dir]);
      t=0;
    }
    t++;

    // Move the snake

    // Updates dir when the move crosses a seam, so the snake carries on
    // across the new face instead of turning straight back.
    std::pair<uint8_t, uint8_t> new_pos = check_move(this->row, this->col, this->dir);
    std::pair<uint8_t, uint16_t> board_vals = board[new_pos.first][new_pos.second];
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
};

class SnakeGame: public Pattern{
    public:
        SnakeGame();
        ~SnakeGame();
        void begin(PatternServices *services) override;
        void tick() override;
        void end() override;
        // One game step per tick, and a snake moves one cell per step, so this
        // sets their speed: about 30 steps a second, as before the render loop
        // could go faster.
        uint32_t frameInterval() const override { return 33; }

      private:
        uint8_t len; // Starting length of all snakes
        Snake * snakes = nullptr; // Array of all snakes in the game
        std::pair<uint8_t,uint16_t> ** board = nullptr; // 2D array representing the board, each element is a pair of uint8_t, the first is the snake id, the second is the length of the snake
        uint8_t n_snakes;
        uint16_t n_food;
        float infinite_vals[20] = {1.05f,1.1f,1.15f,1.2f,1.25f,1.3f,1.35f,1.4f,1.45f,1.5f,1.55f,1.6f,1.65f,1.7f,1.75f,1.8f,1.85f,1.9f,1.95f,2.0f};

        void reset();
        void update();
        void draw();
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
          1, // Infinite
        };
};

#endif