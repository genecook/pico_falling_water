#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pico/stdlib.h>
#include <pico/util/queue.h>
#include <pico/multicore.h>

#include "gui.h"

//#define width 70
#define width 22
#define flipsPerLine 5
#define sleepTime 100

#define NCOLS 22
#define NROWS 20

char display_buffer[NCOLS][NROWS];

void screen_saver();

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
  //    'scroll' the display, wait...
  //    process next queued up line of text...

  while(1) {
    // get next line of text...
    char tbuf[NCOLS + 1];
    queue_remove_blocking(&core1_cmd_queue, &tbuf);

    // scroll display buffer...
    for (int row = 1; row < NROWS; row++) {
      for (int col = 0; col < NCOLS; col++) {
	 display_buffer[col][row - 1] = display_buffer[col][row]; 
      }
    }
    
    // write new line of text to display buffer...
    for (int i = 0; i < NCOLS; i++) {
       display_buffer[i][NROWS - 1] = tbuf[i];
    }
    
    // show updated display buffer...
    print_line(tbuf);
    printf("\n");
  }
}

int main() { 
  stdio_init_all();
  gui_startup();
/*
  queue_init(&core1_cmd_queue, (sizeof(char) * NCOLS) + 1, 2);
  
  multicore_launch_core1(core1_entry);
  uint32_t g = multicore_fifo_pop_blocking();

  if (g == FLAG_VALUE) {
    // what we expected...
    multicore_fifo_push_blocking(FLAG_VALUE);
  } else {
    printf("ERROR, CORE 0 STARTUP???\n");
  }
*/
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
    char tbuf[NCOLS + 1];

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

    //queue_add_blocking(&core1_cmd_queue,&tbuf);

    printf("%s\n",tbuf);
    
    sleep_ms(sleepTime);
  }
}
