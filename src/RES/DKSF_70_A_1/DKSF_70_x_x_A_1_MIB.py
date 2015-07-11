#!python
# -*- coding: cp1251 -*-

output_file = '../../mib_tree_dksf70.c'


## MIB дерево DKSF 70.1.x.A-1

oid_list = [

##  id	OID		                                                     SNMP представление	RW	

(0x0314, '.1.3.6.1.2.1.1.1.0'),			## SNMP_HANDLER2	R	SYS_DESCR	255		Ru: Устройство En:Device 
(0x0332, '.1.3.6.1.2.1.1.2.0'),			## SNMP_HANDLER15	R	SYS_OBJECT_ID	8	Ru: En: 
(0x0333, '.1.3.6.1.2.1.1.3.0'),			## SNMP_HANDLER15	R	SYS_UPTIME	8		Ru: En: 
(0x0334, '.1.3.6.1.2.1.1.4.0'),         ## sysContact       RW  DisplayString (size(64))
(0x0335, '.1.3.6.1.2.1.1.5.0'),         ## sysName          RW  DisplayString (size(64))
(0x0336, '.1.3.6.1.2.1.1.6.0'),         ## sysLocation      RW  DisplayString (size(64))
(0x0337, '.1.3.6.1.2.1.1.7.0'),			## SNMP_HANDLER15	R	SYS_SERVICES	8	Ru: En: 

(0x0350, '.1.3.6.1.2.1.2.1.0'),			## SNMP_HANDLER15	R	SYS_IF_NUMBER	8	Ru: En:
(0x0351, '.1.3.6.1.2.1.2.2.1.1.1'),		## SNMP_HANDLER15	R	SYS_IF_INDEX	8	Ru: En:
(0x0352, '.1.3.6.1.2.1.2.2.1.2.1'),	    ## ifDescr.1        R
(0x0353, '.1.3.6.1.2.1.2.2.1.3.1'),		## SNMP_HANDLER15	R	SYS_IF_TYPE	8		Ru: En: 
(0x0354, '.1.3.6.1.2.1.2.2.1.4.1'),		## SNMP_HANDLER15	R	SYS_IF_MTU	8		Ru: En: 
(0x0355, '.1.3.6.1.2.1.2.2.1.5.1'),		## SNMP_HANDLER15	R	SYS_IF_SPEED	8	Ru: En: 
(0x0303, '.1.3.6.1.2.1.2.2.1.6.1'),		## SNMP_HANDLER15	R	SYS_MAC		48	Ru: En: 

(0x8310, '.1.3.6.1.4.1.25728.8300.1.1.1.table.1'),	## SNMP_HANDLERx	R	LP_CHANNEL	8	Ru: En: 
(0x8320, '.1.3.6.1.4.1.25728.8300.1.1.2.table.1'),	## SNMP_HANDLERx	R	LP_STATUS	8	Ru: En: 
(0x8330, '.1.3.6.1.4.1.25728.8300.1.1.3.table.1'),	## SNMP_HANDLERx	R	LP_I	8	Ru: En: 
(0x8340, '.1.3.6.1.4.1.25728.8300.1.1.4.table.1'),	## SNMP_HANDLERx	R	LP_V	8	Ru: En: 
(0x8350, '.1.3.6.1.4.1.25728.8300.1.1.5.table.1'),	## SNMP_HANDLERx	R	LP_R	8	Ru: En: 
(0x8370, '.1.3.6.1.4.1.25728.8300.1.1.7.table.1'),	## SNMP_HANDLERx	RW	LP_POWER	8	Ru: En: 

(0x8420, '.1.3.6.1.4.1.25728.8400.2.2.0'),	## RO npRelHumSensorValueH INTEGER(0..100)
(0x8430, '.1.3.6.1.4.1.25728.8400.2.3.0'),	## RO npRelHumSensorStatus INTEGER(0..1)
(0x8440, '.1.3.6.1.4.1.25728.8400.2.4.0'),	## RO npRelHumSensorValueT INTEGER(-60..200)
(0x8450, '.1.3.6.1.4.1.25728.8400.2.5.0'),	## RO npRelHumSensorStatusH INTEGER(0..3),
(0x8470, '.1.3.6.1.4.1.25728.8400.2.7.0'),	## RO npRelHumSafeRangeHigh INTEGER(0..100),
(0x8480, '.1.3.6.1.4.1.25728.8400.2.8.0'),	## RO npRelHumSafeRangeLow INTEGER(0..100),
(0x8490, '.1.3.6.1.4.1.25728.8400.2.9.0'),	## RO npRelHumSensorValueT100 INTEGER32,

(0x5501, '.1.3.6.1.4.1.25728.5500.5.1.1.table.1'),  ## npRelayN          RO INTEGER
(0x5502, '.1.3.6.1.4.1.25728.5500.5.1.2.table.1'),  ## npRelayMode       RW INTEGER
(0x5503, '.1.3.6.1.4.1.25728.5500.5.1.3.table.1'),  ## npRelayStartReset RW INTEGER
(0x5506, '.1.3.6.1.4.1.25728.5500.5.1.6.table.1'),  ## npRelayMemo       RO DisplayString
(0x550f, '.1.3.6.1.4.1.25728.5500.5.1.15.table.1'), ## npRelayState      RO INTEGER
##(0x5510, '.1.3.6.1.4.1.25728.5500.5.1.16.table.1'), ## npRelayPowered    RO INTEGER

(0x7901, '.1.3.6.1.4.1.25728.7900.1.1.0'), ## RW IrCommand
(0x7902, '.1.3.6.1.4.1.25728.7900.1.2.0'), ## RW IrReset
(0x7903, '.1.3.6.1.4.1.25728.7900.1.3.0'), ## RW IrStatus
    
(0x8810, '.1.3.6.1.4.1.25728.8800.1.1.1.table.8'),	## SNMP_HANDLER5	R	TERMO_CHANNEL	8	Ru: En: 
(0x8820, '.1.3.6.1.4.1.25728.8800.1.1.2.table.8'),	## SNMP_HANDLER5	R	TERMO_VALUE	8	Ru: En: 
(0x8830, '.1.3.6.1.4.1.25728.8800.1.1.3.table.8'),	## SNMP_HANDLER5	R	TERMO_STATUS	8	Ru: En: 
(0x8840, '.1.3.6.1.4.1.25728.8800.1.1.4.table.8'),	## SNMP_HANDLER5	R	TERMO_LOW		8	Ru: En: 
(0x8850, '.1.3.6.1.4.1.25728.8800.1.1.5.table.8'),	## SNMP_HANDLER5	R	TERMO_HIGH		8	Ru: En: 
(0x8860, '.1.3.6.1.4.1.25728.8800.1.1.6.table.8'),	## SNMP_HANDLER5	R	TERMO_MEMO	8	Ru: En: 

(0x8910, '.1.3.6.1.4.1.25728.8900.1.1.1.table.8'),	## SNMP_HANDLER6	R	IO_LINE_N		8	Ru: En: 
(0x8920, '.1.3.6.1.4.1.25728.8900.1.1.2.table.8'),	## SNMP_HANDLER6	R	IO_LEVEL_IN		8	Ru: En: 
(0x8930, '.1.3.6.1.4.1.25728.8900.1.1.3.table.8'),	## SNMP_HANDLER6	RW	IO_LEVEL_OUT	8	Ru: En: 
(0x8960, '.1.3.6.1.4.1.25728.8900.1.1.6.table.8'),	## SNMP_HANDLER6	R	IO_MEMO		8	Ru: En: 
(0x8990, '.1.3.6.1.4.1.25728.8900.1.1.9.table.8'),	## RW npIoPulseCounter
(0x89b0, '.1.3.6.1.4.1.25728.8900.1.1.12.table.8'),	## RW npIoSinglePulseDuration
(0x89c0, '.1.3.6.1.4.1.25728.8900.1.1.13.table.8'),	## RW npIoSinglePulseStart

(0x3801, '.1.3.6.1.4.1.25728.3800.1.1.0'), ## RO GSM problems
(0x3802, '.1.3.6.1.4.1.25728.3800.1.2.0'), ## RO GSM network registration
(0x3803, '.1.3.6.1.4.1.25728.3800.1.3.0'), ## RO GSM signal level
(0x3809, '.1.3.6.1.4.1.25728.3800.1.9.0'), ## RW npGsmSendSms OctetString

## .npReboot.xxxx
(0xebb1, '.1.3.6.1.4.1.25728.911.1.0'),  ## npSoftReboot      RW  INTEGER
(0xebb2, '.1.3.6.1.4.1.25728.911.2.0'),  ## npResetStack      RW  INTEGER
(0xebb3, '.1.3.6.1.4.1.25728.911.3.0')  ## npForcedReboot    RW INTEGER

]

import treegen
treegen.maketree(oid_list, output_file)
