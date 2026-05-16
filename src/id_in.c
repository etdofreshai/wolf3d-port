// id_in.c - Input Manager (SDL3 port)
// Replaces DOS keyboard ISR, mouse interrupt 33h, and joystick polling
// with SDL3 event handling.

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <SDL3/SDL.h>

#include "id_in.h"
#include "id_vl.h"
#include "id_sd.h"

// Joystick calibration and scaling constants (matching original Wolf3D behavior)
#define JOY_AXIS_MAX   65535
#define JOY_AXIS_MID   (JOY_AXIS_MAX / 2)
#define JOY_SCALE_MAX  32768
#define JOY_SCALE_SHIFT 8

// Globals
boolean    MousePresent;
boolean    JoysPresent[MaxJoys];
boolean    Keyboard[NumCodes];
boolean    Paused;
char       LastASCII;
ScanCode   LastScan;
KeyboardDef KbdDefs;
JoystickDef JoyDefs[MaxJoys];
ControlType Controls[MaxPlayers];
Demo       DemoMode;
byte       *DemoBuffer;
word       DemoOffset, DemoSize;

// Internal state
static int   mouseButtons;
static int   mouseDX, mouseDY;
static boolean ackStarted;
static Uint32 ackStartTime;
static void (*keyHook)(void);
static SDL_Joystick *joyHandles[MaxJoys];
static boolean joyDefaultsSet[MaxJoys];
static int joyLastX[MaxJoys];
static int joyLastY[MaxJoys];

// Forward declarations
static void IN_PumpEvents(void);
static ScanCode SDL_ScanCode_to_PC(SDL_Scancode sc);
static char IN_TranslateKey(ScanCode sc, SDL_Keymod mod);
static int INL_NormalizeAxisValue(SDL_Joystick *joy, int axisIndex);
static void INL_SetJoyScale(int joy);
static word INL_GetJoyButtonsByIndex(int joy);
static void INL_UpdateJoyDefaults(int joy);

