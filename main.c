#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pico/stdlib.h>
#include <pico/util/queue.h>
#include <pico/multicore.h>

#include "gui.h"

//#define USE_MULTICORE 1
//#define USE_LCD 1

//#define width 70
#define width 22
#define flipsPerLine 5
#define sleepTime 100

#define NCOLS 22
#define NROWS 20

void screen_saver();

#ifdef USE_MULTICORE
char display_buffer[NROWS][NCOLS];

#define FLAG_VALUE 123

queue_t core1_cmd_queue;

void core1_entry() {
  multicore_fifo_push_blocking(FLAG_VALUE);

  uint32_t g = multicore_fifo_pop_blocking();

  if (g == FLAG_VALUE) {
    // what we expected...
  } else {
    printf("ERROR, CORE 1 STARTUP???\n");
  }

  // forever:
  //   1. retreive next queued up line of text.
  //   2. scroll the display buffer
  //   3. write line of text to 'display buffer
  //   4. update lcd from display buffer

  while(1) {
    
    // get next line of text...
    char tbuf[NCOLS];
    queue_remove_blocking(&core1_cmd_queue, &tbuf);

    // scroll display buffer...
    for (int row = 1; row < NROWS; row++) {
      for (int col = 0; col < NCOLS; col++) {
	 display_buffer[row - 1][col] = display_buffer[row][col]; 
      }
    }
    
    // write new line of text to display buffer...
    for (int i = 0; i < NCOLS; i++) {
       display_buffer[NROWS - 1][i] = tbuf[i];
    }
    
    // show updated display buffer...
#ifdef USE_LCD
    for (int row = 1; row < NROWS; row++) {
       display_string(0, row, display_buffer[row]);
    }
    
    // need to update entire lcd screen...
#else
    // Neither core should use minimal or no stdlib functions, or any
    // other sdk function that is not explicitely marked as
    // thread-safe...
    for (int i = 0; i < NCOLS; i++) {
       putchar(display_buffer[NROWS - 1][i]);
    }
    putchar('\n');
#endif
    
    sleep_ms(sleepTime); // pause after scrolling
  }

}
#endif

int main() { 
  stdio_init_all();

#ifdef USE_LCD
  lcd_touch_startup();
#endif

#ifdef USE_MULTICORE
  queue_init(&core1_cmd_queue, (sizeof(char) * NCOLS), 2);
  
  multicore_launch_core1(core1_entry);
  uint32_t g = multicore_fifo_pop_blocking();

  if (g == FLAG_VALUE) {
    // what we expected...
    multicore_fifo_push_blocking(FLAG_VALUE);
  } else {
    printf("ERROR, CORE 0 STARTUP???\n");
  }
#endif
  
  screen_saver();
  return 0;
}


void screen_saver() {
  int i = 0, x = 0;

  int switches[width] = {0};
  
  char *ch = "1234567890qwertyuiopasdfghjklzxcvbnm,./';[]!@#$%^&*()-=_+";

  int l = strlen(ch);

  unsigned int rseed = 0;
  
  while(1) {
    char tbuf[NCOLS];

    tbuf[0] = '\0';
    
    for (i = 0; i < width; i += 2) {
       if (switches[i]) { 
         char xc[width];
	 sprintf(xc,"%c ", ch[rand_r(&rseed) % l]);
	 strcat(tbuf,xc);
       } else {
	 strcat(tbuf,"  ");
       }
    }

    for (i = 0; i != flipsPerLine; ++i) {
      x = rand() % width;
      switches[x] = !switches[x];
    }

#ifdef USE_MULTICORE
    queue_add_blocking(&core1_cmd_queue,&tbuf);
#else
    printf("%s\n",tbuf);
    sleep_ms(sleepTime);
#endif
  }
}
