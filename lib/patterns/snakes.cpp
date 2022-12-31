#include "snakes.h"
#include <utility>
#include <algorithm>

unsigned long frameCount = 0;

SnakeGame::SnakeGame(MatrixPanel_I2S_DMA * display, uint8_t n_snakes = 10, uint8_t len = 10, uint16_t n_food = 100){
  this->display = display;
  this->len = len;
  this->n_snakes = n_snakes;
  this->board = (std::pair<uint8_t,uint8_t> **)malloc(PANEL_HEIGHT * sizeof(uint8_t *));
  for(int i = 0; i < PANEL_HEIGHT; i++){
    this->board[i] = (std::pair<uint8_t,uint8_t> *)malloc(PANEL_WIDTH * PANELS_NUMBER * sizeof(std::pair<uint8_t,uint8_t>));
  }
  this->snakes = (Snake *)malloc(n_snakes * sizeof(Snake));
  this->n_food = n_food;
}
SnakeGame::~SnakeGame(){
  for(int i = 0; i < PANEL_HEIGHT; i++){
    free(this->board[i]);
  }
  free(this->board);
  free(this->snakes);
}
void SnakeGame::init(){
  for(int i = 0; i < PANEL_HEIGHT; i++){
    for(int j = 0; j < PANEL_WIDTH * PANELS_NUMBER; j++){
      this->board[i][j].first = 0;
      this->board[i][j].second = 0;
    }
  }
  for(uint8_t i = 0; i < n_snakes; i++){
    spawn_snake(i);
  }
}
void SnakeGame::update(){
  for(uint8_t i = 0; i < n_snakes; i++){
    if(!snakes[i].alive){
      snakes[i].respawn_delay--;
      if(snakes[i].respawn_delay == 0){
        spawn_snake(i);
      }
    }
  }
  uint16_t count_food = 0;
  for(int i = 0; i < PANEL_HEIGHT; i++){
    for(int j = 0; j < PANEL_WIDTH * PANELS_NUMBER; j++){
      if(this->board[i][j].second != 0){
        this->board[i][j].second--;
        if(this->board[i][j].second == 0){
          this->board[i][j].first = 0;
        }
      }
      if(this->board[i][j].first == FOOD_ID){
        count_food++;
      }
    }
  }
  for(uint16_t i = 0; i < n_food - count_food; i++){
    place_food();
  }
  uint8_t n_alive = 0;
  for(uint8_t i = 0; i < n_snakes; i++){
    if(snakes[i].type == SnakeType::STROBE){
      snakes[i].r1 = random(256);
      snakes[i].g1 = random(256);
      snakes[i].b1 = random(256);
    }
    if(snakes[i].alive){
      n_alive++;
      if(snakes[i].type != SnakeType::SLOW){
        snakes[i].move(board);
      } else if(frameCount % snakes[i].slow == 0){
          snakes[i].move(board);
      }
      if (snakes[i].type == SnakeType::FAST){
        snakes[i].move(board);
      }
    }
  }
  if(n_alive == 0){
    init();
  }
}
void SnakeGame::draw(){
  for(int i = 0; i < PANEL_HEIGHT; i++){
    for(int j = 0; j < PANEL_WIDTH * PANELS_NUMBER; j++){
      if(this->board[i][j].second != 0){ // If there is a snake part here
        Snake * s = &snakes[this->board[i][j].first];
        uint8_t r = s->r1, g = s->g1, b = s->b1;
        double c1_factor = (double)(this->board[i][j].second - 1) / (s->len - 1);
        double c2_factor = (double)((s->len-1) - (this->board[i][j].second - 1)) / (s->len - 1);
        
        // Multicolor gradient snake
        if(s->type == SnakeType::GRADIENT){
          r = (c1_factor * s->r1 + c2_factor * s->r2) / 2;
          g = (c1_factor * s->g1 + c2_factor * s->g2) / 2;
          b = (c1_factor * s->b1 + c2_factor * s->b2) / 2;
        } else if(s->type == SnakeType::ALTERNATING){
          if(this->board[i][j].second / s->segment_len % 2 == 0){
            r = s->r1;
            g = s->g1;
            b = s->b1;
          } else {
            r = s->r2;
            g = s->g2;
            b = s->b2;
          }
        } else if(s->type == SnakeType::GHOST){
          r = random(20);
          g = random(20);
          b = random(20);
        } else if(s->type == SnakeType::SPARKLE){
          if(random(10) == 0){
            r = min(255, s->r1 * 4);
            g = min(255, s->g1 * 4);
            b = min(255, s->b1 * 4);
          } else {
            r = s->r1;
            g = s->g1;
            b = s->b1;
          }
        } else if(s->type == SnakeType::STROBE){
          // Actual color is set in update()
          r = s->r1;
          g = s->g1;
          b = s->b1;
        } else if(s->type == SnakeType::STATIC_ALTERNATING){
          if((i+j) / s->segment_len % 2 == 0){
            r = s->r1;
            g = s->g1;
            b = s->b1;
          } else {
            r = s->r2;
            g = s->g2;
            b = s->b2;
          }
        } else if(s->type == SnakeType::FADE){
          double brightness = 0;
          brightness += max(0,((int)frameCount % 120) - 80) / 40.0; // Fade in
          brightness += max(0,(40 - (int)frameCount % 120)) / 40.0;
          r = s->r1 * brightness;
          g = s->g1 * brightness;
          b = s->b1 * brightness;
        } else if(s->type == SnakeType::PULSING){
          double brightness = 0;
          brightness += max(0,((int)frameCount % 30) - 25) / 5.0; // Pulse in
          brightness += max(0,(5 - (int)frameCount % 30)) / 5.0; // Pulse out
          r = min(255, (int)(s->r1 * (1 + brightness)));
          g = min(255, (int)(s->g1 * (1 + brightness)));
          b = min(255, (int)(s->b1 * (1 + brightness))); 
        } else if(s->type == SnakeType::TECHNICOLOR){
          srand(s->id + board[i][j].second);
          r = rand()%256;
          g = rand()%256;
          b = rand()%256;
        } else if(s->type == SnakeType::DASHED){
          if(this->board[i][j].second / s->segment_len % 2 == 0){
            r = s->r1;
            g = s->g1;
            b = s->b1;
          } else {
            r = 0;
            g = 0;
            b = 0;
          }
        }
        if(s->alive){
          display->drawPixelRGB888(j, i, r, g, b);
        } else {
          double brightness = s->respawn_delay / (double)s->len; 
          display->drawPixelRGB888(j, i, 
            min(255, (int)(r * brightness * 4)),
            min(255, (int)(g * brightness * 4)),
            min(255, (int)(b * brightness * 4))
          );
        }

      } else if (this->board[i][j].first == FOOD_ID){
        display->drawPixelRGB888(j, i, 100, 100, 100);
      } else {
        display->drawPixelRGB888(j, i, 0, 0 , 0);
      }
    }
  }
}

