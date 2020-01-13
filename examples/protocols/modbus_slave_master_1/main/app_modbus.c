#include "app_modbus.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "port.h"

static const char *TAG = "APP_MODBUS:";
//#define MB_LOG(...)
#define MB_LOG(...) ESP_LOGW(__VA_ARGS__)

#define T_WAIT_FOREVER 3

USHORT usMDiscInStart = M_DISCRETE_INPUT_START;
#if M_DISCRETE_INPUT_NDISCRETES % 8
UCHAR ucMDiscInBuf[MB_MASTER_TOTAL_SLAVE_NUM][M_DISCRETE_INPUT_NDISCRETES / 8 + 1];
#else
UCHAR ucMDiscInBuf[MB_MASTER_TOTAL_SLAVE_NUM][M_DISCRETE_INPUT_NDISCRETES / 8];
#endif
//Master mode:Coils variables
USHORT usMCoilStart = M_COIL_START;
#if M_COIL_NCOILS % 8
UCHAR ucMCoilBuf[MB_MASTER_TOTAL_SLAVE_NUM][M_COIL_NCOILS / 8 + 1];
#else
UCHAR ucMCoilBuf[MB_MASTER_TOTAL_SLAVE_NUM][M_COIL_NCOILS / 8];
#endif
//Master mode:InputRegister variables
USHORT usMRegInStart = M_REG_INPUT_START;
USHORT usMRegInBuf[MB_MASTER_TOTAL_SLAVE_NUM][M_REG_INPUT_NREGS];
//Master mode:HoldingRegister variables
USHORT usMRegHoldStart = M_REG_HOLDING_START;
USHORT usMRegHoldBuf[MB_MASTER_TOTAL_SLAVE_NUM][M_REG_HOLDING_NREGS];

static const int DESCRETE_DONE_BIT = BIT1;
static const int INPUTREG_DONE_BIT = BIT2;

static EventGroupHandle_t descrete_done_event_group;
static EventGroupHandle_t input_reg_done_event_group;

static int event_groups_create()
{
    descrete_done_event_group = xEventGroupCreate();
    input_reg_done_event_group = xEventGroupCreate();
    return 0;
}
int descret_event_gp_bit_clear()
{
    xEventGroupClearBits(descrete_done_event_group, DESCRETE_DONE_BIT);
    return 0;
}

static int descret_event_gp_bit_set()
{
    xEventGroupSetBits(descrete_done_event_group, DESCRETE_DONE_BIT);
    return 0;
}

static int input_reg_event_gp_bit_set()
{
    xEventGroupSetBits(input_reg_done_event_group, INPUTREG_DONE_BIT);
    return 0;
}

int input_reg_event_gp_bit_clear()
{
    xEventGroupClearBits(input_reg_done_event_group, INPUTREG_DONE_BIT);
    return 0;
}

static int wait_event_gp_bits_done(EventGroupHandle_t hdl, int bit, int timeout)
{
    int ret = -1;
    EventBits_t uxBits;
    TickType_t st = xTaskGetTickCount();
    do
    {
        uxBits = xEventGroupWaitBits(hdl, bit, true, false, portMAX_DELAY);
        if(bit == uxBits)
        {
            MB_LOG(TAG, "%s ok\r\n", __func__);
            ret = 0;
            break;
        }
    } while ((xTaskGetTickCount() < st + timeout));
    return ret;
}

int wait_descret_ev_gp_done(int timeout)
{
    return wait_event_gp_bits_done(descrete_done_event_group, DESCRETE_DONE_BIT, timeout);
}

int wait_input_reg_ev_gp_done(int timeout)
{
    return wait_event_gp_bits_done(input_reg_done_event_group, INPUTREG_DONE_BIT, timeout);
}

/**
 * Modbus master input register callback function.
 *
 * @param pucRegBuffer input register buffer
 * @param usAddress input register address
 * @param usNRegs input register number
 *
 * @return result
 */
