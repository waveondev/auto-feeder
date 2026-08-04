/*
 * FreeRTOS+CLI V1.0.4 (C) 2014 Real Time Engineers ltd.  All rights reserved.
 *
 * This file is part of the FreeRTOS+CLI distribution.  The FreeRTOS+CLI license
 * terms are different to the FreeRTOS license terms.
 *
 * FreeRTOS+CLI uses a dual license model that allows the software to be used
 * under a standard GPL open source license, or a commercial license.  The
 * standard GPL license (unlike the modified GPL license under which FreeRTOS
 * itself is distributed) requires that all software statically linked with
 * FreeRTOS+CLI is also distributed under the same GPL V2 license terms.
 * Details of both license options follow:
 *
 * - Open source licensing -
 * FreeRTOS+CLI is a free download and may be used, modified, evaluated and
 * distributed without charge provided the user adheres to version two of the
 * GNU General Public License (GPL) and does not remove the copyright notice or
 * this text.  The GPL V2 text is available on the gnu.org web site, and on the
 * following URL: http://www.FreeRTOS.org/gpl-2.0.txt.
 *
 * - Commercial licensing -
 * Businesses and individuals that for commercial or other reasons cannot comply
 * with the terms of the GPL V2 license must obtain a low cost commercial
 * license before incorporating FreeRTOS+CLI into proprietary software for
 * distribution in any form.  Commercial licenses can be purchased from
 * http://shop.freertos.org/cli and do not require any source files to be
 * changed.
 *
 * FreeRTOS+CLI is distributed in the hope that it will be useful.  You cannot
 * use FreeRTOS+CLI unless you agree that you use the software 'as is'.
 * FreeRTOS+CLI is provided WITHOUT ANY WARRANTY; without even the implied
 * warranties of NON-INFRINGEMENT, MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. Real Time Engineers Ltd. disclaims all conditions and terms, be they
 * implied, expressed, or statutory.
 *
 * 1 tab == 4 spaces!
 *
 * http://www.FreeRTOS.org
 * http://www.FreeRTOS.org/FreeRTOS-Plus
 *
 */

#include "FreeRTOS_CLI.h"
#include "esp_log.h"
#include "config_cli.h"
#include "set_cli.h"
#include "debug_cli.h"
#include "mqtt_cli.h"
#include "gpio_util.h"
#include "app_slid_motor.h"
#include "app_feed_motor.h"
#include "app_acc_motor.h"
const char jbx_pwd_1[16] = "#80860612";

typedef struct xCOMMAND_INPUT_LIST
{
	const CLI_Command_Definition_t *pxCommandLineDefinition;
	struct xCOMMAND_INPUT_LIST *pxNext;
} CLI_Definition_List_Item_t;

#define CONSOLE_TASK_STACK_SIZE (configMINIMAL_STACK_SIZE*2)
#define CONSOLE_TASK_DELAY_MS(x) (x/portTICK_PERIOD_MS)

static const char* TAG = "console_task";
static const char * const pcEndOfCommandOutputString = "CMD> ";
static const char * const pcNewLine = "\r\n";
static void console_main(void *argument);

#if( configINCLUDE_QUERY_HEAP_COMMAND == 1 )
static BaseType_t prvQueryHeapCommand( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString );
#endif
#if( configINCLUDE_TRACE_RELATED_CLI_COMMANDS == 1 )
static	BaseType_t prvStartStopTraceCommand( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString );
#endif
static BaseType_t prvTaskStatsCommand( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString );
static BaseType_t prvLoginCommand( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString );
static BaseType_t prvLogoutCommand( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString );
static BaseType_t prvHelpCommand( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString );
static BaseType_t prvRunTimeStatsCommand( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString );
static int8_t prvGetNumberOfParameters( const char *pcCommandString );
BaseType_t prvResetCommand( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString );
static BaseType_t prvmotorCommand( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString );
static const CLI_Command_Definition_t xTaskStats;
static const CLI_Command_Definition_t xHelpCommand;
static const CLI_Command_Definition_t xLoginCommand;
static const CLI_Command_Definition_t xLogoutCommand;
static const CLI_Command_Definition_t xSetCommand;


#if( configINCLUDE_QUERY_HEAP_COMMAND == 1 )
static const CLI_Command_Definition_t xQueryHeap;
#endif
#if( configINCLUDE_TRACE_RELATED_CLI_COMMANDS == 1 )
static const CLI_Command_Definition_t xStartStopTrace;
#endif
#if (	configGENERATE_RUN_TIME_STATS == 1 )
static const CLI_Command_Definition_t xRunTimeStats;
#endif
#if 1
static const CLI_Command_Definition_t xmqttCommand =
{
	"mqtt",
	"\033[1;33mmqtt\033[0m\r\n  mqtt command\r\n",
	prvMQTTformationCommand,
	-1,
	DEVEL_MODE
};
static const CLI_Command_Definition_t xDbgCommand =
{
	"dbg",
	"\033[1;33mdbg\033[0m\r\n  dbg command\r\n",
	prvDebugformationCommand,
	-1,
	DEVEL_MODE
};
static const CLI_Command_Definition_t xSetCommand =
{
	"set",
	"\033[1;33mset\033[0m\r\n  set command\r\n",
	prvSetInformationCommand,
	-1,
	DEVEL_MODE
};
static const CLI_Command_Definition_t xConfigCommand =
{
	"config",
	"\033[1;33mconfig\033[0m\r\n  config command\r\n",
	prvConfigInformationCommand,
	-1,
	DEVEL_MODE
};
#endif
/*--------------------------------------wisun_cli----------------------------------------*/
/* Structure that defines the "run-time-stats" command line command.   This
generates a table that shows how much run time each task has */
#if (	configGENERATE_RUN_TIME_STATS == 1 )