// -----------------------------------------------------------------------
// SDL scancode -> PC hardware scan code mapping
// -----------------------------------------------------------------------
static ScanCode SDL_ScanCode_to_PC(SDL_Scancode sc)
{
    switch (sc) {
    case SDL_SCANCODE_ESCAPE:       return 0x01;
    case SDL_SCANCODE_1:            return 0x02;
    case SDL_SCANCODE_2:            return 0x03;
    case SDL_SCANCODE_3:            return 0x04;
    case SDL_SCANCODE_4:            return 0x05;
    case SDL_SCANCODE_5:            return 0x06;
    case SDL_SCANCODE_6:            return 0x07;
    case SDL_SCANCODE_7:            return 0x08;
    case SDL_SCANCODE_8:            return 0x09;
    case SDL_SCANCODE_9:            return 0x0A;
    case SDL_SCANCODE_0:            return 0x0B;
    case SDL_SCANCODE_MINUS:        return 0x0C;
    case SDL_SCANCODE_EQUALS:       return 0x0D;
    case SDL_SCANCODE_BACKSPACE:    return 0x0E;
    case SDL_SCANCODE_TAB:          return 0x0F;
    case SDL_SCANCODE_Q:            return 0x10;
    case SDL_SCANCODE_W:            return 0x11;
    case SDL_SCANCODE_E:            return 0x12;
    case SDL_SCANCODE_R:            return 0x13;
    case SDL_SCANCODE_T:            return 0x14;
    case SDL_SCANCODE_Y:            return 0x15;
    case SDL_SCANCODE_U:            return 0x16;
    case SDL_SCANCODE_I:            return 0x17;
    case SDL_SCANCODE_O:            return 0x18;
    case SDL_SCANCODE_P:            return 0x19;
    case SDL_SCANCODE_LEFTBRACKET:  return 0x1A;
    case SDL_SCANCODE_RIGHTBRACKET: return 0x1B;
    case SDL_SCANCODE_RETURN:       return 0x1C;
    case SDL_SCANCODE_LCTRL:        return 0x1D;
    case SDL_SCANCODE_A:            return 0x1E;
    case SDL_SCANCODE_S:            return 0x1F;
    case SDL_SCANCODE_D:            return 0x20;
    case SDL_SCANCODE_F:            return 0x21;
    case SDL_SCANCODE_G:            return 0x22;
    case SDL_SCANCODE_H:            return 0x23;
    case SDL_SCANCODE_J:            return 0x24;
    case SDL_SCANCODE_K:            return 0x25;
    case SDL_SCANCODE_L:            return 0x26;
    case SDL_SCANCODE_SEMICOLON:    return 0x27;
    case SDL_SCANCODE_APOSTROPHE:   return 0x28;
    case SDL_SCANCODE_GRAVE:        return 0x29;
    case SDL_SCANCODE_LSHIFT:       return 0x2A;
    case SDL_SCANCODE_BACKSLASH:    return 0x2B;
    case SDL_SCANCODE_Z:            return 0x2C;
    case SDL_SCANCODE_X:            return 0x2D;
    case SDL_SCANCODE_C:            return 0x2E;
    case SDL_SCANCODE_V:            return 0x2F;
    case SDL_SCANCODE_B:            return 0x30;
    case SDL_SCANCODE_N:            return 0x31;
    case SDL_SCANCODE_M:            return 0x32;
    case SDL_SCANCODE_COMMA:        return 0x33;
    case SDL_SCANCODE_PERIOD:       return 0x34;
    case SDL_SCANCODE_SLASH:        return 0x35;
    case SDL_SCANCODE_RSHIFT:       return 0x36;
    case SDL_SCANCODE_KP_MULTIPLY:  return 0x37;
    case SDL_SCANCODE_LALT:         return 0x38;
    case SDL_SCANCODE_SPACE:        return 0x39;
    case SDL_SCANCODE_CAPSLOCK:     return 0x3A;
    case SDL_SCANCODE_F1:           return 0x3B;
    case SDL_SCANCODE_F2:           return 0x3C;
    case SDL_SCANCODE_F3:           return 0x3D;
    case SDL_SCANCODE_F4:           return 0x3E;
    case SDL_SCANCODE_F5:           return 0x3F;
    case SDL_SCANCODE_F6:           return 0x40;
    case SDL_SCANCODE_F7:           return 0x41;
    case SDL_SCANCODE_F8:           return 0x42;
    case SDL_SCANCODE_F9:           return 0x43;
    case SDL_SCANCODE_F10:          return 0x44;
    case SDL_SCANCODE_NUMLOCKCLEAR: return 0x45;
    case SDL_SCANCODE_SCROLLLOCK:   return 0x46;
    case SDL_SCANCODE_HOME:         return 0x47;
    case SDL_SCANCODE_UP:           return 0x48;
    case SDL_SCANCODE_PAGEUP:       return 0x49;
    case SDL_SCANCODE_KP_MINUS:     return 0x4A;
    case SDL_SCANCODE_LEFT:         return 0x4B;
    case SDL_SCANCODE_KP_5:         return 0x4C;
    case SDL_SCANCODE_RIGHT:        return 0x4D;
    case SDL_SCANCODE_KP_PLUS:      return 0x4E;
    case SDL_SCANCODE_END:          return 0x4F;
    case SDL_SCANCODE_DOWN:         return 0x50;
    case SDL_SCANCODE_PAGEDOWN:     return 0x51;
    case SDL_SCANCODE_INSERT:       return 0x52;
    case SDL_SCANCODE_DELETE:       return 0x53;
    case SDL_SCANCODE_RCTRL:        return 0x1D;
    case SDL_SCANCODE_RALT:         return 0x38;
    case SDL_SCANCODE_F11:          return 0x57;
    case SDL_SCANCODE_F12:          return 0x58;
    case SDL_SCANCODE_PAUSE:        return 0x59;
    // Numeric keypad maps to the same PC scancodes as the arrow/nav cluster
    // (matches DOS Wolf3D behaviour with NumLock off).
    case SDL_SCANCODE_KP_7:         return 0x47;
    case SDL_SCANCODE_KP_8:         return 0x48;
    case SDL_SCANCODE_KP_9:         return 0x49;
    case SDL_SCANCODE_KP_4:         return 0x4B;
    case SDL_SCANCODE_KP_6:         return 0x4D;
    case SDL_SCANCODE_KP_1:         return 0x4F;
    case SDL_SCANCODE_KP_2:         return 0x50;
    case SDL_SCANCODE_KP_3:         return 0x51;
    case SDL_SCANCODE_KP_0:         return 0x52;
    case SDL_SCANCODE_KP_PERIOD:    return 0x53;
    case SDL_SCANCODE_KP_ENTER:     return 0x1C;
    case SDL_SCANCODE_KP_DIVIDE:    return 0x35;
    default:                        return sc_None;
    }
}

