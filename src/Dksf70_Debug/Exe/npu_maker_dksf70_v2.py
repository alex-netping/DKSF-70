#!python
# -*- coding: cp1251 -*-

# 12.02.2015
#   Enchanced automated version
# 30.04.2015
#   intelhex 2.0 used

"""
dksf 70
FW 258048  4096
MW 258048  262144
"""

fw_length = 0x3f000 
fw_offset = 0x01000

res_length = 0x3f000
res_offset = 0x40000

jsstart =  '75hd95kuDbvf8y3k'
fwstart =  '85nxGT50Df31Pjhg'
fwend =    '9Bf782nHf760Dsa3'
resstart = 'Mnh73f0QgRt20w31'
resend =   'Po922BfSe0nm7F57'
#  start of version info in the HEX firmware file
verdatastart = '6Gkj0Snm1c9mAc55' 


SEED = 0x4C7F
POLY = 0xA001

def crc16(buf):
    crc = SEED
    for c in buf:
        data=ord(c)
        for n in xrange(8):
            y = (crc ^ data) & 1
            crc = (crc>>1) & 0xffff
            if y != 0:
                crc ^= POLY
            data>>=1
        pass
    pass
    return crc & 0xffff

import glob, sys, struct, os
from intelhex import IntelHex, AddressOverlapError

hexfnlist = glob.glob('./dksf*.hex')
if len(hexfnlist) == 0:
    raise ApplicationError, 'No any DKSF*.hex files found!'
hexfn = sorted(hexfnlist, key=os.path.getmtime)[-1]
print
print os.path.abspath(hexfn)

bootfn = glob.glob('..\\..\\Bootloader_Debug\\Exe\\*.hex')
if len(bootfn) != 1:
    raise ApplicationError, 'More than one bootloader .hex file found!'
bootfn = os.path.abspath(bootfn[0])
print bootfn

ih = IntelHex(hexfn)
blih = IntelHex(bootfn)

ih.padding = 0xff
code = ih.tobinstr(start=fw_offset, end=fw_offset+fw_length-1)
res = ih.tobinstr(start=res_offset, end=res_offset+res_length-1)

ver_data_offset = code.index(verdatastart) + len(verdatastart)
model, version, subversion, litera, assmcode =\
    struct.unpack_from('<HHHcB', code, ver_data_offset)
ver = str(model) + '.' + str(version) + '.' + str(subversion) + '.' + litera + '-' + str(assmcode)
print 'Extracted version data:', ver

print 'Merging bootloader file...',
ih.start_addr = None # program start addr record will be from BL part
try:
    ih.merge(blih, overlap='error')
    print 'Ok'
except AddressOverlapError:
    print ' probably already merged, data overlap! CHECK THIS!'
finally:
    hx_fn = 'DKSF_' + ver + '_HX.hex'
    print 'Writing', hx_fn, '...',
    ih.write_hex_file(hx_fn)
    print 'Ok'

npf = ''
npf += fwstart
npf += code 
npf += fwend
npf += resstart
npf += res
npf += resend
npf += jsstart

npf += """function fw_is_updated(v) { return v == 'v%d.%d.%d.%c-%d'; }
""" % (model, version, subversion, litera.upper(), assmcode)

npf += """function fw_is_compatible() { return fwver.split('.')[0] == 'v%d'; }
""" % (model)

npf += """fw_codecrc=0x%x;
fw_rescrc=0x%x;
""" % (crc16(code), crc16(res))

npu_fn = 'DKSF_' + ver + '.npu'
print 'Writing', npu_fn, '...',
open(npu_fn,'wb').write(npf)
print 'Ok\n'
