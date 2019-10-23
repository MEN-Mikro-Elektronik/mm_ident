# mm_ident

mm_ident is based on the M-Module ident library.

The mm_ident is a tool to identify M-Module board on F204/F205
from userspace without any need of an driver.

The tools use the communication function from the library including a
mmap functionality to access the M-Modules from user space.


## Example

### Identifing M-Module Carrier BAR:
lspci -vv
06:0e.0 Bridge: Altera Corporation Device d203
	Subsystem: Device ff00:ff00
	Flags: bus master, slow devsel, latency 32, IRQ 7
	Memory at c0400000 (32-bit, non-prefetchable) [size=4K]


M-Module Slot 0: Adress Space A08 at Offset 0x200
M-Module Slot 1: Adress Space A08 at Offset 0x600

The Tool expect the BAR+M-Module offset and will
map the memory to get access to the EEPROM.

### Get EEPROM Information from M-Module Slot 0:
$ ./mm_ident 0xc0400200
PhysAddr: 0xc0400200
MAGIC: 0x5346
Type: 0x0001, ID: 0x0048, Rev: 0x0000, Name: M72


This information can be used e.g. in a bash script to check which
M-Module is available on the corresponding carrier.

First use in 10MH70I90 Industry PC Linux BSP.
