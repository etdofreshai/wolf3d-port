// id_in.h - Input Manager (SDL3 port)
#ifndef ID_IN_H
#define ID_IN_H

#include "wl_types.h"

#define MaxPlayers  4
#define MaxKbds     2
#define MaxJoys     2
#define NumCodes    128

// Scan codes - match original PC hardware scan codes
typedef byte ScanCode;

enum {
    sc_None = 0x00,
    sc_Bad = 0xFF,
    sc_Return = 0x1C,
    sc_Enter = sc_Return,
    sc_Escape = 0x01,
    sc_Space = 0x39,
    sc_BackSpace = 0x0E,
    sc_Tab = 0x0F,
    sc_Delete = 0x53,
    sc_CapsLock = 0x3A,
    sc_LShift = 0x2A,
    sc_RShift = 0x36,
    sc_Control = 0x1D,
    sc_Alt = 0x38,
    sc_UpArrow = 0x48,
    sc_DownArrow = 0x50,
    sc_LeftArrow = 0x4B,
    sc_RightArrow = 0x4D,
    sc_Home = 0x47,
    sc_End = 0x4F,
    sc_PgUp = 0x49,
    sc_PgDn = 0x51,
    sc_Insert = 0x52,
    sc_F1 = 0x3B, sc_F2, sc_F3, sc_F4, sc_F5, sc_F6,
    sc_F7, sc_F8, sc_F9, sc_F10, sc_F11, sc_F12,
    sc_A = 0x1E, sc_B = 0x30, sc_C = 0x2E, sc_D = 0x20,
    sc_E = 0x12, sc_F = 0x21, sc_G = 0x22, sc_H = 0x23,
    sc_I = 0x17, sc_J = 0x24, sc_K = 0x25, sc_L = 0x26,
    sc_M = 0x32, sc_N = 0x31, sc_O = 0x18, sc_P = 0x19,
    sc_Q = 0x10, sc_R = 0x13, sc_S = 0x1F, sc_T = 0x14,
    sc_U = 0x16, sc_V = 0x2F, sc_W = 0x11, sc_X = 0x2D,
    sc_Y = 0x15, sc_Z = 0x2C,
    sc_1 = 0x02, sc_2, sc_3, sc_4, sc_5,
    sc_6, sc_7, sc_8, sc_9, sc_0,
    sc_Minus = 0x0C, sc_Equals = 0x0D,
    sc_OpenBracket = 0x1A, sc_CloseBracket = 0x1B,
    sc_SemiColon = 0x27, sc_Quote = 0x28,
    sc_BackSlash = 0x2B, sc_Comma = 0x33,
    sc_Period = 0x34, sc_Slash = 0x35,
    sc_NumLock = 0x45, sc_ScrollLock = 0x46,
    sc_Pause = 0x59
};

#define key_None    0
#define key_Return  13
#define key_Escape  27
#define key_Space   32

typedef enum {
    demo_Off, demo_Record, demo_Playback, demo_PlayDone
} Demo;

typedef enum {
    ctrl_Keyboard, ctrl_Keyboard1, ctrl_Keyboard2,
    ctrl_Joystick, ctrl_Joystick1, ctrl_Joystick2,
    ctrl_Mouse
} ControlType;

typedef enum {
    motion_Left = -1, motion_Up = -1, motion_None = 0,
    motion_Right = 1, motion_Down = 1
} Motion;

typedef enum {
    dir_North, dir_NorthEast, dir_East, dir_SouthEast,
    dir_South, dir_SouthWest, dir_West, dir_NorthWest, dir_None
} Direction;

typedef struct {
    boolean button0, button1, button2, button3;
    int x, y;
    Motion xaxis, yaxis;
    Direction dir;
} CursorInfo;

typedef CursorInfo ControlInfo;

typedef struct {
    ScanCode button0, button1;
    ScanCode upleft, up, upright;
    ScanCode left, right;
    ScanCode downleft, down, downright;
} KeyboardDef;

typedef struct {
    word minX, minY, maxX, maxY;
    word threshMinX, threshMinY, threshMaxX, threshMaxY;
    word joyMultXL, joyMultYL, joyMultXH, joyMultYH;
} JoystickDef;

extern boolean    MousePresent;
extern boolean    JoysPresent[MaxJoys];
extern boolean    Keyboard[NumCodes];
extern boolean    Paused;
extern char       LastASCII;
extern ScanCode   LastScan;
extern KeyboardDef KbdDefs;
extern JoystickDef JoyDefs[MaxJoys];
extern ControlType Controls[MaxPlayers];
extern Demo       DemoMode;
extern byte       *DemoBuffer;
extern word       DemoOffset, DemoSize;

void IN_Startup(void);
void IN_Shutdown(void);
void IN_Default(boolean gotit, ControlType which);
void IN_SetKeyHook(void (*hook)(void));
void IN_ClearKeysDown(void);
void IN_Ack(void);
void IN_AckBack(void);
char IN_WaitForASCII(void);
ScanCode IN_WaitForKey(void);
byte *IN_GetScanName(ScanCode scan);
void IN_ReadCursor(CursorInfo *ci);
void IN_ReadControl(int player, ControlInfo *ci);
void IN_SetControlType(int player, ControlType type);
boolean IN_UserInput(longword delay);
void IN_StartAck(void);
boolean IN_CheckAck(void);
void IN_StopDemo(void);
void IN_FreeDemoBuffer(void);
word IN_MouseButtons(void);
boolean IN_KeyDown(ScanCode code);
void IN_ClearKey(ScanCode code);
void IN_GetJoyAbs(int joy, int *x, int *y);
void IN_SetupJoy(int joy, int xmin, int xmax, int ymin, int ymax);
word IN_JoyButtons(void);
void INL_GetJoyDelta(int joy, int *dx, int *dy);

#endif
