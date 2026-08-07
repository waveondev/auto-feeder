#ifndef __APP_FEED_MOTER_H__
#define __APP_FEED_MOTER_H__
#include <stdbool.h>


void Feeder_CW(void);
void Feeder_CCW(void);
void Feeder_break(void);
void Feeder_coast(void);

void init_feed_motor(void) ;

#endif

