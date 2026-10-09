#include <gui/desktop.h>

using namespace myos;
using namespace myos::common;
using namespace myos::gui;

static const uint8_t RGB_WHITE = 0xFF;

Desktop::Desktop(int32_t w, int32_t h, uint8_t r, uint8_t g, uint8_t b)
:   CompositeWidget(0, 0, 0, w, h, r, g, b),
    MouseEventHandler()
{
    MouseX = w / 2;
    MouseY = h / 2;
}

Desktop::~Desktop()
{
}

void Desktop::Draw(GraphicsContext* gc)
{
    CompositeWidget::Draw(gc);

    for(int i = 0; i < 4; i++)
    {
        gc->PutPixel(MouseX - i, MouseY, RGB_WHITE, RGB_WHITE, RGB_WHITE);
        gc->PutPixel(MouseX + i, MouseY, RGB_WHITE, RGB_WHITE, RGB_WHITE);
        gc->PutPixel(MouseX, MouseY - i, RGB_WHITE, RGB_WHITE, RGB_WHITE);
        gc->PutPixel(MouseX, MouseY + i, RGB_WHITE, RGB_WHITE, RGB_WHITE);
    }
}

void Desktop::OnMouseDown(uint8_t button)
{
    CompositeWidget::OnMouseDown(MouseX, MouseY, button);
}

void Desktop::OnMouseUp(uint8_t button)
{
    CompositeWidget::OnMouseUp(MouseX, MouseY, button);
}

void Desktop::OnMouseMove(int x, int y)
{
    x /= 4;
    y /= 4;

    int32_t newMouseX = MouseX + x;
    if(newMouseX < 0) newMouseX = 0;
    if(newMouseX >= w) newMouseX = w - 1;

    int32_t newMouseY = MouseY + y;
    if(newMouseY < 0) newMouseY = 0;
    if(newMouseY >= h) newMouseY = h - 1;

    CompositeWidget::OnMouseMove(MouseX, MouseY, newMouseX, newMouseY);

    MouseX = newMouseX;
    MouseY = newMouseY;
}
