/**
 * Copyright (c)Foursemi Co., Ltd. 2018-2019. All rights reserved.
 * Description: Header File For Fs1894.
 * Author: Fourier Semiconductor Inc.
 * Create: 2019-03-17 File created.
 */
#ifndef __FS1603_H
#define __FS1603_H

#define FS1603_STATUS          0x00
#define MAX_CALIB_COUNT        7
#define IMPEDANCE_JITTER_RATIO 20

struct STATUS_BITS {
    uint16_t BOVDS : 1;
    uint16_t PLLS : 1;
    uint16_t OTDS : 1;
    uint16_t OVDS : 1;
    uint16_t UVDS : 1;
    uint16_t OCDS : 1;
    uint16_t CLKS : 1;
    uint16_t RSVD2 : 3;
    uint16_t SPKS : 1;
    uint16_t SPKT : 1;
    uint16_t RSVD1 : 4;
};
union REG_STATUS {
    uint16_t value;
    struct STATUS_BITS bit;
};

#define FS1603_BATS      0x01
struct BATS_BITS {
    uint16_t BATV : 10;
    uint16_t RSVD1 : 6;
};
union REG_BATS {
    uint16_t value;
    struct BATS_BITS bit;
};

#define FS1603_TEMPS     0x02
struct TEMPS_BITS {
    uint16_t TEMPV : 9;
    uint16_t RSVD1 : 7;
};
union REG_TEMPS {
    uint16_t value;
    struct TEMPS_BITS bit;
};

#define FS1603_ID        0x03
struct ID_BITS {
    uint16_t REV : 8;
    uint16_t DEVICE_ID : 8;
};
union REG_ID {
    uint16_t value;
    struct ID_BITS bit;
};

#define FS1603_I2SCTRL   0x04
struct I2SCTRL_BITS {
    uint16_t I2SF : 3;
    uint16_t CHS12 : 2;
    uint16_t RSVD1 : 5;
    uint16_t DISP : 1;
    uint16_t I2SDOE : 1;
    uint16_t I2SSR : 4;
};
union REG_I2SCTRL {
    uint16_t value;
    struct I2SCTRL_BITS bit;
};

#define FS1603_AUDIOCTRL 0x06
struct AUDIOCTRL_BITS {
    uint16_t RSVD1 : 8;
    uint16_t VOL : 8;
};
union REG_AUDIOCTRL {
    uint16_t value;
    struct AUDIOCTRL_BITS bit;
};

#define FS1603_TEMPSEL   0X08
#define FS1603_SYSCTRL   0x09
struct SYSCTRL_BITS {
    uint16_t PWDN : 1;
    uint16_t I2CR : 1;
    uint16_t RSVD2 : 1;
    uint16_t AMPE : 1;
    uint16_t RSVD1 : 12;
};
union REG_SYSCTRL {
    uint16_t value;
    struct SYSCTRL_BITS bit;
};

#define FS1603_SPKSET    0x0A
struct SPKSET_BITS {
    uint16_t RSVD2 : 9;
    uint16_t SPKR : 2;
    uint16_t RSVD1 : 5;
};
union REG_SPKSET {
    uint16_t value;
    struct SPKSET_BITS bit;
};

#define FS1603_OTPACC    0x0B
#define FS1603_STERC1    0x70
#define FS1603_STERCTRL  0x7E
#define FS1603_STERGAIN  0x7F
#define FS1603_ACSEQWL   0x82
#define FS1603_ACSEQWH   0x83
#define FS1603_ACSEQA    0x86
#define FS1603_ACSCTRL   0x89

#define FS1603_CHIPINI   0x90
struct CHIPINI_BITS {
    uint16_t INIFINISH: 1;
    uint16_t INIOK: 1;
    uint16_t RSVD1: 14;
};
union REG_CHIPINI {
    uint16_t value;
    struct CHIPINI_BITS bit;
};

#define FS1603_DSPCTRL   0xA1
struct DSPCTRL_BITS {
    uint16_t DCCOEF : 3;
    uint16_t RSVD3 : 1;
    uint16_t NOFILTEN : 1;
    uint16_t RSVD2 : 5;
    uint16_t POSTEQBEN : 1;
    uint16_t POSTEQEN: 1;
    uint16_t DSPEN : 1;
    uint16_t EQCOEFSEL : 1;
    uint16_t RSVD1 : 2;
};
union REG_DSPCTRL {
    uint16_t value;
    struct DSPCTRL_BITS bit;
};

