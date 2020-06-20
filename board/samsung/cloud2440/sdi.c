/*****************************************
  NAME: sdi
  DESC: SDI Bus Test
  WWW.YCTEK.COM
 *****************************************/
#include <linux/string.h>
#include "sdi.h"


#define INICLK	300000
//#define NORCLK	25000000
#define SDCLK	24000000	//PCLK=49.392MHz
#define MMCCLK	15000000	//PCLK=49.392MHz

#define POL	0
#define INT	1
#define DMA	2

#define Uart_Printf printf

int ReadS(unsigned int address,unsigned int *data,unsigned int size);
int WriteS(unsigned int address,unsigned int *data,unsigned int size);
static void mmc_decode_csd(uint32_t * resp);
static void mmc_decode_cid(uint32_t * resp);
int CMD13(void);    // Send card status
int CMD9(void);
// Global variables


unsigned int *Tx_buffer;	//128[word]*16[blk]=8192[byte]
unsigned int *Rx_buffer;	//128[word]*16[blk]=8192[byte]
volatile unsigned int rd_cnt;
volatile unsigned int wt_cnt;
volatile unsigned int block;
volatile unsigned int TR_end=0;
uint mmc_dev_if_type = IF_TYPE_UNKNOWN;
uint8 spec_ver;
uint mmc_dev_lba, mmc_dev_blksz;

int Wide=1; // 0:1bit, 1:4bit
int MMC=0;  // 0:SD  , 1:MMC

int  Maker_ID;
char Product_Name[7]; 
int  Serial_Num;

volatile int RCA;

void SD_Op(uint opflag, uint sdaddr, uint size, uint memaddr)
{
	U32 save_rGPEUP, save_rGPECON;
	
    mmc_dev_if_type = IF_TYPE_UNKNOWN;
    spec_ver = 0;
    RCA=0;
    MMC=0;
    //block=3072;   //3072Blocks=1.5MByte, ((2Block=1024Byte)*1024Block=1MByte)
    block=13;

	save_rGPEUP=rGPEUP;
	save_rGPECON=rGPECON;
	
    rGPEUP  = 0xf83f;     // SDCMD, SDDAT[3:0] => PU En.
    rGPECON = 0xaaaaaaaa;	//SDCMD, SDDAT[3:0]
//    rGPECON = 0xaaa800aa;	//SDCMD, SDDAT[3:0] => Input


    if(!SD_card_init())
	return;
 
    if(opflag == SD_READ){
        ReadS(sdaddr,(uint*)memaddr,size);
        printf("read %x from sd block %X to mem %X done\n",
                size, sdaddr, memaddr);
    }
    else if(opflag == SD_WRITE){
        WriteS(sdaddr,(uint*)memaddr,size);
        printf("write %x to sd block %X from mem %X done\n",
                size, sdaddr, memaddr);
    }
    
    if(MMC)
    {
		rSDICON |=(1<<5);    
    }

    Card_sel_desel(0);	// Card deselect

    rSDIDCON=0;//tark???
    rSDICSTA=0xffff;
	rGPEUP=save_rGPEUP;
	rGPECON=save_rGPECON;	
}




