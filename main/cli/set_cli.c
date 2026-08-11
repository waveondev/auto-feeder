#include "set_cli.h"
#include "app_slid_motor.h"
#include "app_led.h"
#include "wifi_task.h"
#include "app_HX711.h"
#include "opmode_task.h"
#include "app_config_flash.h"
#include "ble_parse.h"
#include "ble_task.h"
#include "gpio_util.h"
#include "isd2360.h"
#include "app_feed_motor.h"
BaseType_t prvSetInformationCommand( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString )
{
    const char *pcParameter;
    BaseType_t xParameterStringLength, xReturn;
    static BaseType_t lParameterNumber = 0;
    char ag[6][40];
    char buf[64];
    int  offset=0;
    int j;
    int	val32 = 0;
	app_wifi_config_t* wifi_config = get_wifi_config();
	app_config_t* app_config = get_app_config();

	/* Remove compile time warnings about unused parameters, and check the
	write buffer is not NULL.  NOTE - for simplicity, this example assumes the
	write buffer length is adequate, so does not check for buffer overflows. */
	( void ) pcCommandString;
	( void ) xWriteBufferLen;
	configASSERT( pcWriteBuffer );
	
	if( lParameterNumber == 0 )
	{
		/* The first time the function is called after the command has been
		entered just a header string is returned. */
//		sprintf( pcWriteBuffer, "show/get/set parameters were:\r\n" );
		memset( &ag[0][0],0,sizeof(ag));
		memset( pcWriteBuffer, 0x00, xWriteBufferLen );
		/* Next time the function is called the first parameter will be echoed
		back. */
		lParameterNumber = 1L;

		/* There is more data to be returned as no parameters have been echoed
		back yet. */
		xReturn = pdPASS;
	}
	else
	{
		/* Obtain the parameter string. */
		pcParameter = FreeRTOS_CLIGetParameter
						(
							pcCommandString,		/* The command string itself. */
							lParameterNumber,		/* Return the next parameter. */
							&xParameterStringLength	/* Store the parameter string length. */
						);

		/* Sanity check something was returned. */
//		configASSERT( pcParameter );

		if (pcParameter != NULL)
		{
			memset( pcWriteBuffer, 0x00, xWriteBufferLen );
//			sprintf (pcWriteBuffer, "+=%d+=%s=+\r\n", lParameterNumber, pcParameter);
			/* Return the parameter string. */
			strcpy (ag[lParameterNumber], pcParameter);
			/* If this is the last of the three parameters then there are no more
			strings to return after this one. */
			ag[lParameterNumber][xParameterStringLength] = '\0';
			xReturn = pdTRUE;
			lParameterNumber++;
		}
		else
		{
			memset( pcWriteBuffer, 0x00, xWriteBufferLen );
			if (!strncmp(ag[1], "help", 4))
			{
				offset += sprintf (&pcWriteBuffer[offset], "\r\nset \r\n");
			}
			else if (!strncmp(ag[1], "ledoff", 6))
			{
                set_rgb_len_no_Breathing(0,0,0,0);
            }		
			else if (!strncmp(ag[1], "ledr", 4))
			{
                val32 = atoi(ag[2]);
                set_rgb_len_no_Breathing(val32,0,0,0);
            }	
			else if (!strncmp(ag[1], "ledg", 4))
			{
                val32 = atoi(ag[2]);
				set_rgb_len_no_Breathing(0,val32,0,0);
            }			
			else if (!strncmp(ag[1], "ledb", 4))
			{
                val32 = atoi(ag[2]);
				set_rgb_len_no_Breathing(0,0,val32,0);
            }			
			else if (!strncmp(ag[1], "ledw", 4))
			{
                val32 = atoi(ag[2]);
				set_rgb_len_no_Breathing(0,0,0,val32);
            }	
			else if (!strncmp(ag[1], "led_b", 5))
			{
				uint8_t min = atoi(ag[2]);
				uint8_t max = atoi(ag[3]);
				uint8_t value = (uint8_t)strtol(ag[4], NULL, 16); 

				uint8_t r = (value & 0x08)? 255 : 0;
				uint8_t g = (value & 0x04)? 255 : 0;
				uint8_t b = (value & 0x02)? 255 : 0;
				uint8_t w = (value & 0x01)? 255 : 0;

				Breathing_Setup_Debug(1,2,min,max, r,g,b,w);
            }
			else if (!strncmp(ag[1], "scan", 4))
			{
                wifi_scan_start();
            }	
			else if (!strncmp(ag[1], "cal", 3))
			{
                HX711_cal_init(atoi(ag[2]));
            }		
			else if (!strncmp(ag[1], "testmode", 8))
			{
                Opmode_test_mode();
            }		
			else if (!strncmp(ag[1], "discon", 6))
			{

				Wifi_Disconnect();
			}
			else if (!strncmp(ag[1], "facto", 5))
			{
				reset_all_nvs_data();
			}
			else if (!strncmp(ag[1], "bleota", 6))
			{
				Tracker_All_Send(OTA_MODE_REQUEST,1);
			}
			else if (!strncmp(ag[1], "motion", 6))
			{
				Tracker_All_Send(MOTION_START_REQUEST,1);
			}	
			else if (!strncmp(ag[1], "health", 6))
			{
				Tracker_All_Send(HEALTH_DATA_REQUEST,1);
			}	
			else if (!strncmp(ag[1], "rm", 2))
			{
				if (!strncmp(ag[2], "wifi", 4))
				{
					memset(wifi_config, 0,sizeof(app_wifi_config_t));
					wifi_nvs_save_set();
				}	
				if (!strncmp(ag[2], "app", 3))
				{
					float hx1_scale_buf;
					int32_t hx1_offset_buf;
					uint32_t case_raw_data_buf;
					hx1_scale_buf = app_config->hx1_scale;
					hx1_offset_buf = app_config->hx1_offset;
					case_raw_data_buf = app_config->case_raw_data;
					memset(app_config, 0,sizeof(app_config_t));
					app_config->op_mode = FEED_MODE_SCHEDULED_PORTION;
					app_config->sliding_close_mode = 0;
					app_config->motor_current_limit = 1200;
					app_config->sound_effect_idx = 0;
					app_config->food_low_limit = 10;
					app_config->gate_way_rssi_th = -80;
					app_config->hx1_scale = 1000.0f;
					app_config->hx1_offset = 0;
					app_config->case_raw_data = 0;
					app_config->tof_sense_threshold_l = 250;
					app_config->motion_data_time = 1800;
					app_config->dispense_duration = 240;
					app_config->dispense_amount_g = 50;
					sprintf(app_config->env_mode,"dev");
					if(atoi(ag[3]))
					{
						app_config->hx1_scale = hx1_scale_buf;
						app_config->hx1_offset = hx1_offset_buf;
						app_config->case_raw_data = case_raw_data_buf;
					}

					app_nvs_save_set();
				}										
			}	
			else if (!strncmp(ag[1], "acuum", 5))
			{
				if(atoi(ag[2]))
					 gpio_set_level(ACUUM_PWM_IN, 1);
				else
				 	 gpio_set_level(ACUUM_PWM_IN, 0);
			}	
			else if (!strncmp(ag[1], "wificon", 7))
			{
				app_wifi_config_t* wifi_config = get_wifi_config();
				Wifi_Connect((char*)wifi_config->conn_ssid,(const char*)wifi_config->conn_password);
			}	
			else if (!strncmp(ag[1], "music", 5))
			{
				isd2360_set(atoi(ag[2]));
			}	
			else if (!strncmp(ag[1], "feed", 4))
			{
				feeder_mode_init();
			}		
			else if (!strncmp(ag[1], "onoff", 5))
			{
				gpio_set_level(IR_ONOFF0, atoi(ag[2])?1:0);
			}		
			else if (!strncmp(ag[1], "mode", 4))
			{
				Opmode_Set();
			}		
			else if (!strncmp(ag[1], "int", 3))
			{
				gpio_set_level(PIN_TOF0_INT, atoi(ag[2])?1:0);
			}					

	
    

			  
			/* There are more parameters to return after this one. */
//			pcWriteBuffer[ 0 ] = 0x00;
			xReturn = pdFALSE;
			lParameterNumber = 0L;
		}
	}

	return xReturn;
}



