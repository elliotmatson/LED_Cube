#ifndef VIRTUAL_DISPLAYS_H
#define VIRTUAL_DISPLAYS_H

#include "ESP32-HUB75-MatrixPanel-I2S-DMA.h"

struct VirtualCoords
{
    int16_t x;
    int16_t y;
    int16_t virt_row; // chain of panels row
    int16_t virt_col; // chain of panels col

    VirtualCoords() : x(0), y(0)
    {
    }
};

class SinglePanel : public Adafruit_GFX
{
public:
    int16_t virtualResX;
    int16_t virtualResY;

    MatrixPanel_I2S_DMA *display;

    SinglePanel(MatrixPanel_I2S_DMA &disp, int panel, int rotate)
        : Adafruit_GFX(disp.getCfg().mx_width, disp.getCfg().mx_height)
    {
        this->display = &disp;

        virtualResX = disp.getCfg().mx_width;
        virtualResY = disp.getCfg().mx_height;

        if (panel < disp.getCfg().chain_length)
        {
            _panel = panel;
        }

        if (rotate < 4){
            _rotate = rotate;
        }

        coords.x = coords.y = -1; // By default use an invalid co-ordinates that will be rejected by updateMatrixDMABuffer
    }

    // equivalent methods of the matrix library so it can be just swapped out.
    virtual void drawPixel(int16_t x, int16_t y, uint16_t color);
    virtual void fillScreen(uint16_t color); // overwrite adafruit implementation
    virtual void fillScreenRGB888(uint8_t r, uint8_t g, uint8_t b);

    void clearScreen() { fillScreen(0); }
    void drawPixelRGB888(int16_t x, int16_t y, uint8_t r, uint8_t g, uint8_t b);

    uint16_t color444(uint8_t r, uint8_t g, uint8_t b) { return display->color444(r, g, b); }
    uint16_t color565(uint8_t r, uint8_t g, uint8_t b) { return display->color565(r, g, b); }
    uint16_t color333(uint8_t r, uint8_t g, uint8_t b) { return display->color333(r, g, b); }

    void flipDMABuffer() { display->flipDMABuffer(); }
    void setRotation(int rotate);
    void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color);
    void drawFastHLine(int16_t x, int16_t y, int16_t w, uint8_t r, uint8_t g, uint8_t b);
    void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color);
    void drawFastVLine(int16_t x, int16_t y, int16_t h, uint8_t r, uint8_t g, uint8_t b);

    void drawSprite8(const uint8_t *sprite, int16_t x, int16_t y, uint16_t w, uint16_t h, uint8_t r, uint8_t g, uint8_t b, bool fill = false);
    void drawSprite8(const uint8_t *sprite, int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t color, bool fill = false);
    void drawSprite16(const uint16_t *sprite, int16_t x, int16_t y, uint16_t w, uint16_t h, uint8_t r, uint8_t g, uint8_t b, bool fill = false);
    void drawSprite16(const uint16_t *sprite, int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t color, bool fill = false);

protected:
    virtual VirtualCoords getCoords(int16_t &x, int16_t &y);
    VirtualCoords coords;

    int _panel = 0;
    int _rotate = 0;

}; // end Class header

/// @brief  Calculate virtual->real co-ordinate mapping to underlying single chain of panels connected to ESP32.
///         Updates the private class member variable 'coords', so no need to use the return value.
///         Not thread safe, but not a concern for ESP32 sketch anyway... I think.
/// @param x  Pixel X co-ordinate
/// @param y  Pixel Y co-ordinate
/// @return VirtualCoords object
inline VirtualCoords SinglePanel::getCoords(int16_t &x, int16_t &y)
{
    coords.x = coords.y = -1; // By defalt use an invalid co-ordinates that will be rejected by updateMatrixDMABuffer

    // Do we want to rotate?
    int16_t temp_x = x;
    switch(_rotate) {
        case 1:
            // 90 degrees
            x = y;
            y = virtualResY - 1 - temp_x;
            break;
        case 2:
            // 180 degrees
            x = virtualResX - 1 - x;
            y = virtualResY - 1 - y;
            break;
        case 3:
            // 270 degrees
            x = virtualResX - 1 - y;
            y = temp_x;
            break;
    }

    if (x < 0 || x >= virtualResX || y < 0 || y >= virtualResY)
    { // Co-ordinates go from 0 to X-1 remember! otherwise they are out of range!
        return coords;
    }

    // Calculate the real co-ordinates of the pixel on the underlying single chain of panels.
    coords.x = x + (_panel * virtualResX);
    coords.y = y;

    return coords;
}

