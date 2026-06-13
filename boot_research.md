    V32F20x_V32H20x Reference Manual

    XOH  RCH  XOL  RCL  RCH_DIV4  RCH_DIV48  PLL0  PLL0_DIV  PLL1
        =—:| MUX     DIV     LPPCLK     to LP APB peripherals


    M                 to AHB0 bus, core,
    U     DIV HCLK0   memory and DMA
    X


            M
            U    DIV   PCLK0       to APB0 peripherals
            X

        ———
          — M
            U    DIV   USBHSCLK    to USB_HS
            X

            M
           UX    DIV   USBFSCLK    to USB_FS

            M
            U    DIV   MACPTPREFCLK0/1
            X
MACEXCLK                              to MAC0/1
 50MHz     DIV       MACRMIICLK0/1
           2/20

         «| M
    wy =—
    me
            U    DIV   HCLK1      to AHB1 bus, core,
            X                     memory and DMA

         es M
            U    DIV   PCLK1     to APB1 peripherals
            X




        Figure 3-5 System Clock Genaration Distribution Diagram

    3.3.3 Clock output generation

    Four microcontroller clock output pins (MCOx), MCO1, MCO2, MCO3 and MCO4 are available. A clock

    source can be selected for each output.

    MCOx outputs are controlled via SYSCFGLP_CLKOUT1, SYSCFGLP_CLKOUT2, SYSCFGLP_CLKOUT3 and

    SYSCFGLP_CLKOUT4 located in the SYSCFGLP registers.

    The GPIO port corresponding to each MCO pin must be programmed in alternate function mode.

    The clock provided to the MCOs outputs must not exceed the maximum pin speed.

    Table 3-3 MCOx Pin Mux
    MCOx        Pin      | Clock Source Select
    ~~      Hangzhou Vango Technologies, Inc.        115 / 1446

                 V32F20x_V32H20x Reference Manual

    MCO1     IOA8      Clock output source select signal:

    MCO2     IOC9      0x0: RCH

    MCO3     IOE12     0x1: PLL0

                       0x2: PLL1

                       0x3: XOH

                       0x4: RCL
    MCO4     IOF11
                       0x5: XOL

                       0x6: USBHS UTMI clock

                       0x7: NA


   3.4 Power Supply


The device requires a 2.3-to-3.6 V operating voltage supply (VDD). An embedded linear voltage regulator

is used to supply the internal 1.1 V digital power. Only single power supply is supported. V32F20x/

V32H20x can enter the ultra-low power consumption mode by entering the RTCOnly mode, at this time,

most modules enter the power down state except RTC module.










    i        Hangzhou Vango Technologies, Inc.    116 / 1446

      V32F20x_V32H20x Reference Manual

  VDD               RTC
      VBAT    logic(XOL,RCL,R
              TC,Wakeup logic
  100 nF           Backup
                 register)


=  GPIOs  OUT | Level shift  IO  !||I
IN  Logic

Kernel  |
VCAP  1  logic(CPU,digi
2 × 2.2 μF  !  tal & memory)   |
VDD  |  !
V  |  I
DD  |
1/2/...14/15 Ir Voltage  |
regulator
15 × 100 nF  !  !
+ 1 × 4.7 μF  VSS
1/2/...14/15


  VDD       =

                       VDDA

  100 nF             |
  + 1 μF    VREF      VREF+

                                                               Analog
                                        RCH, XOH,
            100 nF    V        ADC,DAC
                       REF-    ADC,DAC  PLL0, PLL1
            + 1 μF                         ...


                  VSSA





                  Figure 3-6 Power supply overview










  i               Hangzhou Vango Technologies, Inc.            117 / 1446

    V32F20x_V32H20x Reference Manual
3.5 Reset

3.5.1 System reset

A system reset is generated when one of the following events occurs:

1. A low level on the NRST pin (external reset)

2. Window watchdog end of count condition (WWDG reset)

3. Independent watchdog end of count condition (IWDG reset)

4. A software reset (SW reset)


3.6 Boot ROM


64kB on-chip boot ROM with bootloader that allows various options, that based on BOOT0 pin or setting

in FlashA information region:

 • Boot from any address in internal Flash and SRAM

 • Support UART, USB peripheral interfaces ISP mode, automatic detection of the active peripheral

3.6.1  Overview

The internal ROM memory is used to store the boot code. After a reset, CM33 core0 starts its code

execution from this memory. The bootloader code is executed every time the part is power-on reset, is

system reset, or wakes up from a deep sleep mode.

Based on based on BOOT0 pin or setting in FlashA information region, the bootloader decides whether

to boot from internal memory or run into ISP mode. Refer to following table for more details:

