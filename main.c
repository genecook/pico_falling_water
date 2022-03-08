#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pico/stdlib.h>

#include "gui.h"

#define width 70
#define flipsPerLine 5
#define sleepTime 100

void screen_saver();

int main() { 
  stdio_init_all();
  gui_startup();
  screen_saver();
  return 0;
}
  
void screen_saver() {
  int i = 0, x = 0;

  int switches[width] = {0};

  char *ch = "1234567890qwertyuiopasdfghjklzxcvbnm,./';[]!@#$%^&*()-=_+";

  int l = strlen(ch);

  while(1) {
    char tbuf[width];
    for (i = 0; i < width; i += 2) {
       if (switches[i]) { 
	 sprintf(tbuf,"%c ",ch[rand() % l]);
	 print_line(tbuf);
       } else {
	 print_line("  ");
      }
    }
    for (i = 0; i != flipsPerLine; ++i) {
      x = rand() % width;
      switches[x] = !switches[x];
    }

    print_line("\n");

    sleep_ms(sleepTime);
  }
}