/// @brief Rotate the display CCW (does not rotate current contents)
/// @param rotate 0 = No rotation, 1 = 90 degrees, 2 = 180 degrees, 3 = 270 degrees
inline void SinglePanel::setRotation(int rotate)
{
    if (rotate < 4)
    {
        _rotate = rotate;
    }
}

/// @brief Draw a single pixel
/// @param x  Pixel X co-ordinate
/// @param y  Pixel Y co-ordinate
/// @param color  565 color
inline void SinglePanel::drawPixel(int16_t x, int16_t y, uint16_t color)
{ // adafruit virtual void override
    getCoords(x, y);
    this->display->drawPixel(coords.x, coords.y, color);
}

/// @brief  Draw a single pixel
/// @param x  Pixel X co-ordinate
/// @param y  Pixel Y co-ordinate
/// @param r  Red
/// @param g  Green
/// @param b  Blue
inline void SinglePanel::drawPixelRGB888(int16_t x, int16_t y, uint8_t r, uint8_t g, uint8_t b)
{
    getCoords(x, y);
    this->display->drawPixelRGB888(coords.x, coords.y, r, g, b);
}

/// @brief Fill the screen with a single color
/// @param color 565 color
inline void SinglePanel::fillScreen(uint16_t color)
{ // adafruit virtual void override
    this->display->fillRect((_panel * virtualResX), 0, this->virtualResX, this->virtualResY, color);
}

/// @brief  Fill the screen with a single color
/// @param r  Red
/// @param g  Green
/// @param b  Blue
inline void SinglePanel::fillScreenRGB888(uint8_t r, uint8_t g, uint8_t b)
{
    this->display->fillRect((_panel * virtualResX), 0, this->virtualResX, this->virtualResY, r, g, b);
}

/// @brief Draw a horizontal line
/// @param x  Starting pixel X co-ordinate
/// @param y  Starting pixel Y co-ordinate
/// @param w  Line width
/// @param r  Red
/// @param g  Green
/// @param b  Blue
inline void SinglePanel::drawFastHLine(int16_t x, int16_t y, int16_t w, uint8_t r, uint8_t g, uint8_t b)
{
    switch(_rotate) {
        case 1:
            // 90 degrees
            this->display->drawFastVLine((_panel * virtualResX) + y, virtualResY - 1 - x - w + 1, w, r, g, b);
            break;
        case 2:
            // 180 degrees
            this->display->drawFastHLine((_panel * virtualResX) + virtualResX - 1 - x - w + 1, virtualResY - 1 - y, w, r, g, b);
            break;
        case 3:
            // 270 degrees
            this->display->drawFastVLine((_panel * virtualResX) + virtualResX - 1 - y, x, w, r, g, b);
            break;
        default:
            // 0 degrees
            this->display->drawFastHLine((_panel * virtualResX) + x, y, w, r, g, b);
            break;
    }
}

/// @brief Draw a horizontal line
/// @param x  Starting pixel X co-ordinate
/// @param y  Starting pixel Y co-ordinate
/// @param w  Line width
/// @param color 565 color
inline void SinglePanel::drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color)
{
    uint8_t r, g, b;
    display->color565to888(color, r, g, b);
    drawFastHLine(x, y, w, r, g, b);
}

