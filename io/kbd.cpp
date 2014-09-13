/*
 * kbd.cpp
 *
 *  Created on: Aug 10, 2013
 *      Author: qinghua
 */

#include "types.h"
#include "x86.h"
#include "kbd.h"
#include "console.h"


uchar shiftcode[256] = {0};

uchar togglecode[256] = {0};

uchar normalmap[256] =
{
  NO,   0x1B, '1',  '2',  '3',  '4',  '5',  '6',  // 0x00
  '7',  '8',  '9',  '0',  '-',  '=',  '\b', '\t',
  'q',  'w',  'e',  'r',  't',  'y',  'u',  'i',  // 0x10
  'o',  'p',  '[',  ']',  '\n', NO,   'a',  's',
  'd',  'f',  'g',  'h',  'j',  'k',  'l',  ';',  // 0x20
  '\'', '`',  NO,   '\\', 'z',  'x',  'c',  'v',
  'b',  'n',  'm',  ',',  '.',  '/',  NO,   '*',  // 0x30
  NO,   ' ',  NO,   NO,   NO,   NO,   NO,   NO,
  NO,   NO,   NO,   NO,   NO,   NO,   NO,   '7',  // 0x40
  '8',  '9',  '-',  '4',  '5',  '6',  '+',  '1',
  '2',  '3',  '0',  '.',  NO,   NO,   NO,   NO,   // 0x50
};

uchar shiftmap[256] =
{
  NO,   033,  '!',  '@',  '#',  '$',  '%',  '^',  // 0x00
  '&',  '*',  '(',  ')',  '_',  '+',  '\b', '\t',
  'Q',  'W',  'E',  'R',  'T',  'Y',  'U',  'I',  // 0x10
  'O',  'P',  '{',  '}',  '\n', NO,   'A',  'S',
  'D',  'F',  'G',  'H',  'J',  'K',  'L',  ':',  // 0x20
  '"',  '~',  NO,   '|',  'Z',  'X',  'C',  'V',
  'B',  'N',  'M',  '<',  '>',  '?',  NO,   '*',  // 0x30
  NO,   ' ',  NO,   NO,   NO,   NO,   NO,   NO,
  NO,   NO,   NO,   NO,   NO,   NO,   NO,   '7',  // 0x40
  '8',  '9',  '-',  '4',  '5',  '6',  '+',  '1',
  '2',  '3',  '0',  '.',  NO,   NO,   NO,   NO,   // 0x50
};

uchar ctlmap[256] =
{
  NO,      NO,      NO,      NO,      NO,      NO,      NO,      NO,
  NO,      NO,      NO,      NO,      NO,      NO,      NO,      NO,
  C('Q'),  C('W'),  C('E'),  C('R'),  C('T'),  C('Y'),  C('U'),  C('I'),
  C('O'),  C('P'),  NO,      NO,      '\r',    NO,      C('A'),  C('S'),
  C('D'),  C('F'),  C('G'),  C('H'),  C('J'),  C('K'),  C('L'),  NO,
  NO,      NO,      NO,      C('\\'), C('Z'),  C('X'),  C('C'),  C('V'),
  C('B'),  C('N'),  C('M'),  NO,      NO,      C('/'),  NO,      NO,
};

void kbdinit() {

  shiftcode[0x1D] = CTL;
  shiftcode[0x2A] = SHIFT;
  shiftcode[0x36] = SHIFT;
  shiftcode[0x38] = ALT;
  shiftcode[0x9D] = CTL;
  shiftcode[0xB8] = ALT;

  togglecode[0x3A] = CAPSLOCK;
  togglecode[0x45] = NUMLOCK;
  togglecode[0x46] = SCROLLLOCK;

  normalmap[0x9C] = '\n'; // KP_Enter
  normalmap[0xB5] = '/'; // KP_Div
  normalmap[0xC8] = KEY_UP;
  normalmap[0xD0] = KEY_DN;
  normalmap[0xC9] = KEY_PGUP;
  normalmap[0xD1] = KEY_PGDN;
  normalmap[0xCB] = KEY_LF;
  normalmap[0xCD] = KEY_RT;
  normalmap[0x97] = KEY_HOME;
  normalmap[0xCF] = KEY_END;
  normalmap[0xD2] = KEY_INS;
  normalmap[0xD3] = KEY_DEL;

  shiftmap[0x9C] = '\n';
  shiftmap[0xB5] = '/';
  shiftmap[0xC8] = KEY_UP;
  shiftmap[0xD0] = KEY_DN;
  shiftmap[0xC9] = KEY_PGUP;
  shiftmap[0xD1] = KEY_PGDN;
  shiftmap[0xCB] = KEY_LF;
  shiftmap[0xCD] = KEY_RT;
  shiftmap[0x97] = KEY_HOME;
  shiftmap[0xCF] = KEY_END;
  shiftmap[0xD2] = KEY_INS;
  shiftmap[0xD3] = KEY_DEL;

  ctlmap[0x9C] = '\n';
  ctlmap[0xB5] = '/';
  ctlmap[0xC8] = KEY_UP;
  ctlmap[0xD0] = KEY_DN;
  ctlmap[0xC9] = KEY_PGUP;
  ctlmap[0xD1] = KEY_PGDN;
  ctlmap[0xCB] = KEY_LF;
  ctlmap[0xCD] = KEY_RT;
  ctlmap[0x97] = KEY_HOME;
  ctlmap[0xCF] = KEY_END;
  ctlmap[0xD2] = KEY_INS;
  ctlmap[0xD3] = KEY_DEL;
}

int kbdgetc(void) {
  static uint shift;
  uchar *charcode[4] = { normalmap, shiftmap, ctlmap, ctlmap };
  uint st, data, c;

  st = inb(KBSTATP);
  if ((st & KBS_DIB) == 0)
    return -1;
  data = inb(KBDATAP);

  if (data == 0xE0) {
    shift |= E0ESC;
    return 0;
  } else if (data & 0x80) {
    // Key released
    data = (shift & E0ESC ? data : data & 0x7F);
    shift &= ~(shiftcode[data] | E0ESC);
    return 0;
  } else if (shift & E0ESC) {
    // Last character was an E0 escape; or with 0x80
    data |= 0x80;
    shift &= ~E0ESC;
  }

  shift |= shiftcode[data];
  shift ^= togglecode[data];
  c = charcode[shift & (CTL | SHIFT)][data];
  if (shift & CAPSLOCK) {
    if ('a' <= c && c <= 'z')
      c += 'A' - 'a';
    else if ('A' <= c && c <= 'Z')
      c += 'a' - 'A';
  }
  return c;
}

void kbdintr(void) {
  consoleintr(kbdgetc);
}

