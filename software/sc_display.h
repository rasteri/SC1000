#ifndef SC_DISPLAY_H
#define SC_DISPLAY_H

/* Start the OLED display thread. Safe to call even if no display is fitted -
 * if the SSD1306 does not ACK on i2c-0 the thread disables itself and exits. */
void SC_Display_Start(void);

#endif
