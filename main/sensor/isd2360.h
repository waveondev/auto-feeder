#ifndef __ISD2360_H__
#define __ISD2360_H__


//SPI Commands
#define PLAY_VP			0xA6
#define PLAY_VP_RN		0xAE
#define PLAY_VP_LP		0xA4
#define PLAY_VP_LP_RN	0xB2
#define STOP_LP			0x2E
#define EXE_VM			0xB0
#define EXE_VM_RN		0xBC
#define PLAY_SIL		0xA8
#define STOP			0x2A
#define SPI_PCM_READ	0xAC
#define SPI_SND_DEC		0xC0

#define READ_STATUS		0x40
#define READ_INT		0x46
#define READ_ID			0x48

#define DIG_READ		0xA2
#define DIG_WRITE		0xA0
#define ERASE_MEM		0x24
#define CHIP_ERASE		0x26
#define CHECKSUM		0xF2

#define PWR_UP			0x10
#define PWR_DN			0x12
#define SET_CLK_CFG		0xB4
#define RD_CLK_CFG		0xB6
#define WR_CFG_REG		0xB8
#define RD_CFG_REG		0xBA


typedef enum
{
	CMD_BSY = BIT0,
	CBUF_FUL = BIT1,
	VM_BSY = BIT2,
	DBUF_RDY = BIT6,
	PD = BIT7,
}E_ISD3800_STATUS;		

void isd2360_set(int index);
void isd2360_taskinit(void);



#endif



