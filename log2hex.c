#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <strings.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>

#define restrict __restrict__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

#define TITLESTRING					"\n\n(log2hex)\nUtility for modifying Cypress EZ-PD(tm) devices using the MiniProg3 via SWD\n"
#define VERSION						2.2
#define COMPANY						"HYPOXIC"

#define VERSIONSTRING				"v" STR(VERSION) " " COMPANY
#define SPLASHSTRING 				TITLESTRING VERSIONSTRING "\n\n"

// Device defines
#define CYPD2103_20FNXI   0x140011A4
#define CYPD2104_20FNXI   0x140111A4
#define CYPD2105_20FNXI   0x140211A4
#define CYPD2103_14LHXI   0x140311A4
#define CYPD2122_24LQXIT  0x140411A4
#define CYPD2134_24LQXIT  0x140511A4
#define CYPD2122_20FNXIT  0x140611A4
#define CYPD2123_24LQXIT  0x140711A4
#define CYPD2124_24LQXIT  0x140811A4
#define CYPD2119_24LQXIT  0x140911A4
#define CYPD2121_24LQXIT  0x141011A4
#define CYPD2125_24LQXIT  0x141111A4
#define CYPD2120_24LQXIT  0x141211A4
#define CYPD2120_20FNXIT  0x141311A4
#define CYPD2134_16SXI    0x141411A4

// As per 001-96533 Table 2-1
#define CYPD1XXX_HEXFILEVER			0x1
#define CYPD2XXX_HEXFILEVER			0x2

//-----------------------------------------------------------
// Change as per your application
//-----------------------------------------------------------
#define HEXFILEVERSION				CYPD2XXX_HEXFILEVER
#define DEVICE_ID					CYPD2122_20FNXIT
//#define DEVICE_ID					CYPD2104_20FNXI
#define DEVICETYPELOCATION			0x132U	

#define OUTPUT_NAME "extracted"
#define OUTPUT_BIN	"extracted.bin"
#define OUTPUT_HEX	"extracted.hex"

// Blank Metadata
const char metadata[] = \
	":0200000490402A\n"\
	":200000000000000000000000000000000000000000000000000000000000000000000000E0\n";
	
// Set Chip level protection to open	
const char chiplevelprotection[] = \
	":0200000490600A\n"\
	":0100000001FE\n";

// The 512bytes block of the application is the configuration data. 
// It has it's own checksum which will reset to the bootloader if the data is bad. 
// It is located at 0x8 offset and adds from 0xA to end of configuration.
#define CYPD_START_ADDRESS			0x0000
#define CFG_TABLE_CHECKSUM			0x8U
#define CFG_TABLE_START				0xAU
#define CFG_TABLE_END				0x200U

// Identifier for start of flash output from MiniProg3
#define START_TEXT "--- User's Flash Area ---"
#define SIXTYFOURK	0x8000U

#define IHEX_MAX_OUTPUT_LINE_LENGTH 0x40
#include "kk_ihex_write.h"

// Locals
int locatestart(FILE *f);
void PrintData(const unsigned char *c, size_t l);
int getlinedata(FILE *f, unsigned char *buff, int *bindex, int *address);
int createhex(unsigned char *binbuff, int size,unsigned int checksum);

// Global for ihex functionality
FILE *outfile;

/*
 * Adds up each byte from a buffer of len
 */
unsigned int CalcChecksum(unsigned char *buff, size_t len){
	unsigned int bindex;
	// determine checksum 
	int checksum=0;
	
	for(bindex=0;bindex<(unsigned int)len;bindex++){
		checksum+= buff[bindex];
	}
	
	return checksum;
}

/*
 * The configuration array has its own checksum. 
 * Since we want to change this, we have to recalculate it before burning.
 */
void updatecfgcsum(unsigned char *b, int startaddr){
	int i;
	unsigned int csum=0;
	 
	for(i=startaddr+CFG_TABLE_START;i<startaddr+CFG_TABLE_END;i++){
		csum += b[i];
	}
	
	//printf("config checksum is %x neg %x\n",csum,~csum+1);
	// checksum is twos complement of the sum of the array BYTE size
	b[startaddr+CFG_TABLE_CHECKSUM] = (0xFFU)&~csum+1;
}

/*
 * Loads in the passed bootloader into the buffer from start to maxlen
 */
int loadboot(const char *filename, unsigned char *b, int maxlen)  {
	struct stat info;
	int len;

	if (stat(filename, &info) != 0) {
	    printf("bootloader not found!\n");
	}   
	
	len = info.st_size; 
	
	printf("bootloader FILE SIZE: %lu\n", len);
	
	char *content = malloc(len);
	if (content == NULL) {
	    printf("mem\n");
	}   
	
	FILE *fp = fopen(filename, "rb");
	if (fp == NULL) {
	    printf("file open error\n");
	}
	
	/* Try to read a single block of info.st_size bytes */
	size_t blocks_read = fread(content, len, 1, fp);
	
	if (blocks_read != 1) {
		printf("nothing read\n");
	}
	
	fclose(fp);
	
	// truncate the data, otherwise, we expect the buffer to be cleared 
	if(len>maxlen)
		len=maxlen;
	
	// now copy it over
	memcpy(b,content,len);
	
	//printf("bootloader merged in: %lu\n", (unsigned long)len);
}