static const CLI_Command_Definition_t xRunTimeStats =
{
	"runtime", /* The command string to type. */
	"\033[1;33mruntime\033[0m\r\n  Displays a table showing how much processing time each FreeRTOS task has used\r\n",
	prvRunTimeStatsCommand, /* The function to run. */
	0, /* No parameters are expected. */
	DEVEL_MODE,
};
#endif
/*-----------------------------------default_cli-------------------------------------*/



#if configINCLUDE_TRACE_RELATED_CLI_COMMANDS == 1
/* Structure that defines the "trace" command line command.  This takes a single
parameter, which can be either "start" or "stop". */
static const CLI_Command_Definition_t xStartStopTrace =
{
	"trace",
	"\033[1;33mtrace\033[0m [start | stop]:\r\n Starts or stops a trace recording for viewing in FreeRTOS+Trace\r\n",
	prvStartStopTraceCommand, /* The function to run. */
	1 /* One parameter is expected.  Valid values are "start" and "stop". */
};
#endif /* configINCLUDE_TRACE_RELATED_CLI_COMMANDS */


#if( configINCLUDE_QUERY_HEAP_COMMAND == 1 )
/* Structure that defines the "query_heap" command line command. */
static const CLI_Command_Definition_t xQueryHeap =
{
	"heap",
	"\033[1;33mheap\033[0m\r\n Displays the free heap space, and minimum ever free heap space.\r\n",
	prvQueryHeapCommand, /* The function to run. */
	0, /* The user can enter any number of commands. */
	GUEST_MODE
};
#endif /* configQUERY_HEAP_COMMAND */
#if 0
static const CLI_Command_Definition_t xLoginCommand =
{
	"login\0",
	"\033[1;33mlogin\033[0m\r\n  login id passward\r\n",
	prvLoginCommand,
	-1,
	ALL_MODE
};
#else
static const CLI_Command_Definition_t xLoginCommand =
{
	"login\0",
	"\033[1;33mlogin\033[0m\r\n  command guest or develop mode\r\n",
	prvLoginCommand,
	0,
	ALL_MODE
};
#endif
static const CLI_Command_Definition_t xmotorCommand =
{
	"motor\0",
	"\033[1;33mmotor\033[0m\r\n command default mode\r\n",
	prvmotorCommand,
	0,
	ALL_MODE
};
static const CLI_Command_Definition_t xLogoutCommand =
{
	"logout\0",
	"\033[1;33mlogout\033[0m\r\n command default mode\r\n",
	prvLogoutCommand,
	0,
	ALL_MODE
};
/* Structure that defines the "task-stats" command line command.  This generates
a table that gives information on each task in the system. */
static const CLI_Command_Definition_t xTaskStats =
{
	"task", /* The command string to type. */
	"\033[1;33mtask\033[0m\r\n  Displays a table showing the state of each FreeRTOS task\r\n",
	prvTaskStatsCommand, /* The function to run. */
	0, /* No parameters are expected. */
	GUEST_MODE
};
static const CLI_Command_Definition_t xHelpCommand =
{
	"help\0",
	"\r\n\033[1;33mhelp\033[0m\r\n  Lists all the registered commands\r\n",
	prvHelpCommand,
	0,
	ALL_MODE
};
static const CLI_Command_Definition_t xResetCommand =
{
	"reset\0",
	"\r\n\033[1;33mreset\033[0m\r\n  reset commands\r\n",
	prvResetCommand,
	0,
	ALL_MODE
};


/*-----------------------------------default_cli-------------------------------------*/


/* The definition of the list of commands.  Commands that are registered are
added to this list. */
static CLI_Definition_List_Item_t xRegisteredCommands =
{
	&xHelpCommand,	/* The first command in the list is always the help command, defined in this file. */
	NULL			/* The next pointer is initialised to NULL, as there are no other registered commands yet. */
};

static const CLI_Command_Definition_t* CommandList[] = 
{
	&xTaskStats,
	&xLoginCommand,
	&xLogoutCommand,
	&xSetCommand,
	#if( configINCLUDE_QUERY_HEAP_COMMAND == 1 )
	&xQueryHeap,
	#endif
	#if( configINCLUDE_TRACE_RELATED_CLI_COMMANDS == 1 )
	&xStartStopTrace,
	#endif
	#if (	configGENERATE_RUN_TIME_STATS == 1 )
	&xRunTimeStats,
	#endif
	&xConfigCommand,
	&xDbgCommand,
	&xmqttCommand,
	&xmotorCommand,
	&xResetCommand,
	NULL
};


/* A buffer into which command outputs can be written is declared here, rather
than in the command console implementation, to allow multiple command consoles
to share the same buffer.  For example, an application may allow access to the
command interpreter by UART and by Ethernet.  Sharing a buffer is done purely
to save RAM.  Note, however, that the command console itself is not re-entrant,
so only one command interpreter interface can be used at any one time.  For that
reason, no attempt at providing mutual exclusion to the cOutputBuffer array is
attempted. */

PRIVILEGED_DATA static portMUX_TYPE xKernelLock = portMUX_INITIALIZER_UNLOCKED;