// -----------------------------------------------------------------------
// Translate a PC scan code + modifier state to an ASCII character
// -----------------------------------------------------------------------
static char IN_TranslateKey(ScanCode sc, SDL_Keymod mod)
{
    boolean shifted = (mod & SDL_KMOD_SHIFT) != 0;
    boolean caps = (mod & SDL_KMOD_CAPS) != 0;
    boolean upper = shifted ^ caps;

    // Letters
    if (sc >= sc_A && sc <= sc_Z) {
        // PC scancodes for letters are not contiguous alphabetically,
        // but we can map via a lookup.
        static const char letters[] = "abcdefghijklmnopqrstuvwxyz";
        static const ScanCode letter_sc[] = {
            sc_A, sc_B, sc_C, sc_D, sc_E, sc_F, sc_G, sc_H,
            sc_I, sc_J, sc_K, sc_L, sc_M, sc_N, sc_O, sc_P,
            sc_Q, sc_R, sc_S, sc_T, sc_U, sc_V, sc_W, sc_X,
            sc_Y, sc_Z
        };
        for (int i = 0; i < 26; i++) {
            if (sc == letter_sc[i]) {
                return upper ? (letters[i] - 32) : letters[i];
            }
        }
        return 0;
    }

    // Numbers row
    if (sc == sc_1) return shifted ? '!' : '1';
    if (sc == sc_2) return shifted ? '@' : '2';
    if (sc == sc_3) return shifted ? '#' : '3';
    if (sc == sc_4) return shifted ? '$' : '4';
    if (sc == sc_5) return shifted ? '%' : '5';
    if (sc == sc_6) return shifted ? '^' : '6';
    if (sc == sc_7) return shifted ? '&' : '7';
    if (sc == sc_8) return shifted ? '*' : '8';
    if (sc == sc_9) return shifted ? '(' : '9';
    if (sc == sc_0) return shifted ? ')' : '0';

    // Punctuation
    if (sc == sc_Minus)      return shifted ? '_' : '-';
    if (sc == sc_Equals)     return shifted ? '+' : '=';
    if (sc == sc_BackSpace)  return 8;   // backspace
    if (sc == sc_Tab)        return 9;   // tab
    if (sc == sc_OpenBracket)  return shifted ? '{' : '[';
    if (sc == sc_CloseBracket) return shifted ? '}' : ']';
    if (sc == sc_Return)     return 13;  // enter
    if (sc == sc_SemiColon)  return shifted ? ':' : ';';
    if (sc == sc_Quote)      return shifted ? '"' : '\'';
    if (sc == sc_BackSlash)  return shifted ? '|' : '\\';
    if (sc == sc_Comma)      return shifted ? '<' : ',';
    if (sc == sc_Period)     return shifted ? '>' : '.';
    if (sc == sc_Slash)      return shifted ? '?' : '/';
    if (sc == sc_Space)      return ' ';
    if (sc == sc_BackSlash)  return shifted ? '|' : '\\';

    return 0;
}

// -----------------------------------------------------------------------
// SDL event pump - process all pending events
// -----------------------------------------------------------------------
static void IN_PumpEvents(void)
{
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_EVENT_KEY_DOWN: {
            ScanCode pc = SDL_ScanCode_to_PC(event.key.scancode);
            if (SDL_getenv("WOLF3D_INDEBUG")) {
                fprintf(stderr, "KEYDOWN sdlscan=%d -> pc=%d\n", (int)event.key.scancode, (int)pc);
                fflush(stderr);
            }
            if (pc != sc_None && pc < NumCodes) {
                Keyboard[pc] = true;
                LastScan = pc;
                LastASCII = IN_TranslateKey(pc, SDL_GetModState());
            }
            if (keyHook) {
                keyHook();
            }
            break;
        }
        case SDL_EVENT_KEY_UP: {
            ScanCode pc = SDL_ScanCode_to_PC(event.key.scancode);
            if (pc != sc_None && pc < NumCodes) {
                Keyboard[pc] = false;
            }
            break;
        }
        case SDL_EVENT_MOUSE_MOTION:
            mouseDX += event.motion.xrel;
            mouseDY += event.motion.yrel;
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (event.button.button == 1) mouseButtons |= 1;
            if (event.button.button == 2) mouseButtons |= 4;
            if (event.button.button == 3) mouseButtons |= 2;
            if (event.button.button == 4) mouseButtons |= 8;
            break;
        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (event.button.button == 1) mouseButtons &= ~1;
            if (event.button.button == 2) mouseButtons &= ~4;
            if (event.button.button == 3) mouseButtons &= ~2;
            if (event.button.button == 4) mouseButtons &= ~8;
            break;
        case SDL_EVENT_QUIT:
            SDL_Log("Received SDL_QUIT, exiting");
            SDL_Quit();
            exit(0);
            break;
        default:
            break;
        }
    }
}