/*
 * MAIN
 */
int main(int argc, char *argv[]) {
	FILE 			*f,*fo,*fboot;
	int				bindex=0, address=0,r, startaddr=-1,len=-1;
	bool			invalid=true,verbose=false;
	unsigned char 	*buff,*bread;
	unsigned int 	Checksum;
	int devicetype = -1;
	
	//verbose=true;	
	
	printf(SPLASHSTRING);
	
	if(argc < 2){
		printf("usage: log2hex <logfile> <bootloader> <GoPro device id ie. bacpac=0x1001, garter=0x2>\n  bootloader is optional if bootloader is code protected. device id change is optional too");
		return -1;
	}
	
	if(DEVICE_ID == CYPD2122_20FNXIT)
	    printf("built for CYPD2122_20FNXIT");
	else
	    printf("built for CYPD2104_20FNXI"); 
	    
	if(argc >= 4){
		devicetype = strtol(argv[3], NULL, 16);
		devicetype &= 0xFFFF;
		printf("Device Type changed to %X\n",devicetype);
	}	
		
	f = fopen(argv[1],"r");
			
	if(f == 0){
		printf("file %s not found\n",argv[1]);
		return -1;
	}
	
	fo = fopen(OUTPUT_BIN,"wb");
		
	if(fo == 0){
		printf("file %s could not be created\n");
		return -1;
	}
			
	printf("Log file from MiniProg3 %s opened\n",argv[1]);
	
	if(locatestart(f) <0){
		printf("user flash space not found!\n");
		return -1;	
	}
	
	// create the buffer
	bread = (unsigned char*)(calloc(SIXTYFOURK,sizeof(unsigned char)));
	buff = (unsigned char*)(calloc(SIXTYFOURK,sizeof(unsigned char)));
	
	if(bread == NULL || buff == NULL){
		printf("mem\n");
		return -1;	
	}
		
	// now get data from the text log file and read it in binary
	do{
		if((r = getlinedata(f,bread,&bindex,&address)) == 0 && invalid){
			invalid = false;
			startaddr = address;
			printf("Binary starts at 0x%04x\n",startaddr);	
		}
	}while(r >= 0);
	
	len = address+0x10;  // add in the last row
	
	// Merge in the bootloader. Trims at startaddr
	if(argc > 2)
	    loadboot(argv[2],buff,startaddr);
	
	// Merge in application binary
	memcpy(&buff[startaddr],bread,len-startaddr);
		
	// Now make the device type change. This is specific to the device we're modifying VDM SVID0
	// change DEVICETYPELOCATION for your device
	if(devicetype >= 0){
		printf("Device Type at 0x%X Changing from %x to %x\n",startaddr+DEVICETYPELOCATION,(unsigned short)buff[startaddr+DEVICETYPELOCATION],(unsigned short)devicetype);
		buff[startaddr+DEVICETYPELOCATION] = (unsigned char)devicetype;
		buff[startaddr+DEVICETYPELOCATION+1] = (unsigned char)(devicetype>>8);
	}	
	
	// Now recalculate the config area checksum, and write back into the image
	updatecfgcsum(buff,startaddr);
		
	if(verbose){
		printf("buffer of len 0x%x:\n",len+startaddr);
		PrintData(buff, len);
	}
	
	free(bread);

	// Now calculate the image checksum for MiniProg3 to program correctly
	Checksum = CalcChecksum(buff,len);

	// Build the hexfile
	if(createhex(buff,len,Checksum) < 0)
		printf("creating hex failed\n");
	
	// Write out the binary, useful for debugging 
	if(fwrite(	buff, len, 1, fo) != 1)
		printf("failed to write\n");
	
	free(buff);
	fclose(f);
	fclose(fo);
	
	printf("%s created successfully with checksum %04X\n",OUTPUT_HEX,Checksum);
}

// Endian adjustments (word)
void SwapWordEndian(unsigned int* b){
	unsigned char t=((unsigned char *)b)[0];
	
	((unsigned char *)b)[0] = ((unsigned char *)b)[1];
	((unsigned char *)b)[1] = t;
}

// Endian adjustments (int)
void SwapIntEndian(unsigned int* b){
	unsigned int t=0;
	
	((unsigned char *)&t)[0] = ((unsigned char *)b)[3];	
	((unsigned char *)&t)[1] = ((unsigned char *)b)[2];	
	((unsigned char *)&t)[2] = ((unsigned char *)b)[1];	
	((unsigned char *)&t)[3] = ((unsigned char *)b)[0];	
	*b = t;
}