#define FS1603_DACEQWL   0xA2
#define FS1603_DACEQWH   0xA3
#define FS1603_DACEQA    0xA6
#define FS1603_BFLCTRL   0xA7
#define FS1603_BFLSET    0xA8
#define FS1603_AGC       0xAA
#define FS1603_TSCTRL    0xAF
struct TSCTRL_BITS {
    uint16_t GAIN : 3;
    uint16_t EN : 1;
    uint16_t OFF_THD : 3;
    uint16_t RSVD3 : 1;
    uint16_t OFF_DELAY : 3;
    uint16_t RSVD2 : 1;
    uint16_t OFF_ZEROCRS : 1;
    uint16_t OFF_AUTOEN : 1;
    uint16_t OFFSTA : 1;
    uint16_t RSVD1 : 1;
};
union REG_TSCTRL {
    uint16_t value;
    struct TSCTRL_BITS bit;
};

#define FS1603_ADCCTRL   0xB3
#define FS1603_ADCEQWL   0xB4
#define FS1603_ADCEQA    0xB8
#define FS1603_ADCENV    0xB9
#define FS1603_ADCTIME   0xBA
#define FS1603_ZMDATA    0xBB
#define FS1603_DIGSTAT   0xBD
struct DIGSTAT_BITS {
    uint16_t ADCRUN : 1;
    uint16_t DACRUN : 1;
    uint16_t RSVD2 : 1;
    uint16_t DSPFLAG : 1;
    uint16_t SPKM24 : 1;
    uint16_t SPKM6 : 1;
    uint16_t SPKRE : 1;
    uint16_t RSVD1 : 5;
    uint16_t SPKFSM : 4;
};
union REG_DIGSTAT {
    uint16_t value;
    struct DIGSTAT_BITS bit;
};

#define FS1603_BSTCTRL   0xC0
struct BSTCTRL_BITS {
    uint16_t DISCHARGE : 1;
    uint16_t DAC_GAIN : 2;
    uint16_t BSTEN : 1;
    uint16_t MODE_CTRL : 2;
    uint16_t ILIM_SEL : 4;
    uint16_t VOUT_SEL : 4;
    uint16_t RSVD1 : 1;
    uint16_t SSEND : 1;
};
union REG_BSTCTRL {
    uint16_t value;
    struct BSTCTRL_BITS bit;
};

#define FS1603_PLLCTRL1  0xC1
#define FS1603_PLLCTRL2  0xC2
#define FS1603_PLLCTRL3  0xC3
#define FS1603_PLLCTRL4  0xC4
struct PLLCTRL4_BITS {
    uint16_t PLLEN : 1;
    uint16_t OSCEN : 1;
    uint16_t ZMEN : 1;
    uint16_t VBGEN : 1;
    uint16_t RSVD1 : 12;
};
union REG_PLLCTRL4 {
    uint16_t value;
    struct PLLCTRL4_BITS bit;
};

#define FS1603_SPKERR    0xC9
#define FS1603_SPKM24    0xCA
#define FS1603_SPKM6     0xCB
#define FS1603_SPKRE     0xCC
#define FS1603_ANACTRL   0xD0
#define FS1603_ZMCONFIG  0xD5
#define FS1603_OTPCMD    0xDC
struct OTPCMD_BITS {
    uint16_t R : 1;
    uint16_t W : 1;
    uint16_t BUSY : 1;
    uint16_t RSVD3 : 5;
    uint16_t EPROM_LD : 1;
    uint16_t RSVD2 : 3;
    uint16_t PW : 1;
    uint16_t RSVD1 : 3;
};
union REG_OTPCMD {
    uint16_t value;
    struct OTPCMD_BITS bit;
};

#define FS1603_OTPADDR   0xDD
#define FS1603_OTPWDATA  0xDE
#define FS1603_OTPRDATA  0xDF
#define FS1603_OTPPG1W2  0xE6
#define FS1603_OTPPG2    0xE8
#endif
