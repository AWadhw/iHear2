#include "audio_sd.h"

static FRESULT fres;
static FATFS FatFs;
static FIL fil; 		//File handle

static uint32_t wav_file_size;
static uint8_t first_access = 0;
// 0 - 3   -> "RIFF"                     							{0x52, 0x49, 0x46, 0x46}
// 4 - 7   -> size of the file in bytes  							{data_section size + 36}
// 8 - 11  -> File type header, "WAVE"   							{0x57 ,0x41, 0x56, 0x45}
// 12 - 15 -> "fmt "				     							{0x66, 0x6d, 0x74, 0x20}
// 16 - 19 -> Length of format data                         16		{0x10, 0x00, 0x00, 0x00}
// 20 - 21 -> type of format, pcm is                        1		{0x01 0x00}
// 22 - 23 -> number of channels                            2		{0x02 0x00}
// 24 - 27 -> sample rate,                                  32 kHz	{0x80, 0x7d, 0x00, 0x00}
// 28 - 31 -> sample rate x bps x channels                  19200   {0x00, 0xf4, 0x01, 0x00 }
// 32 - 33 -> bps * channels                                4		{0x04, 0x00}
// 34 - 35 -> bits per sample				                16		{0x10, 0x00}
// 36 - 39 -> "data" 												{0x64, 0x61, 0x74, 0x61}
// 40 - 43 -> size of the data section								{data section size}
//	data
static uint8_t wav_file_header[44]={0x52, 0x49, 0x46, 0x46, 0xa4, 0xa9, 0x03, 0x00, 0x57 ,0x41, 0x56, 0x45, 0x66, 0x6d,
		0x74, 0x20, 0x10, 0x00, 0x00, 0x00, 0x01, 0x00, 0x02, 0x00, 0x80, 0xbb, 0x80, 0x00, 0x00, 0xee, 0x02, 0x00,
		0x04, 0x00, 0x10, 0x00, 0x64, 0x61, 0x74, 0x61, 0x80, 0xa9, 0x03, 0x00};

uint16_t   BitPerSample=16;
uint16_t   NbrChannels=2;
uint32_t   ByteRate=48000*(16/8); //check bluecoin

uint32_t   SampleRate=48000;
uint16_t   BlockAlign= 2 * (16/8);

void set_wav_header() {
	  /* Write chunkID, must be 'RIFF'  ------------------------------------------*/
	wav_file_header[0] = 'R';
	wav_file_header[1] = 'I';
	wav_file_header[2] = 'F';
	wav_file_header[3] = 'F';

	  /* Write the file length ----------------------------------------------------*/
	  /* The sampling time: this value will be be written back at the end of the
	     recording opearation.  Example: 661500 Btyes = 0x000A17FC, byte[7]=0x00, byte[4]=0xFC */
	wav_file_header[4] = 0x00;
	wav_file_header[5] = 0x4C;
	wav_file_header[6] = 0x1D;
	wav_file_header[7] = 0x00;

	  /* Write the file format, must be 'WAVE' -----------------------------------*/
	wav_file_header[8]  = 'W';
	wav_file_header[9]  = 'A';
	wav_file_header[10] = 'V';
	wav_file_header[11] = 'E';

	  /* Write the format chunk, must be'fmt ' -----------------------------------*/
	wav_file_header[12]  = 'f';
	wav_file_header[13]  = 'm';
	wav_file_header[14]  = 't';
	wav_file_header[15]  = ' ';

	  /* Write the length of the 'fmt' data, must be 0x10 ------------------------*/
	wav_file_header[16]  = 0x10;
	wav_file_header[17]  = 0x00;
	wav_file_header[18]  = 0x00;
	wav_file_header[19]  = 0x00;

	  /* Write the audio format, must be 0x01 (PCM) ------------------------------*/
	wav_file_header[20]  = 0x01;
	wav_file_header[21]  = 0x00;

	  /* Write the number of channels, ie. 0x01 (Mono) ---------------------------*/
	wav_file_header[22]  = NbrChannels;
	wav_file_header[23]  = 0x00;

	  /* Write the Sample Rate in Hz ---------------------------------------------*/
	  /* Write Little Endian ie. 8000 = 0x00001F40 => byte[24]=0x40, byte[27]=0x00*/
	wav_file_header[24]  = (uint8_t)((SampleRate & 0xFF));
	wav_file_header[25]  = (uint8_t)((SampleRate >> 8) & 0xFF);
	wav_file_header[26]  = (uint8_t)((SampleRate >> 16) & 0xFF);
	wav_file_header[27]  = (uint8_t)((SampleRate >> 24) & 0xFF);

	  /* Write the Byte Rate -----------------------------------------------------*/
	wav_file_header[28]  = (uint8_t)(( ByteRate & 0xFF));
	wav_file_header[29]  = (uint8_t)(( ByteRate >> 8) & 0xFF);
	wav_file_header[30]  = (uint8_t)(( ByteRate >> 16) & 0xFF);
	wav_file_header[31]  = (uint8_t)(( ByteRate >> 24) & 0xFF);

	  /* Write the block alignment -----------------------------------------------*/
	wav_file_header[32]  = BlockAlign;
	wav_file_header[33]  = 0x00;

	  /* Write the number of bits per sample -------------------------------------*/
	wav_file_header[34]  = BitPerSample;
	wav_file_header[35]  = 0x00;

	  /* Write the Data chunk, must be 'data' ------------------------------------*/
	wav_file_header[36]  = 'd';
	wav_file_header[37]  = 'a';
	wav_file_header[38]  = 't';
	wav_file_header[39]  = 'a';

	  /* Write the number of sample data -----------------------------------------*/
	  /* This variable will be written back at the end of the recording operation */
	wav_file_header[40]  = 0x00;
	wav_file_header[41]  = 0x4C;
	wav_file_header[42]  = 0x1D;
	wav_file_header[43]  = 0x00;

}

