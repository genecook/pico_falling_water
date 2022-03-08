#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pico/stdlib.h>

#define width 70
#define flipsPerLine 5
#define sleepTime 100

void screen_saver();

int main() { 
  stdio_init_all();
  screen_saver();
  return 0;
}
  
void screen_saver() {
  int i = 0, x = 0;

  int switches[width] = {0};

  char *ch = "1234567890qwertyuiopasdfghjklzxcvbnm,./';[]!@#$%^&*()-=_+";

  int l = strlen(ch);

  while(1) {
    for (i = 0; i < width; i += 2) {
       if (switches[i])
         printf("%c ",ch[rand() % l]);
       else
	 printf("  ");
    }
    for (i = 0; i != flipsPerLine; ++i) {
      x = rand() % width;
      switches[x] = !switches[x];
    }

    printf("\n");

    sleep_ms(sleepTime);
  }
}
