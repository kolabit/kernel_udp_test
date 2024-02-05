# Linux Kernel UDP Tx socket test
Simple implementation of Linux Kernel UDP socket for Tx speed test.

Tested with the following Linux Kernels:
- 5.15.0-91-generic (x86_64)
- 5.19.0-1021-generic (riscv64)

# How to build
Make sure proper linux kenel headers and gcc are installed. <br>
Fix the code in order to have proper Network interface, Source / Destination MAC and IP addresses: 
```
// Network interface on source Linux PC
#define IF_NAME "eth1"

// Source Interface info
//#define SRC_MAC {0x6c, 0xb3, 0x11, 0x52, 0x77, 0xAA} 
#define SRC_MAC {0x0,0x04,0xa3,0x33,0x6a,0xce}
#define SRC_IP  "10.22.33.16"
#define SRC_PORT 0xC000

// Destination Interface info
#define DST_MAC {0x24, 0x4B, 0xFE, 0x5A, 0x22, 0xFF}
#define DST_IP   "10.22.33.1"
#define DST_PORT 0xD456
```
Fix the number of test packets, and buffer size:
```
// Number of test frames
#define TEST_FRM_NUM 10000
// Size of the test packet(s)
#define TEST_DATA_LEN 3080
```
Build and run the test :
```
make
sudo insmod KernelUdpTxTest.ko
sudo rmmod KernelUdpTxTest
```
