########################################################################
#
#                           GettingStarted.eww
#
# $Revision: 30465 $
#
########################################################################

DESCRIPTION
===========
  This example project shows how to use the IAR Embedded Workbench for ARM
 to develop code for IAR-LPC-1768-SK board. It shows basic use of I/O,
 timer and interrupt controllers.
  It starts by blinking LED1.

COMPATIBILITY
=============

   The example project is compatible with, on IAR-LPC-1768-SK evaluation 
  board. By default, the project is configured to use the J-Link JTAG 
  interface.

CONFIGURATION
=============

   After power-up the controller get clock from internal RC oscillator that
  is unstable and may fail with J-Link auto detect. In this case adaptive clocking
  should be used. The adaptive clock can be select from menu:
  Project->Options..., section Debugger->J-Link/J-Trace  JTAG Speed - Adaptive.

  Jumpers:
   PWR_SEL - depending of power source
   RST_E   - unfilled
   ISP_E   - unfilled
   DBG_E   - filled

  The GettingStarted application is downloaded to the iFlash or iRAM memory
  depending of the project's configuration and executed.

GETTING STARTED
===============

  1) Start the IAR Embedded Workbench for ARM.

  2) Select File->Open->Workspace...
     Open the following workspace:

      <installation-root>\arm\examples\NXP\
     LPC17xx\IAR-LPC-1768-SK\GettingStarted\GettingStarted.eww

  3) Run the program.
