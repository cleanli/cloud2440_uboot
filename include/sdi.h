#ifndef __SD_H__
#define __SD_H__

#include "common.h"
/*
typedef uint8_t __u8;
typedef uint16_t __u16;
typedef uint32_t __u32;
typedef uint32_t u32;
typedef uint32_t ulong;
typedef uint16_t u16;
typedef uint16_t U16;
typedef uint16_t ushort;
typedef uint8_t u8;
typedef uint8_t uchar;
typedef uint32_t uint;
*/
typedef uint32_t U32;
typedef uint8_t uint8;

#ifdef __cplusplus
extern "C" {
#endif

// SD Interface
#define rSDICON     (*(volatile unsigned *)0x5a000000) //SDI control
#define rSDIPRE     (*(volatile unsigned *)0x5a000004) //SDI baud rate prescaler
#define rSDICARG    (*(volatile unsigned *)0x5a000008) //SDI command argument
#define rSDICCON    (*(volatile unsigned *)0x5a00000c) //SDI command control
#define rSDICSTA    (*(volatile unsigned *)0x5a000010) //SDI command status
#define rSDIRSP0    (*(volatile unsigned *)0x5a000014) //SDI response 0
#define rSDIRSP1    (*(volatile unsigned *)0x5a000018) //SDI response 1
#define rSDIRSP2    (*(volatile unsigned *)0x5a00001c) //SDI response 2
#define rSDIRSP3    (*(volatile unsigned *)0x5a000020) //SDI response 3
#define rSDIDTIMER  (*(volatile unsigned *)0x5a000024) //SDI data/busy timer
#define rSDIBSIZE   (*(volatile unsigned *)0x5a000028) //SDI block size
#define rSDIDCON    (*(volatile unsigned *)0x5a00002c) //SDI data control
#define rSDIDCNT    (*(volatile unsigned *)0x5a000030) //SDI data remain counter
#define rSDIDSTA    (*(volatile unsigned *)0x5a000034) //SDI data status
#define rSDIFSTA    (*(volatile unsigned *)0x5a000038) //SDI FIFO status
#define rSDIIMSK    (*(volatile unsigned *)0x5a00003c) //SDI interrupt mask

#ifdef __BIG_ENDIAN
#define rSDIDAT    (*(volatile unsigned *)0x5a00003f) //SDI data
#define SDIDAT     0x5a00003f
#else  // Little Endian
#define rSDIDAT    (*(volatile unsigned *)0x5a000040) //SDI data
#ifdef SDIDAT
#undef SDIDAT
#endif
#define SDIDAT (0x5a000040)
#endif   //SD Interface

#define rGPECON    (*(volatile unsigned *)0x56000040) //Port E control
#define rGPEDAT    (*(volatile unsigned *)0x56000044) //Port E data
#define rGPEUP     (*(volatile unsigned *)0x56000048) //Pull-up control E
                        
// Function prototypes
void SD_Op(uint opflag, uint sdaddr, uint size, uint memaddr);
void set_system_clock(void);
int SD_card_init(void);

void Rd_cid_csd(char reg);

int Check_status(char type);
int Chk_DATend(void);
int Chk_BUSYend(void);
int Chk_CMDend(int cmd, int be_resp);
int Chk_MMC_OCR(void);
int Chk_SD_OCR(void);

void TR_Buf_new(void);

void Card_sel_desel(char sel_desel);

void CMD0(void);
int CMD55(void);

void Rd_CSD(void);
void Rd_CID(void);

void Rd_Block(void);
void Rd_Stream(void);
void Wt_Block(void);
void Wt_Stream(void);

void Set_Prt(void);
void Clr_Prt(void);

void Flush_Rx_buf(void);
void SetBus(void);
void Set_1bit_bus(void);
void Set_4bit_bus(void);
void View_Rx_buf(void);
void View_Tx_buf(void);


#ifdef __cplusplus
}
#endif

/* Interface types: */
#define IF_TYPE_UNKNOWN		0
#define IF_TYPE_IDE		1
#define IF_TYPE_SCSI		2
#define IF_TYPE_ATAPI		3
#define IF_TYPE_USB		4
#define IF_TYPE_DOC		5
#define IF_TYPE_MMC		6
#define IF_TYPE_SD		7
#define IF_TYPE_SATA		8

enum SD_CMD_OPFLAG{
    SD_INFO,
    SD_READ,
    SD_WRITE,
};
#endif /*__SD_H___*/
