#include "snakes.h"
#include <utility>

unsigned long frameCount = 25500;

// v----This function is for allocating memory on the external drive (we have more of that)
//ps_malloc()


SnakeGame::SnakeGame(MatrixPanel_I2S_DMA * display, uint8_t n_snakes = 10, uint8_t len = 10, uint8_t n_food = 100){
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
    snakes[i].alive = true;
    snakes[i].r = random(255);
    snakes[i].g = random(255);
    snakes[i].b = random(255);
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
}
void SnakeGame::update(){
  for(uint8_t i = 0; i < n_snakes; i++){
    if(snakes[i].alive){
      snakes[i].move(board);
    }
  }
  uint8_t count_food = 0;
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
  for(uint8_t i = 0; i < n_food - count_food; i++){
    place_food();
  }
}
void SnakeGame::draw(){
  for(int i = 0; i < PANEL_HEIGHT; i++){
    for(int j = 0; j < PANEL_WIDTH * PANELS_NUMBER; j++){
      if(this->board[i][j].second != 0){
        Snake * s = &snakes[this->board[i][j].first];
        display->drawPixelRGB888(j, i, s->r, s->g , s->b);
      } else if (this->board[i][j].first == FOOD_ID){
        display->drawPixelRGB888(j, i, 255, 255 , 255);
      } else {
        display->drawPixelRGB888(j, i, 0, 0 , 0);
      }
    }
  }
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
}