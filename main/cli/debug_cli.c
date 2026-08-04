#include "debug_cli.h"
#include "app_config_flash.h"
#include "ble_tracker_id.h"
DBG_Resister_t DBG_Resister;


DBG_Resister_t* Debug_Get(void)
{
	return &DBG_Resister;
}


BaseType_t prvDebugformationCommand( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString )
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
			else if (!strncmp(ag[1], "debug", 5))
			{
                DBG_Resister.DBG_EN = atoi(ag[2]) ? 1 : 0;
            }
			else if (!strncmp(ag[1], "motor", 5))
			{
                DBG_Resister.motor = atoi(ag[2]) ? 1 : 0;
            }
			else if (!strncmp(ag[1], "tof", 3))
			{
                DBG_Resister.TOF = atoi(ag[2]) ? 1 : 0;
            }
			else if (!strncmp(ag[1], "hx711", 5))
			{
                DBG_Resister.HX711 = atoi(ag[2]) ? 1 : 0;
            }
			else if (!strncmp(ag[1], "adc", 3))
			{
                DBG_Resister.adc = atoi(ag[2]) ? 1 : 0;
            }
			else if (!strncmp(ag[1], "button", 6))
			{
                DBG_Resister.button = atoi(ag[2]) ? 1 : 0;
            }
			else if (!strncmp(ag[1], "ble", 3))
			{
                DBG_Resister.ble = atoi(ag[2]) ? 1 : 0;
            }
			else if (!strncmp(ag[1], "wifi", 4))
			{
                DBG_Resister.wifi = atoi(ag[2]) ? 1 : 0;
            }		
			else if (!strncmp(ag[1], "led", 3))
			{
                DBG_Resister.led = atoi(ag[2]) ? 1 : 0;
            }		
			else if (!strncmp(ag[1], "nvs", 3))
			{
				dump_all_configurations();
				print_all_nvs_keys();
            }			
			else if (!strncmp(ag[1], "tracker", 7))
			{
				dump_tracker_all_devices();
			}
					
			/* There are more parameters to return after this one. */
//			pcWriteBuffer[ 0 ] = 0x00;
			xReturn = pdFALSE;
			lParameterNumber = 0L;
		}
	}

	return xReturn;
}