static uint8_t Command_Level = DEVEL_MODE;	// hjjeon change ALL_MODE->DEVEL_MODE
static char cOutputBuffer[ configCOMMAND_INT_MAX_OUTPUT_SIZE ];
static CLI_Definition_List_Item_t *pxLastCommandInList = &xRegisteredCommands;
uint8_t Get_Command_Level(void)
{
	return Command_Level;
}
/*-----------------------------------------------------------*/

BaseType_t FreeRTOS_CLIRegisterCommand2(const char *pcCommandString )
{

	CLI_Definition_List_Item_t *pxNewListItem;
	BaseType_t xReturn = pdFAIL;
	const CLI_Command_Definition_t* Command = NULL;
	/* Check the parameter is not NULL. */
	configASSERT( pcCommandString );
	for(pxNewListItem = &xRegisteredCommands ; pxNewListItem	!= NULL ; pxNewListItem = pxNewListItem->pxNext)
	{
		if(strncmp(pxNewListItem->pxCommandLineDefinition->pcCommand,pcCommandString,strlen(pxNewListItem->pxCommandLineDefinition->pcCommand)) == 0)
		{
			return xReturn;
		}
	}
	for(uint8_t a=0;CommandList[a] != NULL;a++)
	{
		//printf("%s [%d]\r\n",CommandList[a]->pcCommand,a);
		if(strncmp(CommandList[a]->pcCommand,pcCommandString,strlen(CommandList[a]->pcCommand)) == 0)
		{
			Command = CommandList[a];
			break;
		}
	}
	if(Command == NULL)
		return xReturn;
	/* Create a new list item that will reference the command being registered. */
	pxNewListItem = ( CLI_Definition_List_Item_t * ) pvPortMalloc( sizeof( CLI_Definition_List_Item_t ) );
	configASSERT( pxNewListItem );

	if( pxNewListItem != NULL )
	{
		taskENTER_CRITICAL(&xKernelLock);
		{
			/* Reference the command being registered from the newly created
			list item. */
			pxNewListItem->pxCommandLineDefinition = Command;

			/* The new list item will get added to the end of the list, so
			pxNext has nowhere to point. */
			pxNewListItem->pxNext = NULL;

			/* Add the newly created list item to the end of the already existing
			list. */
			pxLastCommandInList->pxNext = pxNewListItem;

			/* Set the end of list marker to the new list item. */
			pxLastCommandInList = pxNewListItem;
		}
		taskEXIT_CRITICAL(&xKernelLock);

		xReturn = pdPASS;
		//printf("%s = %p , size %d \r\n",pxNewListItem->pxCommandLineDefinition->pcCommand,pxLastCommandInList,sizeof( CLI_Definition_List_Item_t ));
	}

	return xReturn;
}

BaseType_t FreeRTOS_CLIDeleteCommand( const char *pcCommandString )
{
	CLI_Definition_List_Item_t *pxFirstCommandInList  = NULL;
	CLI_Definition_List_Item_t *pxNewListItem;
	BaseType_t xReturn = pdFAIL;
	const char *pcRegisteredCommandString;
	size_t xCommandStringLength;
	pcRegisteredCommandString = pcCommandString;
	xCommandStringLength = strlen( pcRegisteredCommandString );
	CLI_Command_Definition_t* Command = NULL;
	
	for(uint8_t a=0;CommandList[a] != NULL;a++)
	{
		if(strncmp(CommandList[a]->pcCommand,pcCommandString,strlen(CommandList[a]->pcCommand)) == 0)
		{
			Command = (CLI_Command_Definition_t*)CommandList[a];
			break;
		}
	}
	if(Command == NULL)
		return xReturn;
	
	for(pxNewListItem = &xRegisteredCommands ; pxNewListItem	!= NULL; pxNewListItem = pxNewListItem->pxNext)
	{
		if(strncmp(pxNewListItem->pxCommandLineDefinition->pcCommand,pcRegisteredCommandString,xCommandStringLength) == 0)
		{
			if(pxFirstCommandInList == NULL)
			{
				pxFirstCommandInList = pxNewListItem->pxNext;
			}
			else
			{
				pxFirstCommandInList->pxNext = pxNewListItem->pxNext;
			}
			//printf("del %p , size %d \r\n",pxNewListItem,sizeof( CLI_Definition_List_Item_t ));
			pxNewListItem->pxNext = NULL;
			xReturn = pdPASS;
			vPortFree (pxNewListItem);
			pxNewListItem = pxFirstCommandInList;
		}
		else
			pxFirstCommandInList = pxNewListItem;
	}
	pxLastCommandInList = pxFirstCommandInList;
	//printf("last = %p , size %d \r\n",pxLastCommandInList,sizeof( CLI_Definition_List_Item_t ));
	return xReturn;
}

/*-----------------------------------------------------------*/

