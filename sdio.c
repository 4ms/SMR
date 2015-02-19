/*
 * sdio.c
 *
 *  Created on: Aug 12, 2014
 *      Author: design
 */
#include "stm32f4xx.h"
#include "stm32f4xx_sdio.h"

#include "sdio.h"
#include "inouts.h"

static uint32_t SD_Command(uint32_t cmd, uint32_t resp, uint32_t arg);
static uint32_t SD_Response(uint32_t *response, uint32_t type);
static void SD_Panic(uint32_t code, uint8_t *message);
static void SD_StartBlockTransfer(uint8_t *buf, uint32_t cnt, uint32_t dir);

static uint32_t SDType;
static uint32_t RCA;

static volatile uint32_t SDIOTxRx=0;

//Private Write buffers
static uint8_t DatBuf[512*2];  //2 blocks (One will be in transit while other is being filled)
static uint8_t *pDatBuf=DatBuf;
static uint32_t BufCnt=0;

#define DATATIMEOUT   (0xFFFFFF)  //I simply made this up. A method for computing a realistic values from CSD is described in the specs.

//SDIO Commands  Index
#define CMD0          ((uint8_t)0)
#define CMD8          ((uint8_t)8)
#define CMD55         ((uint8_t)55)
#define ACMD41        ((uint8_t)41)
#define CMD2          ((uint8_t)2)
#define CMD3          ((uint8_t)3)
#define CMD9          ((uint8_t)9)
#define CMD7          ((uint8_t)7)
#define ACMD6         ((uint8_t)6)
#define CMD24         ((uint8_t)24)
#define CMD25         ((uint8_t)25)
#define CMD12         ((uint8_t)12)
#define CMD13         ((uint8_t)13)
#define CMD17         ((uint8_t)17)
#define CMD18         ((uint8_t)18)

//Auxilary defines
#define NORESP        (0x00)
#define SHRESP        (0x40)
#define LNRESP        (0xC0)
#define R3RESP        (0xF40)  //Note this is totaly out of standard. However, becouse of the masking in SD_Command it will be processed as SHRESP
//R3 does not contain a valid CRC. Therefore, CCRCFAIL is set and CMDREND is never set for R3.
//To properly process R3, exit the loop CCRCFAIL condition and don't check CMDREND

#define RESP_R1       (0x01)
#define RESP_R1b      (0x02)
#define RESP_R2       (0x03)
#define RESP_R3       (0x04)
#define RESP_R6       (0x05)
#define RESP_R7       (0x06)

#define UM2SD         (0x00)  //Transfer Direction
#define SD2UM         (0x02)


void SD_LowLevel_Init(void) {
  uint32_t tempreg;

  GPIO_InitTypeDef  GPIO_InitStructure;

  // GPIOC and GPIOD Periph clock enable
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB | RCC_AHB1Periph_GPIOC | RCC_AHB1Periph_GPIOD, ENABLE);

  //Turn off PB.3/4/5 because they are connected to PC12/8/2 on this board
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3 | GPIO_Pin_4 | GPIO_Pin_5;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
  GPIO_Init(GPIOB, &GPIO_InitStructure);

  //Initialize the pins
  GPIO_PinAFConfig(GPIOC, GPIO_PinSource8, GPIO_AF_SDIO);
  GPIO_PinAFConfig(GPIOC, GPIO_PinSource9, GPIO_AF_SDIO);
  GPIO_PinAFConfig(GPIOC, GPIO_PinSource10, GPIO_AF_SDIO);
  GPIO_PinAFConfig(GPIOC, GPIO_PinSource11, GPIO_AF_SDIO);
  GPIO_PinAFConfig(GPIOC, GPIO_PinSource12, GPIO_AF_SDIO);
  GPIO_PinAFConfig(GPIOD, GPIO_PinSource2, GPIO_AF_SDIO);

  // Configure PC.08, PC.09, PC.10, PC.11 pins: D0, D1, D2, D3 pins
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 | GPIO_Pin_11;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
  GPIO_Init(GPIOC, &GPIO_InitStructure);

  // Configure PD.02 CMD line
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
  GPIO_Init(GPIOD, &GPIO_InitStructure);

  // Configure PC.12 pin: CLK pin
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
  GPIO_Init(GPIOC, &GPIO_InitStructure);




  //Enable the SDIO APB2 Clock
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_SDIO, ENABLE);

  // Enable the DMA2 Clock
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA2, ENABLE);

  //Initialize the SDIO (with initial <400Khz Clock)
  tempreg=0;  //Reset value
  tempreg|=SDIO_CLKCR_CLKEN;  //Clock is enabled
  tempreg|=(uint32_t)0x76;  //Clock Divider. Clock=48000/(118+2)=400Khz
  //Keep the rest at 0 => HW_Flow Disabled, Rising Clock Edge, Disable CLK ByPass, Bus Width=0, Power save Disable
  SDIO->CLKCR=tempreg;

  //Power up the SDIO
  SDIO->POWER = 0x03;

}

