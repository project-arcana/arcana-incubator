#pragma once

#include <cstdint>

namespace inc::da
{
// NOTE: these translate exactly to SDL_Scancode and SDL_Keycode,
// the only purpose of this is an enum class and typesafety
// every single scancode and keycovde value is static_asserted in this file's TU, do not rename them

enum class scancode : int
{
    sc_A = 4,
    sc_B = 5,
    sc_C = 6,
    sc_D = 7,
    sc_E = 8,
    sc_F = 9,
    sc_G = 10,
    sc_H = 11,
    sc_I = 12,
    sc_J = 13,
    sc_K = 14,
    sc_L = 15,
    sc_M = 16,
    sc_N = 17,
    sc_O = 18,
    sc_P = 19,
    sc_Q = 20,
    sc_R = 21,
    sc_S = 22,
    sc_T = 23,
    sc_U = 24,
    sc_V = 25,
    sc_W = 26,
    sc_X = 27,
    sc_Y = 28,
    sc_Z = 29,

    sc_1 = 30,
    sc_2 = 31,
    sc_3 = 32,
    sc_4 = 33,
    sc_5 = 34,
    sc_6 = 35,
    sc_7 = 36,
    sc_8 = 37,
    sc_9 = 38,
    sc_0 = 39,

    sc_RETURN = 40,
    sc_ESCAPE = 41,
    sc_BACKSPACE = 42,
    sc_TAB = 43,
    sc_SPACE = 44,

