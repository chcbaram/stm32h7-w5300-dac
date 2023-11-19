#ifndef USB_AUDIO_H_
#define USB_AUDIO_H_


#include "ap_def.h"


bool usbAudioInit(void);
bool usbAudioUpdate(void);
bool usbAudioIsBusy(void);

#endif