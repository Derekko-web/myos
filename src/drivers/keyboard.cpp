
#include <drivers/keyboard.h>

using namespace myos::common;
using namespace myos::drivers;
using namespace myos::hardwarecommunication;

static const uint16_t PS2_DATA_PORT = 0x60;
static const uint16_t PS2_COMMAND_PORT = 0x64;
static const uint8_t PS2_STATUS_OUTPUT_BUFFER_FULL = 0x01;
static const uint8_t PS2_CONFIG_FIRST_PORT_INTERRUPT = 0x01;
static const uint8_t PS2_CONFIG_FIRST_PORT_CLOCK_DISABLED = 0x10;
static const uint8_t PS2_CMD_ENABLE_FIRST_PORT = 0xAE;
static const uint8_t PS2_CMD_READ_CONFIG = 0x20;
static const uint8_t PS2_CMD_WRITE_CONFIG = 0x60;
static const uint8_t KEYBOARD_CMD_ENABLE_SCANNING = 0xF4;
static const uint8_t KEYBOARD_KEY_RELEASED = 0x80;

enum KeyboardScancode
{
    KEY_1_PRESSED = 0x02,
    KEY_2_PRESSED = 0x03,
    KEY_3_PRESSED = 0x04,
    KEY_4_PRESSED = 0x05,
    KEY_5_PRESSED = 0x06,
    KEY_6_PRESSED = 0x07,
    KEY_7_PRESSED = 0x08,
    KEY_8_PRESSED = 0x09,
    KEY_9_PRESSED = 0x0A,
    KEY_0_PRESSED = 0x0B,

    KEY_Q_PRESSED = 0x10,
    KEY_W_PRESSED = 0x11,
    KEY_E_PRESSED = 0x12,
    KEY_R_PRESSED = 0x13,
    KEY_T_PRESSED = 0x14,
    KEY_Z_PRESSED = 0x15,
    KEY_U_PRESSED = 0x16,
    KEY_I_PRESSED = 0x17,
    KEY_O_PRESSED = 0x18,
    KEY_P_PRESSED = 0x19,

    KEY_A_PRESSED = 0x1E,
    KEY_S_PRESSED = 0x1F,
    KEY_D_PRESSED = 0x20,
    KEY_F_PRESSED = 0x21,
    KEY_G_PRESSED = 0x22,
    KEY_H_PRESSED = 0x23,
    KEY_J_PRESSED = 0x24,
    KEY_K_PRESSED = 0x25,
    KEY_L_PRESSED = 0x26,

    KEY_Y_PRESSED = 0x2C,
    KEY_X_PRESSED = 0x2D,
    KEY_C_PRESSED = 0x2E,
    KEY_V_PRESSED = 0x2F,
    KEY_B_PRESSED = 0x30,
    KEY_N_PRESSED = 0x31,
    KEY_M_PRESSED = 0x32,
    KEY_COMMA_PRESSED = 0x33,
    KEY_PERIOD_PRESSED = 0x34,
    KEY_MINUS_PRESSED = 0x35,

    KEY_ENTER_PRESSED = 0x1C,
    KEY_SPACE_PRESSED = 0x39
};


KeyboardEventHandler::KeyboardEventHandler()
{
}

void KeyboardEventHandler::OnKeyDown(char)
{
}

void KeyboardEventHandler::OnKeyUp(char)
{
}





KeyboardDriver::KeyboardDriver(InterruptManager* manager, KeyboardEventHandler *handler)
: InterruptHandler(manager, KEYBOARD_INTERRUPT),
dataport(PS2_DATA_PORT),
commandport(PS2_COMMAND_PORT)
{
    this->handler = handler;
}

KeyboardDriver::~KeyboardDriver()
{
}

void printf(const char*);
void printfHex(uint8_t);