    sc_MINUS = 45,
    sc_EQUALS = 46,
    sc_LEFTBRACKET = 47,
    sc_RIGHTBRACKET = 48,
    sc_BACKSLASH = 49, /**< Located at the lower left of the return
                        *   key on ISO keyboards and at the right end
                        *   of the QWERTY row on ANSI keyboards.
                        *   Produces REVERSE SOLIDUS (backslash) and
                        *   VERTICAL LINE in a US layout, REVERSE
                        *   SOLIDUS and VERTICAL LINE in a UK Mac
                        *   layout, NUMBER SIGN and TILDE in a UK
                        *   Windows layout, DOLLAR SIGN and POUND SIGN
                        *   in a Swiss German layout, NUMBER SIGN and
                        *   APOSTROPHE in a German layout, GRAVE
                        *   ACCENT and POUND SIGN in a French Mac
                        *   layout, and ASTERISK and MICRO SIGN in a
                        *   French Windows layout.
                        */
    sc_NONUSHASH = 50, /**< ISO USB keyboards actually use this code
                        *   instead of 49 for the same key, but all
                        *   OSes I've seen treat the two codes
                        *   identically. So, as an implementor, unless
                        *   your keyboard generates both of those
                        *   codes and your OS treats them differently,
                        *   you should generate sc_BACKSLASH
                        *   instead of this code. As a user, you
                        *   should not rely on this code because SDL
                        *   will never generate it with most (all?)
                        *   keyboards.
                        */
    sc_SEMICOLON = 51,
    sc_APOSTROPHE = 52,
    sc_GRAVE = 53, /**< Located in the top left corner (on both ANSI
                    *   and ISO keyboards). Produces GRAVE ACCENT and
                    *   TILDE in a US Windows layout and in US and UK
                    *   Mac layouts on ANSI keyboards, GRAVE ACCENT
                    *   and NOT SIGN in a UK Windows layout, SECTION
                    *   SIGN and PLUS-MINUS SIGN in US and UK Mac
                    *   layouts on ISO keyboards, SECTION SIGN and
                    *   DEGREE SIGN in a Swiss German layout (Mac:
                    *   only on ISO keyboards), CIRCUMFLEX ACCENT and
                    *   DEGREE SIGN in a German layout (Mac: only on
                    *   ISO keyboards), SUPERSCRIPT TWO and TILDE in a
                    *   French Windows layout, COMMERCIAL AT and
                    *   NUMBER SIGN in a French Mac layout on ISO
                    *   keyboards, and LESS-THAN SIGN and GREATER-THAN
                    *   SIGN in a Swiss German, German, or French Mac
                    *   layout on ANSI keyboards.
                    */
    sc_COMMA = 54,
    sc_PERIOD = 55,
    sc_SLASH = 56,
    sc_CAPSLOCK = 57,
    sc_F1 = 58,
    sc_F2 = 59,
    sc_F3 = 60,
    sc_F4 = 61,
    sc_F5 = 62,
    sc_F6 = 63,
    sc_F7 = 64,
    sc_F8 = 65,
    sc_F9 = 66,
    sc_F10 = 67,
    sc_F11 = 68,
    sc_F12 = 69,
    sc_PRINTSCREEN = 70,
    sc_SCROLLLOCK = 71,
    sc_PAUSE = 72,
    sc_INSERT = 73, /**< insert on PC, help on some Mac keyboards (but
                                   does send code 73, not 117) */
    sc_HOME = 74,
    sc_PAGEUP = 75,
    sc_DELETE = 76,
    sc_END = 77,
    sc_PAGEDOWN = 78,
    sc_RIGHT = 79,
    sc_LEFT = 80,
    sc_DOWN = 81,
    sc_UP = 82,
    sc_NUMLOCKCLEAR = 83, /**< num lock on PC, clear on Mac keyboards
                           */
    sc_KP_DIVIDE = 84,
    sc_KP_MULTIPLY = 85,
    sc_KP_MINUS = 86,
    sc_KP_PLUS = 87,
    sc_KP_ENTER = 88,
    sc_KP_1 = 89,
    sc_KP_2 = 90,
    sc_KP_3 = 91,
    sc_KP_4 = 92,
    sc_KP_5 = 93,
    sc_KP_6 = 94,
    sc_KP_7 = 95,
    sc_KP_8 = 96,
    sc_KP_9 = 97,
    sc_KP_0 = 98,
    sc_KP_PERIOD = 99,
    sc_NONUSBACKSLASH = 100, /**< This is the additional key that ISO
                              *   keyboards have over ANSI ones,
                              *   located between left shift and Y.
                              *   Produces GRAVE ACCENT and TILDE in a
                              *   US or UK Mac layout, REVERSE SOLIDUS
                              *   (backslash) and VERTICAL LINE in a
                              *   US or UK Windows layout, and
                              *   LESS-THAN SIGN and GREATER-THAN SIGN
                              *   in a Swiss German, German, or French
                              *   layout. */
    sc_APPLICATION = 101,    /**< windows contextual menu, compose */
    sc_POWER = 102,          /**< The USB document says this is a status flag,
                              *   not a physical key - but some Mac keyboards
                              *   do have a power key. */
    sc_KP_EQUALS = 103,
    sc_F13 = 104,
    sc_F14 = 105,
    sc_F15 = 106,
    sc_F16 = 107,
    sc_F17 = 108,
    sc_F18 = 109,
    sc_F19 = 110,
    sc_F20 = 111,
    sc_F21 = 112,
    sc_F22 = 113,
    sc_F23 = 114,
    sc_F24 = 115,
    sc_EXECUTE = 116,
    sc_HELP = 117,
    sc_MENU = 118,
    sc_SELECT = 119,
    sc_STOP = 120,
    sc_AGAIN = 121, /**< redo */
    sc_UNDO = 122,
    sc_CUT = 123,
    sc_COPY = 124,
    sc_PASTE = 125,
    sc_FIND = 126,
    sc_MUTE = 127,
    sc_VOLUMEUP = 128,
    sc_VOLUMEDOWN = 129,
    sc_KP_COMMA = 133,
    sc_KP_EQUALSAS400 = 134,
    sc_INTERNATIONAL1 = 135, /**< used on Asian keyboards, see
                                            footnotes in USB doc */
    sc_INTERNATIONAL2 = 136,
    sc_INTERNATIONAL3 = 137, /**< Yen */
    sc_INTERNATIONAL4 = 138,
    sc_INTERNATIONAL5 = 139,
    sc_INTERNATIONAL6 = 140,
    sc_INTERNATIONAL7 = 141,
    sc_INTERNATIONAL8 = 142,
    sc_INTERNATIONAL9 = 143,
    sc_LANG1 = 144,    /**< Hangul/English toggle */
    sc_LANG2 = 145,    /**< Hanja conversion */
    sc_LANG3 = 146,    /**< Katakana */
    sc_LANG4 = 147,    /**< Hiragana */
    sc_LANG5 = 148,    /**< Zenkaku/Hankaku */
    sc_LANG6 = 149,    /**< reserved */
    sc_LANG7 = 150,    /**< reserved */
    sc_LANG8 = 151,    /**< reserved */
    sc_LANG9 = 152,    /**< reserved */
    sc_ALTERASE = 153, /**< Erase-Eaze */
    sc_SYSREQ = 154,
    sc_CANCEL = 155,
    sc_CLEAR = 156,
    sc_PRIOR = 157,
    sc_RETURN2 = 158,
    sc_SEPARATOR = 159,
    sc_OUT = 160,
    sc_OPER = 161,
    sc_CLEARAGAIN = 162,
    sc_CRSEL = 163,
    sc_EXSEL = 164,
    sc_KP_00 = 176,
    sc_KP_000 = 177,
    sc_THOUSANDSSEPARATOR = 178,
    sc_DECIMALSEPARATOR = 179,
    sc_CURRENCYUNIT = 180,
    sc_CURRENCYSUBUNIT = 181,
    sc_KP_LEFTPAREN = 182,
    sc_KP_RIGHTPAREN = 183,
    sc_KP_LEFTBRACE = 184,
    sc_KP_RIGHTBRACE = 185,
    sc_KP_TAB = 186,
    sc_KP_BACKSPACE = 187,
    sc_KP_A = 188,
    sc_KP_B = 189,
    sc_KP_C = 190,
    sc_KP_D = 191,
    sc_KP_E = 192,
    sc_KP_F = 193,
    sc_KP_XOR = 194,
    sc_KP_POWER = 195,
    sc_KP_PERCENT = 196,
    sc_KP_LESS = 197,
    sc_KP_GREATER = 198,
    sc_KP_AMPERSAND = 199,
    sc_KP_DBLAMPERSAND = 200,
    sc_KP_VERTICALBAR = 201,
    sc_KP_DBLVERTICALBAR = 202,
    sc_KP_COLON = 203,
    sc_KP_HASH = 204,
    sc_KP_SPACE = 205,
    sc_KP_AT = 206,
    sc_KP_EXCLAM = 207,
    sc_KP_MEMSTORE = 208,
    sc_KP_MEMRECALL = 209,
    sc_KP_MEMCLEAR = 210,
    sc_KP_MEMADD = 211,
    sc_KP_MEMSUBTRACT = 212,
    sc_KP_MEMMULTIPLY = 213,
    sc_KP_MEMDIVIDE = 214,
    sc_KP_PLUSMINUS = 215,
    sc_KP_CLEAR = 216,
    sc_KP_CLEARENTRY = 217,
    sc_KP_BINARY = 218,
    sc_KP_OCTAL = 219,
    sc_KP_DECIMAL = 220,
    sc_KP_HEXADECIMAL = 221,
    sc_LCTRL = 224,
    sc_LSHIFT = 225,
    sc_LALT = 226, /**< alt, option */
    sc_LGUI = 227, /**< windows, command (apple), meta */
    sc_RCTRL = 228,
    sc_RSHIFT = 229,
    sc_RALT = 230, /**< alt gr, option */
    sc_RGUI = 231, /**< windows, command (apple), meta */
    sc_MODE = 257,
    sc_AUDIONEXT = 258,
    sc_AUDIOPREV = 259,
    sc_AUDIOSTOP = 260,
    sc_AUDIOPLAY = 261,
    sc_AUDIOMUTE = 262,
    sc_MEDIASELECT = 263,
    sc_WWW = 264,
    sc_MAIL = 265,
    sc_CALCULATOR = 266,
    sc_COMPUTER = 267,
    sc_AC_SEARCH = 268,
    sc_AC_HOME = 269,
    sc_AC_BACK = 270,
    sc_AC_FORWARD = 271,
    sc_AC_STOP = 272,
    sc_AC_REFRESH = 273,
    sc_AC_BOOKMARKS = 274,
    sc_BRIGHTNESSDOWN = 275,
    sc_BRIGHTNESSUP = 276,
    sc_DISPLAYSWITCH = 277,
    sc_KBDILLUMTOGGLE = 278,
    sc_KBDILLUMDOWN = 279,
    sc_KBDILLUMUP = 280,
    sc_EJECT = 281,
    sc_SLEEP = 282,
    sc_APP1 = 283,
    sc_APP2 = 284,
    sc_AUDIOREWIND = 285,
    sc_AUDIOFASTFORWARD = 286,
};

#define DA_SCANCODE_TO_KEYCODE(X) (int(scancode::X) | (1 << 30))

enum class keycode : int
{
    kc_RETURN = '\r',
    kc_ESCAPE = '\033',
    kc_BACKSPACE = '\b',
    kc_TAB = '\t',
    kc_SPACE = ' ',
    kc_EXCLAIM = '!',
    kc_QUOTEDBL = '"',
    kc_HASH = '#',
    kc_PERCENT = '%',
    kc_DOLLAR = '$',
    kc_AMPERSAND = '&',
    kc_QUOTE = '\'',
    kc_LEFTPAREN = '(',
    kc_RIGHTPAREN = ')',
    kc_ASTERISK = '*',
    kc_PLUS = '+',
    kc_COMMA = ',',
    kc_MINUS = '-',
    kc_PERIOD = '.',
    kc_SLASH = '/',
    kc_0 = '0',
    kc_1 = '1',
    kc_2 = '2',
    kc_3 = '3',
    kc_4 = '4',
    kc_5 = '5',
    kc_6 = '6',
    kc_7 = '7',
    kc_8 = '8',
    kc_9 = '9',
    kc_COLON = ':',
    kc_SEMICOLON = ';',
    kc_LESS = '<',
    kc_EQUALS = '=',
    kc_GREATER = '>',
    kc_QUESTION = '?',
    kc_AT = '@',
    /*
       Skip uppercase letters
     */
    kc_LEFTBRACKET = '[',
    kc_BACKSLASH = '\\',
    kc_RIGHTBRACKET = ']',
    kc_CARET = '^',
    kc_UNDERSCORE = '_',
    kc_BACKQUOTE = '`',
    kc_a = 'a',
    kc_b = 'b',
    kc_c = 'c',
    kc_d = 'd',
    kc_e = 'e',
    kc_f = 'f',
    kc_g = 'g',
    kc_h = 'h',
    kc_i = 'i',
    kc_j = 'j',
    kc_k = 'k',
    kc_l = 'l',
    kc_m = 'm',
    kc_n = 'n',
    kc_o = 'o',
    kc_p = 'p',
    kc_q = 'q',
    kc_r = 'r',
    kc_s = 's',
    kc_t = 't',
    kc_u = 'u',
    kc_v = 'v',
    kc_w = 'w',
    kc_x = 'x',
    kc_y = 'y',
    kc_z = 'z',

