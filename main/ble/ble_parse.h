#ifndef __BLE_PARSE_H__
#define __BLE_PARSE_H__

#include <stddef.h>
#include <stdint.h>

#define MOTION_START_RESPONSE 0x10
#define MOTION_DATA           0x11


#define MOTION_START_REQUEST  0x20
#define MOTION_DATA_ACK       0x21

#define OTA_MODE_REQUEST      0x30
#define OTA_MODE_RESPONSE     0x31

typedef union {
    struct {
        uint16_t data : 14;  
        uint16_t type : 2;  
    } bit;                   
    uint16_t word;           
} pack_data;
#pragma pack(push, 1)
typedef struct {
  uint8_t event_code;
  union
  {
      struct
      {

        uint8_t interval;
        uint16_t total_points;
        uint16_t padding[8];
      } motion_req;
      struct
      {
        uint8_t seq;
        pack_data pack_data_0;
        pack_data pack_data_1;
        pack_data pack_data_2;
        pack_data pack_data_3;
        pack_data pack_data_4;
        pack_data pack_data_5;
        pack_data pack_data_6;
        pack_data pack_data_7;
        pack_data pack_data_8;
      } motion_data;
      struct
      {
        uint8_t req_type;
        uint16_t padding[9];
      } motion_res;
      struct
      {
        uint8_t ack_seq_no;
        uint16_t padding[9];
      } motion_ack;
      struct
      {
        uint8_t cmd_type;
        uint16_t padding[9];
      } ota_req;
      struct
      {
        uint8_t cmd_type;
        uint8_t status;
        uint8_t padding[17];
      } ota_res;

  };
} Motion_Packet_t;
#pragma pack(pop)

void BLE_Receive_data(uint8_t* data, uint16_t len);
#endif

