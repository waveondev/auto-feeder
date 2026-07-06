#ifndef __MQTT_PARSE_H__
#define __MQTT_PARSE_H__

typedef enum 
{
    CHARGE_WAKEUP = 0,
    CHARGE_ALIVE = 1,
    CHATGE_POWER_ON = 2,
    CHATGE_POWER_OFF = 3,
    CHATGE_SETTING_TIME_GET = 4,

    CHATGE_FACTO_SET = 0x7C,
    CHARGE_VERSION_GET = 0x7D,
    CHARGE_FW_UPDATE_SET = 0x7E,
    CHARGE_FW_UPDATE_STATE = 0x7F,
}Charge_Cmd_t;


typedef struct 
{
    uint32_t settingTime;
}Charge_On_t;



typedef enum 
{
    CHARGE_FW_SUCCESS = 0,
    CHARGE_FW_FAIL = 1,
    CHARGE_FW_ING = 2,
    CHARGE_FW_WAIT = 3,    
}FW_UPDATE_STATE_cmd_t;



typedef struct __attribute__((packed))
{
    uint8_t FW_VER_MAJOR;
    uint8_t FW_VER_MIDDLE;
    uint8_t FW_VER_MINOR;    
}Charge_Version_Get_t;


typedef struct
{
    uint8_t mac[6];
    uint8_t Cmd;    /* 7bit ACK 6~0bit Command*/ 
    uint8_t Seq;
    signed char rssi;
    uint8_t reserved[2];   
    uint8_t Charge_State; /*SERVER = 0*/
    uint32_t RemainTime;  /*SERVER = 0*/
    uint32_t Data_len;
}Mqtt_packet_header_t;

typedef struct
{
    Mqtt_packet_header_t Mqtt_Header;
    uint8_t Data[512];
}Mqtt_packet_t;
typedef struct 
{
    uint32_t count;
    Mqtt_packet_header_t Mqtt_Header;
    uint8_t data[1];
}Mqtt_Queue_t;

struct TxMqtt_queue {
  struct TxMqtt_queue *next;
  Mqtt_Queue_t Mqtt_Queue;
};

#define CHARGE_ACK (1<<7)





uint8_t Mqtt_Messege_input(uint8_t* data, uint32_t len);

#endif