int SD_card_init(void)
{
//-- SD controller & card initialize 
    int i;
    uint cid[4];


    /* Important notice for MMC test condition */
    /* Cmd & Data lines must be enabled by pull up resister */

    rSDIPRE=get_PCLK()/(INICLK)-1;	// 400KHz
//	Uart_Printf("Init. Frequency is %dHz\n",(PCLK/(rSDIPRE+1)));
    
//    rSDICON=(1<<4)|(1<<1)|1;	// Type B, FIFO reset, clk enable
	rSDICON=(1<<4)|1;	// Type B, clk enable
	rSDIFSTA=rSDIFSTA|(1<<16);	//YH 040223 FIFO reset
    rSDIBSIZE=0x200;		// 512byte(128word)
    rSDIDTIMER=0x7fffff;		// Set timeout count

    for(i=0;i<0x1000;i++);  // Wait 74SDCLK for MMC card

    //Uart_Printf("rSDIRSP0=0x%x\n",rSDIRSP0);
    CMD0();
 //   Uart_Printf("\nIn idle\n");

    //-- Check MMC card OCR
    if(Chk_MMC_OCR()) 
    {
	Uart_Printf("\nIn MMC ready\n");
	MMC=1;
    mmc_dev_if_type = IF_TYPE_MMC;
	goto RECMD2;
    }

//    Uart_Printf("MMC check end!!\n");
    //-- Check SD card OCR
    if(Chk_SD_OCR()){ 
        //Uart_Printf("\n SD Card ready\n");
        mmc_dev_if_type = IF_TYPE_SD;
    }
    else
    {
		Uart_Printf("\nInitialize fail\nNo Card assertion\n");
        return 0;
    }

RECMD2:
    //-- Check attaced cards, it makes card identification state
    rSDICARG=0x0;   // CMD2(stuff bit)
    rSDICCON=(0x1<<10)|(0x1<<9)|(0x1<<8)|0x42; //lng_resp, wait_resp, start, CMD2

    //-- Check end of CMD2
    if(!Chk_CMDend(2, 1)) 
	goto RECMD2;
    rSDICSTA=0xa00;	// Clear cmd_end(with rsp)

    //Uart_Printf(" SDIRSP0=0x%x\n SDIRSP1=0x%x\n SDIRSP2=0x%x\n SDIRSP3=0x%x\n", rSDIRSP0,rSDIRSP1,rSDIRSP2,rSDIRSP3);
    cid[3] = rSDIRSP3;
    cid[2] = rSDIRSP2;
    cid[1] = rSDIRSP1;
    cid[0] = rSDIRSP0;
    mmc_decode_cid(cid);

    //Uart_Printf("\nEnd id\n");

RECMD3:
    //--Send RCA
    rSDICARG=MMC<<16;	    // CMD3(MMC:Set RCA, SD:Ask RCA-->SBZ)
    rSDICCON=(0x1<<9)|(0x1<<8)|0x43;	// sht_resp, wait_resp, start, CMD3

    //-- Check end of CMD3
    if(!Chk_CMDend(3, 1)) 
	goto RECMD3;
    rSDICSTA=0xa00;	// Clear cmd_end(with rsp)

    //--Publish RCA
    if(MMC) {
	RCA=1;

        rSDIPRE=(get_PCLK()/MMCCLK)-1;	// YH 0812, Normal clock=20MHz

        Uart_Printf("MMC Frequency is %luHz\n",(get_PCLK()/(rSDIPRE+1)));
    	}
    else 
    	{
			RCA=( rSDIRSP0 & 0xffff0000 )>>16;
		    Uart_Printf("RCA=0x%x\n",RCA);

			rSDIPRE=get_PCLK()/(SDCLK)-1;	// Normal clock=25MHz
			Uart_Printf("SD Frequency is %luHz\n",(get_PCLK()/(rSDIPRE+1)));
    	}	//YH 0716
    //--State(stand-by) check
    if( (rSDIRSP0 & 0x1e00)!=0x600 )  // CURRENT_STATE check
	goto RECMD3;
    
    //Uart_Printf("\nIn stand-by\n");
    
//    rSDIPRE=get_PCLK()/(2*NORCLK)-1;	// Normal clock=25MHz

    if(!CMD9())
	Uart_Printf("Get CSD fail!!!\n");

    Card_sel_desel(1);	// Select

    if(!MMC)
	Set_4bit_bus();
    else
	Set_1bit_bus();

    return 1;
}

void Card_sel_desel(char sel_desel)
{
    //-- Card select or deselect
    if(sel_desel)
    {
RECMDS7:	
	rSDICARG=RCA<<16;	// CMD7(RCA,stuff bit)
	rSDICCON= (0x1<<9)|(0x1<<8)|0x47;   // sht_resp, wait_resp, start, CMD7

	//-- Check end of CMD7
	if(!Chk_CMDend(7, 1))
	    goto RECMDS7;
	rSDICSTA=0xa00;	// Clear cmd_end(with rsp)

	//--State(transfer) check
	if( rSDIRSP0 & 0x1e00!=0x800 )
	    goto RECMDS7;
    }
    else
    {
RECMDD7:	
	rSDICARG=0<<16;		//CMD7(RCA,stuff bit)
	rSDICCON=(0x1<<8)|0x47;	//no_resp, start, CMD7

	//-- Check end of CMD7
	if(!Chk_CMDend(7, 0))
	    goto RECMDD7;
	rSDICSTA=0x800;	// Clear cmd_end(no rsp)
    }
}


