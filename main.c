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

#define width 22
#define flipsPerLine 5
#define sleepTime 50

#define NCOLS 22
#define NROWS 20
#define FONT_16_WIDTH 11
#define FONT_16_HEIGHT 16

void screen_saver();

void erase_row(POINT Xstart, POINT Ystart, sFONT* Font, int Ncount);
void erase_col(POINT Xstart, POINT Ystart, sFONT* Font, int Ncount);
void my_GUI_DisChar(POINT Xpoint, POINT Ypoint, const char Acsii_Char,
                    sFONT* Font, COLOR Color_Background, COLOR Color_Foreground);

char display_buffer[NROWS][NCOLS];

struct coors {
  int row;
  int col;
}
  display_buffer_coors[NROWS][NCOLS];


#define FLAG_VALUE 123

queue_t     core1_cmd_queue;
queue_t     display_queue;
semaphore_t display_char_sem;

//***************************************************************************
// Austensibly, core 1 is responsible for maintaining/displaying a text
// screen buffer. Core 0 generates/queues up lines of text to be written
// to the display.
// 1st cut: Vendor supplied functions to write characters to the display
//          modified in an attempt to optimize (speed up) drawing time.
// 2nd cut: (TBD) Add code to allow core 0 to assist in the rendering of text
//          lines to the display. The display is shared by both cores via
//          semaphore.
//***************************************************************************

void update_display();

void core1_entry() {
  multicore_fifo_push_blocking(FLAG_VALUE);

  uint32_t g = multicore_fifo_pop_blocking();

  if (g == FLAG_VALUE) {
    // what we expected...
  } else {
    printf("ERROR, CORE 1 STARTUP???\n");
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

    // queue up 'request' (column index) to write each column
    // (fudging here; actually every other column)
    
    //for (int col = 0; col < NCOLS; col += 2) {
    //   queue_add_blocking(&display_queue,&col);
    //}

    update_display();
  }
}

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
  }
  
  screen_saver();
  return 0;
}

//***************************************************************************
// take the red pill...
//***************************************************************************

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
       // notice that only every other character is non-blank...
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

    // setup display update requests...
    
    int col_array[NCOLS]; // array of display buffer columns to be updated
    int n = 0;            // # of display buffer columns to update
    for (int col = 0; col < NCOLS; col += 2) { // only update columns
      col_array[n++] = col;                    // with non-blank characters
    }
    // random shuffle of display buffer 'update' columns...
    for (int i = 0; i < n - 1; i++) {
       int j = i + rand_r(&rseed) / (RAND_MAX / (n - i) + 1);
       int t = col_array[j];
       col_array[j] = col_array[i];
       col_array[i] = t;
    }
    // queue up 'request' (column index) to write each column...
    for (int col = 0; col < n; col += 2) {
       queue_add_blocking(&display_queue,&col_array[col]);
    }

    // then issue request to write next line of text to the
    // display...
    
    queue_add_blocking(&core1_cmd_queue,&tbuf);

    // assist the other core with the display update...
    update_display();
  }
  
}

extern LCD_DIS sLCD_DIS;

//***************************************************************************
// instead of erasing individual characters from the display, blank out
// entire rows...
//***************************************************************************

void erase_row(POINT Xstart, POINT Ystart, sFONT* Font, int Ncount) {
  POINT Xend = Xstart + (Font->Width * Ncount);
  POINT Yend = Ystart + Font->Height;
  sem_acquire_blocking(&display_char_sem);
  LCD_SetArealColor( Xstart, Ystart, Xend, Yend, BLACK);
  sem_release(&display_char_sem);
}

void erase_col(POINT Xstart, POINT Ystart, sFONT* Font, int Ncount) {
  POINT Xend = Xstart + Font->Width;
  POINT Yend = Ystart + (Font->Height * Ncount);
  sem_acquire_blocking(&display_char_sem);
  LCD_SetArealColor( Xstart, Ystart, Xend, Yend, BLACK);
  sem_release(&display_char_sem);
}

//***************************************************************************
// adapted from vendor supplied GUI_DisChar routine...
//***************************************************************************

void my_GUI_DisChar(POINT Xpoint, POINT Ypoint, const char Acsii_Char,
                    sFONT* Font, COLOR Color_Background, COLOR Color_Foreground) {

  erase_col(Xpoint,Ypoint,Font,1); // effectively, erase entire character
    
  POINT Page, Column;

  uint32_t Char_Offset = (Acsii_Char - ' ') * Font->Height * (Font->Width / 8 + (Font->Width % 8 ? 1 : 0));
  const unsigned char *ptr = &Font->table[Char_Offset];

  for (Page = 0; Page < Font->Height; Page ++ ) {
     for (Column = 0; Column < Font->Width; Column ++ ) {
	if (*ptr & (0x80 >> (Column % 8))) {
          sem_acquire_blocking(&display_char_sem);
          LCD_SetPointlColor(Xpoint + Column, Ypoint + Page, Color_Foreground);	
          sem_release(&display_char_sem);
        }
        //One pixel is 8 bits
        if (Column % 8 == 7)
          ptr++;
     }/* Write a line */
     if (Font->Width % 8 != 0)
       ptr++;
  }
}