/* addDeviceIdRecord
 * The undocummented device id record creation
 *  
*/
void addDeviceIdRecord(struct ihex_state * const ihex,
	unsigned int checksum )  {
	unsigned int deviceid;	
    unsigned int devicecheck;
    unsigned char b[100];
    
    memset(b,0,sizeof(b));
    
    deviceid = DEVICE_ID;	
    
    // undocummented value Internal Use Table 2-1 pf 001-96533 is just the checksum(32bit version) + the device_ID
    devicecheck = checksum + DEVICE_ID;
    
    // now swap the endians
    SwapIntEndian(&devicecheck);
	SwapIntEndian(&deviceid);
	
	// LOC[0:1] "hex file version" different for CYPD1XXX vs CYPD2XXX 
	b[1]=HEXFILEVERSION;
	
	// LOC[2:5] Target Silicon ID
	memcpy(&b[2],&deviceid,4);
	
	// LOC[6:7] Reserved
	
	// LOC[8:B] Internal use. See above
	memcpy(&b[8],&devicecheck,4);
	    
    ihex_write_extended_address (ihex,0x9050,IHEX_EXTENDED_LINEAR_ADDRESS_RECORD);
	ihex_write_at_address(ihex, 0);
		
	// write in the data	
	ihex_write_bytes(ihex, b, 12);
	ihex_flush(ihex); // flush
}
	
/*
 * Creates the hex file based upon the passed binary. Creates the outband checksum, metadata, device identifier and security fields
*/
int createhex(unsigned char *binbuff, int size,unsigned int checksum){
	struct ihex_state ihex;
	unsigned int checksum_short = checksum & 0xFFFFUL;
	unsigned int devicecheck=0;
	outfile = fopen(OUTPUT_HEX,"wb");
	
	if(outfile==0){
		printf("hex %s not created\n");
		return -1;
	}
	
	// setup the hex file
	ihex_init(&ihex);
    ihex_set_output_line_length(&ihex, IHEX_MAX_OUTPUT_LINE_LENGTH);
    
    // write the data stream from the passed binary stream
    ihex_write_at_address(&ihex, CYPD_START_ADDRESS);  // should be zero
    ihex_write_bytes(&ihex, binbuff, size);
    ihex_flush(&ihex); // flush
    
    // Now add on the checksum for cypress meta data
	ihex_write_extended_address (&ihex,0x9030,IHEX_EXTENDED_LINEAR_ADDRESS_RECORD);
	ihex_write_at_address(&ihex, 0);
	SwapWordEndian(&checksum_short);
	ihex_write_bytes(&ihex, &checksum_short, 2);
    ihex_flush(&ihex); // flush
    
    // Now put the metadata
    fputs(metadata, outfile);

	// Add their top secret device id / second checksum    
	addDeviceIdRecord(&ihex,checksum);
	
    // Add chiplevelprotection
    fputs(chiplevelprotection, outfile);
    
    // EOF hex
	ihex_end_write(&ihex);

    fclose(outfile);
    
    return 0;
}

/* called by ihex */
void
ihex_flush_buffer(struct ihex_state *ihex, char *buffer, char *eptr) {
	(void) ihex;
    *eptr = '\0';
    (void) fputs(buffer, outfile);
}


/*
 * Grabs the next line from the text log file output from the MiniProg3
*/
int getlinedata(FILE *f, unsigned char *buff, int *bindex, int *address){
	char temp[512],*p;
	char dstr[3];
	bool invalid=false;
	int data,i,s;
	
	if(fgets(temp, sizeof(temp), f) == NULL)
		return -1;
	
	s=strlen(temp);
	if(s < 74){ 
		return -1;
	}
		
	p = strstr(temp, "|");
	if(p == 0)
		return -1;
		
	//fast forward to the address
	p+=2;
	
	// get the string
	sscanf(p,"%x",address);
	
	p+=5; // fast forward to data
	
	for(i=0;i<16;i++){
		sscanf(p,"%02s ",dstr);	
	
		if(strcmp(dstr,"xx")==0){
			data =0; // don't cares
			invalid = true;
		}
		else{
			data = strtoul (dstr, NULL, 16);
			
			p+=3;
			buff[(*bindex)++]=(unsigned char)(data&0xFF);
		}
	}
	
	if(invalid)
		return 1;
	else
		return 0;
}

/*
*/
int locatestart(FILE *f){
	char temp[512];
	int find_result=0;
	
	while(fgets(temp, sizeof(temp), f) != NULL && find_result == 0) {
		if((strstr(temp, START_TEXT)) != NULL) {
			find_result++;
		}
	}
	
	if(!find_result){
		return -1;	
	}
	else 
		return 0;	
}

/*
 * debug function
*/
void PrintData(const unsigned char *c, size_t l){
	size_t i;

	for(i=0;i<l;i++){
	    if(i!=0){ 
   	    	if(!(i % 16))
	    		putchar('\n');
		}
		printf("%02x ", c[i]);
	}
	    
	putchar('\n');    
}
