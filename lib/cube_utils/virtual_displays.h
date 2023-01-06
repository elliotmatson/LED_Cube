#ifndef VIRTUAL_DISPLAYS_H
#define VIRTUAL_DISPLAYS_H

#include "ESP32-HUB75-MatrixPanel-I2S-DMA.h"
#include <Fonts/FreeSansBold12pt7b.h>

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

    void clearScreen() { display->clearScreen(); }
    void drawPixelRGB888(int16_t x, int16_t y, uint8_t r, uint8_t g, uint8_t b);

    uint16_t color444(uint8_t r, uint8_t g, uint8_t b)
    {
        return display->color444(r, g, b);
    }
    uint16_t color565(uint8_t r, uint8_t g, uint8_t b) { return display->color565(r, g, b); }
    uint16_t color333(uint8_t r, uint8_t g, uint8_t b) { return display->color333(r, g, b); }

    void flipDMABuffer() { display->flipDMABuffer(); }
    void drawDisplayTest();
    void setRotation(int rotate);

protected:
    virtual VirtualCoords getCoords(int16_t &x, int16_t &y);
    VirtualCoords coords;

    int _panel = 0;
    int _rotate = 0;

}; // end Class header

/**
 * Calculate virtual->real co-ordinate mapping to underlying single chain of panels connected to ESP32.
 * Updates the private class member variable 'coords', so no need to use the return value.
 * Not thread safe, but not a concern for ESP32 sketch anyway... I think.
 */
inline VirtualCoords SinglePanel::getCoords(int16_t &x, int16_t &y)
{
    // Serial.println("Called Base.");
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
        // Serial.printf("VirtualMatrixPanel::getCoords(): Invalid virtual display coordinate. x,y: %d, %d\r\n", x, y);
        return coords;
    }

    // Calculate the real co-ordinates of the pixel on the underlying single chain of panels.
    coords.x = x + (_panel * virtualResX);
    coords.y = y;

    // Serial.print("Mapping to x: "); Serial.print(coords.x, DEC);  Serial.print(", y: "); Serial.println(coords.y, DEC);
    return coords;
}

inline void SinglePanel::drawPixel(int16_t x, int16_t y, uint16_t color)
{ // adafruit virtual void override
    getCoords(x, y);
    this->display->drawPixel(coords.x, coords.y, color);
}

inline void SinglePanel::fillScreen(uint16_t color)
{ // adafruit virtual void override
    this->display->fillRect((_panel * virtualResX), 0, this->virtualResX, this->virtualResY, color);
}

inline void SinglePanel::fillScreenRGB888(uint8_t r, uint8_t g, uint8_t b)
{
    this->display->fillRect((_panel * virtualResX), 0, this->virtualResX, this->virtualResY, this->display->color565(r, g, b));
}

inline void SinglePanel::drawPixelRGB888(int16_t x, int16_t y, uint8_t r, uint8_t g, uint8_t b)
{
    getCoords(x, y);
    this->display->drawPixelRGB888(coords.x, coords.y, r, g, b);
}

inline void SinglePanel::setRotation(int rotate)
{
    if (rotate < 4) {
        _rotate = rotate;
    }
}

inline void SinglePanel::drawDisplayTest()
{
    this->display->setFont(&FreeSansBold12pt7b);
    this->display->setTextColor(this->display->color565(255, 255, 0));
    this->display->setTextSize(1);

    this->display->drawRect((_panel * virtualResX), 0, virtualResX, virtualResY, this->display->color565(0, 255, 0));
    this->display->setCursor((_panel * virtualResX), virtualResY - 3);
    this->display->print(1);
}

#endif