static int INL_NormalizeAxisValue(SDL_Joystick *joy, int axisIndex)
{
    const int axisCount = SDL_GetNumJoystickAxes(joy);
    if (axisIndex < 0 || axisIndex >= axisCount)
        return JOY_AXIS_MID;

    return (int)SDL_GetJoystickAxis(joy, axisIndex) + 32768;
}

static void INL_SetJoyScale(int joy)
{
    JoystickDef *def = &JoyDefs[joy];
    if (def->threshMinX > def->minX)
        def->joyMultXL = JOY_SCALE_MAX / (def->threshMinX - def->minX);
    else
        def->joyMultXL = 0;

    if (def->maxX > def->threshMaxX)
        def->joyMultXH = JOY_SCALE_MAX / (def->maxX - def->threshMaxX);
    else
        def->joyMultXH = 0;

    if (def->threshMinY > def->minY)
        def->joyMultYL = JOY_SCALE_MAX / (def->threshMinY - def->minY);
    else
        def->joyMultYL = 0;

    if (def->maxY > def->threshMaxY)
        def->joyMultYH = JOY_SCALE_MAX / (def->maxY - def->threshMaxY);
    else
        def->joyMultYH = 0;
}

static void INL_UpdateJoyDefaults(int joy)
{
    JoyDefs[joy].minX = 0;
    JoyDefs[joy].minY = 0;
    JoyDefs[joy].maxX = JOY_AXIS_MAX;
    JoyDefs[joy].maxY = JOY_AXIS_MAX;
    JoyDefs[joy].threshMinX = JoyDefs[joy].minX + (JOY_AXIS_MAX / 3);
    JoyDefs[joy].threshMaxX = JoyDefs[joy].maxX - (JOY_AXIS_MAX / 3);
    JoyDefs[joy].threshMinY = JoyDefs[joy].minY + (JOY_AXIS_MAX / 3);
    JoyDefs[joy].threshMaxY = JoyDefs[joy].maxY - (JOY_AXIS_MAX / 3);
    INL_SetJoyScale(joy);
    joyDefaultsSet[joy] = true;
}

static word INL_GetJoyButtonsByIndex(int joy)
{
    int i;
    SDL_Joystick *handle;
    word result = 0;

    if (joy < 0 || joy >= MaxJoys)
        return 0;

    handle = joyHandles[joy];
    if (!handle)
        return 0;

    for (i = 0; i < 4; i++) {
        if (SDL_GetJoystickButton(handle, i)) {
            result |= (1 << i);
        }
    }

    return result;
}

// -----------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------