BaseType_t FreeRTOS_CLIProcessCommand( const char * const pcCommandInput, char * pcWriteBuffer, size_t xWriteBufferLen  )
{
static const CLI_Definition_List_Item_t *pxCommand = NULL;
BaseType_t xReturn = pdTRUE;
const char *pcRegisteredCommandString;
size_t xCommandStringLength;

	/* Note:  This function is not re-entrant.  It must not be called from more
	thank one task. */

	if( pxCommand == NULL )
	{
		/* Search for the command string in the list of registered commands. */
		for( pxCommand = &xRegisteredCommands; pxCommand != NULL; pxCommand = pxCommand->pxNext )
		{
			pcRegisteredCommandString = pxCommand->pxCommandLineDefinition->pcCommand;
			xCommandStringLength = strlen( pcRegisteredCommandString );

			/* To ensure the string lengths match exactly, so as not to pick up
			a sub-string of a longer command, check the byte after the expected
			end of the string is either the end of the string or a space before
			a parameter. */
			if( ( pcCommandInput[ xCommandStringLength ] == ' ' ) || ( pcCommandInput[ xCommandStringLength ] == 0x00 ) )
			{
				if( strncmp( pcCommandInput, pcRegisteredCommandString, xCommandStringLength ) == 0 )
				{
					/* The command has been found.  Check it has the expected
					number of parameters.  If cExpectedNumberOfParameters is -1,
					then there could be a variable number of parameters and no
					check is made. */
					if( pxCommand->pxCommandLineDefinition->cExpectedNumberOfParameters >= 0 )
					{
						if( prvGetNumberOfParameters( pcCommandInput ) != pxCommand->pxCommandLineDefinition->cExpectedNumberOfParameters )
						{
							xReturn = pdFALSE;
						}
					}

					break;
				}
			}
		}
	}

	if( ( pxCommand != NULL ) && ( xReturn == pdFALSE ) )
	{
		/* The command was found, but the number of parameters with the command
		was incorrect. */
		strncpy( pcWriteBuffer, "Incorrect command parameter(s).\r\n\r\n", xWriteBufferLen );
		//strncpy( pcWriteBuffer, "Incorrect command parameter(s).  Enter \"help\" to view a list of available commands.\r\n\r\n", xWriteBufferLen );
		pxCommand = NULL;
	}
	else if( pxCommand != NULL )
	{
		/* Call the callback function that is registered to this command. */
		xReturn = pxCommand->pxCommandLineDefinition->pxCommandInterpreter( pcWriteBuffer, xWriteBufferLen, pcCommandInput );

		/* If xReturn is pdFALSE, then no further strings will be returned
		after this one, and	pxCommand can be reset to NULL ready to search
		for the next entered command. */
		if( xReturn == pdFALSE )
		{
			pxCommand = NULL;
		}
	}
	else
	{
		/* pxCommand was NULL, the command was not found. */
		strncpy( pcWriteBuffer, "Command not recognised.\r\n\r\n", xWriteBufferLen );
		//strncpy( pcWriteBuffer, "Command not recognised.  Enter 'help' to view a list of available commands.\r\n\r\n", xWriteBufferLen );
		xReturn = pdFALSE;
	}

	return xReturn;
}
/*-----------------------------------------------------------*/

char *FreeRTOS_CLIGetOutputBuffer( void )
{
	cOutputBuffer[0] = '\0';	// clear null for output buffer
	return cOutputBuffer;
}
/*-----------------------------------------------------------*/

