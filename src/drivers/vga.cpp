
#include <drivers/vga.h>

using namespace myos::common;
using namespace myos::drivers;

static const uint16_t VGA_MISC_PORT = 0x3C2;
static const uint16_t VGA_CRTC_INDEX_PORT = 0x3D4;
static const uint16_t VGA_CRTC_DATA_PORT = 0x3D5;
static const uint16_t VGA_SEQUENCER_INDEX_PORT = 0x3C4;
static const uint16_t VGA_SEQUENCER_DATA_PORT = 0x3C5;
static const uint16_t VGA_GRAPHICS_CONTROLLER_INDEX_PORT = 0x3CE;
static const uint16_t VGA_GRAPHICS_CONTROLLER_DATA_PORT = 0x3CF;
static const uint16_t VGA_ATTRIBUTE_CONTROLLER_INDEX_PORT = 0x3C0;
static const uint16_t VGA_ATTRIBUTE_CONTROLLER_READ_PORT = 0x3C1;
static const uint16_t VGA_ATTRIBUTE_CONTROLLER_WRITE_PORT = 0x3C0;
static const uint16_t VGA_ATTRIBUTE_CONTROLLER_RESET_PORT = 0x3DA;

static const uint8_t VGA_CRTC_END_HORIZONTAL_BLANKING = 0x03;
static const uint8_t VGA_CRTC_VERTICAL_RETRACE_END = 0x11;
static const uint8_t VGA_CRTC_REGISTER_UNLOCK = 0x80;
static const uint8_t VGA_ATTRIBUTE_CONTROLLER_ENABLE = 0x20;
static const uint8_t VGA_GRAPHICS_MISC_REGISTER = 0x06;
static const uint8_t VGA_GRAPHICS_MEMORY_MAP_MASK = 0x0C;

static const uint32_t VGA_SEGMENT_UNUSED = 0x00000;
static const uint32_t VGA_SEGMENT_A0000 = 0xA0000;
static const uint32_t VGA_SEGMENT_B0000 = 0xB0000;
static const uint32_t VGA_SEGMENT_B8000 = VGA_TEXT_MEMORY;

static const uint8_t VGA_RGB_LOW = 0x00;
static const uint8_t VGA_RGB_MID = 0xA8;
static const uint8_t VGA_RGB_FULL = 0xFF;
static const uint8_t VGA_COLOR_INDEX_BLACK = 0x00;
static const uint8_t VGA_COLOR_INDEX_BLUE = 0x01;
static const uint8_t VGA_COLOR_INDEX_GREEN = 0x02;
static const uint8_t VGA_COLOR_INDEX_RED = 0x04;
static const uint8_t VGA_COLOR_INDEX_WHITE = 0x3F;

           
            
VideoGraphicsArray::VideoGraphicsArray() : 
    miscPort(VGA_MISC_PORT),
    crtcIndexPort(VGA_CRTC_INDEX_PORT),
    crtcDataPort(VGA_CRTC_DATA_PORT),
    sequencerIndexPort(VGA_SEQUENCER_INDEX_PORT),
    sequencerDataPort(VGA_SEQUENCER_DATA_PORT),
    graphicsControllerIndexPort(VGA_GRAPHICS_CONTROLLER_INDEX_PORT),
    graphicsControllerDataPort(VGA_GRAPHICS_CONTROLLER_DATA_PORT),
    attributeControllerIndexPort(VGA_ATTRIBUTE_CONTROLLER_INDEX_PORT),
    attributeControllerReadPort(VGA_ATTRIBUTE_CONTROLLER_READ_PORT),
    attributeControllerWritePort(VGA_ATTRIBUTE_CONTROLLER_WRITE_PORT),
    attributeControllerResetPort(VGA_ATTRIBUTE_CONTROLLER_RESET_PORT)
{
}