eMBErrorCode eMBRegInputCB(UCHAR *pucRegBuffer, USHORT usAddress, USHORT usNRegs)
{
    MB_LOG(TAG, "%s\r\n", __func__);
    eMBErrorCode eStatus = MB_ENOERR;
    USHORT iRegIndex;
    USHORT *pusRegInputBuf;
    USHORT REG_INPUT_START;
    USHORT REG_INPUT_NREGS;
    USHORT usRegInStart;

    pusRegInputBuf = usMRegInBuf[ucMBMasterGetDestAddress() - 1];
    REG_INPUT_START = M_REG_INPUT_START;
    REG_INPUT_NREGS = M_REG_INPUT_NREGS;
    usRegInStart = usMRegInStart;

    /* it already plus one in modbus function method. */
    usAddress--;

    if ((usAddress >= REG_INPUT_START) && (usAddress + usNRegs <= REG_INPUT_START + REG_INPUT_NREGS))
    {
        iRegIndex = usAddress - usRegInStart;
        while (usNRegs > 0)
        {
            pusRegInputBuf[iRegIndex] = *pucRegBuffer++ << 8;
            pusRegInputBuf[iRegIndex] |= *pucRegBuffer++;
            iRegIndex++;
            usNRegs--;
        }
        //usr add
        input_reg_event_gp_bit_set();
    }
    else
    {
        eStatus = MB_ENOREG;
    }

#if 0
    for (int i = 0; i < M_REG_INPUT_NREGS; i++)
    {
        //MB_LOG("$", " %02x -", pusRegInputBuf[i]);
        printf(" %02x -", pusRegInputBuf[i]);
    }
    MB_LOG(TAG, "\r\n");
#endif

    return eStatus;
}

/**
 * Modbus master holding register callback function.
 *
 * @param pucRegBuffer holding register buffer
 * @param usAddress holding register address
 * @param usNRegs holding register number
 * @param eMode read or write
 *
 * @return result
 */
eMBErrorCode eMBRegHoldingCB(UCHAR *pucRegBuffer, USHORT usAddress,
                                   USHORT usNRegs, eMBRegisterMode eMode)
{
    MB_LOG(TAG, "%s\r\n", __func__);
    eMBErrorCode eStatus = MB_ENOERR;
    USHORT iRegIndex;
    USHORT *pusRegHoldingBuf;
    USHORT REG_HOLDING_START;
    USHORT REG_HOLDING_NREGS;
    USHORT usRegHoldStart;

    pusRegHoldingBuf = usMRegHoldBuf[ucMBMasterGetDestAddress() - 1];
    REG_HOLDING_START = M_REG_HOLDING_START;
    REG_HOLDING_NREGS = M_REG_HOLDING_NREGS;
    usRegHoldStart = usMRegHoldStart;
    /* if mode is read, the master will write the received date to buffer. */
    eMode = MB_REG_WRITE;

    /* it already plus one in modbus function method. */
    usAddress--;

    if ((usAddress >= REG_HOLDING_START) && (usAddress + usNRegs <= REG_HOLDING_START + REG_HOLDING_NREGS))
    {
        iRegIndex = usAddress - usRegHoldStart;
        switch (eMode)
        {
        /* read current register values from the protocol stack. */
        case MB_REG_READ:
            while (usNRegs > 0)
            {
                *pucRegBuffer++ = (UCHAR)(pusRegHoldingBuf[iRegIndex] >> 8);
                *pucRegBuffer++ = (UCHAR)(pusRegHoldingBuf[iRegIndex] & 0xFF);
                iRegIndex++;
                usNRegs--;
            }
            break;
        /* write current register values with new values from the protocol stack. */
        case MB_REG_WRITE:
            while (usNRegs > 0)
            {
                pusRegHoldingBuf[iRegIndex] = *pucRegBuffer++ << 8;
                pusRegHoldingBuf[iRegIndex] |= *pucRegBuffer++;
                iRegIndex++;
                usNRegs--;
            }
            break;
        }
    }
    else
    {
        eStatus = MB_ENOREG;
    }

#if 0
    for (int i = 0; i < M_REG_HOLDING_NREGS; i++)
    {
        MB_LOG(TAG, " %02x -", pucRegBuffer[i]);
    }
    MB_LOG(TAG, "\r\n");
#endif

    return eStatus;
}

/**
 * Modbus master coils callback function.
 *
 * @param pucRegBuffer coils buffer
 * @param usAddress coils address
 * @param usNCoils coils number
 * @param eMode read or write
 *
 * @return result
 */
