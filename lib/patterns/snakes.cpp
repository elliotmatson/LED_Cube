#include "snakes.h"
#include <utility>

unsigned long frameCount = 25500;

// v----This function is for allocating memory on the external drive (we have more of that)
//ps_malloc()

SnakeGame::SnakeGame(MatrixPanel_I2S_DMA *display, uint8_t n_snakes, uint8_t len = 10)
{
  this->display = display;
  this->len = len;
  this->n_snakes = n_snakes;
  this->board = (std::pair<uint8_t,uint8_t> **)malloc(PANEL_HEIGHT * sizeof(uint8_t *));
  for(int i = 0; i < PANEL_HEIGHT; i++){
    this->board[i] = (std::pair<uint8_t,uint8_t> *)malloc(PANEL_WIDTH * PANELS_NUMBER * sizeof(std::pair<uint8_t,uint8_t>));
  }
  this->snakes = (Snake *)malloc(n_snakes * sizeof(Snake));
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
    } while(this->board[snakes[i].row][snakes[i].col].second != 0);
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
  for(int i = 0; i < PANEL_HEIGHT; i++){
    for(int j = 0; j < PANEL_WIDTH * PANELS_NUMBER; j++){
      if(this->board[i][j].second != 0){
        this->board[i][j].second--;
      }
    }
  }
}
void SnakeGame::draw(){
  for(int i = 0; i < PANEL_HEIGHT; i++){
    for(int j = 0; j < PANEL_WIDTH * PANELS_NUMBER; j++){
      if(this->board[i][j].second != 0){
        Snake * s = &snakes[this->board[i][j].first];
        display->drawPixelRGB888(j, i, s->r, s->g , s->b);
      } else{
        display->drawPixelRGB888(j, i, 0, 0 , 0);
      }
    }
  }
}

void SnakeGame::show(){
  this->update();
  this->draw();
  Serial.print(this->fast_cos(0));
}