Table 3-4 Boot mode and ISP download modes

 nBOOT0 bit in BOOT0 pin nSWBOOT0 bit in  nTZMEN bit in Boot mode or ISP mode

 OPT_WORD         PI12       OPT_WORD        OPT_WORD

  -                0            1               1   Passive mode, boot from address

  1                -            0               1   defined by NSBOOTADDR

  -                0            1               0   Passive mode, boot from address

  1                -            0               1   defined by SECBOOTADDR

  -                1            1               -   ISP mode, automatic detection of

  0                -            0               -   the active peripheral

Following table show the ISP pin and boot pin assignments and is the default pin assignment used by


  Hangzhou Vango Technologies, Inc.                 118 / 1446

                               V32F20x_V32H20x Reference Manual

the ROM code that cannot be changed.

Table 3-5 Boot and ISP pin assignments

Boot and ISP Pin           Port pin assignment

BOOT0                      PI12, can be used as GPIO when nSWBOOT0=0

UART ISP mode

FC7_TX                     PA2

FC7_RX                     PA3

USB ISP mode

USBFS_DP                   IOA12

USBFS_DM                   IOA11

The boot starts after reset is released. The CM33 CORE0 AHB/ APB clock and system clock is switched

to 48 MHz based on 48 MHz RCH at the start of bootloader code. After a power-on/ external/ watchdog

timer reset, the SWD access is disable and therefore, the debugger is unable to connect the chip

during this period of time.

The following table shows the CM33 CORE0 boot process.










                           Hangzhou Vango Technologies, Inc.    119 / 1446

    V32F20x_V32H20x Reference Manual


    Reset


   BOOT0=1 or    Assert with           BOOT0=0 or
     nBOOT0=0    BOOT0 pin or nBOOT0   nBOOT0=1

ISP boot mode        Passive boot mode


Configure clocks and    CRC32 mode     Normal mode  SRA2K mode
    peripherals


    Enter ISP handler    CRC32 passed?    Verify passed?


    Yes  Get PC and Stack              Yes
        Pointer from image


        Jump to APP Yes                MSP and Boot
         address correct ?

    Yes    No    No                                      No

            ISP_CHKFAIL                    No Lock the part
           mode Enabled?



                           Figure 3-7 CM33 CORE0 boot process

    3.6.2   FlashA INFO Region

    The FlashA information region words are accessible through the FlashA interface registers interface.

    Table 3-6 FlashA information region description

    Block   Start offset   End offset              Read         Write
                                                                          Description
    No.     address        address                 protection   protection

                                                                          User option
    0       0x0000         0x1FFF                  No           No
                                                                          configuration

    1       0x2000         0x3FFF                  Yes          Option    Key

    2       0x4000         0x5FFF                  No           No        Analog trimming data

    3       0x6000         0x7FFF                  No           Yes       Chip information






    i                      Hangzhou Vango Technologies, Inc.              120 / 1446

                                                                V32F20x_V32H20x Reference Manual
    3.6.2.1 Block 0

    The FlashA INFO block 0 is used as the persistent storage for user optional configurations. It starts at

    offset address 0x0000 of FlashA INFO region. Please see following table for more details.

    Table 3-7 The FlashA INFO block 0 registers mapping

    Offset              Symbol       Description

    0x0000              OPT_WORD     User option word, refer to Table 3-8

    0x0004              BAUDRATE     ROM bootloader UART baudrate

    0x0008 ~ 0x000C     -            Reserved

    0x0010              NSBOOTADDR   Passive boot address (when nTZMEN=1)

    0x0014              SECBOOTADDR  Passive boot address (when nTZMEN=0)

    0x0018 ~ 0x002C     -            Reserved

    0x0030              USB_VID      USB VID for ROM ISP DFU mode

    0x0034              USB_PID      USB PID for ROM ISP DFU mode

    0x0038 ~ 0x003C     -            Reserved

0x0040 ~ 0x00BC MSD0 ~ MSD31 Manufacturer string descriptor 0~31 for ROM ISP DFU mode

    0x00C0 ~ 0x0144     -            Reserved

    0x0150 ~ 0x0194     CLK_CFG      Oscillators enable and system/USB/UART clocks configuration

    0x0198 ~ 0x1FFC     -            Reserved

    Table 3-8 OPT_WORD (0x0000) Description

    Bit       Symbol              Description

    31:25     Rsvd                Reserved

    24        WDTEN2              WDT2 enable

                                  0x1: Enable WDT2

                                  0x0: Disable WDT2

    23        WDTEN1              WDT1 enble

                                  0x1: Enable WDT1

                                  0x0: Disable WDT1

    22        WDTEN0              WDT0 enable

                                  0x1: Enable WDT0

                                  0x0: Disable WDT0

21 nENCENG_KEY_WP ENCENG keys write protection enable
    §                             Hangzhou Vango Technologies, Inc.                   121 / 1446

                            V32F20x_V32H20x Reference Manual

                        0x1: Disable ENCENG keys write protection

                        0x0: Enable ENCENG keys write protection

