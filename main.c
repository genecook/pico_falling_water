#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pico/stdlib.h>
#include <pico/util/queue.h>
#include <pico/sem.h>
#include <pico/multicore.h>

#include "LCD_Driver.h"
#include "LCD_GUI.h"
#include "LCD_Bmp.h"

//***************************************************************************
// Program that writes random columns of text to an LCD display, crudely
// simulating the streams of text you see in the Matrix.
//
// Runs on Raspberry Pi Pico. Uses Waveshare 2.8 or 3.5 LCD/touch panel.
//***************************************************************************

//#define LCD_SIZE_2_8
#define LCD_SIZE_3_5

// using the vendor-supplied ascii character font, size 16 (16 high, 11 wide)...

#define FONT_16_HEIGHT 16
#define FONT_16_WIDTH  11

#define NROWS 20
#ifdef LCD_SIZE_3_5
// results in 20 rows of 44 characters per row...
#define NCOLS 44
#else
// results in 20 rows of 22 characters per row...
#define NCOLS 22
#endif

#define FLAG_VALUE 123

queue_t     core1_cmd_queue;  // used to write new lines of text to display buffer
queue_t     display_queue;    // used to cause individual display buffer columns
                              //   to be updated
semaphore_t display_char_sem; // used to insure only one core at a time writes
                              //   to LCD

void screen_saver();
void core1_entry();
void setup_display_updates(unsigned int *rseed);
void update_display();

//***************************************************************************
// main entry point...
//***************************************************************************

int main() { 
  System_Init();
  LCD_Init(SCAN_DIR_DFT,800);
  GUI_Show();
  GUI_Clear(BLACK);

  queue_init(&core1_cmd_queue, (sizeof(char) * NCOLS), 2);
  queue_init(&display_queue, sizeof(int), NCOLS);
  sem_init(&display_char_sem,1,1);
  
  multicore_launch_core1(core1_entry);
  uint32_t g = multicore_fifo_pop_blocking();

  if (g == FLAG_VALUE) {
    // what we expected...
    multicore_fifo_push_blocking(FLAG_VALUE);
  } else {
    printf("ERROR, CORE 0 STARTUP???\n");
    return 0;
  }
  
  screen_saver();
  return 0;
}

//***************************************************************************
// take the red pill...
//***************************************************************************

#define width NCOLS
#define flipsPerLine 5

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
       // notice that only every other character is non-blank
       // (we'll use this to cheat on display update later on)
       if (switches[i]) { 
         char xc[width];
	 sprintf(xc,"%c ", ch[rand_r(&rseed) % l]);
	 strcat(tbuf,xc); // note that one core can safely use the Pico C
	                  // runtime, but not both, ie, the SDK is not
	                  // thread safe, except where explicitely stated
       } else {
	 strcat(tbuf,"  ");
       }
    }

    for (i = 0; i != flipsPerLine; ++i) {
       x = rand() % width;
       switches[x] = !switches[x];
    }

    // prep for display updating...
    setup_display_updates(&rseed);
    
    // issue request to write next line of text to the display...
    queue_add_blocking(&core1_cmd_queue,&tbuf);

    // assist the other core with the display update...
    update_display();
  } 
}


void my_GUI_DisChar(POINT Xpoint, POINT Ypoint, const char Acsii_Char,
                    sFONT* Font, COLOR Color_Background, COLOR Color_Foreground);

// this is our text 'display buffer':

char display_buffer[NROWS][NCOLS];

// (LCD) screen coordinates for every character in the display buffer above:
// (used in early attempt to speed things up, but LCD speed, or lack thereof,
// renders this 'optimization' ineffective. sigh)

struct coors {
  int row;
  int col;
}
  display_buffer_coors[NROWS][NCOLS];

//***************************************************************************
// Austensibly, core 1 is responsible for maintaining/displaying a text
// screen buffer. Core 0 generates/queues up lines of text to be written
// to the display.
//
// 1st cut: Vendor supplied functions to write characters to the display
//          modified in an attempt to optimize (speed up) drawing time.
// 2nd cut: Added code to allow core 0 to assist in the rendering of text
//          lines to the display. The display is shared by both cores via
//          semaphore.
// 3rd cut: Let core 0 dictate the order in which columns of the display
//          are updated. Results in a more random display update.
//***************************************************************************