void SD_Init(void){
  //uint32_t data;
  uint32_t response;
  uint32_t TimeOut=0xFFFF;
  uint32_t tempreg;

  //CMD0: GO_IDLE_STATE (No Response)
  SD_Command(CMD0, NORESP, 0);

  //CMD8: SEND_IF_COND  //Response to CMD8 is R7. But I will ignore that response
  SD_Command(CMD8, SHRESP, 0x000001AA); //Non v2.0 compliant sd's will cause panic here due to the timeout
  SD_Response(&response, RESP_R7); //AA is the check pattern. If response does not match with it, execution will be blocked in panic


  while (1) {
    ////Send ACMD41
    //CMD55
    SD_Command(CMD55, SHRESP, 0); //Note that argument should be RCA. But at this point RCA of SD is 0. (It will be changed after cmd3)
    SD_Response(&response, RESP_R1);

    //ACMD41 (Response is R3 which does not contain any CRC)
    //Second argument in the argument indicates that host supports SDHC. We will check acmd41 response if the SD is SC or HC
    SD_Command(ACMD41, R3RESP, (uint32_t) 0x80100000 | (uint32_t) 0x40000000);
    SD_Response(&response, RESP_R3);

    //Check the ready status in the response (R3)
    if ((response >> 31) == 1) {  //When card is busy this bit will be 0
      //Card is now initialized. Check to see if SD is SC or HC
      SDType=(response & 0x40000000) >> 30;  //1=HC, 0=SC
      break;
    } else {
      TimeOut--;
      if (!TimeOut) {SD_Panic(ACMD41, "SDIO:ACMD41 Timeout\n");}
    }
   }

  //Now we are in the Ready State. Ask for CID using CMD2
  //Response is R2. RESP1234 are filled with CID. I will ignore them
  SD_Command(CMD2, LNRESP, 0);

  //Now the card is in the identification mode. Request for the RCA number with cmd3
  SD_Command(CMD3, SHRESP, 0);
  SD_Response(&response, RESP_R6);
  //Read the RCA
  RCA=response>>16;

  //Now the card is in stand-by mode. From this point on I can change frequency as I wish (max24MHz)


  //Use cmd9 to read the card specific information
  //Response is R2 with CSI. I will ignore the response
  SD_Command(CMD9, LNRESP, (RCA << 16));


  //Put the Card in the tranfer mode using cmd7. (I will change the clock spped later together with bus width)
  //Bus width can only be changed in transfer mode
  SD_Command(CMD7, SHRESP, (RCA << 16));
  SD_Response(&response, RESP_R1);

  //Change the bus-width with cmd6
  //CMD55
  SD_Command(CMD55, SHRESP, (RCA << 16)); //Note the real RCA in the argument
  SD_Response(&response, RESP_R1);
  //ACMD6
  SD_Command(ACMD6, SHRESP, 0x02);
  SD_Response(&response, RESP_R1);

  //Configure SDIO->CLKCr for wide-bus width and new clock
  tempreg=0;  //Reset value
  tempreg|=(0x01)<<11; //4 bit Bus Width
  tempreg|=SDIO_CLKCR_CLKEN;  //Clock is enabled

  //DG:
  tempreg|=(0x04); //Divide-by-4
  //Keep the rest at 0=> HW_Flow:Disabled, Rising Edge, Disable bypass, Power save Disable, Clock Division=0
  //As the clock divider=0 => New clock=48/(Div+2)=48/2=24
  SDIO->CLKCR=tempreg;

  //Now we can start issuing read/write commands
}

