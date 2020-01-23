# merge's in CYACD file with binary file

import sys
import os
import binascii
import struct

from intelhex import IntelHex
from io  import StringIO

CYPD2104_20FNXIT  = "140111A4"
CYPD2122_20FNXIT = "140611A4"

meta_identifier = ":0200000490303A"
ihex_eof = ":00000001FF"	
	
CFG_TABLE_CHECKSUM = 0x8
CFG_TABLE_START = 0xA
CFG_TABLE_END = 0x200

# As per 001-96533 Table 2-1
CYPD1XXX_HEXFILEVER	=		0x1
CYPD2XXX_HEXFILEVER =		0x2

HEXFILEVERSION = CYPD2XXX_HEXFILEVER
DEVICETYPELOCATION = 0x132

CHIP_PROTECT = 1
FLASH_PROTECT = 1

def twos_comp(val):
    val = 0xFF & ((val ^ 0xFFFFFFFF)+1)
    return val      

def updateCfgCsum( config_table_addr, b ):
    csum = 0
    
    for i in range (config_table_addr+CFG_TABLE_START, config_table_addr+CFG_TABLE_END ):
        csum += b[i]
	
    # checksum is twos complement of the sum of the array BYTE size
    b[config_table_addr+CFG_TABLE_CHECKSUM] = twos_comp(csum)

def calcChecksum( b ):
    checksum = 0
    
    for c in b:
        checksum += c

    checksum &= 0xFFFFFFFF
    return checksum


def makedataline(ba_data, addr, rec_type):
    s = [':']
    
    csum = 0
    l = len(ba_data)
    
    # add len
    s.append(str("%02x" % l))
    csum += l
    
    # add address
    s.append("0000")
    csum += 0x00  # so I don't forget 
    csum += 0x00
    
    # add rec type
    s.append("00")
    csum += 0x00
    
    for c in ba_data:
        s.append( str("%02x" % c) )        
        csum += c

    csum = twos_comp(csum)
    s.append( str("%02x" % csum) )        
    #print(s)
    
    stro = ''.join(s)

    return(stro)    


def createDeviceIdRecord( checksum, deviceid ):
    devicecheck = 0

    b = bytearray(10)

    # undocummented value Internal Use Table 2-1 pf 001-96533 is just the checksum(32bit version) + the device_ID
    devicecheck = (checksum + deviceid)  & 0xFFFFFFFF
    devicecheck = devicecheck.to_bytes(4, byteorder='big')

    #convert endianess
    deviceid = deviceid.to_bytes(4, byteorder='big')

    # LOC[0:1] "hex file version" different for CYPD1XXX vs CYPD2XXX 
    b[1]= HEXFILEVERSION

    # LOC[2:5] Target Silicon ID
    #print(len(deviceid))
    b[2:5] = deviceid

    # LOC[6:7] Reserved
    	
    # LOC[8:B] Internal use. See above
    b[8:0xB] = devicecheck
    
    return (b)       

def main():
    if len(sys.argv) < 3:
        print("Switches a CCG2 device to another")
        print("Usage: %s <hex file> <device name ie. CYPD2104-20FNXIT or CYPD2122-20FNXIT>")
        sys.exit()
        
    hex_file = sys.argv[1]
    devicetype = sys.argv[2]
    
    if "CYPD2104-20FNXIT" in devicetype:
        deviceid = int(CYPD2104_20FNXIT, 16)
    elif "CYPD2122-20FNXIT" in devicetype:
        deviceid =  int(CYPD2122_20FNXIT, 16)
    else:
        print("%s is not a recognized device type" % devicetype)
        sys.exit()            
        
    if not os.path.isfile(hex_file):
        print("hex file path %s does not exist. Exiting..." %  (hex_file))
        sys.exit()
    else:
        print("hex file: " + hex_file)       

    with open(hex_file, mode='r') as hfp:
        hf = hfp.read()
        
        # break off hex file from meta data
        hexdatas = hf.split(meta_identifier, 1)
        hexdata = hexdatas[0]
        metadata = meta_identifier + hexdatas[1]
        #print(metadata)

    # can't do it with ihex without a file, make one
    with open("tmp_123456", mode='w') as tfp:    
        tfp.write(hexdata)
    
    ih = IntelHex("tmp_123456")
    b = ih.tobinstr()
            
    # calculate the binary checksum
    csum = calcChecksum( b )
    csum_short = csum & 0xFFFF
    
    # Create the device record meta data
    DiDR = createDeviceIdRecord( csum, deviceid )
    
    # no need to create it since we already have it stored in their method
    hexstr = hexdata
    """
    # create the hex file string
    sio = StringIO()
    ihex = IntelHex()
    ihex.start_addr = None
    ihex.padding = 0x00
    
    # ihex failed to decode a character, so doing it one by one
    for i, c in enumerate(b):
        ihex[i] = c
    
    # write out the ihex string 
    ihex.write_hex_file( sio )
    hexstr = sio.getvalue()
    sio.close()
    
    # remove the EOF so we can add the cypress metadata
    hexstr = hexstr[:hexstr.rfind(ihex_eof)]
    """
    
    # metadata stoarge    
    suffix = []
    
    # add checksum short
    suffix.append(":0200000490303A")
    suffix.append(makedataline(csum_short.to_bytes(2, byteorder='big'), 0, 0))
    
    # metadata
    suffix.append(":0200000490402A")
    if FLASH_PROTECT:
        suffix.append(":20000000FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF7F80")
    else:        
        suffix.append(":200000000000000000000000000000000000000000000000000000000000000000000000E0")
    
    # Device id record
    suffix.append(":0200000490501A")
    suffix.append( makedataline( DiDR, 0, 0 ) )
    
    # chip level protection
    suffix.append(":0200000490600A")
    if CHIP_PROTECT:
        suffix.append(":0100000002FD")
    else:
        suffix.append(":0100000001FE")        
    
    # EOF
    suffix.append(ihex_eof)
    
    tail = '\n'.join(suffix)
    
    hexstr = hexstr + tail.upper()

    fn = devicetype + "_converted.hex"            
    with open(fn, mode='w') as ofp:
        ofp.write(hexstr)
        print("device switched into %s" % (fn))

if __name__ == '__main__':
    main()