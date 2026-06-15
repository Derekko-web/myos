
#include <drivers/mouse.h>


using namespace myos::common;
using namespace myos::drivers;
using namespace myos::hardwarecommunication;

static const uint16_t PS2_DATA_PORT = 0x60;
static const uint16_t PS2_COMMAND_PORT = 0x64;
static const uint8_t PS2_CMD_ENABLE_SECOND_PORT = 0xA8;
static const uint8_t PS2_CMD_READ_CONFIG = 0x20;
static const uint8_t PS2_CMD_WRITE_CONFIG = 0x60;
static const uint8_t PS2_CMD_WRITE_TO_MOUSE = 0xD4;
static const uint8_t PS2_CONFIG_SECOND_PORT_INTERRUPT = 0x02;
static const uint8_t PS2_STATUS_MOUSE_OUTPUT_BUFFER_FULL = 0x20;
static const uint8_t MOUSE_CMD_ENABLE_PACKET_STREAMING = 0xF4;
static const uint8_t MOUSE_BUTTON_BIT = 0x01;


void printf(const char*);

    MouseEventHandler::MouseEventHandler()
    {
    }
    
    void MouseEventHandler::OnActivate()
    {
    }
    
    void MouseEventHandler::OnMouseDown(uint8_t button)
    {
    }
    
    void MouseEventHandler::OnMouseUp(uint8_t button)
    {
    }
    
    void MouseEventHandler::OnMouseMove(int x, int y)
    {
    }





    MouseDriver::MouseDriver(InterruptManager* manager, MouseEventHandler* handler)
    : InterruptHandler(manager, MOUSE_INTERRUPT),
    dataport(PS2_DATA_PORT),
    commandport(PS2_COMMAND_PORT)
    {
        this->handler = handler;
    }

    MouseDriver::~MouseDriver()
    {
    }
    
    void MouseDriver::Activate()
    {
        offset = 0;
        buttons = 0;

        if(handler != 0)
            handler->OnActivate();
        
        commandport.Write(PS2_CMD_ENABLE_SECOND_PORT);
        commandport.Write(PS2_CMD_READ_CONFIG);
        uint8_t status = dataport.Read() | PS2_CONFIG_SECOND_PORT_INTERRUPT;
        commandport.Write(PS2_CMD_WRITE_CONFIG);
        dataport.Write(status);

        commandport.Write(PS2_CMD_WRITE_TO_MOUSE);
        dataport.Write(MOUSE_CMD_ENABLE_PACKET_STREAMING);
        dataport.Read();        
    }
    
    uint32_t MouseDriver::HandleInterrupt(uint32_t esp)
    {
        uint8_t status = commandport.Read();
        if (!(status & PS2_STATUS_MOUSE_OUTPUT_BUFFER_FULL))
            return esp;

        buffer[offset] = dataport.Read();
        
        if(handler == 0)
            return esp;
        
        offset = (offset + 1) % 3;

        if(offset == 0)
        {
            if(buffer[1] != 0 || buffer[2] != 0)
            {
                handler->OnMouseMove((int8_t)buffer[1], -((int8_t)buffer[2]));
            }

            for(uint8_t i = 0; i < 3; i++)
            {
                if((buffer[0] & (MOUSE_BUTTON_BIT << i)) != (buttons & (MOUSE_BUTTON_BIT << i)))
                {
                    if(buttons & (MOUSE_BUTTON_BIT << i))
                        handler->OnMouseUp(i+1);
                    else
                        handler->OnMouseDown(i+1);
                }
            }
            buttons = buffer[0];
        }
        
        return esp;
    }