int Chk_CMDend(int cmd, int be_resp)
//0: Timeout
{
    int finish0;

    if(!be_resp)    // No response
    {
    	finish0=rSDICSTA;
		while((finish0&0x800)!=0x800)	// Check cmd end
	    finish0=rSDICSTA;

	rSDICSTA=finish0;// Clear cmd end state

	return 1;
    }
    else	// With response
    {
    	finish0=rSDICSTA;
	while( !( ((finish0&0x200)==0x200) || ((finish0&0x400)==0x400) ))    // Check cmd/rsp end


		finish0=rSDICSTA;

	if(cmd==1 || cmd==41)	// CRC no check, CMD9 is a long Resp. command.

	{
	    if( (finish0&0xf00) != 0xa00 )  // Check error
	    {
		rSDICSTA=finish0;   // Clear error state

		if(((finish0&0x400)==0x400))
		    return 0;	// Timeout error
    	    }
	    rSDICSTA=finish0;	// Clear cmd & rsp end state
	}
	else	// CRC check
	{
	    if( (finish0&0x1f00) != 0xa00 )	// Check error
	    {
		//Uart_Printf("CMD%d:rSDICSTA=0x%x, rSDIRSP0=0x%x\n",cmd, rSDICSTA, rSDIRSP0);
		rSDICSTA=finish0;   // Clear error state

		if(((finish0&0x400)==0x400))
		    return 0;	// Timeout error
    	    }
	    rSDICSTA=finish0;
	}
	return 1;
    }
}

int Chk_DATend(void)
{
    int finish;

    finish=rSDIDSTA;
    while( !( ((finish&0x10)==0x10) || ((finish&0x20)==0x20) )){
        // Chek timeout or data end
        finish=rSDIDSTA;
    }

    if( (finish&0xfc) != 0x10 )
    {
        Uart_Printf("DATA:finish=0x%x\n", finish);
        rSDIDSTA=0xec;  // Clear error state
        return 0;
    }
    return 1;
}

int Chk_BUSYend(void)
{
    int finish, ct=5;

    finish=rSDIDSTA;

    while( !( ((finish&0x08)==0x08) || ((finish&0x20)==0x20) ))
    {
        if(ct-- == 0){
            break;
        }
        finish=rSDIDSTA;
        udelay(40000);
        printf("rSDIDSTA %X\n", finish);
    }

    if( (finish&0xfc) != 0x08 )
    {
        Uart_Printf("DATA:finish=0x%x\n", finish);
        rSDIDSTA=0xf4;  //clear error state
        return 0;
    }
    return 1;
}

void CMD0(void)
{
    //-- Make card idle state 
    rSDICARG=0x0;	    // CMD0(stuff bit)
    rSDICCON=(1<<8)|0x40;   // No_resp, start, CMD0

    //-- Check end of CMD0
    Chk_CMDend(0, 0);
    rSDICSTA=0x800;	    // Clear cmd_end(no rsp)
}

int Chk_MMC_OCR(void)
{
    int i;


    //-- Negotiate operating condition for MMC, it makes card ready state
    for(i=0;i<100;i++)	//Negotiation time is dependent on CARD Vendors.
    {
//    	rSDICARG=0xffc000;	    	    //CMD1(MMC OCR:2.6V~3.6V)
		rSDICARG=0xff8000;	    	    //CMD1(SD OCR:2.7V~3.6V)
    	rSDICCON=(0x1<<9)|(0x1<<8)|0x41;    //sht_resp, wait_resp, start, CMD1

    	//-- Check end of CMD1
// 	if(Chk_CMDend(1, 1) & rSDIRSP0==0x80ffc000)	//[31]:Card Power up status bit (busy)
		//0xffc000 is Voltage window
	if(Chk_CMDend(1, 1) && (rSDIRSP0>>16)==0x80ff) //YH 0903 [31]:Card Power up status bit (busy)
//	if(Chk_CMDend(1, 1) & rSDIRSP0==0x80ff8000) 
	{
	    rSDICSTA=0xa00;	// Clear cmd_end(with rsp)
	    return 1;	// Success
	}
    }
    rSDICSTA=0xa00;	// Clear cmd_end(with rsp)
    return 0;		// Fail
}