void KeyboardDriver::Activate()
{
    while(commandport.Read() & PS2_STATUS_OUTPUT_BUFFER_FULL)
        dataport.Read();
    commandport.Write(PS2_CMD_ENABLE_FIRST_PORT);
    commandport.Write(PS2_CMD_READ_CONFIG);
    uint8_t status = (dataport.Read() | PS2_CONFIG_FIRST_PORT_INTERRUPT)
        & ~PS2_CONFIG_FIRST_PORT_CLOCK_DISABLED;
    commandport.Write(PS2_CMD_WRITE_CONFIG);
    dataport.Write(status);
    dataport.Write(KEYBOARD_CMD_ENABLE_SCANNING);
}

uint32_t KeyboardDriver::HandleInterrupt(uint32_t esp)
{
    uint8_t key = dataport.Read();
    
    if(handler == 0)
        return esp;
    
    if(key < KEYBOARD_KEY_RELEASED)
    {
        switch(key)
        {
            case KEY_1_PRESSED: handler->OnKeyDown('1'); break;
            case KEY_2_PRESSED: handler->OnKeyDown('2'); break;
            case KEY_3_PRESSED: handler->OnKeyDown('3'); break;
            case KEY_4_PRESSED: handler->OnKeyDown('4'); break;
            case KEY_5_PRESSED: handler->OnKeyDown('5'); break;
            case KEY_6_PRESSED: handler->OnKeyDown('6'); break;
            case KEY_7_PRESSED: handler->OnKeyDown('7'); break;
            case KEY_8_PRESSED: handler->OnKeyDown('8'); break;
            case KEY_9_PRESSED: handler->OnKeyDown('9'); break;
            case KEY_0_PRESSED: handler->OnKeyDown('0'); break;

            case KEY_Q_PRESSED: handler->OnKeyDown('q'); break;
            case KEY_W_PRESSED: handler->OnKeyDown('w'); break;
            case KEY_E_PRESSED: handler->OnKeyDown('e'); break;
            case KEY_R_PRESSED: handler->OnKeyDown('r'); break;
            case KEY_T_PRESSED: handler->OnKeyDown('t'); break;
            case KEY_Z_PRESSED: handler->OnKeyDown('z'); break;
            case KEY_U_PRESSED: handler->OnKeyDown('u'); break;
            case KEY_I_PRESSED: handler->OnKeyDown('i'); break;
            case KEY_O_PRESSED: handler->OnKeyDown('o'); break;
            case KEY_P_PRESSED: handler->OnKeyDown('p'); break;

            case KEY_A_PRESSED: handler->OnKeyDown('a'); break;
            case KEY_S_PRESSED: handler->OnKeyDown('s'); break;
            case KEY_D_PRESSED: handler->OnKeyDown('d'); break;
            case KEY_F_PRESSED: handler->OnKeyDown('f'); break;
            case KEY_G_PRESSED: handler->OnKeyDown('g'); break;
            case KEY_H_PRESSED: handler->OnKeyDown('h'); break;
            case KEY_J_PRESSED: handler->OnKeyDown('j'); break;
            case KEY_K_PRESSED: handler->OnKeyDown('k'); break;
            case KEY_L_PRESSED: handler->OnKeyDown('l'); break;

            case KEY_Y_PRESSED: handler->OnKeyDown('y'); break;
            case KEY_X_PRESSED: handler->OnKeyDown('x'); break;
            case KEY_C_PRESSED: handler->OnKeyDown('c'); break;
            case KEY_V_PRESSED: handler->OnKeyDown('v'); break;
            case KEY_B_PRESSED: handler->OnKeyDown('b'); break;
            case KEY_N_PRESSED: handler->OnKeyDown('n'); break;
            case KEY_M_PRESSED: handler->OnKeyDown('m'); break;
            case KEY_COMMA_PRESSED: handler->OnKeyDown(','); break;
            case KEY_PERIOD_PRESSED: handler->OnKeyDown('.'); break;
            case KEY_MINUS_PRESSED: handler->OnKeyDown('-'); break;

            case KEY_ENTER_PRESSED: handler->OnKeyDown('\n'); break;
            case KEY_SPACE_PRESSED: handler->OnKeyDown(' '); break;

            default:
            {
                printf("KEYBOARD 0x");
                printfHex(key);
                break;
            }
        }
    }
    return esp;
}