void sd_card_init()
{
	//	mounting an sd card
	fres = f_mount(&FatFs, "", 1);
	if(fres != FR_OK)
	{
		myprintf("error in mounting an sd card: %d \n", fres);
		while (fres != FR_OK) {
			fres = f_mount(&FatFs, "", 1);
		}
	}
	else
	{
		myprintf("succeded in mounting an sd card \n");
		set_wav_header();
	}
}

void start_recording(uint32_t frequency)
{
	static char file_name[] = "w_000.wav";
	static uint8_t file_counter = 1; //TODO: check if 10
	int file_number_digits = file_counter;
//	uint32_t byte_rate = frequency * 2 * 2;
//	wav_file_header[24] = (uint8_t)frequency;
//	wav_file_header[25] = (uint8_t)(frequency >> 8);
//	wav_file_header[26] = (uint8_t)(frequency >> 16);
//	wav_file_header[27] = (uint8_t)(frequency >> 24);
//	wav_file_header[28] = (uint8_t)byte_rate;
//	wav_file_header[29] = (uint8_t)(byte_rate >> 8);
//	wav_file_header[30] = (uint8_t)(byte_rate >> 16);
//	wav_file_header[31] = (uint8_t)(byte_rate >> 24);

	// defining a wave file name
	file_name[4] = file_number_digits%10 + 48; //48 is digit 0
	file_number_digits /= 10;
	file_name[3] = file_number_digits%10 + 48;
	file_number_digits /= 10;
	file_name[2] = file_number_digits%10 + 48;
	printf("file name %s \n", file_name);
	myprintf("file name %s \n", file_name);
	file_counter++;

	// creating a file
	fres = f_open(&fil ,file_name, FA_WRITE|FA_CREATE_ALWAYS);
	if(fres != 0)
	{
		myprintf("error in creating a file: %d \n", fres);
	}
	else
	{
		myprintf("succeeded in opening a file \n");
	}
	wav_file_size = 0;
}

void dump_audio_content(uint8_t *data, uint16_t data_size){
	 //some variables for FatFs

	uint32_t temp_number;
	printf("w\n");
	if(first_access == 0) {
		for(int i = 0; i < 44; i++){
			*(data + i) = wav_file_header[i];
		}
		first_access = 1;
	}
	 //Now let's try to open file "audio.txt"
	//fres = f_open(&fil, "audio.raw", FA_WRITE | FA_OPEN_ALWAYS | FA_CREATE_ALWAYS);
//	if(fres == FR_OK) {
//	myprintf("I was able to open 'audio.txt' for writing\r\n");
//	} else {
//	myprintf("f_open error In dump_audio :( (%i)\r\n", fres);
//	}
	fres = f_write(&fil,(void *)data, data_size,(UINT*)&temp_number);
//	for (int i = 0; i < 20; i++) {
//		myprintf("My data is: %x", (*data));
//	}
	if(fres != FR_OK) {
	//myprintf("Wrote %i bytes to '.lav'!\r\n", temp_number);
	myprintf("f_write error while dumping (%i)\r\n", fres);
	}

	wav_file_size += data_size;

	//f_close(&fil);
}

