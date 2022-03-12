#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pico/stdlib.h>
#include <pico/util/queue.h>
#include <pico/multicore.h>

#include "LCD_Driver.h"
#include "LCD_Touch.h"
#include "LCD_GUI.h"
#include "LCD_Bmp.h"

//#define USE_MULTICORE 1
//#define USE_LCD 1

//#define width 70
#define width 22
#define flipsPerLine 5
#define sleepTime 50

#define NCOLS 22
#define NROWS 20
#define FONT_16_WIDTH 11
#define FONT_16_HEIGHT 16

void screen_saver();

#ifdef USE_LCD
void erase_line(POINT Xstart, POINT Ystart, sFONT* Font, int Ncount);
void my_GUI_DisChar(POINT Xpoint, POINT Ypoint, const char Acsii_Char,
                    sFONT* Font, COLOR Color_Background, COLOR Color_Foreground);
#endif

#ifdef USE_MULTICORE
char display_buffer[NROWS][NCOLS];
int starting_row = NROWS - 1; // use to speed up initial display

struct coors {
  int row;
  int col;
}
  display_buffer_coors[NROWS][NCOLS];


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
    sFONT* TP_Font = &Font16;
    for (int row = NROWS - 1; row >= starting_row; row--) {
       erase_line(display_buffer_coors[row][0].col,
		  display_buffer_coors[row][0].row,
		  TP_Font,
		  NCOLS);
       for (int col = 0; (col < NCOLS) && (display_buffer[row][col] != '\0'); col++) {
	  if (display_buffer[row][col] == ' ') {
            // skip blanks
	    continue;
	  }
	  my_GUI_DisChar(display_buffer_coors[row][col].col,
		         display_buffer_coors[row][col].row,
		         display_buffer[row][col],
		         TP_Font,
		         BLACK,
		         GREEN
		        );
       }
    }

    // since display is initially blank, only need to (re)draw
    // the last N rows, 'til the display buffer is full...
    starting_row--;
    if (starting_row < 0)
      starting_row = 0;

    // no need to pause as its slow to write character (bit maps)
    //   to lcd...
#else
    // Both cores should use minimal or no stdlib functions, or any
    // other sdk function that is not explicitely marked as
    // thread-safe...
    for (int i = 0; i < NCOLS; i++) {
       putchar(display_buffer[NROWS - 1][i]);
    }
    putchar('\n');
    sleep_ms(sleepTime); // pause after scrolling
#endif    
  }

}
#endif

#ifdef USE_LCD
void InitTouchPanel( LCD_SCAN_DIR Lcd_ScanDir );
#endif

int main() { 
  stdio_init_all();

#ifdef USE_LCD
  System_Init();
  LCD_Init(SCAN_DIR_DFT,800);
  InitTouchPanel(SCAN_DIR_DFT);
  GUI_Show();
  GUI_Clear(BLACK);
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

#ifdef USE_LCD

extern LCD_DIS sLCD_DIS;

void erase_line(POINT Xstart, POINT Ystart, sFONT* Font, int Ncount) {
  POINT Xend = Xstart + (Font->Width * Ncount);
  POINT Yend = Ystart + Font->Height;
  LCD_SetArealColor( Xstart, Ystart, Xend, Yend, BLACK);
}

// adapted from vendor supplied GUI_DisChar routine...

void my_GUI_DisChar(POINT Xpoint, POINT Ypoint, const char Acsii_Char,
                    sFONT* Font, COLOR Color_Background, COLOR Color_Foreground) {

    POINT Page, Column;

    uint32_t Char_Offset = (Acsii_Char - ' ') * Font->Height * (Font->Width / 8 + (Font->Width % 8 ? 1 : 0));
    const unsigned char *ptr = &Font->table[Char_Offset];

    for (Page = 0; Page < Font->Height; Page ++ ) {
       for (Column = 0; Column < Font->Width; Column ++ ) {
	  if (*ptr & (0x80 >> (Column % 8))) {
            LCD_SetPointlColor(Xpoint + Column, Ypoint + Page, Color_Foreground);
	  }
          //One pixel is 8 bits
          if (Column % 8 == 7)
            ptr++;
       }/* Write a line */
       if (Font->Width % 8 != 0)
         ptr++;
   }
}
#endif

