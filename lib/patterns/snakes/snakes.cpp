#include "snakes.h"
#include <utility>
#include <algorithm>

SnakeGame::SnakeGame()
{
  data.id = "snake";
  data.name = "Snake";
}

SnakeGame::~SnakeGame()
{
  end();
}

void SnakeGame::begin(PatternServices *services)
{
  this->pattern = services;
  this->frameCount = 0;
  this->len = 3;
  this->n_snakes = 25;
  this->n_food = 200;
  // Hardware RNG, so each run of the pattern gets different lognormal sizes.
  this->generator.seed(esp_random());
  this->board = (std::pair<uint8_t, uint16_t> **)heap_caps_malloc(PANEL_HEIGHT * sizeof(*this->board), MALLOC_CAP_SPIRAM);
  for (int i = 0; i < PANEL_HEIGHT; i++)
  {
    this->board[i] = (std::pair<uint8_t, uint16_t> *)heap_caps_malloc(PANEL_WIDTH * PANELS_NUMBER * sizeof(std::pair<uint8_t, uint16_t>), MALLOC_CAP_SPIRAM);
  }
  this->snakes = (Snake *)heap_caps_malloc(n_snakes * sizeof(Snake), MALLOC_CAP_SPIRAM);
  reset();
}

void SnakeGame::tick()
{
  this->update();
  this->draw();
  frameCount++;
}

void SnakeGame::end()
{
  // Cleared after freeing, so end() twice (or the destructor after an end())
  // does not free them again.
  if (this->board)
  {
    for (int i = 0; i < PANEL_HEIGHT; i++)
    {
      free(this->board[i]);
    }
    free(this->board);
    this->board = nullptr;
  }
  free(this->snakes);
  this->snakes = nullptr;
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
        // A living Infinite snake never shrinks, and a stasis snake's body
        // stays put while it is frozen.
        if((s->type != SnakeType::INFINITE || !s->alive) && (s->type != SnakeType::STASIS || s->stasis_left == 0)){
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
    if(snakes[i].type == SnakeType::STROBE){
      snakes[i].r1 = random(256);
      snakes[i].g1 = random(256);
      snakes[i].b1 = random(256);
    } else if(snakes[i].type == SnakeType::STASIS && snakes[i].stasis_left == 0){
      // Both colours wander while it moves, and hold while it is frozen.
      auto drift = [](uint8_t &c){ c = std::clamp(c + (int)random(41) - 20, 0, 255); };
      drift(snakes[i].r1);
      drift(snakes[i].g1);
      drift(snakes[i].b1);
      drift(snakes[i].r2);
      drift(snakes[i].g2);
      drift(snakes[i].b2);
    }
    if(snakes[i].alive){
      n_alive++;
      if(snakes[i].type != SnakeType::SLOW || frameCount % snakes[i].slow == 0){
        snakes[i].move(board, snakes);
      }
      // An Eater of Worlds with prey in sight (set by its first move) gets
      // a second move, like a Fast snake.
      if (snakes[i].alive && (snakes[i].type == SnakeType::FAST || (snakes[i].type == SnakeType::EATER_OF_WORLDS && snakes[i].hunting))){
        snakes[i].move(board, snakes);
      }
    }
  }
  if(n_alive == 0){
    reset();
  }
}