    kc_CAPSLOCK = DA_SCANCODE_TO_KEYCODE(sc_CAPSLOCK),

    kc_F1 = DA_SCANCODE_TO_KEYCODE(sc_F1),
    kc_F2 = DA_SCANCODE_TO_KEYCODE(sc_F2),
    kc_F3 = DA_SCANCODE_TO_KEYCODE(sc_F3),
    kc_F4 = DA_SCANCODE_TO_KEYCODE(sc_F4),
    kc_F5 = DA_SCANCODE_TO_KEYCODE(sc_F5),
    kc_F6 = DA_SCANCODE_TO_KEYCODE(sc_F6),
    kc_F7 = DA_SCANCODE_TO_KEYCODE(sc_F7),
    kc_F8 = DA_SCANCODE_TO_KEYCODE(sc_F8),
    kc_F9 = DA_SCANCODE_TO_KEYCODE(sc_F9),
    kc_F10 = DA_SCANCODE_TO_KEYCODE(sc_F10),
    kc_F11 = DA_SCANCODE_TO_KEYCODE(sc_F11),
    kc_F12 = DA_SCANCODE_TO_KEYCODE(sc_F12),

    kc_PRINTSCREEN = DA_SCANCODE_TO_KEYCODE(sc_PRINTSCREEN),
    kc_SCROLLLOCK = DA_SCANCODE_TO_KEYCODE(sc_SCROLLLOCK),
    kc_PAUSE = DA_SCANCODE_TO_KEYCODE(sc_PAUSE),
    kc_INSERT = DA_SCANCODE_TO_KEYCODE(sc_INSERT),
    kc_HOME = DA_SCANCODE_TO_KEYCODE(sc_HOME),
    kc_PAGEUP = DA_SCANCODE_TO_KEYCODE(sc_PAGEUP),
    kc_DELETE = '\177',
    kc_END = DA_SCANCODE_TO_KEYCODE(sc_END),
    kc_PAGEDOWN = DA_SCANCODE_TO_KEYCODE(sc_PAGEDOWN),
    kc_RIGHT = DA_SCANCODE_TO_KEYCODE(sc_RIGHT),
    kc_LEFT = DA_SCANCODE_TO_KEYCODE(sc_LEFT),
    kc_DOWN = DA_SCANCODE_TO_KEYCODE(sc_DOWN),
    kc_UP = DA_SCANCODE_TO_KEYCODE(sc_UP),