VideoGraphicsArray::~VideoGraphicsArray()
{
}


            
void VideoGraphicsArray::WriteRegisters(uint8_t* registers)
{
    //  misc
    miscPort.Write(*(registers++));
    
    // sequencer
    for(uint8_t i = 0; i < 5; i++)
    {
        sequencerIndexPort.Write(i);
        sequencerDataPort.Write(*(registers++));
    }
    
    // cathode ray tube controller
    crtcIndexPort.Write(VGA_CRTC_END_HORIZONTAL_BLANKING);
    crtcDataPort.Write(crtcDataPort.Read() | VGA_CRTC_REGISTER_UNLOCK);
    crtcIndexPort.Write(VGA_CRTC_VERTICAL_RETRACE_END);
    crtcDataPort.Write(crtcDataPort.Read() & ~VGA_CRTC_REGISTER_UNLOCK);
    
    registers[VGA_CRTC_END_HORIZONTAL_BLANKING] =
        registers[VGA_CRTC_END_HORIZONTAL_BLANKING] | VGA_CRTC_REGISTER_UNLOCK;
    registers[VGA_CRTC_VERTICAL_RETRACE_END] =
        registers[VGA_CRTC_VERTICAL_RETRACE_END] & ~VGA_CRTC_REGISTER_UNLOCK;
    
    for(uint8_t i = 0; i < 25; i++)
    {
        crtcIndexPort.Write(i);
        crtcDataPort.Write(*(registers++));
    }
    
    // graphics controller
    for(uint8_t i = 0; i < 9; i++)
    {
        graphicsControllerIndexPort.Write(i);
        graphicsControllerDataPort.Write(*(registers++));
    }
    
    // attribute controller
    for(uint8_t i = 0; i < 21; i++)
    {
        attributeControllerResetPort.Read();
        attributeControllerIndexPort.Write(i);
        attributeControllerWritePort.Write(*(registers++));
    }
    
    attributeControllerResetPort.Read();
    attributeControllerIndexPort.Write(VGA_ATTRIBUTE_CONTROLLER_ENABLE);
    
}

bool VideoGraphicsArray::SupportsMode(uint32_t width, uint32_t height, uint32_t colordepth)
{
    return width == 320 && height == 200 && colordepth == 8;
}

bool VideoGraphicsArray::SetMode(uint32_t width, uint32_t height, uint32_t colordepth)
{
    if(!SupportsMode(width, height, colordepth))
        return false;
    
    unsigned char g_320x200x256[] =
    {
        /* MISC */
            0x63,
        /* SEQ */
            0x03, 0x01, 0x0F, 0x00, 0x0E,
        /* CRTC */
            0x5F, 0x4F, 0x50, 0x82, 0x54, 0x80, 0xBF, 0x1F,
            0x00, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x9C, 0x0E, 0x8F, 0x28, 0x40, 0x96, 0xB9, 0xA3,
            0xFF,
        /* GC */
            0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x05, 0x0F,
            0xFF,
        /* AC */
            0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
            0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
            0x41, 0x00, 0x0F, 0x00, 0x00
    };
    
    WriteRegisters(g_320x200x256);
    return true;
}


uint8_t* VideoGraphicsArray::GetFrameBufferSegment()
{
    graphicsControllerIndexPort.Write(VGA_GRAPHICS_MISC_REGISTER);
    uint8_t segmentNumber = graphicsControllerDataPort.Read() & VGA_GRAPHICS_MEMORY_MAP_MASK;
    switch(segmentNumber)
    {
        default:
        case 0<<2: return (uint8_t*)VGA_SEGMENT_UNUSED;
        case 1<<2: return (uint8_t*)VGA_SEGMENT_A0000;
        case 2<<2: return (uint8_t*)VGA_SEGMENT_B0000;
        case 3<<2: return (uint8_t*)VGA_SEGMENT_B8000;
    }
}
            
void VideoGraphicsArray::PutPixel(int32_t x, int32_t y, uint8_t colorIndex)
{
    if(x < 0 || 320 <= x || y < 0 || 200 <= y)
        return;

    uint8_t* pixelAddress = GetFrameBufferSegment() + 320*y + x;
    *pixelAddress = colorIndex;
}

uint8_t VideoGraphicsArray::GetColorIndex(uint8_t r, uint8_t g, uint8_t b)
{
    if(r == VGA_RGB_LOW && g == VGA_RGB_LOW && b == VGA_RGB_LOW) return VGA_COLOR_INDEX_BLACK;
    if(r == VGA_RGB_LOW && g == VGA_RGB_LOW && b == VGA_RGB_MID) return VGA_COLOR_INDEX_BLUE;
    if(r == VGA_RGB_LOW && g == VGA_RGB_MID && b == VGA_RGB_LOW) return VGA_COLOR_INDEX_GREEN;
    if(r == VGA_RGB_MID && g == VGA_RGB_LOW && b == VGA_RGB_LOW) return VGA_COLOR_INDEX_RED;
    if(r == VGA_RGB_FULL && g == VGA_RGB_FULL && b == VGA_RGB_FULL) return VGA_COLOR_INDEX_WHITE;
    return VGA_COLOR_INDEX_BLACK;
}
           
void VideoGraphicsArray::PutPixel(int32_t x, int32_t y, uint8_t r, uint8_t g, uint8_t b)
{
    PutPixel(x,y, GetColorIndex(r,g,b));
}

void VideoGraphicsArray::FillRectangle(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                                       uint8_t r, uint8_t g, uint8_t b)
{
    for(int32_t Y = y; Y < y+h; Y++)
        for(int32_t X = x; X < x+w; X++)
            PutPixel(X, Y, r, g, b);
}