int Chk_SD_OCR(void)
{
    int i=0;

    //-- Negotiate operating condition for SD, it makes card ready state
    for(i=0;i<50;i++)	//If this time is short, init. can be fail.
    {
        CMD55();    // Make ACMD

        rSDICARG=0xff8000;	//ACMD41(SD OCR:2.7V~3.6V)
        //    	rSDICARG=0xffc000;	//ACMD41(MMC OCR:2.6V~3.6V)
        rSDICCON=(0x1<<9)|(0x1<<8)|0x69;//sht_resp, wait_resp, start, ACMD41

        //-- Check end of ACMD41
        if( Chk_CMDend(41, 1)){ 
            printf("ACMD41 rSDIRSP0 %x\n", rSDIRSP0);
            if( rSDIRSP0==0x80ff8000 ) {
                rSDICSTA=0xa00;	// Clear cmd_end(with rsp)
                return 1;	// Success	    
            }
        }
        udelay(200*1000); // Wait Card power up status
    }
    //Uart_Printf("SDIRSP0=0x%x\n",rSDIRSP0);
    rSDICSTA=0xa00;	// Clear cmd_end(with rsp)
    return 0;		// Fail
}

int CMD55(void)
{
    //--Make ACMD
    rSDICARG=RCA<<16;			//CMD7(RCA,stuff bit)
    rSDICCON=(0x1<<9)|(0x1<<8)|0x77;	//sht_resp, wait_resp, start, CMD55

    //-- Check end of CMD55
    if(!Chk_CMDend(55, 1)) 
	return 0;

    rSDICSTA=0xa00;	// Clear cmd_end(with rsp)
    return 1;
}

int CMD13(void)//SEND_STATUS
{
    int response0;

    rSDICARG=RCA<<16;			// CMD13(RCA,stuff bit)
    rSDICCON=(0x1<<9)|(0x1<<8)|0x4d;	// sht_resp, wait_resp, start, CMD13

    //-- Check end of CMD13
    if(!Chk_CMDend(13, 1)) 
	return 0;
    //Uart_Printf("rSDIRSP0=0x%x\n", rSDIRSP0);
    if(rSDIRSP0&0x100){
        Uart_Printf("Ready for Data\n");
    }
    else{
        Uart_Printf("Not Ready\n");
    }
    response0=rSDIRSP0;
    response0 &= 0x3c00;
    response0 = response0 >> 9;
    Uart_Printf("Current Status=%d\n", response0);
    if(response0==6)
	SD_Op(SD_INFO, 0, 0, 0);

    rSDICSTA=0xa00;	// Clear cmd_end(with rsp)
    return 1;
}

int CMD9(void)//SEND_CSD
{
    uint csd[4];
    rSDICARG=RCA<<16;				// CMD9(RCA,stuff bit)
    rSDICCON=(0x1<<10)|(0x1<<9)|(0x1<<8)|0x49;	// long_resp, wait_resp, start, CMD9

    //Uart_Printf("\n****CSD register****\n");
    //-- Check end of CMD9
    if(!Chk_CMDend(9, 1)) 
	return 0;

    //Uart_Printf(" SDIRSP0=0x%x\n SDIRSP1=0x%x\n SDIRSP2=0x%x\n SDIRSP3=0x%x\n", rSDIRSP0,rSDIRSP1,rSDIRSP2,rSDIRSP3);
    csd[3] = rSDIRSP3;
    csd[2] = rSDIRSP2;
    csd[1] = rSDIRSP1;
    csd[0] = rSDIRSP0;
    mmc_decode_csd(csd);
    return 1;
}

void Set_1bit_bus(void)
{
    Wide=0;
    if(!MMC)
	SetBus();
    Uart_Printf("\n****1bit bus****\n");
}

void Set_4bit_bus(void)
{
    Wide=1;
    SetBus();
    Uart_Printf("\n****4bit bus****\n");
}

void SetBus(void)
{
SET_BUS:
    CMD55();	// Make ACMD
    //-- CMD6 implement
    rSDICARG=Wide<<1;	    //Wide 0: 1bit, 1: 4bit
    rSDICCON=(0x1<<9)|(0x1<<8)|0x46;	//sht_resp, wait_resp, start, CMD55

    if(!Chk_CMDend(6, 1))   // ACMD6
	goto SET_BUS;
    rSDICSTA=0xa00;	    // Clear cmd_end(with rsp)
}