    kc_NUMLOCKCLEAR = DA_SCANCODE_TO_KEYCODE(sc_NUMLOCKCLEAR),
    kc_KP_DIVIDE = DA_SCANCODE_TO_KEYCODE(sc_KP_DIVIDE),
    kc_KP_MULTIPLY = DA_SCANCODE_TO_KEYCODE(sc_KP_MULTIPLY),
    kc_KP_MINUS = DA_SCANCODE_TO_KEYCODE(sc_KP_MINUS),
    kc_KP_PLUS = DA_SCANCODE_TO_KEYCODE(sc_KP_PLUS),
    kc_KP_ENTER = DA_SCANCODE_TO_KEYCODE(sc_KP_ENTER),
    kc_KP_1 = DA_SCANCODE_TO_KEYCODE(sc_KP_1),
    kc_KP_2 = DA_SCANCODE_TO_KEYCODE(sc_KP_2),
    kc_KP_3 = DA_SCANCODE_TO_KEYCODE(sc_KP_3),
    kc_KP_4 = DA_SCANCODE_TO_KEYCODE(sc_KP_4),
    kc_KP_5 = DA_SCANCODE_TO_KEYCODE(sc_KP_5),
    kc_KP_6 = DA_SCANCODE_TO_KEYCODE(sc_KP_6),
    kc_KP_7 = DA_SCANCODE_TO_KEYCODE(sc_KP_7),
    kc_KP_8 = DA_SCANCODE_TO_KEYCODE(sc_KP_8),
    kc_KP_9 = DA_SCANCODE_TO_KEYCODE(sc_KP_9),
    kc_KP_0 = DA_SCANCODE_TO_KEYCODE(sc_KP_0),
    kc_KP_PERIOD = DA_SCANCODE_TO_KEYCODE(sc_KP_PERIOD),

