pico_falling_water
==================
Program that writes random columns of text to an LCD display, crudely
simulating the streams of text you see in the Matrix.

Runs on Raspberry Pi Pico. Uses Waveshare 2.8 LCD/touch panel.

Austensibly, core 1 is responsible for maintaining/displaying a text
screen buffer. Core 0 generates/queues up lines of text to be written
to the display.

 1st cut: Vendor supplied functions to write characters to the display
          modified in an attempt to optimize (speed up) drawing time.
	  
 2nd cut: Added code to allow core 0 to assist in the rendering of text
          lines to the display. The display is shared by both cores via
          semaphore.
	  
 3rd cut: Let core 0 dictate the order in which columns of the display
          are updated. Results in a more random display update.
	  
 4th cut: Implement optimized version of my_GUI_DisChar function. No
          discernable speed up, but does confirm that bit (pixel) streams
	  to programmatically defined windows are possible.
	  