void Set_Prt(void)
{
    //-- Set protection addr.0 ~ 262144(32*16*512) 
    Uart_Printf("[Set protection(addr.0 ~ 262144) test]\n");

RECMD28:
    //--Make ACMD
    rSDICARG=0;	    // CMD28(addr) 
    rSDICCON=(0x1<<9)|(0x1<<8)|0x5c;	//sht_resp, wait_resp, start, CMD28

    //-- Check end of CMD28
    if(!Chk_CMDend(28, 1)) 
	goto RECMD28;
    rSDICSTA=0xa00;	// Clear cmd_end(with rsp)
}

void Clr_Prt(void)
{
    //-- Clear protection addr.0 ~ 262144(32*16*512) 
    Uart_Printf("[Clear protection(addr.0 ~ 262144) test]\n");

RECMD29:
    //--Make ACMD
    rSDICARG=0;	    // CMD29(addr)
    rSDICCON=(0x1<<9)|(0x1<<8)|0x5d;	//sht_resp, wait_resp, start, CMD29

    //-- Check end of CMD29
    if(!Chk_CMDend(29, 1)) 
	goto RECMD29;
    rSDICSTA=0xa00;	// Clear cmd_end(with rsp)
}

#define UNSTUFF_BITS(resp,start,size)					\
	({								\
		const int __size = size;				\
		const uint32_t __mask = (__size < 32 ? 1 << __size : 0) - 1;	\
		const int32_t __off = 3 - ((start) / 32);			\
		const int32_t __shft = (start) & 31;			\
		uint32_t __res;						\
									\
		__res = resp[__off] >> __shft;				\
		if (__size + __shft > 32)				\
			__res |= resp[__off-1] << ((32 - __shft) % 32);	\
		__res & __mask;						\
	})

/*
 * Given the decoded CSD structure, decode the raw CID to our CID structure.
 */
static void mmc_decode_cid(uint32_t * resp)
{
    char mmc_dev_vendor[40+1];
    char mmc_dev_product[20+1];
    char mmc_dev_revision[8+1];
	if (IF_TYPE_SD == mmc_dev_if_type) {
		/*
		 * SD doesn't currently have a version field so we will
		 * have to assume we can parse this.
		 */
		sprintf((char *)mmc_dev_vendor,
			"Man %02x OEM %c%c \"%c%c%c%c%c\" Date %u/%u",
			UNSTUFF_BITS(resp, 120, 8), UNSTUFF_BITS(resp, 112, 8),
			UNSTUFF_BITS(resp, 104, 8), UNSTUFF_BITS(resp, 96, 8),
			UNSTUFF_BITS(resp, 88, 8), UNSTUFF_BITS(resp, 80, 8),
			UNSTUFF_BITS(resp, 72, 8), UNSTUFF_BITS(resp, 64, 8),
			UNSTUFF_BITS(resp, 8, 4), UNSTUFF_BITS(resp, 12,
							       8) + 2000);
		sprintf((char *)mmc_dev_revision, "%d.%d",
			UNSTUFF_BITS(resp, 60, 4), UNSTUFF_BITS(resp, 56, 4));
		sprintf((char *)mmc_dev_product, "%u",
			UNSTUFF_BITS(resp, 24, 32));
	} else {
		/*
		 * The selection of the format here is based upon published
		 * specs from sandisk and from what people have reported.
		 */
		switch (spec_ver) {
		case 0:	/* MMC v1.0 - v1.2 */
		case 1:	/* MMC v1.4 */
			sprintf((char *)mmc_dev_vendor,
				"Man %02x%02x%02x \"%c%c%c%c%c%c%c\" Date %02u/%04u",
				UNSTUFF_BITS(resp, 120, 8), UNSTUFF_BITS(resp,
									 112,
									 8),
				UNSTUFF_BITS(resp, 104, 8), UNSTUFF_BITS(resp,
									 96, 8),
				UNSTUFF_BITS(resp, 88, 8), UNSTUFF_BITS(resp,
									80, 8),
				UNSTUFF_BITS(resp, 72, 8), UNSTUFF_BITS(resp,
									64, 8),
				UNSTUFF_BITS(resp, 56, 8), UNSTUFF_BITS(resp,
									48, 8),
				UNSTUFF_BITS(resp, 12, 4), UNSTUFF_BITS(resp, 8,
									4) +
				1997);
			sprintf((char *)mmc_dev_revision, "%d.%d",
				UNSTUFF_BITS(resp, 44, 4), UNSTUFF_BITS(resp,
									40, 4));
			sprintf((char *)mmc_dev_product, "%u",
				UNSTUFF_BITS(resp, 16, 24));
			break;

		case 2:	/* MMC v2.0 - v2.2 */
		case 3:	/* MMC v3.1 - v3.3 */
		case 4:	/* MMC v4 */
			sprintf((char *)mmc_dev_vendor,
				"Man %02x OEM %04x \"%c%c%c%c%c%c\" Date %02u/%04u",
				UNSTUFF_BITS(resp, 120, 8), UNSTUFF_BITS(resp,
									 104,
									 16),
				UNSTUFF_BITS(resp, 96, 8), UNSTUFF_BITS(resp,
									88, 8),
				UNSTUFF_BITS(resp, 80, 8), UNSTUFF_BITS(resp,
									72, 8),
				UNSTUFF_BITS(resp, 64, 8), UNSTUFF_BITS(resp,
									56, 8),
				UNSTUFF_BITS(resp, 12, 4), UNSTUFF_BITS(resp, 8,
									4) +
				1997);
			sprintf((char *)mmc_dev_product, "%u",
				UNSTUFF_BITS(resp, 16, 32));
			sprintf((char *)mmc_dev_revision, "N/A");
			break;

		default:
			printf("MMC card has unknown MMCA version %d\n",
			       spec_ver);
			break;
		}
	}
	printf("%s card:\nVendor: %s\nProduct: %s\nRevision: %s\n",
	       (IF_TYPE_SD == mmc_dev_if_type) ? "SD" : "MMC", mmc_dev_vendor,
	       mmc_dev_product, mmc_dev_revision);
}

