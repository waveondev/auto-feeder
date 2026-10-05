#include "setting_cmd.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "app_config_flash.h"
#include "app_HX711.h"
#include "app_adc.h"
#include "debug_cli.h"
#include "app_led.h"

static const char *TAG = __FILE__;
#if 0
typedef enum
{
  HX711_Case_cal    = 0,
  HX711_Scale_cal   = 1,
  HX711_GetValue    = 2,
}Setting_cmd_e;
#endif

static Motion_Packet_t Motion_Packet_buf;
void Setting_callback(Motion_Packet_t* Motion_Packet)
{
    uint16_t liter_g;
    char hx_data[30];
    uint16_t* data_16;
    uint32_t* data_32;
    memcpy(&Motion_Packet_buf,Motion_Packet,sizeof(Motion_Packet_t));
    switch(Motion_Packet->setting_req.cmd_type)
    {
        case HX711_Case_cal:
            HX711_cal_init(1);
        break;
        case HX711_Scale_cal:
            liter_g = *(uint16_t*)Motion_Packet->setting_req.data;
            HX711_cal_init(liter_g);    
        break;
        case HX711_GetValue:
            Motion_Packet_buf.event_code = SETTING_RESPONSE;
            Motion_Packet_buf.setting_res.cmd_type = HX711_GetValue;

            data_32 = (uint32_t*)&Motion_Packet_buf.setting_res.data[0];
            *data_32 = (uint32_t)(loadcell_data_get() * 100.0f);
           // sprintf((char*)Motion_Packet_buf.setting_res.data,"%.2f", loadcell_data_get());         
            ble_send_data_to_queue(NULL,(uint8_t*)&Motion_Packet_buf,sizeof(Motion_Packet_t));        
        break;        
        case IR_GetValue:
            Motion_Packet_buf.event_code = SETTING_RESPONSE;
            Motion_Packet_buf.setting_res.cmd_type = IR_GetValue;
            data_16 = (uint16_t*)&Motion_Packet_buf.setting_res.data[0];
            *data_16++ = GetAcc_ADC();
            *data_16++ = GetBat_ADC();
            *data_16++ = GetSlid_ADC();
            *data_16++ = GetFeed_ADC();
            *data_16++ = GetIR_ADC();
            //sprintf((char*)Motion_Packet_buf.setting_res.data,"%04d %04d %04d",GetMotor_adc(), GetIR_LEFT(),GetIR_RIGHT());
            ble_send_data_to_queue(NULL,(uint8_t*)&Motion_Packet_buf,sizeof(Motion_Packet_t));
        break;            
        case LED_SetValue:
            Motion_Packet_buf.event_code = SETTING_RESPONSE;
            Motion_Packet_buf.setting_res.cmd_type = LED_SetValue;
            DBG_Resister_t *DBG_Resister = Debug_Get();
            DBG_Resister->led = 1;
            set_rgb_led_for_number(Motion_Packet_buf.setting_res.data[0]
                                    ,Motion_Packet_buf.setting_res.data[1]
                                    ,Motion_Packet_buf.setting_res.data[2]
                                    ,Motion_Packet_buf.setting_res.data[3]
                                    ,Motion_Packet_buf.setting_res.data[4]);
            ble_send_data_to_queue(NULL,(uint8_t*)&Motion_Packet_buf,sizeof(Motion_Packet_t));        
        break;              
    }
}


void Setting_Enable(Setting_cmd_e cmd)
{
    uint16_t liter_g;
    Motion_Packet_t Motion_Packet;
    char hx_data[30];

    memcpy(Motion_Packet.setting_res.data,Motion_Packet_buf.setting_req.data,sizeof(Motion_Packet.setting_res.data));
    Motion_Packet.event_code = SETTING_RESPONSE;
    Motion_Packet.setting_res.cmd_type = cmd;
 
    ble_send_data_to_queue(NULL,(uint8_t*)&Motion_Packet,sizeof(Motion_Packet_t));   

}