uint8_t SD_WriteSingleBlock(uint8_t *buf, uint32_t blk) {
  uint32_t WriteAddr;
  uint32_t response;
  uint8_t err=0;

  //uint32_t response;
  if (SDType==1) {  //High Capacity
    WriteAddr=blk;
  } else if (SDType==0) {//Standard Capacity
    WriteAddr=blk*512;
  }
  //CMD24:WRITE_SINGLE_BLOCK
  SD_Command(CMD24, SHRESP, WriteAddr);
  SD_Response(&response, RESP_R1);

  //Card is now waiting some data from the Data lines. Start actual data transmission
  SD_StartBlockTransfer(buf, 512, UM2SD);

  //Wait for transmission to end
  err=SD_WaitTransmissionEnd();

  return(err);
}


uint8_t SD_ReadSingleBlock(uint8_t *buf, uint32_t blk) {
  uint32_t ReadAddr;
  uint32_t response;
  uint8_t err=0;

  //uint32_t response;
  if (SDType==1) {  //High Capacity
    ReadAddr=blk;
  } else if (SDType==0) {//Standard Capacity
    ReadAddr=blk*512;
  }

  //Send CMD17:READ_SINGLE_BLOCK
  SD_Command(CMD17, SHRESP, ReadAddr);
  SD_Response(&response, RESP_R1);

  //Card is now ready to send some data from the Data lines. Start actual data transmission
  SD_StartBlockTransfer(buf, 512, SD2UM);

  //Wait for transmission to end
  err=SD_WaitTransmissionEnd();

  return(err);

}

void SD_StartMultipleBlockWrite(uint32_t blk) {
  uint32_t WriteAddr;
  uint32_t response;

  //uint32_t response;
  if (SDType==1) {  //High Capacity
    WriteAddr=blk;
  } else if (SDType==0) {//Standard Capacity
    WriteAddr=blk*512;
  }

  //CMD25:WRITE_MULT_BLOCK with argument data address
  SD_Command(CMD25, SHRESP, WriteAddr);
  SD_Response(&response, RESP_R1);

  //Clear any flags
  SDIO->ICR=(uint32_t) 0xA003FF;
  return;
}

void SD_StopMultipleBlockWrite(void) {
  uint32_t response;

  //If there is previously programmed communication, wait for it to end first
  SD_WaitTransmissionEnd();

  //CMD12:STOP_TRANSMISSION
  SD_Command(CMD12, SHRESP, 0);
  SD_Response(&response, RESP_R1b);
  //Discard the untransferred buffer
  pDatBuf=DatBuf;
  BufCnt=0;

  while (((response & 0x100)>>8)==0) {
    //After CMD12, SD Card puts itself to the prg mode, by pulling D0 to low.
    //DPSM is not aware of this. (I hope it is aware of this during multiblock communication. I need to verify this.
    //If it is not, then I have to repeat the same procedure after each block during multiblock write which will cause too much overhead)
    //Check Card status with CMD13 till, we get READY_FOR_DATA response
    //CMD13:Send Status
    SD_Command(CMD13, SHRESP, RCA << 16);
    SD_Response(&response, RESP_R1b);
  }
}


void SD_WriteData(uint8_t *buf, uint32_t cnt) {
  while (cnt>0) {
    //Copy the data to internal buffer
    *pDatBuf=*buf;
    pDatBuf++;
    buf++;
    cnt--;
    BufCnt++;

    if (BufCnt==512) {//Half Buffer is full.
      //Write Buffer to the SD
      SD_WaitTransmissionEnd(); //Wait for any previous transmission
      //Wait for busy.
      //With SPI we can check this by reading the dataline. SD stops holding the data line when it is not busy.
      //However in SDIO, the only way seems to use CMD13.
      //I am going to skip this part assuming that DPSM handles this properly.
      SD_StartBlockTransfer(pDatBuf-512, 512, UM2SD);


      //Switch to other half (if necessary)
      if (pDatBuf==(DatBuf+(512*2))) {pDatBuf=DatBuf;}

      //Reset Buffer counter
      BufCnt=0;
    }
  }
}


