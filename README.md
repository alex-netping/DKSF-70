# DKSF-70

Firmware DKSF 70 for Uniping Server v3

Folder structure:
================
/src/ - firmware source code and build projects

Building the firmware
=====================
1. If not already installed, install IAR Embedded Workbench for ARM v6.5 from IAR systems. Run the installation, then from the main install menu install
- Jlink drivers
- IAR Embedded Workbench

2. If not already installed, install Python 2.x for Windows from Active State
http://www.activestate.com/activepython/downloads

3. Install IntelHex python package
pip install intelhex

4. Run IAR Embedded Workbench IDE, from the main menu select File -> Open -> Workspace, browse to the /src folder and select IAR workspace file DKSF-70

5. In the workspace listview on the left, select the "Bootloader debug", then in the main menu click Project -> Rebuild all
This step builds the bootloader code. The ouput file is \Bootloader_Debug\Exe\dksf70_bootloader.hex

6. In the workspace listview on the left, select the "DKSF_70 - RAM Debug", then in the main menu click Project -> Rebuild all
This step builds the main firmware. The output file is \Dksf70_Debug\Exe\DKSF_70.hex

7. To build the *.npu file for firmware upgrade via Uniping Server web UI:
- change to folder \Dksf70_Debug\Exe\
- run python script npu_maker_dksf70_v2.py as follows: c:\Python27\python.exe npu_maker_dksf70_v2.py
(The above example assumes the location of python folder at c:\Python27, it may be in a different place on your system)

The output files generated in the same folder are:
- DKSF_71.4.5.A-1.npu
- DKSF_71.4.5.A-1_HX.hex

To run the script again, delete the above two files first.