    kc_APPLICATION = DA_SCANCODE_TO_KEYCODE(sc_APPLICATION),
    kc_POWER = DA_SCANCODE_TO_KEYCODE(sc_POWER),
    kc_KP_EQUALS = DA_SCANCODE_TO_KEYCODE(sc_KP_EQUALS),
    kc_F13 = DA_SCANCODE_TO_KEYCODE(sc_F13),
    kc_F14 = DA_SCANCODE_TO_KEYCODE(sc_F14),
    kc_F15 = DA_SCANCODE_TO_KEYCODE(sc_F15),
    kc_F16 = DA_SCANCODE_TO_KEYCODE(sc_F16),
    kc_F17 = DA_SCANCODE_TO_KEYCODE(sc_F17),
    kc_F18 = DA_SCANCODE_TO_KEYCODE(sc_F18),
    kc_F19 = DA_SCANCODE_TO_KEYCODE(sc_F19),
    kc_F20 = DA_SCANCODE_TO_KEYCODE(sc_F20),
    kc_F21 = DA_SCANCODE_TO_KEYCODE(sc_F21),
    kc_F22 = DA_SCANCODE_TO_KEYCODE(sc_F22),
    kc_F23 = DA_SCANCODE_TO_KEYCODE(sc_F23),
    kc_F24 = DA_SCANCODE_TO_KEYCODE(sc_F24),
    kc_EXECUTE = DA_SCANCODE_TO_KEYCODE(sc_EXECUTE),
    kc_HELP = DA_SCANCODE_TO_KEYCODE(sc_HELP),
    kc_MENU = DA_SCANCODE_TO_KEYCODE(sc_MENU),
    kc_SELECT = DA_SCANCODE_TO_KEYCODE(sc_SELECT),
    kc_STOP = DA_SCANCODE_TO_KEYCODE(sc_STOP),
    kc_AGAIN = DA_SCANCODE_TO_KEYCODE(sc_AGAIN),
    kc_UNDO = DA_SCANCODE_TO_KEYCODE(sc_UNDO),
    kc_CUT = DA_SCANCODE_TO_KEYCODE(sc_CUT),
    kc_COPY = DA_SCANCODE_TO_KEYCODE(sc_COPY),
    kc_PASTE = DA_SCANCODE_TO_KEYCODE(sc_PASTE),
    kc_FIND = DA_SCANCODE_TO_KEYCODE(sc_FIND),
    kc_MUTE = DA_SCANCODE_TO_KEYCODE(sc_MUTE),
    kc_VOLUMEUP = DA_SCANCODE_TO_KEYCODE(sc_VOLUMEUP),
    kc_VOLUMEDOWN = DA_SCANCODE_TO_KEYCODE(sc_VOLUMEDOWN),
    kc_KP_COMMA = DA_SCANCODE_TO_KEYCODE(sc_KP_COMMA),
    kc_KP_EQUALSAS400 = DA_SCANCODE_TO_KEYCODE(sc_KP_EQUALSAS400),