const char *FreeRTOS_CLIGetParameter( const char *pcCommandString, UBaseType_t uxWantedParameter, BaseType_t *pxParameterStringLength )
{
UBaseType_t uxParametersFound = 0;
const char *pcReturn = NULL;

	*pxParameterStringLength = 0;

	while( uxParametersFound < uxWantedParameter )
	{
		/* Index the character pointer past the current word.  If this is the start
		of the command string then the first word is the command itself. */
		while( ( ( *pcCommandString ) != 0x00 ) && ( ( *pcCommandString ) != ' ' ) )
		{
			pcCommandString++;
		}

		/* Find the start of the next string. */
		while( ( ( *pcCommandString ) != 0x00 ) && ( ( *pcCommandString ) == ' ' ) )
		{
			pcCommandString++;
		}

		/* Was a string found? */
		if( *pcCommandString != 0x00 )
		{
			/* Is this the start of the required parameter? */
			uxParametersFound++;

			if( uxParametersFound == uxWantedParameter )
			{
				/* How long is the parameter? */
				pcReturn = pcCommandString;
				while( ( ( *pcCommandString ) != 0x00 ) && ( ( *pcCommandString ) != ' ' ) )
				{
					( *pxParameterStringLength )++;
					pcCommandString++;
				}

				if( *pxParameterStringLength == 0 )
				{
					pcReturn = NULL;
				}

				break;
			}
		}
		else
		{
			break;
		}
	}

	return pcReturn;
}
char GetData(void)
{
	uint8_t cRxedChar = 0;
    cRxedChar = getchar();
    if(cRxedChar != '\0' && cRxedChar != 0xFF) //ignores null input, 0xFF, CR in CRLF
    {
        return cRxedChar;
    }
    return 0;
}
#include "esp_log.h"
void vOutputString(const char* func, uint32_t len, uint8_t* data)
{
	for(uint32_t i = 0; i < len; i++)
    {
        putchar(data[i]); // 혹은 printf("%c", data[i]);
    }
}
/*-----------------------------------------------------------*/
static BaseType_t prvLoginCommand( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString )
{
	( void ) pcCommandString;
	( void ) xWriteBufferLen;
	char ID_Data[20] = "\r\nID > ";
	char PASSWORD_Data[20] = "\r\nPASSWORD > ";
	configASSERT( pcWriteBuffer );
	sprintf (pcWriteBuffer, "\r\n");
	vOutputString(__FUNCTION__,strlen(ID_Data),(uint8_t*)ID_Data);
	memset(ID_Data,0x00,sizeof(ID_Data));
	uint8_t cRxedChar = 0;
	uint8_t cInputIndex = 0;
	while(1)
	{
			while(1)
			{
                    cRxedChar = GetData();
                    if(cRxedChar != 0) //ignores null input, 0xFF, CR in CRLF
                    {
                        break;
                    }
					vTaskDelay(CONSOLE_TASK_DELAY_MS(10));			
			}
			if( cRxedChar == '\r' || cRxedChar == '\n' )
			{
					if(cInputIndex == 0)
					{
							sprintf(ID_Data,"\r\n ID > ");
							vOutputString(__FUNCTION__,strlen(ID_Data),(uint8_t*)ID_Data);
							memset(ID_Data,0x00,sizeof(ID_Data));
					}
					else
					{
							vOutputString(__FUNCTION__,strlen(PASSWORD_Data),(uint8_t*)PASSWORD_Data);		
							break;
					}
			}
			else
			{
					if(cRxedChar == ASCII_CTRL_C)
					{
							return pdFALSE;
					}
					vOutputString("1", sizeof( cRxedChar ), (uint8_t *)&cRxedChar);
					if( ( cRxedChar == '\b' ) || ( cRxedChar == cmdASCII_DEL ) )
					{
							/* Backspace was pressed.  Erase the last character in the
							string - if any. */
							if( cInputIndex > 0 )
							{
									cInputIndex--;
									ID_Data[ cInputIndex ] = '\0';
							}
					}
					else
					{
							if( ( cRxedChar >= ' ' ) && ( cRxedChar <= '~' ) )
							{
									if( cInputIndex < sizeof(ID_Data) )
									{
										ID_Data[ cInputIndex ] = cRxedChar;
										cInputIndex++;
									}
							}
					}
			}
	}
	cInputIndex = 0;
	memset(PASSWORD_Data,0x00,sizeof(PASSWORD_Data));
	while(1)
	{
			while(1)
			{
                    cRxedChar = GetData();
                    if(cRxedChar != 0) //ignores null input, 0xFF, CR in CRLF
                    {
                        break;
                    }
					vTaskDelay(CONSOLE_TASK_DELAY_MS(10));			
			}
			if( cRxedChar == '\r' || cRxedChar == '\n' )
			{
					if(cInputIndex == 0)
					{
							sprintf(PASSWORD_Data,"\r\n PASSWORD > ");
							vOutputString(__FUNCTION__,strlen(PASSWORD_Data),(uint8_t*)PASSWORD_Data);
							memset(PASSWORD_Data,0x00,sizeof(PASSWORD_Data));
					}
					else
					{
							if((strncmp(ID_Data,"root",4) == 0) && (strncmp(PASSWORD_Data,jbx_pwd_1,9) == 0))
							{
									vRegisterDefaultCLICommands(DEVEL_MODE);
							}
							else if((strncmp(ID_Data,"guest",5) == 0) && (strncmp(PASSWORD_Data,"guest1",6) == 0))
							{
									vRegisterDefaultCLICommands(GUEST_MODE);
							}
							else
							{
									sprintf(ID_Data,"\r\nLogin Fail\r\n");
									vOutputString(__FUNCTION__,strlen(ID_Data),(uint8_t*)ID_Data);
							}
							return pdFALSE;
					}
			}
			else
			{
					if(cRxedChar == ASCII_CTRL_C)
					{
							return pdFALSE;
					}
					if( ( cRxedChar == '\b' ) || ( cRxedChar == cmdASCII_DEL ) )
					{
							/* Backspace was pressed.  Erase the last character in the
							string - if any. */
							vOutputString("1", sizeof( cRxedChar ), (uint8_t *)&cRxedChar);
							if( cInputIndex > 0 )
							{
									cInputIndex--;
									PASSWORD_Data[ cInputIndex ] = '\0';
							}
					}
					else
					{
							if( ( cRxedChar >= ' ' ) && ( cRxedChar <= '~' ) )
							{
									if( cInputIndex < sizeof(PASSWORD_Data) )
									{
										PASSWORD_Data[ cInputIndex ] = cRxedChar;
										cInputIndex++;
										cRxedChar = 0x2A;
										vOutputString("1", sizeof( cRxedChar ), (uint8_t *)&cRxedChar);
									}
							}
					}
			}
	}
	/* There is no more data to return after this single string, so return
	pdFALSE. */
	return pdFALSE;
}

static BaseType_t prvLogoutCommand( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString )
{
	( void ) pcCommandString;
	( void ) xWriteBufferLen;
	sprintf (pcWriteBuffer, "LogOut %d -> %d \r\n",Get_Command_Level(),ALL_MODE);
	vRegisterDefaultCLICommands(ALL_MODE);
	return pdFALSE;
}
#define KEY_ESC 27
/*-----------------------------------------------------------*/
static BaseType_t prvmotorCommand( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString )
{
	( void ) pcCommandString;
	( void ) xWriteBufferLen;
	configASSERT( pcWriteBuffer );
	sprintf (pcWriteBuffer, "\r\n");
	uint8_t cRxedChar = 0;
	uint8_t cInputIndex = 0;
	uint8_t index = 100;
	while(1)
	{
			while(1)
			{
                    cRxedChar = GetData();
                    if(cRxedChar != 0) //ignores null input, 0xFF, CR in CRLF
                    {
                        break;
                    }
					vTaskDelay(CONSOLE_TASK_DELAY_MS(10));			
			}
			switch(cRxedChar)
			{
				case 'q':
					Sliding_CW(index);
				break;
				case 'w':
					Sliding_CCW(index);
				break;
				case 'e':
					Sliding_break();
				break;
				case 'r':
					Sliding_coast();
				break;
				case '<':
					if(index)
						index--;
					printf("index = %d \n",index);
				break;
				case '>':
					if(index < 100)
						index++;
					printf("index = %d \n",index);						
				break;
				case 'a':
					Feeder_CW();
				break;
				case 's':
					Feeder_CCW();
				break;
				case 'd':
					Feeder_break();
				break;
				case 'f':
					Feeder_coast();
				break;
				case 'z':
					Accum_Set(true);
				break;
				case 'x':
					Accum_Set(false);
				break;
				case KEY_ESC:
					
					return pdFALSE;	
				break;
				default:
								
				break;
			}
			
	}
	return pdFALSE;
}