void IN_Startup(void)
{
    int i;
    int numJoys = 0;
    SDL_JoystickID *joys = NULL;

    MousePresent = true;  // SDL3 handles mice transparently

    memset(Keyboard, 0, sizeof(Keyboard));
    memset(JoysPresent, 0, sizeof(JoysPresent));
    memset(Controls, 0, sizeof(Controls));

    LastScan = sc_None;
    LastASCII = 0;
    Paused = false;
    mouseButtons = 0;
    mouseDX = 0;
    mouseDY = 0;
    ackStarted = false;
    keyHook = NULL;
    DemoMode = demo_Off;
    DemoBuffer = NULL;
    DemoOffset = 0;
    DemoSize = 0;
    for (i = 0; i < MaxJoys; i++) {
        joyDefaultsSet[i] = false;
        joyLastX[i] = JOY_AXIS_MID;
        joyLastY[i] = JOY_AXIS_MID;
        if (joyHandles[i]) {
            SDL_CloseJoystick(joyHandles[i]);
            joyHandles[i] = NULL;
        }
    }

    // Default keyboard bindings (Wolf3D defaults)
    KbdDefs.button0 = sc_Control;
    KbdDefs.button1 = sc_Alt;
    KbdDefs.upleft   = sc_Home;
    KbdDefs.up       = sc_UpArrow;
    KbdDefs.upright  = sc_PgUp;
    KbdDefs.left     = sc_LeftArrow;
    KbdDefs.right    = sc_RightArrow;
    KbdDefs.downleft = sc_End;
    KbdDefs.down     = sc_DownArrow;
    KbdDefs.downright = sc_PgDn;

    // Default control type
    for (i = 0; i < MaxPlayers; i++) {
        Controls[i] = ctrl_Keyboard;
    }

    // Detect and open up to MaxJoys devices
    joys = SDL_GetJoysticks(&numJoys);
    for (i = 0; i < MaxJoys; i++) {
        if (i < numJoys) {
            joyHandles[i] = SDL_OpenJoystick(joys[i]);
            JoysPresent[i] = (joyHandles[i] != NULL);
            if (JoysPresent[i]) {
                INL_UpdateJoyDefaults(i);
            }
        } else {
            JoysPresent[i] = false;
        }
    }
    SDL_free(joys);
}

void IN_Shutdown(void)
{
    int i;
    for (i = 0; i < MaxJoys; i++) {
        if (joyHandles[i]) {
            SDL_CloseJoystick(joyHandles[i]);
            joyHandles[i] = NULL;
        }
        JoysPresent[i] = false;
        joyDefaultsSet[i] = false;
    }
}

void IN_ClearKeysDown(void)
{
    memset(Keyboard, 0, sizeof(Keyboard));
    LastScan = sc_None;
    LastASCII = 0;
}

void IN_SetKeyHook(void (*hook)(void))
{
    keyHook = hook;
}

void IN_Default(boolean gotit, ControlType which)
{
    if (!gotit) {
        Controls[0] = which;
    }
}

// -----------------------------------------------------------------------
// Wait for any key/mouse press
// -----------------------------------------------------------------------
void IN_Ack(void)
{
    IN_StartAck();
    while (!IN_CheckAck()) {
        SD_Poll();
        VL_Present();
    }
}

void IN_AckBack(void)
{
    IN_ClearKeysDown();
    IN_Ack();
}

void IN_StartAck(void)
{
    IN_ClearKeysDown();
    mouseButtons = 0;
    mouseDX = 0;
    mouseDY = 0;
    ackStarted = true;
    ackStartTime = SDL_GetTicks();
}

boolean IN_CheckAck(void)
{
    IN_PumpEvents();

    if (LastScan != sc_None)  return true;
    if (mouseButtons)         return true;

    return false;
}

// -----------------------------------------------------------------------
// Wait for a key press, return the scan code
// -----------------------------------------------------------------------
ScanCode IN_WaitForKey(void)
{
    IN_ClearKeysDown();

    while (true) {
        IN_PumpEvents();
        SD_Poll();
        VL_Present();

        if (LastScan != sc_None) {
            ScanCode result = LastScan;
            IN_ClearKeysDown();
            return result;
        }
    }
}

// -----------------------------------------------------------------------
// Wait for a key press, return the ASCII character
// -----------------------------------------------------------------------
char IN_WaitForASCII(void)
{
    IN_ClearKeysDown();

    while (true) {
        IN_PumpEvents();
        VL_Present();

        if (LastASCII) {
            char result = LastASCII;
            IN_ClearKeysDown();
            return result;
        }
    }
}

