#include "snakes.h"
#include <utility>
#include <algorithm>

SnakeGame::SnakeGame()
{
  data.name = "Snake";
  this->generator.seed(rand());
}

SnakeGame::~SnakeGame(){
  stop();
}

void SnakeGame::init(PatternServices *pattern)
{
  this->pattern = pattern;
  this->len = 3;
  this->n_snakes = 25;
  this->n_food = 200;
  this->board = (std::pair<uint8_t, uint16_t> **)heap_caps_malloc(PANEL_HEIGHT * sizeof(uint8_t *), MALLOC_CAP_SPIRAM);
  for (int i = 0; i < PANEL_HEIGHT; i++)
  {
    this->board[i] = (std::pair<uint8_t, uint16_t> *)heap_caps_malloc(PANEL_WIDTH * PANELS_NUMBER * sizeof(std::pair<uint8_t, uint16_t>), MALLOC_CAP_SPIRAM);
  }
  this->snakes = (Snake *)heap_caps_malloc(n_snakes * sizeof(Snake), MALLOC_CAP_SPIRAM);
  reset();
} 

void SnakeGame::start()
{
  xTaskCreate(
      [](void *o)
      { static_cast<SnakeGame *>(o)->show(); }, // This is disgusting, but it works
      "Snake - Refresh",                        // Name of the task (for debugging)
      8000,                                     // Stack size (bytes)
      this,                                     // Parameter to pass
      1,                                        // Task priority
      &refreshTask                              // Task handle
  );
}

void SnakeGame::stop()
{
  vTaskDelete(refreshTask);
  for (int i = 0; i < PANEL_HEIGHT; i++)
  {
    free(this->board[i]);
  }
  free(this->board);
  free(this->snakes);
}

void SnakeGame::reset()
{
  for (int i = 0; i < PANEL_HEIGHT; i++)
  {
    for (int j = 0; j < PANEL_WIDTH * PANELS_NUMBER; j++)
    {
      this->board[i][j].first = SPACE_ID;
      this->board[i][j].second = 0;
    }
  }
  for (uint8_t i = 0; i < n_snakes; i++)
  {
    spawn_snake(i);
  }
}