eMBErrorCode eMBRegCoilsCB(UCHAR *pucRegBuffer, USHORT usAddress,
                                 USHORT usNCoils, eMBRegisterMode eMode)
{
    MB_LOG(TAG, "%s\r\n", __func__);
    eMBErrorCode eStatus = MB_ENOERR;
    USHORT iRegIndex, iRegBitIndex, iNReg;
    UCHAR *pucCoilBuf;
    USHORT COIL_START;
    USHORT COIL_NCOILS;
    USHORT usCoilStart;
    iNReg = usNCoils / 8 + 1;

    pucCoilBuf = ucMCoilBuf[ucMBMasterGetDestAddress() - 1];
    COIL_START = M_COIL_START;
    COIL_NCOILS = M_COIL_NCOILS;
    usCoilStart = usMCoilStart;

    /* if mode is read,the master will write the received date to buffer. */
    eMode = MB_REG_WRITE;

    /* it already plus one in modbus function method. */
    usAddress--;

    if ((usAddress >= COIL_START) && (usAddress + usNCoils <= COIL_START + COIL_NCOILS))
    {
        iRegIndex = (USHORT)(usAddress - usCoilStart) / 8;
        iRegBitIndex = (USHORT)(usAddress - usCoilStart) % 8;
        switch (eMode)
        {
            /* read current coil values from the protocol stack. */
        case MB_REG_READ:
            while (iNReg > 0)
            {
                *pucRegBuffer++ = xMBUtilGetBits(&pucCoilBuf[iRegIndex++],
                                                 iRegBitIndex, 8);
                iNReg--;
            }
            pucRegBuffer--;
            /* last coils */
            usNCoils = usNCoils % 8;
            /* filling zero to high bit */
            *pucRegBuffer = *pucRegBuffer << (8 - usNCoils);
            *pucRegBuffer = *pucRegBuffer >> (8 - usNCoils);
            break;

        /* write current coil values with new values from the protocol stack. */
        case MB_REG_WRITE:
            while (iNReg > 1)
            {
                xMBUtilSetBits(&pucCoilBuf[iRegIndex++], iRegBitIndex, 8,
                               *pucRegBuffer++);
                iNReg--;
            }
            /* last coils */
            usNCoils = usNCoils % 8;
            /* xMBUtilSetBits has bug when ucNBits is zero */
            if (usNCoils != 0)
            {
                xMBUtilSetBits(&pucCoilBuf[iRegIndex++], iRegBitIndex, usNCoils,
                               *pucRegBuffer++);
            }
            break;
        }
    }
    else
    {
        eStatus = MB_ENOREG;
    }

#if 0
    for (int i = 0; i < M_COIL_NCOILS; i++)
    {
        MB_LOG(TAG, " %02x -", pucCoilBuf[i]);
    }
    MB_LOG(TAG, "\r\n");
#endif

    return eStatus;
}

/**
 * Modbus master discrete callback function.
 *
 * @param pucRegBuffer discrete buffer
 * @param usAddress discrete address
 * @param usNDiscrete discrete number
 *
 * @return result
 */
eMBErrorCode eMBRegDiscreteCB(UCHAR *pucRegBuffer, USHORT usAddress, USHORT usNDiscrete)
{
    MB_LOG(TAG, "%s\r\n", __func__);
    eMBErrorCode eStatus = MB_ENOERR;
    USHORT iRegIndex, iRegBitIndex, iNReg;
    UCHAR *pucDiscreteInputBuf;
    USHORT DISCRETE_INPUT_START;
    USHORT DISCRETE_INPUT_NDISCRETES;
    USHORT usDiscreteInputStart;
    iNReg = usNDiscrete / 8 + 1;

    pucDiscreteInputBuf = ucMDiscInBuf[ucMBMasterGetDestAddress() - 1];
    DISCRETE_INPUT_START = M_DISCRETE_INPUT_START;
    DISCRETE_INPUT_NDISCRETES = M_DISCRETE_INPUT_NDISCRETES;
    usDiscreteInputStart = usMDiscInStart;

    /* it already plus one in modbus function method. */
    usAddress--;

    if ((usAddress >= DISCRETE_INPUT_START) && (usAddress + usNDiscrete <= DISCRETE_INPUT_START + DISCRETE_INPUT_NDISCRETES))
    {
        iRegIndex = (USHORT)(usAddress - usDiscreteInputStart) / 8;
        iRegBitIndex = (USHORT)(usAddress - usDiscreteInputStart) % 8;

        /* write current discrete values with new values from the protocol stack. */
        while (iNReg > 1)
        {
            xMBUtilSetBits(&pucDiscreteInputBuf[iRegIndex++], iRegBitIndex, 8,
                           *pucRegBuffer++);
            iNReg--;
        }
        /* last discrete */
        usNDiscrete = usNDiscrete % 8;
        /* xMBUtilSetBits has bug when ucNBits is zero */
        if (usNDiscrete != 0)
        {
            xMBUtilSetBits(&pucDiscreteInputBuf[iRegIndex++], iRegBitIndex,
                           usNDiscrete, *pucRegBuffer++);
        }
        //usr add
        descret_event_gp_bit_set();
    }
    else
    {
        eStatus = MB_ENOREG;
    }
#if 0
    for (int i = 0; i < M_DISCRETE_INPUT_NDISCRETES; i++)
    {
        printf(" %02x -", (char)ucMDiscInBuf[ucMBMasterGetDestAddress() - 1][i]);
    }
    MB_LOG(TAG, "\r\n");
#endif
    return eStatus;
}

