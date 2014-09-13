#ifndef CONSOLE_H
#define CONSOLE_H

void consoleinit(void);
void cprintf(const char*, ...);
void consoleintr(int (*)(void));
void panic(const char*) __attribute__((noreturn));

//void kbdintr(void);

#endif //CONSOLE_H
