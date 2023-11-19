#include "ap.h"
#include "wiznet/wiznet.h"
#include "cmd/cmd_thread.h"
#include "osd/osd_thread.h"
#include "audio/usb_audio.h"


static void lcdUpdate(void);




void apInit(void)
{
  cliOpen(HW_UART_CH_CLI, 115200);
  logBoot(false);

  cmdThreadInit();
  osdThreadInit();
  usbAudioInit();
}

void apMain(void)
{
  uint32_t pre_time;
  uint8_t rot_i = 0;
  uint8_t rot_mode_tbl[2] = {4, 3};
  uint8_t cli_ch;


  pre_time = millis();
  while(1)
  {
    if (millis()-pre_time >= 500)
    {
      pre_time = millis();
      ledToggle(_DEF_LED1);
      ledToggle(_DEF_LED2);
    }    

    if (buttonGetPressed(_DEF_BUTTON3))
    {
      rot_i = (rot_i + 1) % 2;
      lcdSetRotation(rot_mode_tbl[rot_i]);
      lcdLogoOn();
      delay(50);
      while(buttonGetPressed(_DEF_BUTTON3));
    }
    

    if (usbIsOpen() && usbGetType() == USB_CON_CLI)
    {
      cli_ch = HW_UART_CH_USB;
    }
    else
    {
      cli_ch = HW_UART_CH_SWD;
    }
    if (cli_ch != cliGetPort())
    {
      cliOpen(cli_ch, 0);
    }


    cliMain();

    sdUpdate();
    cmdThreadUpdate();
    osdThreadUpdate();
    usbAudioUpdate();
    lcdUpdate();
  }
}

#include <time.h>

void lcdUpdate(void)
{
  static uint32_t pre_time = 0;
  bool is_busy = false;


  is_busy |= cmdAudioIsBusy();
  is_busy |= cmdBootIsBusy();
  is_busy |= usbAudioIsBusy();

  if (is_busy)
  {
    return;
  }

  if (lcdDrawAvailable() && millis()-pre_time >= 100)
  {
    pre_time = millis();

    lcdClearBuffer(black);  

    lcdLogoDraw(-1, -1);

    lcdRequestDraw();
  }
}