// -----------------------------------------------------------------------
// Read cursor/mouse state
// -----------------------------------------------------------------------
void IN_ReadCursor(CursorInfo *ci)
{
    IN_PumpEvents();

    memset(ci, 0, sizeof(CursorInfo));

    ci->x = mouseDX;
    ci->y = mouseDY;
    ci->button0 = (mouseButtons & 1) != 0;
    ci->button1 = (mouseButtons & 2) != 0;
    ci->button2 = (mouseButtons & 4) != 0;
    ci->button3 = (mouseButtons & 8) != 0;

    // Determine xaxis / yaxis motion from delta
    if (ci->x < 0)       ci->xaxis = motion_Left;
    else if (ci->x > 0)  ci->xaxis = motion_Right;
    else                  ci->xaxis = motion_None;

    if (ci->y < 0)       ci->yaxis = motion_Up;
    else if (ci->y > 0)  ci->yaxis = motion_Down;
    else                  ci->yaxis = motion_None;

    // Determine 8-direction from xaxis + yaxis
    ci->dir = dir_None;
    if (ci->xaxis == motion_None && ci->yaxis == motion_Up)    ci->dir = dir_North;
    if (ci->xaxis == motion_None && ci->yaxis == motion_Down)  ci->dir = dir_South;
    if (ci->xaxis == motion_Left  && ci->yaxis == motion_None) ci->dir = dir_West;
    if (ci->xaxis == motion_Right && ci->yaxis == motion_None) ci->dir = dir_East;
    if (ci->xaxis == motion_Right && ci->yaxis == motion_Up)   ci->dir = dir_NorthEast;
    if (ci->xaxis == motion_Right && ci->yaxis == motion_Down) ci->dir = dir_SouthEast;
    if (ci->xaxis == motion_Left  && ci->yaxis == motion_Up)   ci->dir = dir_NorthWest;
    if (ci->xaxis == motion_Left  && ci->yaxis == motion_Down) ci->dir = dir_SouthWest;

    // Consume deltas
    mouseDX = 0;
    mouseDY = 0;
}

// -----------------------------------------------------------------------
// Read player control input
// -----------------------------------------------------------------------
void IN_ReadControl(int player, ControlInfo *ci)
{
    KeyboardDef *def;
    boolean button0 = false, button1 = false;
    boolean up = false, down = false, left = false, right = false;
    int joydx = 0, joydy = 0;
    word buttons = 0;

    IN_PumpEvents();

    memset(ci, 0, sizeof(ControlInfo));

    ControlType type = Controls[player];

    switch (type) {
    case ctrl_Keyboard:
    case ctrl_Keyboard1:
    case ctrl_Keyboard2:
        def = &KbdDefs;

        if (Keyboard[def->button0])  button0 = true;
        if (Keyboard[def->button1])  button1 = true;
        if (Keyboard[def->up])       up = true;
        if (Keyboard[def->down])     down = true;
        if (Keyboard[def->left])     left = true;
        if (Keyboard[def->right])    right = true;
        if (Keyboard[def->upleft])   { up = true; left = true; }
        if (Keyboard[def->upright])  { up = true; right = true; }
        if (Keyboard[def->downleft]) { down = true; left = true; }
        if (Keyboard[def->downright]){ down = true; right = true; }

        ci->xaxis = motion_None;
        ci->yaxis = motion_None;
        if (left)  ci->xaxis = motion_Left;
        if (right) ci->xaxis = motion_Right;
        if (up)    ci->yaxis = motion_Up;
        if (down)  ci->yaxis = motion_Down;
        ci->dir = dir_None;
        if (ci->xaxis == motion_None && ci->yaxis == motion_Up)    ci->dir = dir_North;
        if (ci->xaxis == motion_None && ci->yaxis == motion_Down)  ci->dir = dir_South;
        if (ci->xaxis == motion_Left  && ci->yaxis == motion_None) ci->dir = dir_West;
        if (ci->xaxis == motion_Right && ci->yaxis == motion_None) ci->dir = dir_East;
        if (ci->xaxis == motion_Right && ci->yaxis == motion_Up)   ci->dir = dir_NorthEast;
        if (ci->xaxis == motion_Right && ci->yaxis == motion_Down) ci->dir = dir_SouthEast;
        if (ci->xaxis == motion_Left  && ci->yaxis == motion_Up)   ci->dir = dir_NorthWest;
        if (ci->xaxis == motion_Left  && ci->yaxis == motion_Down) ci->dir = dir_SouthWest;

        ci->button0 = button0;
        ci->button1 = button1;
        break;

    case ctrl_Mouse:
        ci->x = mouseDX;
        ci->y = mouseDY;
        ci->button0 = (mouseButtons & 1) != 0;
        ci->button1 = (mouseButtons & 2) != 0;
        ci->button2 = (mouseButtons & 4) != 0;
        ci->button3 = (mouseButtons & 8) != 0;

        if (ci->x < 0)       ci->xaxis = motion_Left;
        else if (ci->x > 0)  ci->xaxis = motion_Right;
        else                  ci->xaxis = motion_None;

        if (ci->y < 0)       ci->yaxis = motion_Up;
        else if (ci->y > 0)  ci->yaxis = motion_Down;
        else                  ci->yaxis = motion_None;

        mouseDX = 0;
        mouseDY = 0;
        break;

    case ctrl_Joystick:
    case ctrl_Joystick1:
    case ctrl_Joystick2:
        {
            int joy = type - ctrl_Joystick;
            if (joy >= 0 && joy < MaxJoys) {
                INL_GetJoyDelta(joy, &joydx, &joydy);
                buttons = INL_GetJoyButtonsByIndex(joy);
                ci->x = joydx;
                ci->y = joydy;
            }
            else {
                ci->x = 0;
                ci->y = 0;
            }
        }

        if (joydx < 0)       ci->xaxis = motion_Left;
        else if (joydx > 0)  ci->xaxis = motion_Right;
        else                  ci->xaxis = motion_None;

        if (joydy < 0)       ci->yaxis = motion_Up;
        else if (joydy > 0)  ci->yaxis = motion_Down;
        else                  ci->yaxis = motion_None;

        ci->button0 = (buttons & 1) != 0;
        ci->button1 = (buttons & 2) != 0;
        ci->button2 = (buttons & 4) != 0;
        ci->button3 = (buttons & 8) != 0;

        ci->dir = dir_None;
        if (ci->xaxis == motion_None && ci->yaxis == motion_Up)    ci->dir = dir_North;
        if (ci->xaxis == motion_None && ci->yaxis == motion_Down)  ci->dir = dir_South;
        if (ci->xaxis == motion_Left  && ci->yaxis == motion_None) ci->dir = dir_West;
        if (ci->xaxis == motion_Right && ci->yaxis == motion_None) ci->dir = dir_East;
        if (ci->xaxis == motion_Right && ci->yaxis == motion_Up)   ci->dir = dir_NorthEast;
        if (ci->xaxis == motion_Right && ci->yaxis == motion_Down) ci->dir = dir_SouthEast;
        if (ci->xaxis == motion_Left  && ci->yaxis == motion_Up)   ci->dir = dir_NorthWest;
        if (ci->xaxis == motion_Left  && ci->yaxis == motion_Down) ci->dir = dir_SouthWest;
        break;
    }
}