static void vTaskListCustom(void)
{
    // 1. 현재 시스템의 총 태스크 개수 확인
    UBaseType_t uxArraySize = uxTaskGetNumberOfTasks();
    
    // 구조체 배열 동적 할당
    TaskStatus_t *pxTaskStatusArray = (TaskStatus_t *)malloc(uxArraySize * sizeof(TaskStatus_t));

    if (pxTaskStatusArray != NULL) {
        // 2. 모든 태스크의 시스템 상태 정보 가져오기
        uxArraySize = uxTaskGetSystemState(pxTaskStatusArray, uxArraySize, NULL);

        // 헤더 출력 (요청하신 양식 오른쪽에 Core 추가)
        printf("\nTask            State  Priority  Stack    #    Core\n");
        printf("====================================================\n");

        for (UBaseType_t x = 0; x < uxArraySize; x++) {
            // 상태(State) 문자 변환
            char state_char = '?';
            switch (pxTaskStatusArray[x].eCurrentState) {
                case eRunning:   state_char = 'R'; break;
                case eReady:     state_char = 'Y'; break;
                case eBlocked:   state_char = 'B'; break;
                case eSuspended: state_char = 'S'; break;
                case eDeleted:   state_char = 'D'; break;
                default: break;
            }

            // Core ID 문자열 변환 (고정 안 됨 = Any, 고정됨 = Core 0 또는 1)
            char core_str[32]; // 32바이트로 넉넉하게 변경

			if (pxTaskStatusArray[x].xCoreID == tskNO_AFFINITY) {
				snprintf(core_str, sizeof(core_str), "Any (0/1)");
			} else {
				// 이제 컴파일러가 32바이트 공간을 보고 안심하고 통과시킵니다.
				snprintf(core_str, sizeof(core_str), "Core %d", (int)pxTaskStatusArray[x].xCoreID);
			}

            // 기존 vTaskList와 완전히 동일한 너비와 정렬을 유지하며 출력
            printf("%-15s  %c       %u         %-7u  %-3u  %s\n",
                   pxTaskStatusArray[x].pcTaskName,
                   state_char,
                   (unsigned int)pxTaskStatusArray[x].uxCurrentPriority,
                   (unsigned int)pxTaskStatusArray[x].usStackHighWaterMark,
                   (unsigned int)pxTaskStatusArray[x].xTaskNumber,
                   core_str);
        }
        printf("====================================================\n\n");

        // 메모리 해제
        free(pxTaskStatusArray);
    }
}
static BaseType_t prvTaskStatsCommand( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString )
{
	const char *const pcHeader = "Task          State  Priority  Stack	#\r\n================================================\r\n";

	/* Remove compile time warnings about unused parameters, and check the
	write buffer is not NULL.  NOTE - for simplicity, this example assumes the
	write buffer length is adequate, so does not check for buffer overflows. */
	( void ) pcCommandString;
	( void ) xWriteBufferLen;
	configASSERT( pcWriteBuffer );

	/* Generate a table of task stats. */
	//strcpy( pcWriteBuffer, pcHeader );
	//vTaskList( pcWriteBuffer + strlen( pcHeader ) );
	vTaskListCustom();
	/* There is no more data to return after this single string, so return
	pdFALSE. */
	return pdFALSE;
}

#if( configINCLUDE_QUERY_HEAP_COMMAND == 1 )

static BaseType_t prvQueryHeapCommand( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString )
{
	/* Remove compile time warnings about unused parameters, and check the
	write buffer is not NULL.  NOTE - for simplicity, this example assumes the
	write buffer length is adequate, so does not check for buffer overflows. */
	( void ) pcCommandString;
	( void ) xWriteBufferLen;
	configASSERT( pcWriteBuffer );

	sprintf( pcWriteBuffer, "Current free heap %d bytes, minimum ever free heap %d bytes\r\n", ( int ) xPortGetFreeHeapSize(), ( int ) xPortGetMinimumEverFreeHeapSize() );

	/* There is no more data to return after this single string, so return
	pdFALSE. */
	return pdFALSE;
}

#endif /* configINCLUDE_QUERY_HEAP */
#if configINCLUDE_TRACE_RELATED_CLI_COMMANDS == 1