    kc_ALTERASE = DA_SCANCODE_TO_KEYCODE(sc_ALTERASE),
    kc_SYSREQ = DA_SCANCODE_TO_KEYCODE(sc_SYSREQ),
    kc_CANCEL = DA_SCANCODE_TO_KEYCODE(sc_CANCEL),
    kc_CLEAR = DA_SCANCODE_TO_KEYCODE(sc_CLEAR),
    kc_PRIOR = DA_SCANCODE_TO_KEYCODE(sc_PRIOR),
    kc_RETURN2 = DA_SCANCODE_TO_KEYCODE(sc_RETURN2),
    kc_SEPARATOR = DA_SCANCODE_TO_KEYCODE(sc_SEPARATOR),
    kc_OUT = DA_SCANCODE_TO_KEYCODE(sc_OUT),
    kc_OPER = DA_SCANCODE_TO_KEYCODE(sc_OPER),
    kc_CLEARAGAIN = DA_SCANCODE_TO_KEYCODE(sc_CLEARAGAIN),
    kc_CRSEL = DA_SCANCODE_TO_KEYCODE(sc_CRSEL),
    kc_EXSEL = DA_SCANCODE_TO_KEYCODE(sc_EXSEL),

    kc_KP_00 = DA_SCANCODE_TO_KEYCODE(sc_KP_00),
    kc_KP_000 = DA_SCANCODE_TO_KEYCODE(sc_KP_000),
    kc_THOUSANDSSEPARATOR = DA_SCANCODE_TO_KEYCODE(sc_THOUSANDSSEPARATOR),
    kc_DECIMALSEPARATOR = DA_SCANCODE_TO_KEYCODE(sc_DECIMALSEPARATOR),
    kc_CURRENCYUNIT = DA_SCANCODE_TO_KEYCODE(sc_CURRENCYUNIT),
    kc_CURRENCYSUBUNIT = DA_SCANCODE_TO_KEYCODE(sc_CURRENCYSUBUNIT),
    kc_KP_LEFTPAREN = DA_SCANCODE_TO_KEYCODE(sc_KP_LEFTPAREN),
    kc_KP_RIGHTPAREN = DA_SCANCODE_TO_KEYCODE(sc_KP_RIGHTPAREN),
    kc_KP_LEFTBRACE = DA_SCANCODE_TO_KEYCODE(sc_KP_LEFTBRACE),
    kc_KP_RIGHTBRACE = DA_SCANCODE_TO_KEYCODE(sc_KP_RIGHTBRACE),
    kc_KP_TAB = DA_SCANCODE_TO_KEYCODE(sc_KP_TAB),
    kc_KP_BACKSPACE = DA_SCANCODE_TO_KEYCODE(sc_KP_BACKSPACE),
    kc_KP_A = DA_SCANCODE_TO_KEYCODE(sc_KP_A),
    kc_KP_B = DA_SCANCODE_TO_KEYCODE(sc_KP_B),
    kc_KP_C = DA_SCANCODE_TO_KEYCODE(sc_KP_C),
    kc_KP_D = DA_SCANCODE_TO_KEYCODE(sc_KP_D),
    kc_KP_E = DA_SCANCODE_TO_KEYCODE(sc_KP_E),
    kc_KP_F = DA_SCANCODE_TO_KEYCODE(sc_KP_F),
    kc_KP_XOR = DA_SCANCODE_TO_KEYCODE(sc_KP_XOR),
    kc_KP_POWER = DA_SCANCODE_TO_KEYCODE(sc_KP_POWER),
    kc_KP_PERCENT = DA_SCANCODE_TO_KEYCODE(sc_KP_PERCENT),
    kc_KP_LESS = DA_SCANCODE_TO_KEYCODE(sc_KP_LESS),
    kc_KP_GREATER = DA_SCANCODE_TO_KEYCODE(sc_KP_GREATER),
    kc_KP_AMPERSAND = DA_SCANCODE_TO_KEYCODE(sc_KP_AMPERSAND),
    kc_KP_DBLAMPERSAND = DA_SCANCODE_TO_KEYCODE(sc_KP_DBLAMPERSAND),
    kc_KP_VERTICALBAR = DA_SCANCODE_TO_KEYCODE(sc_KP_VERTICALBAR),
    kc_KP_DBLVERTICALBAR = DA_SCANCODE_TO_KEYCODE(sc_KP_DBLVERTICALBAR),
    kc_KP_COLON = DA_SCANCODE_TO_KEYCODE(sc_KP_COLON),
    kc_KP_HASH = DA_SCANCODE_TO_KEYCODE(sc_KP_HASH),
    kc_KP_SPACE = DA_SCANCODE_TO_KEYCODE(sc_KP_SPACE),
    kc_KP_AT = DA_SCANCODE_TO_KEYCODE(sc_KP_AT),
    kc_KP_EXCLAM = DA_SCANCODE_TO_KEYCODE(sc_KP_EXCLAM),
    kc_KP_MEMSTORE = DA_SCANCODE_TO_KEYCODE(sc_KP_MEMSTORE),
    kc_KP_MEMRECALL = DA_SCANCODE_TO_KEYCODE(sc_KP_MEMRECALL),
    kc_KP_MEMCLEAR = DA_SCANCODE_TO_KEYCODE(sc_KP_MEMCLEAR),
    kc_KP_MEMADD = DA_SCANCODE_TO_KEYCODE(sc_KP_MEMADD),
    kc_KP_MEMSUBTRACT = DA_SCANCODE_TO_KEYCODE(sc_KP_MEMSUBTRACT),
    kc_KP_MEMMULTIPLY = DA_SCANCODE_TO_KEYCODE(sc_KP_MEMMULTIPLY),
    kc_KP_MEMDIVIDE = DA_SCANCODE_TO_KEYCODE(sc_KP_MEMDIVIDE),
    kc_KP_PLUSMINUS = DA_SCANCODE_TO_KEYCODE(sc_KP_PLUSMINUS),
    kc_KP_CLEAR = DA_SCANCODE_TO_KEYCODE(sc_KP_CLEAR),
    kc_KP_CLEARENTRY = DA_SCANCODE_TO_KEYCODE(sc_KP_CLEARENTRY),
    kc_KP_BINARY = DA_SCANCODE_TO_KEYCODE(sc_KP_BINARY),
    kc_KP_OCTAL = DA_SCANCODE_TO_KEYCODE(sc_KP_OCTAL),
    kc_KP_DECIMAL = DA_SCANCODE_TO_KEYCODE(sc_KP_DECIMAL),
    kc_KP_HEXADECIMAL = DA_SCANCODE_TO_KEYCODE(sc_KP_HEXADECIMAL),