void IN_SetControlType(int player, ControlType type)
{
    if (player >= 0 && player < MaxPlayers) {
        Controls[player] = type;
    }
}

// -----------------------------------------------------------------------
// Timed user input check - returns true if input received within delay ms
// -----------------------------------------------------------------------
boolean IN_UserInput(longword delay)
{
    // delay is in 70Hz ticks; convert to milliseconds for SDL_GetTicks
    Uint32 ms = (Uint32)delay * 1000u / 70u;
    Uint32 target = SDL_GetTicks() + ms;

    IN_StartAck();

    do {
        IN_PumpEvents();
        SD_Poll();
        VL_Present();

        if (LastScan != sc_None || mouseButtons) {
            IN_ClearKeysDown();
            return true;
        }
    } while (SDL_GetTicks() < target);

    return false;
}

// -----------------------------------------------------------------------
// Demo buffer stubs
// -----------------------------------------------------------------------
void IN_StopDemo(void)
{
    DemoMode = demo_Off;
}

void IN_FreeDemoBuffer(void)
{
    if (DemoBuffer) {
        free(DemoBuffer);
        DemoBuffer = NULL;
    }
    DemoOffset = 0;
    DemoSize = 0;
}

// -----------------------------------------------------------------------
// Return current mouse button bitmask
// -----------------------------------------------------------------------
word IN_MouseButtons(void)
{
    IN_PumpEvents();
    return (word)mouseButtons;
}

// -----------------------------------------------------------------------
// Missing IN_* functions
// -----------------------------------------------------------------------

boolean IN_KeyDown(ScanCode code)
{
    if (code < NumCodes)
        return Keyboard[code];
    return false;
}

void IN_ClearKey(ScanCode code)
{
    if (code < NumCodes)
        Keyboard[code] = false;
    if (LastScan == code)
        LastScan = sc_None;
}