static BaseType_t prvStartStopTraceCommand( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString )
	{
	const char *pcParameter;
	BaseType_t lParameterStringLength;

		/* Remove compile time warnings about unused parameters, and check the
		write buffer is not NULL.  NOTE - for simplicity, this example assumes the
		write buffer length is adequate, so does not check for buffer overflows. */
		( void ) pcCommandString;
		( void ) xWriteBufferLen;
		configASSERT( pcWriteBuffer );

		/* Obtain the parameter string. */
		pcParameter = FreeRTOS_CLIGetParameter
						(
							pcCommandString,		/* The command string itself. */
							1,						/* Return the first parameter. */
							&lParameterStringLength	/* Store the parameter string length. */
						);

		/* Sanity check something was returned. */
		configASSERT( pcParameter );

		/* There are only two valid parameter values. */
		if( strncmp( pcParameter, "start", strlen( "start" ) ) == 0 )
		{
			/* Start or restart the trace. */
			vTraceStop();
			vTraceClear();
			vTraceStart();

			sprintf( pcWriteBuffer, "Trace recording (re)started.\r\n" );
		}
		else if( strncmp( pcParameter, "stop", strlen( "stop" ) ) == 0 )
		{
			/* End the trace, if one is running. */
			vTraceStop();
			sprintf( pcWriteBuffer, "Stopping trace recording.\r\n" );
		}
		else
		{
			sprintf( pcWriteBuffer, "Valid parameters are 'start' and 'stop'.\r\n" );
		}

		/* There is no more data to return after this single string, so return
		pdFALSE. */
		return pdFALSE;
	}

#endif /* configINCLUDE_TRACE_RELATED_CLI_COMMANDS */
/*-----------------------------------------------------------*/
static BaseType_t prvHelpCommand( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString )
{
static const CLI_Definition_List_Item_t * pxCommand = NULL;
BaseType_t xReturn;

	( void ) pcCommandString;

	if( pxCommand == NULL )
	{
		/* Reset the pxCommand pointer back to the start of the list. */
		pxCommand = &xRegisteredCommands;
	}

	/* Return the next command help string, before moving the pointer on to
	the next command in the list. */
	strncpy( pcWriteBuffer, pxCommand->pxCommandLineDefinition->pcHelpString, xWriteBufferLen );
	pxCommand = pxCommand->pxNext;

	if( pxCommand == NULL )
	{
		/* There are no more commands in the list, so there will be no more
		strings to return after this one and pdFALSE should be returned. */
		strcpy( &pcWriteBuffer[strlen(pcWriteBuffer)], "\r\n" );
		xReturn = pdFALSE;
	}
	else
	{
		xReturn = pdTRUE;
	}

	return xReturn;
}
/*-----------------------------------------------------------*/
BaseType_t prvRunTimeStatsCommand( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString )
{
const char *const pcHeader = "Task            Abs Time      % Time\r\n========================================\r\n";

	/* Remove compile time warnings about unused parameters, and check the
	write buffer is not NULL.  NOTE - for simplicity, this example assumes the
	write buffer length is adequate, so does not check for buffer overflows. */
	( void ) pcCommandString;
	( void ) xWriteBufferLen;
	configASSERT( pcWriteBuffer );

	/* Generate a table of task stats. */
	strcpy( pcWriteBuffer, pcHeader );
	#if configGENERATE_RUN_TIME_STATS
	vTaskGetRunTimeStats( pcWriteBuffer + strlen( pcHeader ) );
	#endif

	/* There is no more data to return after this single string, so return
	pdFALSE. */
	return pdFALSE;
}

BaseType_t prvResetCommand( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString )
{
const char *const pcHeader = "Task            Abs Time      % Time\r\n========================================\r\n";

	/* Remove compile time warnings about unused parameters, and check the
	write buffer is not NULL.  NOTE - for simplicity, this example assumes the
	write buffer length is adequate, so does not check for buffer overflows. */
	( void ) pcCommandString;
	( void ) xWriteBufferLen;
	configASSERT( pcWriteBuffer );
	esp_restart();

	/* There is no more data to return after this single string, so return
	pdFALSE. */
	return pdFALSE;
}

static int8_t prvGetNumberOfParameters( const char *pcCommandString )
{
int8_t cParameters = 0;
BaseType_t xLastCharacterWasSpace = pdFALSE;

	/* Count the number of space delimited words in pcCommandString. */
	while( *pcCommandString != 0x00 )
	{
		if( ( *pcCommandString ) == ' ' )
		{
			if( xLastCharacterWasSpace != pdTRUE )
			{
				cParameters++;
				xLastCharacterWasSpace = pdTRUE;
			}
		}
		else
		{
			xLastCharacterWasSpace = pdFALSE;
		}

		pcCommandString++;
	}

	/* If the command string ended with spaces, then there will have been too
	many parameters counted. */
	if( xLastCharacterWasSpace == pdTRUE )
	{
		cParameters--;
	}

	/* The value returned is one less than the number of space delimited words,
	as the first word should be the command itself. */
	return cParameters;
}

void vRegisterDefaultCLICommands(uint8_t level)
{
	Command_Level = level;
	for(uint8_t a=0;CommandList[a] != NULL;a++)
	{
		//printf("%s [%d]\r\n",CommandList[a]->pcCommand,a);
		if(CommandList[a]->level <= Command_Level)
		{
			FreeRTOS_CLIRegisterCommand2(CommandList[a]->pcCommand);
		}
		else
		{
			FreeRTOS_CLIDeleteCommand(CommandList[a]->pcCommand);
		}
	}
}