void stop_recording() {
	uint16_t temp_number;
		// updating data size sector
//	wav_file_size -= 8;
//	wav_file_header[4] = (uint8_t)wav_file_size;
//	wav_file_header[5] = (uint8_t)(wav_file_size >> 8);
//	wav_file_header[6] = (uint8_t)(wav_file_size >> 16);
//	wav_file_header[7] = (uint8_t)(wav_file_size >> 24);
//	wav_file_size -= 36;
//	wav_file_header[40] = (uint8_t)wav_file_size;
//	wav_file_header[41] = (uint8_t)(wav_file_size >> 8);
//	wav_file_header[42] = (uint8_t)(wav_file_size >> 16);
//	wav_file_header[43] = (uint8_t)(wav_file_size >> 24);
	wav_file_header[4] = (uint8_t)(wav_file_size);
	wav_file_header[5] = (uint8_t)(wav_file_size >> 8);
	wav_file_header[6] = (uint8_t)(wav_file_size >> 16);
	wav_file_header[7] = (uint8_t)(wav_file_size >> 24);
	  /* Write the number of sample data -----------------------------------------*/
	  /* This variable will be written back at the end of the recording operation */
	wav_file_size -=44;
	  wav_file_header[40] = (uint8_t)(wav_file_size);
	  wav_file_header[41] = (uint8_t)(wav_file_size >> 8);
	  wav_file_header[42] = (uint8_t)(wav_file_size >> 16);
	  wav_file_header[43] = (uint8_t)(wav_file_size >> 24);
	  /* Return 0 if all operations are OK */
	  //return 0;

	// moving to the beginning of the file to update the file format
	f_lseek(&fil, 0);
	f_write(&fil,(void *)wav_file_header, sizeof(wav_file_header),(UINT*)&temp_number);
	if(fres != FR_OK)
	{
		printf("error in updating the first sector: %d \n", fres);
//		while(1);
	}

	f_close(&fil);
	first_access = 0;
	myprintf("Closing file now....");
}

void sd_demo(void) {
//	  	myprintf("\r\n~ SD card demo by Azaan~\r\n\r\n"); //TODO: check, it is also defined in main
//
//	    HAL_Delay(1000); //a short delay is important to let the SD card settle
//
//	    //some variables for FatFs
//	    FATFS FatFs; 	//Fatfs handle
//	    FIL fil; 		//File handle
//	    //FRESULT fres; //Result after operations
//
//	    //Open the file system
//	    fres = f_mount(&FatFs, "", 1); //1=mount now
//	    if (fres != FR_OK) {
//	  	myprintf("f_mount error (%i)\r\n", fres);
//	  	while(1);
//	    }

	    //Let's get some statistics from the SD card
	    DWORD free_clusters, free_sectors, total_sectors;

	    FATFS* getFreeFs;

	    fres = f_getfree("", &free_clusters, &getFreeFs);
	    if (fres != FR_OK) {
	  	myprintf("f_getfree error (%i)\r\n", fres);
	  	while(1);
	    }

	    //Formula comes from ChaN's documentation
	    total_sectors = (getFreeFs->n_fatent - 2) * getFreeFs->csize;
	    free_sectors = free_clusters * getFreeFs->csize;

	    myprintf("SD card stats:\r\n%10lu KiB total drive space.\r\n%10lu KiB available.\r\n", total_sectors / 2, free_sectors / 2);

	    //Now let's try to open file "test.txt"
	    fres = f_open(&fil, "test.txt", FA_READ);
	    if (fres != FR_OK) {
	  	myprintf("f_open error (%i)\r\n");
	  	while(1);
	    }
	    myprintf("I was able to open 'test.txt' for reading!\r\n");

	    //Read 30 bytes from "test.txt" on the SD card
	    BYTE readBuf[30];

	    //We can either use f_read OR f_gets to get data out of files
	    //f_gets is a wrapper on f_read that does some string formatting for us
	    TCHAR* rres = f_gets((TCHAR*)readBuf, 30, &fil);
	    if(rres != 0) {
	  	myprintf("Read string from 'test.txt' contents: %s\r\n", readBuf);
	    } else {
	  	myprintf("f_gets error (%i)\r\n", fres);
	    }

	    //Be a tidy kiwi - don't forget to close your file!
	    f_close(&fil);

	    //Now let's try and write a file "write.txt"
	    fres = f_open(&fil, "write.txt", FA_WRITE | FA_OPEN_ALWAYS | FA_CREATE_ALWAYS);
	    if(fres == FR_OK) {
	  	myprintf("I was able to open 'write.txt' for writing\r\n");
	    } else {
	  	myprintf("f_open error (%i)\r\n", fres);
	    }

	    //Copy in a string
	    strncpy((char*)readBuf, "a new file is made!", 19);
	    UINT bytesWrote;
	    fres = f_write(&fil, readBuf, 19, &bytesWrote);
	    if(fres == FR_OK) {
	  	myprintf("Wrote %i bytes to 'write.txt'!\r\n", bytesWrote);
	    } else {
	  	myprintf("f_write error (%i)\r\n");
	    }

	    //Be a tidy kiwi - don't forget to close your file!
	    f_close(&fil);

	    //We're done, so de-mount the drive
	    f_mount(NULL, "", 0);
}