/// @brief Draw a vertical line
/// @param x  Starting pixel X co-ordinate
/// @param y  Starting pixel Y co-ordinate
/// @param h  Line height
/// @param r  Red
/// @param g  Green
/// @param b  Blue
inline void SinglePanel::drawFastVLine(int16_t x, int16_t y, int16_t h, uint8_t r, uint8_t g, uint8_t b)
{
    switch (_rotate) {
        case 1:
            // 90 degrees
            this->display->drawFastHLine((_panel * virtualResX) + y, virtualResX - 1 - x, h, r, g, b);
            break;
        case 2:
            // 180 degrees
            this->display->drawFastVLine((_panel * virtualResX) + virtualResX - 1 - x, virtualResY - 1 - y - h + 1, h, r, g, b);
            break;
        case 3:
            // 270 degrees
            this->display->drawFastHLine((_panel * virtualResX) + virtualResX - 1 - y - h + 1, x, h, r, g, b);
            break;
        default:
            // 0 degrees
            this->display->drawFastVLine((_panel * virtualResX) + x, y, h, r, g, b);
            break;
    }
}

/// @brief Draw a vertical line
/// @param x  Starting pixel X co-ordinate
/// @param y  Starting pixel Y co-ordinate
/// @param h  Line height
/// @param color 565 color
inline void SinglePanel::drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color)
{
    uint8_t r, g, b;
    display->color565to888(color, r, g, b);
    drawFastVLine(x, y, h, r, g, b);
}

/// @brief          Draws a binary sprite to the display
/// @param sprite   an array of 8 bit integers, each bit represents a pixel
/// @param x        x position to draw the sprite
/// @param y        y position to draw the sprite
/// @param w        width of the sprite
/// @param h        height of the sprite
/// @param r        Red
/// @param g        Green
/// @param b        Blue
/// @param fill     if true, fill the background with black
inline void SinglePanel::drawSprite8(const uint8_t *sprite, int16_t x, int16_t y, uint16_t w, uint16_t h, uint8_t r, uint8_t g, uint8_t b, bool fill)
{
    for (int16_t i = 0; i < w; i++)
    {
        for (int16_t j = 0; j < h; j++)
        {
            if ((sprite[j] >> (7 - i)) & 1)
            {
                this->drawPixelRGB888(x + i, y + j, r, g, b);
            }
            else if (fill)
            {
                this->drawPixelRGB888(x + i, y + j, 0, 0, 0);
            }
        }
    }
}

/// @brief          Draws a binary sprite to the display, 565 color overload
/// @param sprite   an array of 8 bit integers, each bit represents a pixel
/// @param x        x position to draw the sprite
/// @param y        y position to draw the sprite
/// @param w        width of the sprite
/// @param h        height of the sprite
/// @param color    16 bit color value of the sprite
/// @param fill     if true, fill the background with black
inline void SinglePanel::drawSprite8(const uint8_t *sprite, int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t color, bool fill)
{
    uint8_t r, g, b;
    display->color565to888(color, r, g, b);
    drawSprite8(sprite, x, y, w, h, r, g, b, fill);
}

/// @brief          Draws a binary sprite to the display
/// @param sprite   an array of 16 bit integers, each bit represents a pixel
/// @param x        x position to draw the sprite
/// @param y        y position to draw the sprite
/// @param w        width of the sprite
/// @param h        height of the sprite
/// @param r        Red
/// @param g        Green
/// @param b        Blue
/// @param fill     if true, fill the background with black
inline void SinglePanel::drawSprite16(const uint16_t *sprite, int16_t x, int16_t y, uint16_t w, uint16_t h, uint8_t r, uint8_t g, uint8_t b, bool fill)
{
    for (int16_t i = 0; i < w; i++)
    {
        for (int16_t j = 0; j < h; j++)
        {
            if ((sprite[j] >> (15 - i)) & 1)
            {
                this->drawPixelRGB888(x + i, y + j, r, g, b);
            }
            else if (fill)
            {
                this->drawPixelRGB888(x + i, y + j, 0, 0, 0);
            }
        }
    }
}

/// @brief          Draws a binary sprite to the display, 565 color overload
/// @param sprite   an array of 16 bit integers, each bit represents a pixel
/// @param x        x position to draw the sprite
/// @param y        y position to draw the sprite
/// @param w        width of the sprite
/// @param h        height of the sprite
/// @param color    16 bit color value of the sprite
/// @param fill     if true, fill the background with black
inline void SinglePanel::drawSprite16(const uint16_t *sprite, int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t color, bool fill)
{
    uint8_t r, g, b;
    display->color565to888(color, r, g, b);
    drawSprite16(sprite, x, y, w, h, r, g, b, fill);
}

#endif