void IN_GetJoyAbs(int joy, int *x, int *y)
{
    int axisX, axisY;

    if (joy < 0 || joy >= MaxJoys || !JoysPresent[joy] || joyHandles[joy] == NULL) {
        if (x) *x = JOY_AXIS_MID;
        if (y) *y = JOY_AXIS_MID;
        return;
    }

    axisX = INL_NormalizeAxisValue(joyHandles[joy], 0);
    axisY = INL_NormalizeAxisValue(joyHandles[joy], 1);

    if (axisX < 0) axisX = 0;
    if (axisX > JOY_AXIS_MAX) axisX = JOY_AXIS_MAX;
    if (axisY < 0) axisY = 0;
    if (axisY > JOY_AXIS_MAX) axisY = JOY_AXIS_MAX;

    if (x) *x = axisX;
    if (y) *y = axisY;
}

void IN_SetupJoy(int joy, int xmin, int xmax, int ymin, int ymax)
{
    int range;

    if (joy < 0 || joy >= MaxJoys) return;
    if (xmax <= xmin || ymax <= ymin) return;

    JoyDefs[joy].minX = (word)xmin;
    JoyDefs[joy].maxX = (word)xmax;
    range = xmax - xmin;
    JoyDefs[joy].threshMinX = ((range / 2) - (range / 3)) + xmin;
    JoyDefs[joy].threshMaxX = ((range / 2) + (range / 3)) + xmin;

    JoyDefs[joy].minY = (word)ymin;
    JoyDefs[joy].maxY = (word)ymax;
    range = ymax - ymin;
    JoyDefs[joy].threshMinY = ((range / 2) - (range / 3)) + ymin;
    JoyDefs[joy].threshMaxY = ((range / 2) + (range / 3)) + ymin;

    INL_SetJoyScale(joy);
    joyDefaultsSet[joy] = true;

    if (joyLastX[joy] < xmin) joyLastX[joy] = xmin;
    if (joyLastX[joy] > xmax) joyLastX[joy] = xmax;
    if (joyLastY[joy] < ymin) joyLastY[joy] = ymin;
    if (joyLastY[joy] > ymax) joyLastY[joy] = ymax;
}

word IN_JoyButtons(void)
{
    word result = 0;
    result |= (INL_GetJoyButtonsByIndex(0) << 0);
    result |= (INL_GetJoyButtonsByIndex(1) << 2);
    return result;
}

void INL_GetJoyDelta(int joy, int *dx, int *dy)
{
    int x, y;
    word minX, maxX, minY, maxY;
    longword scaled;
    JoystickDef *def;

    if (!dx || !dy)
        return;

    if (!JoysPresent[joy] || joy < 0 || joy >= MaxJoys) {
        *dx = 0;
        *dy = 0;
        return;
    }

    if (!joyDefaultsSet[joy]) {
        INL_UpdateJoyDefaults(joy);
    }

    IN_GetJoyAbs(joy, &x, &y);
    def = &JoyDefs[joy];

    minX = def->minX;
    maxX = def->maxX;
    minY = def->minY;
    maxY = def->maxY;

    if (x < def->threshMinX) {
        if (x < minX)
            x = minX;

        scaled = (longword)(def->threshMinX - x) * def->joyMultXL;
        scaled >>= JOY_SCALE_SHIFT;
        *dx = (scaled > 127u) ? -127 : -(int)scaled;
    }
    else if (x > def->threshMaxX) {
        if (x > maxX)
            x = maxX;

        scaled = (longword)(x - def->threshMaxX) * def->joyMultXH;
        scaled >>= JOY_SCALE_SHIFT;
        *dx = (scaled > 127u) ? 127 : (int)scaled;
    }
    else
        *dx = 0;

    if (y < def->threshMinY) {
        if (y < minY)
            y = minY;

        scaled = (longword)(def->threshMinY - y) * def->joyMultYL;
        scaled >>= JOY_SCALE_SHIFT;
        *dy = (scaled > 127u) ? -127 : -(int)scaled;
    }
    else if (y > def->threshMaxY) {
        if (y > maxY)
            y = maxY;

        scaled = (longword)(y - def->threshMaxY) * def->joyMultYH;
        scaled >>= JOY_SCALE_SHIFT;
        *dy = (scaled > 127u) ? 127 : (int)scaled;
    }
    else
        *dy = 0;

    joyLastX[joy] = x;
    joyLastY[joy] = y;
}