static uint32_t SD_Command(uint32_t cmd, uint32_t resp, uint32_t arg) {
  //Response must be:
  //0,2:No response (expect cmdsent) ->NORESP
  //1:Short Response  (expect cmdrend and ccrcfail) ->SHRESP
  //3:Long Response   (expect cmdrend and ccrcfail) ->LNRESP

  //Clear the Command Flags
  SDIO->ICR=(SDIO_STA_CCRCFAIL | SDIO_STA_CTIMEOUT | SDIO_STA_CMDREND | SDIO_STA_CMDSENT);

  SDIO->ARG=arg;  //First adjust the argument (because I will immediately enable CPSM next)
  SDIO->CMD=(uint32_t)(cmd & SDIO_CMD_CMDINDEX) | (resp & SDIO_CMD_WAITRESP) | (0x0400);  //The last argument is to enable CSPM

  //Block till we get a response
  if (resp==NORESP) {
    //We should wait for CMDSENT
    while (!(SDIO->STA & (SDIO_STA_CTIMEOUT | SDIO_STA_CMDSENT))) {};
  }
  else {//SHRESP or LNRESP or R3RESP
    //We should wait for CMDREND or CCRCFAIL
    while (!(SDIO->STA & (SDIO_STA_CTIMEOUT | SDIO_STA_CMDREND | SDIO_FLAG_CCRCFAIL))) {};
  }

  //Check to see if the response is valid
  //We consider all R3 responses without a timeout as a valid response
  //It seems CMDSENT and CMDREND are mutually exlusive. (though I am not sure. Check this later)
  if (SDIO->STA & SDIO_STA_CTIMEOUT) {
    SD_Panic(cmd, "SDIO: Command Timeout Error\n");
  } else if ((SDIO->STA & SDIO_FLAG_CCRCFAIL) && (resp!=R3RESP)) {
    SD_Panic(cmd, "SDIO: Command CRC Error\n");
  }

  return SDIO->STA;
}

static void SD_Panic(uint32_t code, uint8_t *message) {
	uint32_t i;
 // UART_SendLine((char *) message);

  //Block the execution with blinky leds
  while (1) {
    LED_ON(LED_BLUE);
    i=168000000 / 8;
    while(i--){}
    LED_OFF(LED_BLUE);
    i=168000000 / 8;
    while(i--){}
  }
}

static uint32_t SD_Response(uint32_t *response, uint32_t type) {
  //I mainly use this to block the execution in case an unexpected response is received.
  //Actually I don't need this at all. However, just for the sake of extra information I keep this. All I reall need is for this function to return SDIO->RESP1
  //In the main code, I don't use the retun values at all. Perhaps I ought to have used void.

  //R1 Responses
  if ((type==RESP_R1) || (type==RESP_R1b)) {
    *response=SDIO->RESP1;
    if (SDIO->RESP1 & (uint32_t)0xFDFFE008) {   //All error bits must be zero
      SD_Panic(SDIO->RESPCMD, "SDIO:Response Error\n");
    }
    return (*response & 0x1F00)>>8; //Return the card status
  }
  else if (type==RESP_R2) { //CSD or CSI register. 128 bit
    *response++=SDIO->RESP1;
    *response++=SDIO->RESP2;
    *response++=SDIO->RESP3;
    *response=SDIO->RESP4;

    return 0;
  }
  else if (type==RESP_R3) { //OCR
    if (SDIO->RESPCMD != 0x3F) {SD_Panic(SDIO->RESPCMD,"SDIO:Unexpected command index\n");} //CMD index for R3 must be 0x3F
    *response=SDIO->RESP1;  //Equals to OCR
    return 0;
  }
  else if (type==RESP_R6) { //RCA Response
    if (SDIO->RESPCMD != 0x03) {SD_Panic(SDIO->RESPCMD,"SDIO:Unexpected command index\n");} //Only cmd3 generates R6 response
    *response=SDIO->RESP1;  //Equals to OCR

    return (*response)>>16; //Return is equal to RCA. (The first 16 bit is equal to status)
  }
  else {  //RESP_R7:Card Interface condition. Obtained after CMD8
    if (SDIO->RESPCMD != 0x08) {SD_Panic(SDIO->RESPCMD,"SDIO:Unexpected command index\n");} //Only cmd8 generates R7 response
    *response=SDIO->RESP1;
    if ((*response & 0xFF)!=0xAA) {SD_Panic(CMD8, "SDIO:Pattern did not match\n");} //Only cmd8 generates R7 response
    return ((*response) & 0xFF00)>>8; //Echo back value
  }
}