/*
 * Given a 128-bit response, decode to our card CSD structure.
 */
static void mmc_decode_csd(uint32_t * resp)
{
	unsigned int mult, csd_struct;

	if (IF_TYPE_SD == mmc_dev_if_type) {
		csd_struct = UNSTUFF_BITS(resp, 126, 2);
		if (csd_struct != 0) {
			printf("SD: unrecognised CSD structure version %d\n",
			       csd_struct);
			return;
		}
	} else {
		/*
		 * We only understand CSD structure v1.1 and v1.2.
		 * v1.2 has extra information in bits 15, 11 and 10.
		 */
		csd_struct = UNSTUFF_BITS(resp, 126, 2);
		if (csd_struct != 1 && csd_struct != 2) {
			printf("MMC: unrecognised CSD structure version %d\n",
			       csd_struct);
			return;
		}

		spec_ver = UNSTUFF_BITS(resp, 122, 4);
		mmc_dev_if_type = IF_TYPE_MMC;
	}

	mult = 1 << (UNSTUFF_BITS(resp, 47, 3) + 2);
	mmc_dev_lba = (1 + UNSTUFF_BITS(resp, 62, 12)) * mult;
	mmc_dev_blksz = 1 << UNSTUFF_BITS(resp, 80, 4);
    rSDIBSIZE=mmc_dev_blksz;

	printf("Volume: %u(%x) blocks of %u bytes (%uMB) ",
		mmc_dev_lba,
		mmc_dev_lba,
		mmc_dev_blksz,
		mmc_dev_lba * mmc_dev_blksz / (1024 * 1024));
}

