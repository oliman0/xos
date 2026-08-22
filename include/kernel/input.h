#ifndef XOS_INPUT_H
#define XOS_INPUT_H

#include <stdint.h>

typedef enum {
    KEY_NONE = 0,

    KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_H, KEY_I, KEY_J,
    KEY_K, KEY_L, KEY_M, KEY_N, KEY_O, KEY_P, KEY_Q, KEY_R, KEY_S, KEY_T,
    KEY_U, KEY_V, KEY_W, KEY_X, KEY_Y, KEY_Z,

    KEY_0, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9,

    KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6,
    KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_F11, KEY_F12,

    KEY_ESCAPE, KEY_ENTER, KEY_BACKSPACE, KEY_TAB, KEY_SPACE,
    KEY_CAPSLOCK, KEY_NUMLOCK, KEY_SCROLLLOCK,
    KEY_LSHIFT, KEY_RSHIFT, KEY_LCTRL, KEY_RCTRL,
    KEY_LALT, KEY_RALT, KEY_LMETA, KEY_RMETA,

    KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT,
    KEY_INSERT, KEY_DELETE, KEY_HOME, KEY_END,
    KEY_PAGEUP, KEY_PAGEDOWN, KEY_PRINTSCREEN, KEY_PAUSE,

    KEY_GRAVE,
    KEY_MINUS,
    KEY_EQUAL,
    KEY_LEFTBRACKET,
    KEY_RIGHTBRACKET,
    KEY_BACKSLASH,
    KEY_SEMICOLON,
    KEY_APOSTROPHE,
    KEY_COMMA,
    KEY_DOT,
    KEY_SLASH,

    KEY_KP_0, KEY_KP_1, KEY_KP_2, KEY_KP_3, KEY_KP_4,
    KEY_KP_5, KEY_KP_6, KEY_KP_7, KEY_KP_8, KEY_KP_9,
    KEY_KP_DECIMAL, KEY_KP_DIVIDE, KEY_KP_MULTIPLY,
    KEY_KP_MINUS, KEY_KP_PLUS, KEY_KP_ENTER,

    KEY_MAX
} key_code_t;

typedef enum {
    MOD_NONE     = 0,
    MOD_LSHIFT   = (1 << 0),
    MOD_RSHIFT   = (1 << 1),
    MOD_LCTRL    = (1 << 2),
    MOD_RCTRL    = (1 << 3),
    MOD_LALT     = (1 << 4),
    MOD_RALT     = (1 << 5),
    MOD_CAPSLOCK = (1 << 6),
    MOD_NUMLOCK  = (1 << 7),
} key_modifier_t;

typedef struct
{
    key_code_t key_code;
    bool pressed;
    uint8_t modifiers;
} keyboard_event_t;

static const char ascii_base_map[KEY_MAX] = {
    [KEY_A] = 'a', [KEY_B] = 'b', [KEY_C] = 'c', [KEY_D] = 'd', [KEY_E] = 'e',
    [KEY_F] = 'f', [KEY_G] = 'g', [KEY_H] = 'h', [KEY_I] = 'i', [KEY_J] = 'j',
    [KEY_K] = 'k', [KEY_L] = 'l', [KEY_M] = 'm', [KEY_N] = 'n', [KEY_O] = 'o',
    [KEY_P] = 'p', [KEY_Q] = 'q', [KEY_R] = 'r', [KEY_S] = 's', [KEY_T] = 't',
    [KEY_U] = 'u', [KEY_V] = 'v', [KEY_W] = 'w', [KEY_X] = 'x', [KEY_Y] = 'y', [KEY_Z] = 'z',
    [KEY_1] = '1', [KEY_2] = '2', [KEY_3] = '3', [KEY_4] = '4', [KEY_5] = '5',
    [KEY_6] = '6', [KEY_7] = '7', [KEY_8] = '8', [KEY_9] = '9', [KEY_0] = '0',
    [KEY_ENTER] = '\n', [KEY_TAB] = '\t', [KEY_BACKSPACE] = '\b', [KEY_SPACE] = ' ',
    [KEY_MINUS] = '-', [KEY_EQUAL] = '=', [KEY_LEFTBRACKET] = '[', [KEY_RIGHTBRACKET] = ']',
    [KEY_SEMICOLON] = ';', [KEY_APOSTROPHE] = '\'', [KEY_GRAVE] = '`', [KEY_BACKSLASH] = '\\',
    [KEY_COMMA] = ',', [KEY_DOT] = '.', [KEY_SLASH] = '/'
};

static const char ascii_shift_map[KEY_MAX] = {
    [KEY_A] = 'A', [KEY_B] = 'B', [KEY_C] = 'C', [KEY_D] = 'D', [KEY_E] = 'E',
    [KEY_F] = 'F', [KEY_G] = 'G', [KEY_H] = 'H', [KEY_I] = 'I', [KEY_J] = 'J',
    [KEY_K] = 'K', [KEY_L] = 'L', [KEY_M] = 'M', [KEY_N] = 'N', [KEY_O] = 'O',
    [KEY_P] = 'P', [KEY_Q] = 'Q', [KEY_R] = 'R', [KEY_S] = 'S', [KEY_T] = 'T',
    [KEY_U] = 'U', [KEY_V] = 'V', [KEY_W] = 'W', [KEY_X] = 'X', [KEY_Y] = 'Y', [KEY_Z] = 'Z',
    [KEY_1] = '!', [KEY_2] = '@', [KEY_3] = '#', [KEY_4] = '$', [KEY_5] = '%',
    [KEY_6] = '^', [KEY_7] = '&', [KEY_8] = '*', [KEY_9] = '(', [KEY_0] = ')',
    [KEY_ENTER] = '\n', [KEY_TAB] = '\t', [KEY_BACKSPACE] = '\b', [KEY_SPACE] = ' ',
    [KEY_MINUS] = '_', [KEY_EQUAL] = '+', [KEY_LEFTBRACKET] = '{', [KEY_RIGHTBRACKET] = '}',
    [KEY_SEMICOLON] = ':', [KEY_APOSTROPHE] = '"', [KEY_GRAVE] = '~', [KEY_BACKSLASH] = '|',
    [KEY_COMMA] = '<', [KEY_DOT] = '>', [KEY_SLASH] = '?'
};

#endif
