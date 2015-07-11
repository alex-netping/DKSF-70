struct nvic_s
{
  volatile unsigned iser[8];                 /*!< Offset: 0x000 (R/W)  Interrupt Set Enable Register           */
           unsigned reserved0[24];
  volatile unsigned ICER[8];                 /*!< Offset: 0x080 (R/W)  Interrupt Clear Enable Register         */
       unsigned RSERVED1[24];
  volatile unsigned ISPR[8];                 /*!< Offset: 0x100 (R/W)  Interrupt Set Pending Register          */
       unsigned RESERVED2[24];
  volatile unsigned ICPR[8];                 /*!< Offset: 0x180 (R/W)  Interrupt Clear Pending Register        */
       unsigned RESERVED3[24];
  volatile unsigned IABR[8];                 /*!< Offset: 0x200 (R/W)  Interrupt Active bit Register           */
       unsigned RESERVED4[56];
  volatile unsigned char IP[240];                 /*!< Offset: 0x300 (R/W)  Interrupt Priority Register (8Bit wide) */
       unsigned RESERVED5[644];
  volatile unsigned stir;                    /*!< Offset: 0xE00 ( /W)  Software Trigger Interrupt Register     */
}  NVIC_Type;