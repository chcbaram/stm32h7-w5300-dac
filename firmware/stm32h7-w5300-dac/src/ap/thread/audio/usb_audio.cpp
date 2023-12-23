#include "usb_audio.h"


#define AUDIO_Q_BUF_LEN (16*1024)
#define FFT_LEN         2048
#define BLOCK_X_CNT     12
#define BLOCK_Y_CNT     12


typedef struct
{
  uint32_t pre_time_lcd;
  uint8_t  update_cnt;
  uint16_t q15_buf_index;
  q15_t    buf_q15[FFT_LEN*2];

  uint8_t block_target[BLOCK_X_CNT];
  uint8_t block_peak[BLOCK_X_CNT];
  uint8_t block_value[BLOCK_X_CNT];
} fft_t;

static void usbAudioCallback(int16_t *p_data, uint32_t samples);
static void usbAudioUpdateLcd(void);

static int16_t   audio_q_w_buf[AUDIO_Q_BUF_LEN];
static qbuffer_t audio_q;
static fft_t     audio_fft;
static uint16_t  freq_min  = 2;
static uint16_t  freq_max  = 16;




bool usbAudioInit(void)
{
  qbufferCreateBySize(&audio_q, (uint8_t *)audio_q_w_buf, 2, AUDIO_Q_BUF_LEN);
  
  Audio_SetReceiveFunc(usbAudioCallback);
  return true;
}

bool usbAudioUpdate(void)
{
  if (usbAudioIsBusy() == false)
  {
    return true;
  }

  usbAudioUpdateLcd();

  return true;
}

bool usbAudioIsBusy(void)
{
  return usbAudioIsConnect();
}

void usbAudioCallback(int16_t *p_data, uint32_t samples)
{
  if (usbAudioIsBusy())
  {
    qbufferWrite(&audio_q, (uint8_t *)p_data, samples);
  }
}



static void drawBlock(int16_t bx, int16_t by, uint16_t color)
{
  int16_t x;
  int16_t y;
  int16_t bw;
  int16_t bh;
  int16_t top_space = 80;
  int16_t bottom_space = 16;
  int16_t sw;
  int16_t sh;


  sw = lcdGetWidth();
  sh = lcdGetHeight()-top_space-bottom_space;

  bw = (sw / BLOCK_X_CNT);
  bh = (sh / BLOCK_Y_CNT);

  x = bx*bw;
  y = sh - bh*by - bh;

  lcdDrawFillRect(x, y+top_space, bw-2, bh-2, color);
}

static void drawFFT(fft_t *p_args)
{
  #if FFT_LEN == 4096
  arm_cfft_q15(&arm_cfft_sR_q15_len4096, p_args->buf_q15, 0, 1);
  #elif FFT_LEN == 2048
  arm_cfft_q15(&arm_cfft_sR_q15_len2048, p_args->buf_q15, 0, 1);
  #else
  #error "FFT LEN ERROR"
  #endif

  int16_t  xi;
  uint16_t fft_max_h = 200;
  uint16_t bar_max_h = 100;
  uint16_t fft_total_len = FFT_LEN/2;
  uint16_t fft_step;
  uint16_t fft_offset;
  uint16_t fft_len;

  fft_step = fft_total_len / (48/2);
  fft_offset = freq_min * fft_step;
  fft_len = (freq_max-freq_min) * fft_step;

  xi = fft_offset;
  for (int i=0; i<BLOCK_X_CNT; i++)
  {
    int32_t h;
    int32_t max_h;
    int32_t fft_block_len;


    max_h = 0;
    fft_block_len = fft_len/BLOCK_X_CNT;
    for (int j=0; j<fft_block_len; j++)
    {
      h = p_args->buf_q15[2*xi + 1];
      h = constrain(h, 0, fft_max_h);
      h = cmap(h, 0, fft_max_h, 0, bar_max_h);
      if (h > max_h)
      {
        max_h = h;
      }
      xi++;
    }
    h = cmap(max_h, 0, bar_max_h, 0, BLOCK_Y_CNT-1);

    p_args->block_target[i] = h;

    if (p_args->update_cnt%2 == 0)
    {
      if (p_args->block_peak[i] > 0)
      {
        p_args->block_peak[i]--;
      }
    }
    if (h >= p_args->block_peak[i])
    {
      p_args->block_peak[i] = h;
      p_args->block_value[i] = h;
    }
  }

  p_args->update_cnt++;

  for (int i=0; i<BLOCK_X_CNT; i++)
  {
    drawBlock(i, p_args->block_peak[i], red);

    if (p_args->block_value[i] > p_args->block_target[i])
    {
      p_args->block_value[i]--;
    }
    for (int j=0; j<p_args->block_value[i]; j++)
    {
      drawBlock(i, j, yellow);
    }
  }
}

void usbAudioUpdateLcd(void)
{
  uint32_t q_len;
  uint32_t length;



  q_len = qbufferAvailable(&audio_q);
  length = q_len;
  if (length > 0)
  {
    int16_t p_buf[2];

    for (uint32_t i=0; i<length; i+=2)
    {
      qbufferRead(&audio_q, (uint8_t *)p_buf, 2);

      if (audio_fft.q15_buf_index < FFT_LEN)
      {
        audio_fft.buf_q15[audio_fft.q15_buf_index*2 + 0] = p_buf[0];
        audio_fft.buf_q15[audio_fft.q15_buf_index*2 + 1] = 0;      
        audio_fft.q15_buf_index++;            
      }    
      else
      {
        break;
      }
    }
  }

  if (lcdDrawAvailable() && audio_fft.q15_buf_index == FFT_LEN && millis() - audio_fft.pre_time_lcd >= 40)
  {
    audio_fft.pre_time_lcd = millis();

    lcdClearBuffer(black);

    lcdDrawFillRect(0, 0, LCD_WIDTH, 32, white);     
    lcdPrintfRect  (0, 0, LCD_WIDTH, 32, black, 32, LCD_ALIGN_H_CENTER|LCD_ALIGN_V_CENTER, 
      "USB-DAC");

    drawFFT(&audio_fft);
    audio_fft.q15_buf_index = 0;


    uint32_t sample_rate;

    sample_rate = saiGetSampleRate();

    lcdPrintfRect(0, 40+ 0, LCD_WIDTH, 32, white, 32, LCD_ALIGN_H_CENTER|LCD_ALIGN_V_CENTER,
      "%dKHz 16Bit",sample_rate/1000);

    // lcdPrintfResize(0, 40+32, blue, 32, "%dKB %d%%", file_index/1024, percent_buf);
    
    // lcdDrawRect    (0, LCD_HEIGHT-16, LCD_WIDTH, 16, white);
    // lcdDrawFillRect(0, LCD_HEIGHT-16, LCD_WIDTH * percent / 100, 16, green);     

    lcdPrintf(0              , LCD_HEIGHT-16, white, "%d", freq_min);
    lcdPrintf(LCD_WIDTH/2 - 8, LCD_HEIGHT-16, white, "%d", (freq_max-freq_min)/2 + 1);
    lcdPrintf(LCD_WIDTH   -16, LCD_HEIGHT-16, white, "%d", freq_max);

    lcdRequestDraw();
  }
}