    kc_LCTRL = DA_SCANCODE_TO_KEYCODE(sc_LCTRL),
    kc_LSHIFT = DA_SCANCODE_TO_KEYCODE(sc_LSHIFT),
    kc_LALT = DA_SCANCODE_TO_KEYCODE(sc_LALT),
    kc_LGUI = DA_SCANCODE_TO_KEYCODE(sc_LGUI),
    kc_RCTRL = DA_SCANCODE_TO_KEYCODE(sc_RCTRL),
    kc_RSHIFT = DA_SCANCODE_TO_KEYCODE(sc_RSHIFT),
    kc_RALT = DA_SCANCODE_TO_KEYCODE(sc_RALT),
    kc_RGUI = DA_SCANCODE_TO_KEYCODE(sc_RGUI),

    kc_MODE = DA_SCANCODE_TO_KEYCODE(sc_MODE),

    kc_AUDIONEXT = DA_SCANCODE_TO_KEYCODE(sc_AUDIONEXT),
    kc_AUDIOPREV = DA_SCANCODE_TO_KEYCODE(sc_AUDIOPREV),
    kc_AUDIOSTOP = DA_SCANCODE_TO_KEYCODE(sc_AUDIOSTOP),
    kc_AUDIOPLAY = DA_SCANCODE_TO_KEYCODE(sc_AUDIOPLAY),
    kc_AUDIOMUTE = DA_SCANCODE_TO_KEYCODE(sc_AUDIOMUTE),
    kc_MEDIASELECT = DA_SCANCODE_TO_KEYCODE(sc_MEDIASELECT),
    kc_WWW = DA_SCANCODE_TO_KEYCODE(sc_WWW),
    kc_MAIL = DA_SCANCODE_TO_KEYCODE(sc_MAIL),
    kc_CALCULATOR = DA_SCANCODE_TO_KEYCODE(sc_CALCULATOR),
    kc_COMPUTER = DA_SCANCODE_TO_KEYCODE(sc_COMPUTER),
    kc_AC_SEARCH = DA_SCANCODE_TO_KEYCODE(sc_AC_SEARCH),
    kc_AC_HOME = DA_SCANCODE_TO_KEYCODE(sc_AC_HOME),
    kc_AC_BACK = DA_SCANCODE_TO_KEYCODE(sc_AC_BACK),
    kc_AC_FORWARD = DA_SCANCODE_TO_KEYCODE(sc_AC_FORWARD),
    kc_AC_STOP = DA_SCANCODE_TO_KEYCODE(sc_AC_STOP),
    kc_AC_REFRESH = DA_SCANCODE_TO_KEYCODE(sc_AC_REFRESH),
    kc_AC_BOOKMARKS = DA_SCANCODE_TO_KEYCODE(sc_AC_BOOKMARKS),

