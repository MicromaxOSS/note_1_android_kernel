/************************************************************                            $
*
* file: NE6153_wireless_power.h 
*
* Description: AP to NE6153 IIC firmware
*
*------------------------------------------------------------

*************************************************************/

#ifndef __NE6153_WIRELESS_POWER_H__
#define __NE6153_WIRELESS_POWER_H__

// RX -> TX
#define PROPRIETARY18        0x18
#define PROPRIETARY28        0x28
#define PROPRIETARY38        0x38
#define PROPRIETARY48        0x48
#define PROPRIETARY58        0x58

// bits mask
#define BIT0                 0x01
#define BIT1                 0x02
#define BIT2                 0x04
#define BIT3                 0x08
#define BIT4                 0x10
#define BIT5                 0x20
#define BIT6                 0x40
#define BIT7                 0x80

/* status low regiter bits define */
#define STATUS_VOUT_ON       BIT7
#define STATUS_VOUT_OFF      BIT6
#define STATUS_OV_TEMP       BIT2
#define STATUS_OV_VOL        BIT1
#define STATUS_OV_CURR       BIT0

// bitmap for status flags
// 1: indicates a pending interrupt for LDO Vout state change – from OFF to ON
#define VOUTCHANGED          BIT7 // Stat_Vout_ON
// 1: indicates a pending interrupt for TX Data Received. (Change from “No Received Data” state to “Data Received” state)
#define TXDATARCVD           BIT4 // TX Data Received

/* used registers define */
#define REG_CHIP_ID          0x0002 //NE6153 chip id
#define REG_FW_VER          0x0004 //NE6153 fw ver
//#define REG_CHIP_REV         0x001c
#define REG_STATUS           0x0034 //status
#define REG_INTR             0x0036 //interrupt status
#define REG_INTR_EN          0x0038
#define REG_CHG_STATUS       0x003A
#define REG_ADC_VOUT         0x003C //NE6153 vout
#define REG_VOUT_SET         0x003E //NE6153 set vout
#define REG_VRECT_ADJ        0x003F
#define REG_RX_LOUT          0x0044 //NE6153 i_out
#define REG_ILIM_SET         0x004A //NE6153 set limit i
#define REG_COMMAND          0x004E //AP to RX command
#define REG_PROPPKT_ADDR     0x0050 // Proprietary Packet Header Register, PPP_Header (0x50)
#define REG_PPPDATA_ADDR     0x0051 // PPP Data Value Register(0X51, 0x52, 0x53, 0x54, 0x55)
#define REG_INT_CLEAR        0x0056 //none or 0x5e
#define REG_BCHEADER_ADDR    0x004B // Back Channel Packet Register (0x4B,0x4C) header and  cmd
#define REG_BCDATA_ADDR      0x0058 // Back Channel Packet Register (0x58, 0x59, 0x5A, 0x5B)
//#define REG_FC_VOLTAGE       0x0078
#define REG_RPPO       0x0068 //set fod offset  NE6153
#define REG_RPPG       0x0069 //set fod gain  NE6153

// bitmap for SSCmnd register 0x4e

#define VSWITCH        BIT7   //NE6153 Proprietary Send Step-up
#define CLRINT           BIT5// If AP sets this bit to "1" then NE6153  clears the interrupt corresponding to the bit(s) which has a value of “1”
#define SENDEPT        BIT3//If AP sets this bit to "1" then NE6153 send EPT to TX
#define LDOTGL          BIT1// If AP sets this bit to "1" then NE6153  toggles LDO output once (from on to off, or from off to on), and then sets this bit to “0”
#define SENDPROPP    BIT0// If AP sets this bit to “1” then NE6153 sends the Proprietary Packet

// bitmap for customer command
#define BC_NONE              0x00
#define BC_SET_FREQ          0x03
#define BC_GET_FREQ          0x04
#define BC_READ_FW_VER       0x05
#define BC_READ_Iin          0x06
#define BC_READ_Vin          0x07
#define BC_SET_Vin           0x0a
#define BC_ADAPTER_TYPE      0x0b
#define BC_RESET             0x0c
#define BC_READ_I2C          0x0d
#define BC_WRITE_I2C         0x0e
#define BC_VI2C_INIT         0x10

// Adapter Type
typedef enum {
    ADAPTER_UNKNOWN        = 0x00,
    ADAPTER_SDP            = 0x01,
    ADAPTER_CDP            = 0x02,
    ADAPTER_DCP            = 0x03,
    ADAPTER_QC20           = 0x05,
    ADAPTER_QC30           = 0x06,
    ADAPTER_PD             = 0x07,
} Adapter_list;

// proprietary packet type
typedef struct {
    u8 header;             // The header consists of a single byte that indicates the Packet type.
    u8 cmd;                // Back channel command
    u8 msg[5];             // Send data buffer
} ProPkt_Type;


unsigned char NE6153_rx_fw[] = { /// 0x00

};

enum tx_type {
	TX_TYPE_NONE = 0,
	TX_TYPE_BPP = 1,	//5w
	TX_TYPE_SAMSUNG = 2,	//10w
	TX_TYPE_EPP = 3,	//large than 15w
	TX_TYPE_UNKNOWN,
};

#endif