void console_task_init(void)
{
    ESP_LOGI(TAG,"console task_start");
    if (xTaskCreatePinnedToCore(
            console_main,                  // 태스크 함수
            "Console_Main",                // 태스크 이름
            CONSOLE_TASK_STACK_SIZE,       // 스택 크기
            NULL,        // 파라미터
            tskIDLE_PRIORITY + 1,      // 우선순위
            NULL,                  // 태스크 핸들
            1                          // ⭐ 코어 ID (1번 코어 = APP_CPU)
        ) != pdPASS) {                 // pdTRUE 대신 pdPASS를 쓰는 것이 FreeRTOS 관례입니다.
              ESP_LOGE(TAG, "Error creating Console_Main on Core 1");
    }

}


static void console_main(void *argument)
{
	int8_t cRxedChar, cInputIndex = 0;
	char *pcOutputString;
	static char cInputString[ cmdMAX_INPUT_SIZE ], cLastInputString[ cmdMAX_INPUT_SIZE ];
	portBASE_TYPE xReturned = pdFALSE;

	vRegisterDefaultCLICommands(Get_Command_Level());
	pcOutputString = FreeRTOS_CLIGetOutputBuffer();
	char str_buf[2];
	for( ;; )
	{
		//while( Login_Check_fn(*(uint32_t*)argument) )
        while(1)
        {
                cRxedChar = GetData();
                if(cRxedChar != 0) //ignores null input, 0xFF, CR in CRLF
                {
                    break;
                }
                vTaskDelay(CONSOLE_TASK_DELAY_MS(10));			
        }
		sprintf(str_buf,"%c",cRxedChar);
		/* Echo the character back. */
		vOutputString("1", strlen( str_buf ), (uint8_t *)str_buf);

		if( cRxedChar == '\r' || cRxedChar == '\n' )
		{
			/* The input command string is complete.  Ensure the previous
			UART transmission has finished before sending any more data.
			This task will be held in the Blocked state while the Tx completes,
			if it has not already done so, so no CPU time will be wasted by
			polling. */

			/* Start to transmit a line separator, just to make the output
			easier to read. */
					
			vOutputString("3", strlen( pcNewLine ), (uint8_t *) pcNewLine);

			/* See if the command is empty, indicating that the last command is
			to be executed again. */
			if( cInputIndex == 0 )
			{
				// hjjeon changed : prohibit last cmd re-execute
				//strcpy( ( char * ) cInputString, ( char * ) cLastInputString );
				APP_String_printf("%s",pcEndOfCommandOutputString);
				//vOutputString("5", strlen( pcEndOfCommandOutputString ), (uint8_t *) pcEndOfCommandOutputString);
				vTaskDelay(CONSOLE_TASK_DELAY_MS(1));
				continue;
			}

			/* Pass the received command to the command interpreter.  The
			command interpreter is called repeatedly until it returns
			pdFALSE as it might generate more than one string. */
			do
			{
				/* Once again, just check to ensure the UART has completed
				sending whatever it was sending last.  This task will be held
				in the Blocked state while the Tx completes, if it has not
				already done so, so no CPU time	is wasted polling. */

					xReturned = pdPASS;

                                
				if( xReturned == pdPASS )
				{
					/* Get the string to write to the UART from the command
					interpreter. */
					xReturned = FreeRTOS_CLIProcessCommand( cInputString,pcOutputString , configCOMMAND_INT_MAX_OUTPUT_SIZE );

					/* Write the generated string to the UART. */
					if(strlen( pcOutputString ) != 0)
					{
						vOutputString("4", strlen( pcOutputString ), (uint8_t *) pcOutputString);
					}
				}
			} while( xReturned != pdFALSE );

			/* All the strings generated by the input command have been sent.
			Clear the input	string ready to receive the next command.  Remember
			the command that was just processed first in case it is to be
			processed again. */

			strcpy( ( char * ) cLastInputString, ( char * ) cInputString );

			cInputIndex = 0;
			memset( cInputString, 0x00, cmdMAX_INPUT_SIZE );

			/* Ensure the last string to be transmitted has completed. */
			APP_String_printf("%s",pcEndOfCommandOutputString);
			//vOutputString("5", strlen( pcEndOfCommandOutputString ), (uint8_t *) pcEndOfCommandOutputString);

		}
		else
		{
			if( ( cRxedChar == '\b' ) || ( cRxedChar == cmdASCII_DEL ) )
			{
				/* Backspace was pressed.  Erase the last character in the
				string - if any. */
				if( cInputIndex > 0 )
				{
					cInputIndex--;
					cInputString[ cInputIndex ] = '\0';
				}
			}
			else
			{
				/* A character was entered.  Add it to the string
				entered so far.  When a \n is entered the complete
				string will be passed to the command interpreter. */
				if( ( cRxedChar >= ' ' ) && ( cRxedChar <= '~' ) )
				{
					if( cInputIndex < cmdMAX_INPUT_SIZE )
					{
						cInputString[ cInputIndex ] = cRxedChar;
						cInputIndex++;
					}
				}
			}
		}
		vTaskDelay(CONSOLE_TASK_DELAY_MS(10));
	}
}



void APP_Printf(char c)
{
	printf("%c",c);
	fflush(stdout);
}
void APP_String_printf(const char *format, ...)
{
    va_list args;
    
    // 1. 가변 인자 리스트 시작
    va_start(args, format);
    
    // 2. printf처럼 가변 인자를 받아 포맷팅 출력해주는 vprintf 사용
    vprintf(format, args);
    
    // 3. 가변 인자 리스트 종료
    va_end(args);
    
    // 4. 개행(\n)이 없어도 즉시 화면에 출력되도록 버퍼 비우기
    fflush(stdout);
}
/*-----------------------------------------------------------*/