    kc_BRIGHTNESSDOWN = DA_SCANCODE_TO_KEYCODE(sc_BRIGHTNESSDOWN),
    kc_BRIGHTNESSUP = DA_SCANCODE_TO_KEYCODE(sc_BRIGHTNESSUP),
    kc_DISPLAYSWITCH = DA_SCANCODE_TO_KEYCODE(sc_DISPLAYSWITCH),
    kc_KBDILLUMTOGGLE = DA_SCANCODE_TO_KEYCODE(sc_KBDILLUMTOGGLE),
    kc_KBDILLUMDOWN = DA_SCANCODE_TO_KEYCODE(sc_KBDILLUMDOWN),
    kc_KBDILLUMUP = DA_SCANCODE_TO_KEYCODE(sc_KBDILLUMUP),
    kc_EJECT = DA_SCANCODE_TO_KEYCODE(sc_EJECT),
    kc_SLEEP = DA_SCANCODE_TO_KEYCODE(sc_SLEEP),
    kc_APP1 = DA_SCANCODE_TO_KEYCODE(sc_APP1),
    kc_APP2 = DA_SCANCODE_TO_KEYCODE(sc_APP2),

    kc_AUDIOREWIND = DA_SCANCODE_TO_KEYCODE(sc_AUDIOREWIND),
    kc_AUDIOFASTFORWARD = DA_SCANCODE_TO_KEYCODE(sc_AUDIOFASTFORWARD)
};

#undef DA_SCANCODE_TO_KEYCODE

// NOTE: While these can be renamed, their values must still match
// DO NOT change / reorder

enum class controller_axis : int
{
    ca_left_x = 0,
    ca_left_y,
    ca_right_x,
    ca_right_y,
    ca_left_trigger,
    ca_right_trigger
};

enum class controller_button : int
{
    cb_A = 0,
    cb_B,
    cb_X,
    cb_Y,
    cb_back,
    cb_guide,
    cb_start,
    cb_left_stick,
    cb_right_stick,
    cb_left_shoulder,
    cb_right_shoulder,
    cb_dpad_up,
    cb_dpad_down,
    cb_dpad_left,
    cb_dpad_right
};

enum class mouse_button : int
{
    mb_left = 1,
    mb_middle,
    mb_right,
    mb_4,
    mb_5
};
}