void core1_entry() {
  multicore_fifo_push_blocking(FLAG_VALUE);

  uint32_t g = multicore_fifo_pop_blocking();

  if (g == FLAG_VALUE) {
    // what we expected...
  } else {
    printf("ERROR, CORE 1 STARTUP???\n");
    return;
  }

  // calculate,record, display coordinates for each display buffer
  // character...
  for (int row = 0; row < NROWS; row++) {
     for (int col = 0; col < NCOLS; col++) {
	display_buffer[row][col] = ' ';
	display_buffer_coors[row][col].row = row * FONT_16_HEIGHT;
	display_buffer_coors[row][col].col = col * FONT_16_WIDTH;	
     }
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

    // scroll display buffer down one row...
    for (int row = NROWS - 1; row > 0; row--) {
      for (int col = 0; col < NCOLS; col++) {
	 display_buffer[row][col] = display_buffer[row - 1][col]; 
      }
    }
    
    // insert new line of text in display buffer, row zero...
    for (int i = 0; i < NCOLS; i++) {
       display_buffer[0][i] = tbuf[i];
    }

    // show updated display buffer...
    
    sFONT* TP_Font = &Font16;

    update_display();
  }
}

//***************************************************************************
// Setup for updating the display by queueing up the indices for display
// columns to be updated, in random order...
//***************************************************************************

void setup_display_updates(unsigned int *rseed) {
    int col_array[NCOLS]; // array of display buffer columns to be updated
    int n = 0;            // # of display buffer columns to update
    for (int col = 0; col < NCOLS; col += 2) { // only update columns
      col_array[n++] = col;                    // with non-blank characters
    }
    // random shuffle of display buffer 'update' columns...
    for (int i = 0; i < n - 1; i++) {
       int j = i + rand_r(rseed) / (RAND_MAX / (n - i) + 1);
       int t = col_array[j];
       col_array[j] = col_array[i];
       col_array[i] = t;
    }
    // queue up 'request' (column index) to write each column...
    for (int col = 0; col < n; col += 2) {
       queue_add_blocking(&display_queue,&col_array[col]);
    }
}

//***************************************************************************
// Retreive queued up display column indices, update display columns,
// until there are no more to update. Both cores may use this method...
//***************************************************************************

void update_display() {    
  while( !queue_is_empty(&display_queue) ) {
    int col;
    queue_remove_blocking(&display_queue, &col);
    sFONT* TP_Font = &Font16;

    for (int row = 0; row < NROWS; row++) {
       my_GUI_DisChar(display_buffer_coors[row][col].col,
		      display_buffer_coors[row][col].row,
		      display_buffer[row][col],
		      TP_Font,
		      BLACK,
		      GREEN
		     );
    }
  }
}

//***************************************************************************
// Write a single character to the (LCD) display.
//
// Adapted from vendor supplied GUI_DisChar routine...
//
// Uses semaphore to allow both cores to use this function to update
// the display.
//***************************************************************************

void my_GUI_DisChar(POINT Xstart, POINT Ystart, const char Acsii_Char,
                    sFONT* Font, COLOR Color_Background, COLOR Color_Foreground) {
  
  uint32_t Char_Offset = (Acsii_Char - ' ') * Font->Height * (Font->Width / 8 + (Font->Width % 8 ? 1 : 0));
  
  const unsigned char *ptr = &Font->table[Char_Offset];

  sem_acquire_blocking(&display_char_sem); // only one core at a time should access the LCD 

  LCD_SetWindow(Xstart, Ystart, Xstart + Font->Width, Ystart + Font->Height);

  DEV_Digital_Write(LCD_DC_PIN,1);
  DEV_Digital_Write(LCD_CS_PIN,0);

  for (POINT Page = 0; Page < Font->Height; Page++) {         // for Height pixel rows..
     for (POINT Column = 0; Column < Font->Width; Column++) { //    of Width pixels per row...
        COLOR Color_to_use = (*ptr & (0x80 >> (Column % 8))) ? Color_Foreground : Color_Background;
        SPI4W_Write_Byte(Color_to_use >> 8);
        SPI4W_Write_Byte(Color_to_use & 0xff);
        if (Column % 8 == 7) ptr++;  // eight pixels per byte
     }
     if (Font->Width % 8 != 0) ptr++; // font 'pixel rows' are byte aligned
  }

  DEV_Digital_Write(LCD_CS_PIN,1);
  
  sem_release(&display_char_sem); // 'this' core done with LCD access
}

