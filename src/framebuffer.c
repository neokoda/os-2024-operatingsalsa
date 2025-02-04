#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "header/text/framebuffer.h"
#include "header/stdlib/string.h"
#include "header/cpu/portio.h"

void framebuffer_set_cursor(uint8_t r, uint8_t c) {
	uint16_t pos = r * 80 + c;
 
	out(CURSOR_PORT_CMD, 0x0F);
	out(CURSOR_PORT_DATA, (uint8_t) (pos & 0xFF));
	out(CURSOR_PORT_CMD, 0x0E);
	out(CURSOR_PORT_DATA, (uint8_t) ((pos >> 8) & 0xFF));
}

void framebuffer_write(uint8_t row, uint8_t col, char c, uint8_t fg, uint8_t bg) {
    uint16_t attrib = (bg << 4) | (fg & 0x0F);
    uint16_t* fbmo = (uint16_t*) FRAMEBUFFER_MEMORY_OFFSET;
    fbmo[(row * 80 + col)] = c | (attrib << 8);
}

void framebuffer_clear(void) {
	for (int row = 0; row < 25; row++) {
		for (int col = 0; col < 80; col++) {
			framebuffer_write(row, col, '\0', 0xF, 0);
		}
	}
}