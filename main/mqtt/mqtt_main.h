#ifndef __MQTT_MAIN_H__
#define __MQTT_MAIN_H__

void mqtt_main(void);
void MQTT_Send(uint8_t ACK, uint8_t Direct, uint8_t* Seq, uint8_t CMD, uint8_t* data, uint32_t len);

void mqtt_ack_input(uint8_t Cmd, uint8_t Seq);

#endif


