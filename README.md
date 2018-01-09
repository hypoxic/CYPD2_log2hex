# CYPD2_log2hex
Tool for reading a CCG2 CYPRESS PD device via a MiniProg3 through SWD and creating a hex file to reprogram the device. Allows to change parameters as it has the ability to recalculate the various checksums throughout the hex file.
This tool was made to be able to do research on devices you own. Not liable for improper use. 

## Notes
This assumes the bootloader is code protected, may need to change the code if it's not. 
The following may need to be changed depending on your product. 
```
#define HEXFILEVERSION				CYPD2XXX_HEXFILEVER
#define DEVICE_ID					CYPD2122_20FNXIT
#define DEVICETYPELOCATION			0x132U	
```

## USAGE
1. Connect the device via SWD and use the read via MINIPROG3 program interface.
2. Right click on the output and save the log file
3. Locate a bootloader in Cypress's examples such as CYPD2122-24LQXI-mobile_i2c_boot-nb_1_0_0_699_0_0_0_nb.hex. Remove the outband data at the end of the hex file. Leave the EOF identifier ":00000001FF"
```
REMOVE: 
:0200000490303A
:02000000A2025A
:0200000490402A
:200000000000000000000000000000000000000000000000000000000000000000000000E0
:0200000490501A
:0C0000000002140411A400001408B3A6B0
:0200000490600A
:0100000001FE
:00000001FF  <--- Leave
```
4. Use ibin2hex on this file to create the binary representation of the bootloader. You may need to compile this for your operating system from [arkku/ihex] (https://github.com/arkku/ihex).
5. Run `log2hex.exe <logfile from MiniProg3 text> <bootloader binary.bin> <SVID product type>` ie. `log2hex.exe read_parakeet.txt noteboot_i2c_boot.bin 0x1001`
6. Verify with MiniProg3 and your should see failure for the protected bootloader region. Everything else should pass. 
7. Program it!