void SnakeGame::draw(){
  // Every pixel is drawn every frame, so whole rows are written straight into
  // the canvas. float rather than double throughout: the S3's FPU is single
  // precision, and double arithmetic is emulated in software.
  uint8_t *row = nullptr;
  auto put = [&](int j, uint8_t r, uint8_t g, uint8_t b)
  {
    uint8_t *px = row + j * 3;
    px[0] = r;
    px[1] = g;
    px[2] = b;
  };
  for(int i = 0; i < PANEL_HEIGHT; i++){
    row = pattern->display->rowForWrite(i, 0, PANEL_WIDTH * PANELS_NUMBER);
    if (row == nullptr) {
      return;
    }
    for(int j = 0; j < PANEL_WIDTH * PANELS_NUMBER; j++){
      if(this->board[i][j].second != 0){ // If there is a snake part here
        Snake * s = &snakes[this->board[i][j].first];
        uint8_t r = s->r1, g = s->g1, b = s->b1;
        float c1_factor = (float)(this->board[i][j].second - 1) / (s->len - 1);
        float c2_factor = (float)((s->len-1) - (this->board[i][j].second - 1)) / (s->len - 1);
        
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
          float brightness = 0;
          // Take the modulo before the cast: (int)frameCount % 120 goes
          // negative once frameCount passes INT_MAX.
          brightness += max(0,40 - (int)(frameCount % 120)) / 40.0f; // Fade out
          brightness += max(0,(int)(frameCount % 120) - 80) / 40.0f; // Fade in
          r = s->r1 * brightness;
          g = s->g1 * brightness;
          b = s->b1 * brightness;
        } else if(s->type == SnakeType::PULSING){
          float brightness = 0;
          brightness += max(0,(int)(frameCount % 30) - 25) / 5.0f; // Pulse in
          brightness += max(0,5 - (int)(frameCount % 30)) / 5.0f; // Pulse out
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
        } else if(s->type == SnakeType::EATER_OF_WORLDS){
          r = 100 * c1_factor  + 155 * c1_factor * (fast_cos((u_int8_t)(frameCount* 6)) / 255.0f);
          g = 0;
          b = 0;
        } else if(s->type == SnakeType::INFINITE){
          uint8_t c = (this->board[i][j].second - (frameCount * 2)) % 50;
          if(c < 20){
            float multiplier = infinite_vals[c];
            r = min((int)(s->r1 * multiplier),255);
            g = min((int)(s->g1 * multiplier),255);
            b = min((int)(s->b1 * multiplier),255);
          }
        } else if(s->type == SnakeType::RAYCASTER){
          // A blinking head in its colour on a body in the complement.
          if(this->board[i][j].second == s->len * s->slow){
            if(frameCount % 20 >= 10){
              r = min(255, s->r1 + 100);
              g = min(255, s->g1 + 100);
              b = min(255, s->b1 + 100);
            }
          } else {
            r = 255 - s->r1;
            g = 255 - s->g1;
            b = 255 - s->b1;
          }
        } else if(s->type == SnakeType::STASIS){
          r = (c1_factor * s->r1 + c2_factor * s->r2) / 2;
          g = (c1_factor * s->g1 + c2_factor * s->g2) / 2;
          b = (c1_factor * s->b1 + c2_factor * s->b2) / 2;
          if(s->stasis_left > 0){
            // Frozen: dim and flickering at first, recovering as the stasis
            // runs down (stasis_left counts down to 0).
            float k = 0.2f + 0.8f / s->stasis_left;
            r *= k;
            g *= k;
            b *= k;
            if(random(s->stasis_len) < s->stasis_left * 2 / 3){
              r = g = b = 0;
            }
          }
        }
        if(s->alive){
          put(j, r, g, b);
        } else {
          float brightness = s->respawn_delay / (float)s->len; 
          put(j, 
            min(255, (int)(r * brightness * 4)),
            min(255, (int)(g * brightness * 4)),
            min(255, (int)(b * brightness * 4))
          );
        }

      } else if (this->board[i][j].first == FOOD_ID){
        put(j, 100, 100, 100);
      } else { // Background
        put(j, 0, 0 , 0);
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
  snakes[i].dir = random(4);
  snakes[i].len = this->len;
  snakes[i].id = i;
  snakes[i].segment_len = 1;
  snakes[i].stasis_len = 0;
  snakes[i].stasis_left = 0;
  snakes[i].hunting = false;


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
      snakes[i].type = j;
      break;
    }
  }

  // Per-type sizes
  if(snakes[i].type == SnakeType::SLOW){
    snakes[i].slow = sample(slow_distribution, 2);
  } else if(snakes[i].type == SnakeType::ALTERNATING
         || snakes[i].type == SnakeType::STATIC_ALTERNATING
         || snakes[i].type == SnakeType::DASHED){
    snakes[i].segment_len = sample(segment_distribution, 1);
  } else if(snakes[i].type == SnakeType::STASIS){
    snakes[i].stasis_len = sample(stasis_distribution, STASIS_DURATION_MIN);
  }

  // Place the new snake and initialize the location
  do{
    snakes[i].col = random(PANEL_WIDTH * PANELS_NUMBER);
    snakes[i].row = random(PANEL_HEIGHT);
  } while(this->board[snakes[i].row][snakes[i].col].second != 0);
  this->board[snakes[i].row][snakes[i].col].second = this->len * snakes[i].slow;
  this->board[snakes[i].row][snakes[i].col].first = i;
}

uint8_t SnakeGame::sample(std::lognormal_distribution<float> &dist, uint8_t offset){
  float v = dist(generator) + offset;
  return v >= 255.0f ? 255 : (uint8_t)v;
}

void SnakeGame::place_food(){
  u_int8_t row, col;
  do{
    row = random(PANEL_HEIGHT);
    col = random(PANEL_WIDTH * PANELS_NUMBER);
  } while(this->board[row][col].first != SPACE_ID);
  this->board[row][col].first = FOOD_ID;
}