int get_p_reg_in_buf(int i)
{
    return (int)usMRegInBuf[ucMBMasterGetDestAddress() - 1][i];
}

int get_p_hold_buf(int i)
{
    return (int)usMRegHoldBuf[ucMBMasterGetDestAddress() - 1][i];
}

int get_p_coil_buf(int i)
{
    return (int)ucMCoilBuf[ucMBMasterGetDestAddress() - 1][i];
}

int get_p_disc_buf(int i)
{
    return (int)ucMDiscInBuf[ucMBMasterGetDestAddress() - 1][i];
}

void modebus_task(void *parameter)
{
    eMBErrorCode eStatus;
    eStatus = eMBTCPInit(MB_TCP_PORT_USE_DEFAULT); //115200
    if (0 == eStatus)
    {
        MB_LOG(TAG, "eMBInit OK. eStatus: %d ", eStatus);
        eStatus = eMBEnable();
        if (0 == eStatus)
        {
            event_groups_create();
            MB_LOG(TAG, "eMBEnable OK. eStatus: %d", eStatus);
            MB_LOG(TAG, " starting eMBMasterPoll...");
            while (1)
            {
                eMBPoll();
                //vTaskDelay(30);
            }
        }
        else
        {
            MB_LOG(TAG, "eMBEnable failed!!! eStatus: %d", eStatus);
        }
    }
    else
    {
        MB_LOG(TAG, "eMBInit failed !!! eStatus: %d", eStatus);
    }
    vTaskDelete(NULL);
}

void SysMonitor(void *parameter)
{
    eMBReqErrCode errorCode = MB_MRE_NO_ERR;
    uint16_t errorCount = 0;
    while (1)
    {
        for (int i = 0; i < 100; i++)
        {
            if (errorCount)
                errorCode = eMBReqWriteCoil(1, i, 0xFF00, 1);
            else
                errorCode = eMBReqWriteCoil(1, i, 0x0, 1);
            vTaskDelay(200);
        }
        errorCount ^= 1;
        //errorCode = eMBMasterReqWriteCoil(1,8,0xFF00,4);
        //errorCode = eMBMasterReqReadCoils(1,3,8,400);
        //errorCode = eMBMasterReqReadInputRegister(1,3,2,400);
        //MB_LOG(TAG, "eMBMasterReqReadInputRegister");
        /*

		ucModbusUserData[0] = 0x1F;
//		errorCode = eMBMasterReqReadDiscreteInputs(1,3,8,RT_WAITING_FOREVER);
//		errorCode = eMBMasterReqWriteMultipleCoils(1,3,5,ucModbusUserData,RT_WAITING_FOREVER);
		errorCode = eMBMasterReqWriteCoil(1,8,0xFF00,RT_WAITING_FOREVER);
//		errorCode = eMBMasterReqReadCoils(1,3,8,RT_WAITING_FOREVER);
//		errorCode = eMBMasterReqReadInputRegister(1,3,2,RT_WAITING_FOREVER);
//		errorCode = eMBMasterReqWriteHoldingRegister(1,3,usModbusUserData[0],RT_WAITING_FOREVER);
//		errorCode = eMBMasterReqWriteMultipleHoldingRegister(1,3,2,usModbusUserData,RT_WAITING_FOREVER);
//		errorCode = eMBMasterReqReadHoldingRegister(1,3,2,RT_WAITING_FOREVER);
//		errorCode = eMBMasterReqReadWriteMultipleHoldingRegister(1,3,2,usModbusUserData,5,2,RT_WAITING_FOREVER);

        */
        if (errorCode != MB_MRE_NO_ERR)
        {
            //errorCount++;
            //vTaskDelay(200);
        }
        else
        {
            //vTaskDelay(100);
        }
    }
}

