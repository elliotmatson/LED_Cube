#include "snakes.h"
#include <utility>
#include <algorithm>

unsigned long frameCount = 0;

struct Color{
  uint8_t r, g, b;
};

Color transform_hue(Color * c, float angle){
  float U = cos(angle*M_PI/180);
  float W = sin(angle*M_PI/180);

  Color ret;
  ret.r = (.299+.701*U+.168*W)*c->r
    + (.587-.587*U+.330*W)*c->g
    + (.114-.114*U-.497*W)*c->b;
  ret.g = (.299-.299*U-.328*W)*c->r
    + (.587+.413*U+.035*W)*c->g
    + (.114-.114*U+.292*W)*c->b;
  ret.b = (.299-.3*U+1.25*W)*c->r
    + (.587-.588*U-1.05*W)*c->g
    + (.114+.886*U-.203*W)*c->b;
  return ret;
}

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
      snakes[i].move(board);
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
        uint8_t r = 0, g = 0, b = 0;
        double c1_factor = (double)(this->board[i][j].second - 1) / (s->len - 1);
        double c2_factor = (double)((s->len-1) - (this->board[i][j].second - 1)) / (s->len - 1);
        
        // Multicolor gradient snake
        if(s->type == SnakeType::GRADIENT){
          r = (c1_factor * s->r1 + c2_factor * s->r2) / 2;
          g = (c1_factor * s->g1 + c2_factor * s->g2) / 2;
          b = (c1_factor * s->b1 + c2_factor * s->b2) / 2;
        } else if(s->type == SnakeType::REGULAR){
          r = s->r1;
          g = s->g1;
          b = s->b1;
        } else if(s->type == SnakeType::ALTERNATING){
          if(this->board[i][j].second / 4 % 2 == 0){
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
        } else if(s->type == SnakeType::CHROMATIC){
          Color c;
          c.r = s->r1;
          c.g = s->g1;
          c.b = s->b1;
          c = transform_hue(&c, frameCount % 360);
          r = c.r;
          g = c.g;
          b = c.b;
        } else if(s->type == SnakeType::STROBE){
          // Actual color is set in update()
          r = s->r1;
          g = s->g1;
          b = s->b1;
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

  // Determines how the snake is colored
  int typeGen = random(1000);
  if(typeGen < 40){
    snakes[i].type = SnakeType::GRADIENT;
  } else if(typeGen < 80){
    snakes[i].type = SnakeType::ALTERNATING;
  } else if(typeGen < 81 ){
    snakes[i].type = SnakeType::GHOST;
  } else if(typeGen < 82){
    snakes[i].type = SnakeType::SPARKLE;
  } else if(typeGen < 83){
    snakes[i].type = SnakeType::STROBE;
  } 
  else {
    snakes[i].type = SnakeType::REGULAR;
  }

  // snakes[i].r1 = min(255, snakes[i].r2 + 50);
  // snakes[i].g1 = min(255, snakes[i].g2 + 50);
  // snakes[i].b1 = min(255, snakes[i].b2 + 50);

  snakes[i].t = 0;
  snakes[i].dir = 0;
  snakes[i].len = this->len;
  snakes[i].id = i;
  do{
    snakes[i].col = random(PANEL_WIDTH * PANELS_NUMBER);
    snakes[i].row = random(PANEL_HEIGHT);
  } while(this->board[snakes[i].row][snakes[i].col].first != 0);
  this->board[snakes[i].row][snakes[i].col].second = this->len;
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

  display->drawPixelRGB888(0, 0, 255, 0, 0); // Red
  display->drawPixelRGB888(63, 0, 0, 255, 0); // Green
  display->drawPixelRGB888(63, 63, 0, 0, 255); // Blue
  display->drawPixelRGB888(0, 63, 255, 255, 255); // White

  display->drawPixelRGB888(64, 0, 255, 0, 0);
  display->drawPixelRGB888(127, 0, 0, 255, 0);
  display->drawPixelRGB888(127, 63, 0, 0, 255);
  display->drawPixelRGB888(64, 63, 255, 255, 0); // Yellow

  display->drawPixelRGB888(128, 0, 255, 0, 0);
  display->drawPixelRGB888(191, 0, 0, 255, 0);
  display->drawPixelRGB888(191, 63, 0, 0, 255);
  display->drawPixelRGB888(128, 63, 0, 255, 255); // Cyan
  frameCount++;
}