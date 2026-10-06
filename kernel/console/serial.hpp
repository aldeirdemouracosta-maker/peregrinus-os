#pragma once
namespace peregrinus::serial {
// Returns false (and serial output is skipped) when no UART answers at COM1.
bool init();
bool present();
void putc(char c);
void write(const char* s);
void writeln(const char* s);
// Optional second sink (the framebuffer text console). Every byte written to the serial port
// is also passed to it, so the screen shows exactly the serial log.
void set_mirror(void (*mirror)(char));
// Raw bytes to COM1 only: no '\n' translation and no screen mirror (protocol frames).
void write_raw(const char* s, unsigned long n);
// Non-blocking read of one received byte (COM1). Lets the shell be driven over the serial
// line as well as from the keyboard.
bool poll_input(char& c);
}