// 0x01
int app_coil_read(const uint8_t addr, const int func, const int index, const int num) //single or multi-coils
{
    const long timeout = T_WAIT_FOREVER;
    eMBReqErrCode errorCode = MB_MRE_NO_ERR;
    const USHORT saddr = index;
 //   rs485_trans_toggle(1);
    errorCode = eMBReqReadCoils(addr, saddr, num, timeout);
 //   rs485_wait_tx_done();
//    rs485_trans_toggle(0);
    return errorCode;
}

//  0x02
int app_coil_discrete_input_read(const uint8_t addr, const int func, const int index, const int num) //single or multi-coils
{
    const long timeout = T_WAIT_FOREVER;
    eMBReqErrCode errorCode = MB_MRE_NO_ERR;
    const USHORT saddr = index;
//    rs485_trans_toggle(1);
    errorCode = eMBReqReadDiscreteInputs(addr, saddr, num, timeout);
    //_wait_tx_done();
 //   rs485_trans_toggle(0);
    return errorCode;
}

//  0x03
int app_holding_register_read(const uint8_t addr, const int func, const int index, const int num) //single or multi-coils
{
    const long timeout = T_WAIT_FOREVER;
    eMBReqErrCode errorCode = MB_MRE_NO_ERR;
    const USHORT saddr = index;
    //_trans_toggle(1);
    errorCode = eMBReqReadHoldingRegister(addr, saddr, num, timeout);
    //_wait_tx_done();
    //_trans_toggle(0);
    return errorCode;
}
//  0x04
int app_input_register_read(const uint8_t addr, const int func, const int index, const int num) //single or multi-coils
{
    const long timeout = T_WAIT_FOREVER;
    eMBReqErrCode errorCode = MB_MRE_NO_ERR;
    const USHORT saddr = index;
    printf("app_input_register_read::saddr : %d\r\n", saddr);
    //_trans_toggle(1);
    errorCode = eMBReqReadInputRegister(addr, saddr, num, timeout);
    //_wait_tx_done();
    //_trans_toggle(0);
    return errorCode;
}
//  0x05
int app_coil_single_write(const uint8_t addr, const int func, const int index, const USHORT sendData) //single
{
    const long timeout = T_WAIT_FOREVER;
    eMBReqErrCode errorCode = MB_MRE_NO_ERR;
    const USHORT saddr = index;
    //_trans_toggle(1);
    errorCode = eMBReqWriteCoil(addr, saddr, sendData, timeout);
    //_wait_tx_done();
    //_trans_toggle(0);
    return errorCode;
}
//  0x06
int app_register_single_write(const uint8_t addr, const int func, const int index, const USHORT sendData) //single
{
    const long timeout = T_WAIT_FOREVER;
    eMBReqErrCode errorCode = MB_MRE_NO_ERR;
    const USHORT saddr = index;
    //_trans_toggle(1);
    errorCode = eMBReqWriteHoldingRegister(addr, saddr, sendData, timeout);
    //_wait_tx_done();
    //_trans_toggle(0);
    return errorCode;
}
//  0x0f
int app_coil_multi_write(const uint8_t addr, const int func, const int index, const int num, UCHAR *sendData) //single or multi-coils
{
    const long timeout = T_WAIT_FOREVER;
    eMBReqErrCode errorCode = MB_MRE_NO_ERR;
    const USHORT saddr = index;
    //_trans_toggle(1);
    errorCode = eMBReqWriteMultipleCoils(addr, saddr, num, sendData, timeout);
    //_wait_tx_done();
    //_trans_toggle(0);
    return errorCode;
}

// 0x10
int app_register_multi_write(const uint8_t addr, const int func, const int index, const int num, USHORT *sendData) //single or multi-coils
{
    const long timeout = T_WAIT_FOREVER;
    eMBReqErrCode errorCode = MB_MRE_NO_ERR;
    const USHORT saddr = index;
    //_trans_toggle(1);
    errorCode = eMBReqWriteMultipleHoldingRegister(addr, saddr, num, sendData, timeout);
    //_wait_tx_done();
    //_trans_toggle(0);
    return errorCode;
}

// 0x17
int app_register_multi_write_read(const uint8_t addr, const int func, const int index, const int num, USHORT *sendData) //single or multi-coils
{
    const long timeout = T_WAIT_FOREVER;
    eMBReqErrCode errorCode = MB_MRE_NO_ERR;
    const USHORT saddr = index;
    //_trans_toggle(1);
    errorCode = eMBReqReadWriteMultipleHoldingRegister(addr, saddr, num, sendData, saddr, num, timeout);
    //_wait_tx_done();
    //_trans_toggle(0);
    return errorCode;
}