static void SD_StartBlockTransfer(uint8_t *buf, uint32_t cnt, uint32_t dir){
  //cnt must be integer multiple of 512!!! I will enforce this inside this function
  //Starts the actual data tranfer using the DMA.
  //Prior to calling this command. The SDCard must have been adjusted using commands
  uint32_t tempreg;

  //Make cnt an integer multiple of 512
  //Then mask it with the maximum value allowed (2^24)
  cnt=0x01FFFFFF & ((cnt>>8) << 8);


  /////PART I::::Adjust the DMA
  //Reset the control register (0x00 is the default value. this also disables the dma. When EN=0, it stops any ongoing DMA transfer)
  DMA2_Stream3->CR=0;

  //Clear all the flags
  DMA2->LIFCR=DMA_LIFCR_CTCIF3 | DMA_LIFCR_CTEIF3 | DMA_LIFCR_CDMEIF3 | DMA_LIFCR_CFEIF3 | DMA_LIFCR_CHTIF3;

  //Set the DMA Addresses
  DMA2_Stream3->PAR=((uint32_t) 0x40012C80);  //SDIO FIFO Address (=SDIO Base+0x80)
  DMA2_Stream3->M0AR=(uint32_t) buf;    //Memory address

  //Set the number of data to transfer
  DMA2_Stream3->NDTR=0;   //Peripheral controls, therefore we don't need to indicate a size

  //Set the DMA CR
  tempreg=0;
  tempreg|=(0x04<<25) & DMA_SxCR_CHSEL;  //Select Channel 4
  tempreg|=(0x01<<23) & DMA_SxCR_MBURST;  //4 beat memory burst (memory is 32word. Therefore, each time dma access memory, it reads 4*32 bits) (FIFO size must be integer multiple of memory burst)(FIFO is 4byte. Therefore we can only use 4 beat in this case)
  //Note: Ref manual (p173 (the node at the end of 8.3.11) says that burst mode is not allowed when Pinc=0. However, it appears that this is not true at all. Furthermore. when I set pBurst=0, the SDIO's dma control does not work at all.)
  tempreg|=(0x01<<21) & DMA_SxCR_PBURST;  //4 beat memory burst Mode ([Burst Size*Psize] must be equal to [FIFO size] to prevent FIFO underrun and overrun errors) (burst also does not work in direct mode).
  tempreg|=(0x00<<18) & DMA_SxCR_DBM;   //Disable double buffer mode (when this is set, circluar mode is also automatically set. (the actual value is don't care)
  tempreg|=(0x03<<16) & DMA_SxCR_PL;     //Priority is very_high
  tempreg|=(0x00<<15) & DMA_SxCR_PINCOS;  //Peripheral increment offset (if this is 1 and Pinc=1, then Peripheral will be incremented by 4 regardless of Psize)
  tempreg|=(0x02<<13) & DMA_SxCR_MSIZE;  //Memory data size is 32bit (word)
  tempreg|=(0x02<<11) & DMA_SxCR_PSIZE;  //Peripheral data size is 32bit (word)
  tempreg|=(0x01<<10) & DMA_SxCR_MINC;  //Enable Memory Increment
  tempreg|=(0x00<<9) & DMA_SxCR_MINC;  //Disable Peripheral Increment
  tempreg|=(0x00<<8) & DMA_SxCR_CIRC;   //Disable Circular mode
  //tempreg|=(0x00<<6) & DMA_SxCR_DIR;  //Direction 0:P2M, 1:M2P
  tempreg|=(0x01<<5) & DMA_SxCR_PFCTRL; //Peripheral controls the flow control. (The DMA tranfer ends when the data issues end of transfer signal regardless of ndtr value)
  //Bit [4..1] is for interupt mask. I don't use interrupts here
  //Bit 0 is EN. I will set it after I set the FIFO CR. (FIFO CR cannot be modified when EN=1)
  DMA2_Stream3->CR=tempreg;

  //Set the FIFO CR
  tempreg=0x21; //Reset value
  tempreg|=(0<<7); //FEIE is disabled
  tempreg|=(1<<2); //Fifo is enabled (Direct mode is disabled);
  tempreg|=3;   //Full fifo (Fifo threshold selection)
  DMA2_Stream3->FCR=tempreg;

  //Set the Direction of transfer
  if (dir==UM2SD) {
    DMA2_Stream3->CR|=(0x01<<6) & DMA_SxCR_DIR;
  } else if (dir==SD2UM) {
    DMA2_Stream3->CR|=(0x00<<6) & DMA_SxCR_DIR;
  }

  //Enable the DMA (When it is enabled, it starts to respond dma requests)
  DMA2_Stream3->CR|=DMA_SxCR_EN;
  //END of PART I


  ////PART II::::Adjust and enable SDIO Peripheral
  //Clear the Data status flags
  SDIO->ICR=(SDIO_STA_DCRCFAIL | SDIO_STA_DTIMEOUT | SDIO_STA_TXUNDERR | SDIO_STA_RXOVERR | SDIO_STA_DATAEND | SDIO_STA_STBITERR | SDIO_STA_DBCKEND);

  //First adjust the Dtimer and Data length
  SDIO->DTIMER=(uint32_t) DATATIMEOUT;
  SDIO->DLEN=cnt;

  //Now adjust DCTRL (and enable it at the same time)
  tempreg=0;  //Reset value
  tempreg|=(uint32_t) 9 << 4;  //Block size is 512 Compute log2(BlockSize) and shift 4bit
  tempreg|= 1<<3; //Enable the DMA
  tempreg|= 0<<2; //DTMode=Block Transfer (Actualy this is the reset value. Just a remainder)
  tempreg|=(dir & SDIO_DCTRL_DTDIR);  //Direction. 0=Controller to card, 1=Card to Controller
  tempreg|=1; //DPSM is enabled
  //Keep the rest at 0 => OTher SDIO functions is disabled(we don't need them)
  SDIO->DCTRL=tempreg;
  //End of PART II

  //Warn everyone that there may be a transfer in progress
  SDIOTxRx=1;
}