int ReadS(unsigned int address,unsigned int *data,unsigned int size)   
{   
    int status,block;   

    address=address<<9;   //address*512   

    block=(size/512) + ((size%512 == 0)?0:1);   

    rd_cnt=0;       

    rSDIFSTA=rSDIFSTA|(1<<16);    // FIFO reset   
    rSDIDCON=(2<<22)|(1<<19)|(1<<17)|(Wide<<16)|(1<<14)|(2<<12)|(block<<0);   

    rSDICARG=address;   // CMD17/18(addr)   

RERDCMD:   
    if(block<2)  //SINGLE_READ      
    {      
        rSDICCON=(0x1<<9)|(0x1<<8)|0x51;    //sht_resp, wait_resp, dat, start, CMD24      
        if(!Chk_CMDend(17,1))               //Check end of CMD17      
            goto RERDCMD;      
    }      
    else    //MULTI_READ      
    {      
        rSDICCON=(0x1<<9)|(0x1<<8)|0x52;    //sht_resp, wait_resp, dat, start, CMD25      
        if(!Chk_CMDend(18,1))               //Check end of CMD18      
            goto RERDCMD;   
    }   

    rSDICSTA=0xa00; // Clear cmd_end(with rsp)   

    while(rd_cnt<128*block)  // 512*block bytes   
    {   
        if((rSDIDSTA&0x20)==0x20) // Check timeout    
        {   
            rSDIDSTA=(0x1<<0x5);  // Clear timeout flag   
            break;   
        }   
        status=rSDIFSTA;   
        if((status&0x1000)==0x1000) // Is Rx data?   
        {   
            *data++ = rSDIDAT;   
            rd_cnt++;   
        }   
    }   
    //-- Check end of DATA   
    if(!Chk_DATend())   
    {   
        Uart_Printf(0,"Dat error\n");   
        rSDIDSTA=0x10;  // Clear data Tx/Rx end detect   
        return 0;   
    }   

    rSDIDSTA=0x10;   

    if(block>1)      
    {      
        do      
        {      
            rSDICARG=0x0;                       //CMD12(stuff bit)      
            rSDICCON=(0x1<<9)|(0x1<<8)|0x4c;    //sht_resp, wait_resp, start, CMD12      
        }while(!Chk_CMDend(12, 1));      
    }    

    rSDIDCON=rSDIDCON&~(7<<12);          
    rSDIFSTA=rSDIFSTA&0x200;    //Clear Rx FIFO Last data Ready   

    rSDIDSTA=0x10;  // Clear data Tx/Rx end detect   

    return 1;   
}   


int WriteS(unsigned int address,unsigned int *data,unsigned int size)   
{   
    int status,block;   

    address=address<<9;   //address*512   

    block=(size/512) + ((size%512 == 0)?0:1);   

    wt_cnt=0;       

    rSDIFSTA=rSDIFSTA|(1<<16);    // FIFO reset   
    rSDIDCON=(2<<22)|(1<<20)|(1<<17)|(Wide<<16)|(1<<14)|(3<<12)|(block<<0);   


    rSDICARG=address;       // CMD24/25(addr)   

REWTCMD:   
    if(block<2)  //SINGLE_WRITE      
    {      
        rSDICCON=(0x1<<9)|(0x1<<8)|0x58;    //sht_resp, wait_resp, dat, start, CMD24      
        if(!Chk_CMDend(24, 1))  //Check end of CMD24      
            goto REWTCMD;      
    }      
    else    //MULTI_WRITE      
    {      
        rSDICCON=(0x1<<9)|(0x1<<8)|0x59;    //sht_resp, wait_resp, dat, start, CMD25      
        if(!Chk_CMDend(25, 1))  //Check end of CMD25      
            goto REWTCMD;   
    }   

    rSDICSTA=0xa00; // Clear cmd_end(with rsp)     
    while(wt_cnt<128*block)   
    {   
        status=rSDIFSTA;   
        if((status&0x2000)==0x2000)    
        {   
            rSDIDAT = *data++;   
            wt_cnt++;   
            //Uart_Printf("Block No.=%d, wt_cnt=%d\n",block,wt_cnt);   
        }   
    }   
    //-- Check end of DATA   
    if(!Chk_DATend())   
    {   
        Uart_Printf(0,"Dat error\n");   
        rSDIDSTA=0x10;  // Clear data Tx/Rx end   
        return 1;   
    }   

    rSDIDSTA=0x10;   

    if(block>1)      
    {      
        do   
        {      
            rSDIDCON=(1<<18)|(1<<17)|(0<<16)|(1<<12)|(block<<0);      

            rSDICARG=0x0;                       //CMD12(stuff bit)      
            rSDICCON=(0x1<<9)|(0x1<<8)|0x4c;    //sht_resp, wait_resp, start, CMD12      

        }while(!Chk_CMDend(12,1));  //Stop cmd(CMD12)     

        if(!Chk_BUSYend())      //Check end of DATA(with busy state)      
            Uart_Printf(0,"Error\n");      

        rSDIDSTA=0x08;      
    }    

    rSDIDCON=rSDIDCON&~(7<<12);       //Clear Data Transfer mode => no operation, Cleata Data Transfer start   

    rSDIDSTA=0x10;  // Clear data Tx/Rx end   

    return 0;   
}   