void SnakeGame::spawn_snake(uint8_t i){
  snakes[i].alive = true;
  snakes[i].r2 = random(255);
  snakes[i].g2 = random(255);
  snakes[i].b2 = random(255);

  snakes[i].r1 = random(255);
  snakes[i].g1 = random(255);
  snakes[i].b1 = random(255);

  snakes[i].slow = 1;
  snakes[i].t = 0;
  snakes[i].dir = 0;
  snakes[i].len = this->len;
  snakes[i].id = i;
  snakes[i].segment_len = 1;


  // Determines the snake type
  int sum = 0;
  for(int j = 0; j < N_SNAKE_TYPES; j++){
    sum += snake_type_to_rarity[j];
  }
  int typeGen = random(sum);
  sum = 0;
  for(int j = 0; j < N_SNAKE_TYPES; j++){
    sum += snake_type_to_rarity[j];
    if(typeGen < sum){
      snakes[i].type = (SnakeType)j;
      break;
    }
  }

  // Special modification for slow snakes
  if(snakes[i].type == SnakeType::SLOW){
    snakes[i].slow = random(3) + 2;
  } else if(snakes[i].type == SnakeType::ALTERNATING){
    snakes[i].segment_len = random(7) + 1;
  } else if(snakes[i].type == SnakeType::STATIC_ALTERNATING){
    snakes[i].segment_len = random(7) + 1;
  } else if(snakes[i].type == SnakeType::DASHED){
    snakes[i].segment_len = random(7) + 1;
  }

  // Place the new snake and initialize the location
  do{
    snakes[i].col = random(PANEL_WIDTH * PANELS_NUMBER);
    snakes[i].row = random(PANEL_HEIGHT);
  } while(this->board[snakes[i].row][snakes[i].col].first != 0);
  this->board[snakes[i].row][snakes[i].col].second = this->len * snakes[i].slow;
  this->board[snakes[i].row][snakes[i].col].first = i;
}

void SnakeGame::place_food(){
  u_int8_t row, col;
  do{
    row = random(PANEL_HEIGHT);
    col = random(PANEL_WIDTH * PANELS_NUMBER);
  } while(this->board[row][col].first != 0);
  this->board[row][col].first = FOOD_ID;
}

void SnakeGame::show(){
  this->update();
  this->draw();

  frameCount++;
}