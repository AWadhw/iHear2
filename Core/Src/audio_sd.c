#include "audio_sd.h"

static FRESULT fres;
static FATFS FatFs;
static FIL fil; 		//File handle

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
		0x74, 0x20, 0x10, 0x00, 0x00, 0x00, 0x01, 0x00, 0x02, 0x00, 0x80, 0x7d, 0x00, 0x00, 0x00, 0xf4, 0x01, 0x00,
		0x04, 0x00, 0x10, 0x00, 0x64, 0x61, 0x74, 0x61, 0x80, 0xa9, 0x03, 0x00};

void sd_card_init()
{
	//	mounting an sd card
	fres = f_mount(&FatFs, "", 1);
	if(fres != 0)
	{
		printf("error in mounting an sd card: %d \n", fres);
		while(1);
	}
	else
	{
		printf("succeded in mounting an sd card \n");
	}
}

void start_recording(uint32_t frequency)
{
	static char file_name[] = "w_000.lav";
	static uint8_t file_counter = 1; //TODO: check if 10
	int file_number_digits = file_counter;
	uint32_t byte_rate = frequency * 2 * 2;
	wav_file_header[24] = (uint8_t)frequency;
	wav_file_header[25] = (uint8_t)(frequency >> 8);
	wav_file_header[26] = (uint8_t)(frequency >> 16);
	wav_file_header[27] = (uint8_t)(frequency >> 24);
	wav_file_header[28] = (uint8_t)byte_rate;
	wav_file_header[29] = (uint8_t)(byte_rate >> 8);
	wav_file_header[30] = (uint8_t)(byte_rate >> 16);
	wav_file_header[31] = (uint8_t)(byte_rate >> 24);

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
		while(1);
	}
	else
	{
		myprintf("succeeded in opening a file \n");
	}
	//wav_file_size = 0;

}

void dump_audio_content(uint8_t *data, uint16_t data_size){
	 //some variables for FatFs

	uint32_t temp_number;
	printf("w\n");
//	if(first_access == 0) {
//		for(int i = 0; i < 44; i++){
//			*(data + i) = wav_file_header[i];
//		}
//		first_access = 1;
//	}
	 //Now let's try to open file "audio.txt"
	//fres = f_open(&fil, "audio.raw", FA_WRITE | FA_OPEN_ALWAYS | FA_CREATE_ALWAYS);
//	if(fres == FR_OK) {
//	myprintf("I was able to open 'audio.txt' for writing\r\n");
//	} else {
//	myprintf("f_open error In dump_audio :( (%i)\r\n", fres);
//	}
	fres = f_write(&fil,(void *)data, data_size,(UINT*)&temp_number);

	if(fres == FR_OK) {
	myprintf("Wrote %i bytes to '.lav'!\r\n", temp_number);
	} else {
	myprintf("f_write error (%i)\r\n");
	}

	//f_close(&fil);
}

void stop_recording() {
//	if(fres != 0)
//	{
//		printf("error in updating the first sector: %d \n", fres);
//		while(1);
//	}
	myprintf("Closing file now....");
	f_close(&fil);
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