void SnakeGame::update(){
  for(uint8_t i = 0; i < n_snakes; i++){
    if(!snakes[i].alive){
      snakes[i].respawn_delay--;
      if(snakes[i].respawn_delay <= 0){
        spawn_snake(i);
      }
    }
  }
  uint16_t count_food = 0;
  for(int i = 0; i < PANEL_HEIGHT; i++){
    for(int j = 0; j < PANEL_WIDTH * PANELS_NUMBER; j++){
      if(this->board[i][j].second != 0){
        Snake * s = &snakes[this->board[i][j].first];
        if((s->type != SnakeType::INFINITE || !s->alive) && (s->type != SnakeType::STASIS_SNAKE || s->misc2 == 0)){
          this->board[i][j].second--;
          if(this->board[i][j].second == 0){
            this->board[i][j].first = SPACE_ID;
          }
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
    switch(snakes[i].type){
      case SnakeType::STROBE:{
        snakes[i].r1 = random(256);
        snakes[i].g1 = random(256);
        snakes[i].b1 = random(256);
        break;
      }
      case SnakeType::STASIS_SNAKE:{
        if(snakes[i].misc2 == 0){
          snakes[i].r1 = max(0,min(255,snakes[i].r1 + (rand()%41 - 20)));
          snakes[i].g1 = max(0,min(255,snakes[i].g1 + (rand()%41 - 20)));
          snakes[i].b1 = max(0,min(255,snakes[i].b1 + (rand()%41 - 20)));
          snakes[i].r2 = max(0,min(255,snakes[i].r2 + (rand()%41 - 20)));
          snakes[i].g2 = max(0,min(255,snakes[i].g2 + (rand()%41 - 20)));
          snakes[i].b2 = max(0,min(255,snakes[i].b2 + (rand()%41 - 20)));
        }
        break;
      }
    }
    if(snakes[i].alive){
      n_alive++;
      if(snakes[i].type != SnakeType::SLOW || frameCount % snakes[i].slow == 0){
        snakes[i].move(board, snakes);
      }
      if (snakes[i].type == SnakeType::FAST || (snakes[i].type == SnakeType::EATER_OF_WORLDS && snakes[i].misc2 == 1)){
        snakes[i].move(board, snakes);
      }
    }
  }
  if(n_alive == 0){
    reset();
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
        
        switch(s->type){
          // Multicolor gradient snake
          case SnakeType::GRADIENT:{
            r = (c1_factor * s->r1 + c2_factor * s->r2) / 2;
            g = (c1_factor * s->g1 + c2_factor * s->g2) / 2;
            b = (c1_factor * s->b1 + c2_factor * s->b2) / 2;
            break;
          }
          case SnakeType::ALTERNATING:{
            if(this->board[i][j].second / s->misc1 % 2 == 0){
              r = s->r1;
              g = s->g1;
              b = s->b1;
            } else {
              r = s->r2;
              g = s->g2;
              b = s->b2;
            }
            break;
          }
          case SnakeType::GHOST:{
            r = random(20);
            g = random(20);
            b = random(20);
            break;
          }
          case SnakeType::SPARKLE:{
            if(random(10) == 0){
              r = min(255, s->r1 * 4);
              g = min(255, s->g1 * 4);
              b = min(255, s->b1 * 4);
            } else {
              r = s->r1;
              g = s->g1;
              b = s->b1;
            }
            break;
          }
          case SnakeType::STROBE:{
            // Actual color is set in update()
            r = s->r1;
            g = s->g1;
            b = s->b1;
            break;
          }
          case SnakeType::STATIC_ALTERNATING:{
            if((i+j) / s->misc1 % 2 == 0){
              r = s->r1;
              g = s->g1;
              b = s->b1;
            } else {
              r = s->r2;
              g = s->g2;
              b = s->b2;
            }
            break;
          }
          case SnakeType::FADE:{
            double brightness = 0;
            brightness += max(0,40 - (int)(frameCount % 120)) / 40.0; // Fade out
            brightness += max(0,(int)(frameCount % 120) - 80) / 40.0; // Fade in
            r = s->r1 * brightness;
            g = s->g1 * brightness;
            b = s->b1 * brightness;
            break;
          }
          case SnakeType::PULSING:{
            double brightness = 0;
            brightness += max(0,(int)(frameCount % 30) - 25) / 5.0; // Pulse in
            brightness += max(0,5 - (int)(frameCount % 30)) / 5.0; // Pulse out
            r = min(255, (int)(s->r1 * (1 + brightness)));
            g = min(255, (int)(s->g1 * (1 + brightness)));
            b = min(255, (int)(s->b1 * (1 + brightness)));
            break;
          }
          case SnakeType::TECHNICOLOR:{
            srand(s->id + board[i][j].second);
            r = rand()%256;
            g = rand()%256;
            b = rand()%256;
            srand(rand());
            r = rand()%256;
            break;
          }
          case SnakeType::DASHED:{
            if(this->board[i][j].second / s->misc1 % 2 == 0){
              r = s->r1;
              g = s->g1;
              b = s->b1;
            } else {
              r = 0;
              g = 0;
              b = 0;
            }
            break;
          }
          case SnakeType::RAYCASTER:{
            if(this->board[i][j].second == s->len){
              if(frameCount % 20 < 10){
                r = s->r1;
                g = s->g1;
                b = s->b1;
              } else {
                r = min(255, s->r1+100);
                g = min(255, s->g1+100);
                b = min(255, s->b1+100);
              }
            }
            else {
              r = 255 - s->r1;
              g = 255 - s->g1;
              b = 255 - s->b1;
            }
            break;
          }
          case SnakeType::EATER_OF_WORLDS:{
            r = 100 * c1_factor  + 155 * c1_factor * (fast_cos((u_int8_t)(frameCount* 6)) / 255.0);
            g = 0;
            b = 0;
            break;
          }
          case SnakeType::INFINITE:{
            uint8_t c = (this->board[i][j].second - (frameCount * 2)) % 50;
            if(c < 20){
              double multiplier = infinite_vals[c];
              r = min((int)(s->r1 * multiplier),255);
              g = min((int)(s->g1 * multiplier),255);
              b = min((int)(s->b1 * multiplier),255);
            }
            break;
          }
          case SnakeType::STASIS_SNAKE:{
            r = (c1_factor * s->r1 + c2_factor * s->r2) / 2;
            g = (c1_factor * s->g1 + c2_factor * s->g2) / 2;
            b = (c1_factor * s->b1 + c2_factor * s->b2) / 2;
            if(s->misc2 > 0){
              r = (int)(r/5.0 + r*4.0/5/s->misc2);
              g = (int)(g/5.0 + g*4.0/5/s->misc2);
              b = (int)(b/5.0 + b*4.0/5/s->misc2);
              if(random(s->misc1) < s->misc2 * 2 / 3){
                r = 0;
                g = 0;
                b = 0;
              }
            }
            break;
          }
        }
        if(s->alive){
          pattern->display->drawPixelRGB888(j, i, r, g, b);
        } else {
          double brightness = s->respawn_delay / (double)s->len; 
          pattern->display->drawPixelRGB888(j, i, 
            min(255, (int)(r * brightness * 4)),
            min(255, (int)(g * brightness * 4)),
            min(255, (int)(b * brightness * 4))
          );
        }

      } else if (this->board[i][j].first == FOOD_ID){
        pattern->display->drawPixelRGB888(j, i, 100, 100, 100);
      } else { // Background
        pattern->display->drawPixelRGB888(j, i, 0, 0 , 0);
      }
    }
  }
  // // Top panel
  // pattern->display->drawPixelRGB888(0,0,255,0,0);
  // pattern->display->drawPixelRGB888(0,63,0,255,0);
  // pattern->display->drawPixelRGB888(63,0,0,0,255);
  // pattern->display->drawPixelRGB888(63,63,255,255,255);

  // // Right panel
  // pattern->display->drawPixelRGB888(64,0,255,0,0);
  // pattern->display->drawPixelRGB888(64,63,0,255,0);
  // pattern->display->drawPixelRGB888(127,0,0,0,255);
  // pattern->display->drawPixelRGB888(127,63,255,255,0);

  // // Left panel  
  // pattern->display->drawPixelRGB888(128,0,255,0,0);
  // pattern->display->drawPixelRGB888(128,63,0,255,0);
  // pattern->display->drawPixelRGB888(191,0,0,0,255);
  // pattern->display->drawPixelRGB888(191,63,0,255,255);
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
  snakes[i].dir = random(3);
  snakes[i].len = this->len;
  snakes[i].id = i;
  snakes[i].misc1 = 0;
  snakes[i].misc2 = 0;


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

  // Special modification for various snakes
  switch(snakes[i].type){
    case SnakeType::SLOW:{
      snakes[i].slow =  min(255.0,this->slow_distribution(this->generator) + 2);
      break;
    } case SnakeType::ALTERNATING:{
      snakes[i].misc1 = min(255.0, this->segment_distribution(this->generator) + 1);
      break;
    } case SnakeType::STATIC_ALTERNATING:{
      snakes[i].misc1 = min(255.0, this->segment_distribution(this->generator) + 1);
      break;
    } case SnakeType::DASHED:{
      snakes[i].misc1 = min(255.0, this->segment_distribution(this->generator) + 1);
      break;
    } case SnakeType::STASIS_SNAKE:{
      snakes[i].misc1 = min(255.0,this->stasis_distribution(this->generator) + STASIS_DURATION_MIN);
      break;
    }
  }

  // Place the new snake and initialize the location
  do{
    snakes[i].col = random(PANEL_WIDTH * PANELS_NUMBER);
    snakes[i].row = random(PANEL_HEIGHT);
  } while(this->board[snakes[i].row][snakes[i].col].second != 0);
  this->board[snakes[i].row][snakes[i].col].second = this->len * snakes[i].slow;
  this->board[snakes[i].row][snakes[i].col].first = i;
}

void SnakeGame::place_food(){
  u_int8_t row, col;
  do{
    row = random(PANEL_HEIGHT);
    col = random(PANEL_WIDTH * PANELS_NUMBER);
  } while(this->board[row][col].first != SPACE_ID);
  this->board[row][col].first = FOOD_ID;
}

void SnakeGame::show(){
  while(true){
    this->update();
    this->draw();

    frameCount++;
    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
}