20     nENCENG_KEY_RP   ENCENG keys read protection enable

                        0x1: Disable ENCENG keys read protection

                        0x0: Enable ENCENG keys read protection

19     DBGEN            SWD debug enable

                        0x1: Enable SWD debug

                        0x0: Disable SWD debug

18:17  Rsvd             Reserved

16     nUSBCALEN        RCH calibration for USB enable

                        0x1: Disable RCH calibration for USB

                        0x0: Enable RCH calibration for USB

15:10  Rsvd             Reserved

9      nLOGEN           ROM log output enable

                        0x1: Disable ROM log output

                        0x0: Enable ROM log output

8      ISP_CHKFAIL      Enter ISP mode enable

                        0x1: Check or verify fail when passive mode, enter ISP mode.

                        0x0: Check or verify fail when passive mode, lock the part.

7:5    RTC_DLY          RTC delay

                        0x7: 0

                        0x6: 0.25ms

                        0x5: 0.5ms

                        0x4: 0.75ms

                        0x3: 1ms

                        0x2: 2ms

                        0x1: 3ms

                        0x0: 4ms

4:3    SB_MODE          Secure boot mode

                        0x3: Normal mode

                        0x2: CRC32 mode（with CRC32 value）


                        Hangzhou Vango Technologies, Inc.    122 / 1446

                                                  V32F20x_V32H20x Reference Manual

                           0x1: Secure boot mode（with RSA2048 MD-SHA256）

                           0x0: Normal mode

2         nBOOT0           Boot 0 mode

                           0x0: ISP mode

                           0x1: Passive mode

1         nSWBOOT0         BOOT0 taken selection

                           0x0: BOOT0 taken from the INFO bit nBOOT0

                           0x1: BOOT0 taken from BOOT0 pin

0         nTZMEN           Global TrustZone security enable

                           0x0: Global TrustZone security enabled

                           0x1: Global TrustZone security disabled

3.6.2.2 Block 1

The FlashA INFO block 1 is used as the persistent storage for key. It starts at offset address 0x2000 of

FlashA INFO region. Please see following table for more details.

Table 3-9 The FlashA INFO block 1 registers mapping

Offset                Symbol   Description

0x2000 ~ 0x3FFC       KEY      RSA 2048 bits key, ENCENG key and AES key

3.6.2.3 Block 2

The FlashA INFO block 2 is used as the persistent storage for analog information. It starts at offset

address 0x4000 of FlashA INFO region. Please see following table for more details.

Table 3-10 The FlashA INFO block 2 registers mapping

Offset                 Symbol   Description

0x4000 ~ 0x5FFC        -        Analog information

3.6.2.4 Block 3

The FlashA INFO block 3 is used as the persistent storage for chip information. It starts at offset address

0x6000 of FlashA INFO region. Please see following table for more details.

Table 3-11 The FlashA INFO block 3 registers mapping

Offset                 Symbol   Description

                           Hangzhou Vango Technologies, Inc.            123 / 1446

V32F20x_V32H20x Reference Manual

0x6000 ~ 0x7FFC INFO Chip information










Hangzhou Vango Technologies, Inc.    124 / 1446

                               V32F20x_V32H20x Reference Manual
3.7 System IRQ

3.7.1 CM33 IRQ
Table 3-12 CM33 IRQ table        ||
No. | Interrupt Request Source     26                SCR
 0     CM33_MAILBOX                27                CAN0

 1     CM0_MAILBOX                 28                CAN1

 2         REV2                    29              ETMR0_UP

 3         REV3                    30             ETMR0_BRK

 4         I2S0                    31              ETMR0_CC

 5         I2S1                    32             ETMR0_TRG_COM

 6        USB_HS                   33              ETMR1_UP

 7        USB_FS                   34             ETMR1_BRK

 8        OSPI0                    35              ETMR1_CC

 9        OSPI1                    36             ETMR1_TRG_COM

 10       FLASHA                   37               GTMR0
 11       FLASHB                   38               GTMR1
 12    ISO7816_0_1                 39               GTMR2
 13      MEM_ECC                   40               GTMR3

 14       BTMR0                    41              DMA0_CH0

 15       BTMR1                    42              DMA0_CH1

 16       BTMR2                    43              DMA0_CH2

 17       BTMR3                    44              DMA0_CH3

 18       BPWM0                    45              DMA0_CH4

 19       BPWM1                    46              DMA0_CH5

 20       BPWM2                    47    |         DMA0_CH6   |
 21       BPWM3                    48              DMA0_CH7

 22       SDIO0                    49               REV49

 23       SDIO1                    50               REV50

 24        RNG                     51                ADC0

 25        EADC                    52                ADC1
§ dango      Hangzhou Vango Technologies, Inc.       125 / 1446