uint8_t SD_WaitTransmissionEnd(void) {
  //This function first checks if there is an ogoing tranmission and block till it ends.
  //It then checks the data flags to see if there is an error. In case of an error it blocks
  //Before the start of data transmission the data flags are all cleared. Therefore, calling this fucntion after a real transmission works as expected.

  ////Check if there is an ongoing transmission
  //Check if the DMA is disabled (SDIO disables the DMA after it is done with it)
  while (DMA2_Stream3->CR & DMA_SxCR_EN) {};
  //Wait for the DMA Interrupt flags if there exist a previous SDIO transfer.
  if (SDIOTxRx) {
    if (DMA2->LISR & (DMA_LISR_TCIF3 | DMA_LISR_TEIF3 | DMA_LISR_DMEIF3 | DMA_LISR_FEIF3)) {
      if (!(DMA2->LISR & DMA_LISR_TCIF3)) {//A DMA error has occured. Panic!
        SD_Panic(DMA2->LISR, "SDIO:DMA Error");
      }
    }
  }

  //Wait till SDIO is not active
  while (SDIO->STA & (SDIO_STA_RXACT | SDIO_STA_TXACT)) {};

  //if there exist a previous transmission, check if the transmission has been completed without error
  if (SDIOTxRx) {
    //I will block here till I get a data response
    while (!(SDIO->STA & (SDIO_STA_DCRCFAIL | SDIO_STA_DTIMEOUT | SDIO_STA_DBCKEND | SDIO_IT_STBITERR))) {};

    if (!(SDIO->STA & SDIO_STA_DBCKEND)) {  //An Error has occured.
      //SD_Panic(SDIO->STA, "SDIO:Data Transmission Error\n");
    	return(1);
    }
  }

  //If we are here, we can be sure that there is no ongoing transmission any more
  SDIOTxRx=0;
  return(0);
}
