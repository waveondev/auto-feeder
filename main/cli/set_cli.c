#include "set_cli.h"
#include "app_moter.h"
#include "app_led.h"
#include "wifi_task.h"
#include "app_HX711.h"
#include "opmode_task.h"
#include "app_config_flash.h"

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

			else if (!strncmp(ag[1], "moter", 5))
			{
                start_motor_with_boost(atoi(ag[2]),atoi(ag[3]));
            }
			else if (!strncmp(ag[1], "duty", 4))
			{
				set_motor_speed_percent(atoi(ag[2]));
            }
			else if (!strncmp(ag[1], "ledr", 4))
			{
                val32 = atoi(ag[2]);
                set_rgb_led(val32,0,0,0);
            }	
			else if (!strncmp(ag[1], "ledg", 4))
			{
                val32 = atoi(ag[2]);
				set_rgb_led(0,val32,0,0);
            }			
			else if (!strncmp(ag[1], "ledb", 4))
			{
                val32 = atoi(ag[2]);
				set_rgb_led(0,0,val32,0);
            }			
			else if (!strncmp(ag[1], "ledw", 4))
			{
                val32 = atoi(ag[2]);
				set_rgb_led(0,0,0,val32);
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
		
			/* There are more parameters to return after this one. */
//			pcWriteBuffer[ 0 ] = 0x00;
			xReturn = pdFALSE;
			lParameterNumber = 0L;
		}
	}

	return xReturn;
}



