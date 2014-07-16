/**
 ******************************************************************************
 * @addtogroup PIOS PIOS Core hardware abstraction layer
 * @{
 * @addtogroup PIOS_HMC5x83 HMC5x83 Functions
 * @brief Deals with the hardware interface to the magnetometers
 * @{
 * @file       pios_hmc5x83.c
 * @author     The OpenPilot Team, http://www.openpilot.org Copyright (C) 2012.
 * @brief      HMC5x83 Magnetic Sensor Functions from AHRS
 * @see        The GNU Public License (GPL) Version 3
 *
 ******************************************************************************
 */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include "pios.h"
#include <pios_hmc5x83.h>

#ifdef PIOS_INCLUDE_HMC5X83

/* Global Variables */

/* Local Types */

/* Local Variables */
volatile bool pios_hmc5x83_data_ready;

static int32_t PIOS_HMC5x83_Config(const struct pios_hmc5x83_cfg *cfg);
// static int32_t PIOS_HMC5x83_Read(uint8_t address, uint8_t *buffer, uint8_t len);
// static int32_t PIOS_HMC5x83_Write(uint8_t address, uint8_t buffer);

static const struct pios_hmc5x83_cfg *dev_cfg;
static uint32_t dev_port_id;
static uint8_t dev_slave_num;
/**
 * @brief Initialize the HMC5x83 magnetometer sensor.
 * @return none
 */
void PIOS_HMC5x83_Init(const struct pios_hmc5x83_cfg *cfg, uint32_t port_id, uint8_t slave_num)
{
    dev_cfg       = cfg; // store config before enabling interrupt
    dev_port_id   = port_id;
    dev_slave_num = slave_num;
#ifdef PIOS_HMC5X83_HAS_GPIOS
    PIOS_EXTI_Init(cfg->exti_cfg);
#endif

    int32_t val = PIOS_HMC5x83_Config(cfg);

    PIOS_Assert(val == 0);

    pios_hmc5x83_data_ready = false;
}

/**
 * @brief Initialize the HMC5x83 magnetometer sensor
 * \return none
 * \param[in] PIOS_HMC5x83_ConfigTypeDef struct to be used to configure sensor.
 *
 * CTRL_REGA: Control Register A
 * Read Write
 * Default value: 0x10
 * 7:5  0   These bits must be cleared for correct operation.
 * 4:2 DO2-DO0: Data Output Rate Bits
 *             DO2 |  DO1 |  DO0 |   Minimum Data Output Rate (Hz)
 *            ------------------------------------------------------
 *              0  |  0   |  0   |            0.75
 *              0  |  0   |  1   |            1.5
 *              0  |  1   |  0   |            3
 *              0  |  1   |  1   |            7.5
 *              1  |  0   |  0   |           15 (default)
 *              1  |  0   |  1   |           30
 *              1  |  1   |  0   |           75
 *              1  |  1   |  1   |           Not Used
 * 1:0 MS1-MS0: Measurement Configuration Bits
 *             MS1 | MS0 |   MODE
 *            ------------------------------
 *              0  |  0   |  Normal
 *              0  |  1   |  Positive Bias
 *              1  |  0   |  Negative Bias
 *              1  |  1   |  Not Used
 *
 * CTRL_REGB: Control RegisterB
 * Read Write
 * Default value: 0x20
 * 7:5 GN2-GN0: Gain Configuration Bits.
 *             GN2 |  GN1 |  GN0 |   Mag Input   | Gain       | Output Range
 *                 |      |      |  Range[Ga]    | [LSB/mGa]  |
 *            ------------------------------------------------------
 *              0  |  0   |  0   |  ±0.88Ga      |   1370     | 0xF8000x07FF (-2048:2047)
 *              0  |  0   |  1   |  ±1.3Ga (def) |   1090     | 0xF8000x07FF (-2048:2047)
 *              0  |  1   |  0   |  ±1.9Ga       |   820      | 0xF8000x07FF (-2048:2047)
 *              0  |  1   |  1   |  ±2.5Ga       |   660      | 0xF8000x07FF (-2048:2047)
 *              1  |  0   |  0   |  ±4.0Ga       |   440      | 0xF8000x07FF (-2048:2047)
 *              1  |  0   |  1   |  ±4.7Ga       |   390      | 0xF8000x07FF (-2048:2047)
 *              1  |  1   |  0   |  ±5.6Ga       |   330      | 0xF8000x07FF (-2048:2047)
 *              1  |  1   |  1   |  ±8.1Ga       |   230      | 0xF8000x07FF (-2048:2047)
 *                               |Not recommended|
 *
 * 4:0 CRB4-CRB: 0 This bit must be cleared for correct operation.
 *
 * _MODE_REG: Mode Register
 * Read Write
 * Default value: 0x02
 * 7:2  0   These bits must be cleared for correct operation.
 * 1:0 MD1-MD0: Mode Select Bits
 *             MS1 | MS0 |   MODE
 *            ------------------------------
 *              0  |  0   |  Continuous-Conversion Mode.
 *              0  |  1   |  Single-Conversion Mode
 *              1  |  0   |  Negative Bias
 *              1  |  1   |  Sleep Mode
 */
static uint8_t CTRLB = 0x00;
static int32_t PIOS_HMC5x83_Config(const struct pios_hmc5x83_cfg *cfg)
{
    uint8_t CTRLA = 0x00;
    uint8_t MODE  = 0x00;

    CTRLB  = 0;

    CTRLA |= (uint8_t)(cfg->M_ODR | cfg->Meas_Conf);
    CTRLB |= (uint8_t)(cfg->Gain);
    MODE  |= (uint8_t)(cfg->Mode);

    // CRTL_REGA
    if (dev_cfg->Driver->Write(PIOS_HMC5x83_CONFIG_REG_A, CTRLA) != 0) {
        return -1;
    }

    // CRTL_REGB
    if (dev_cfg->Driver->Write(PIOS_HMC5x83_CONFIG_REG_B, CTRLB) != 0) {
        return -1;
    }

    // Mode register
    if (dev_cfg->Driver->Write(PIOS_HMC5x83_MODE_REG, MODE) != 0) {
        return -1;
    }

    return 0;
}

/**
 * @brief Read current X, Z, Y values (in that order)
 * \param[out] int16_t array of size 3 to store X, Z, and Y magnetometer readings
 * \return 0 for success or -1 for failure
 */
int32_t PIOS_HMC5x83_ReadMag(int16_t out[3])
{
    pios_hmc5x83_data_ready = false;
    uint8_t buffer[6];
    int32_t temp;
    int32_t sensitivity;

    if (dev_cfg->Driver->Read(PIOS_HMC5x83_DATAOUT_XMSB_REG, buffer, 6) != 0) {
        return -1;
    }

    switch (CTRLB & 0xE0) {
    case 0x00:
        sensitivity = PIOS_HMC5x83_Sensitivity_0_88Ga;
        break;
    case 0x20:
        sensitivity = PIOS_HMC5x83_Sensitivity_1_3Ga;
        break;
    case 0x40:
        sensitivity = PIOS_HMC5x83_Sensitivity_1_9Ga;
        break;
    case 0x60:
        sensitivity = PIOS_HMC5x83_Sensitivity_2_5Ga;
        break;
    case 0x80:
        sensitivity = PIOS_HMC5x83_Sensitivity_4_0Ga;
        break;
    case 0xA0:
        sensitivity = PIOS_HMC5x83_Sensitivity_4_7Ga;
        break;
    case 0xC0:
        sensitivity = PIOS_HMC5x83_Sensitivity_5_6Ga;
        break;
    case 0xE0:
        sensitivity = PIOS_HMC5x83_Sensitivity_8_1Ga;
        break;
    default:
        PIOS_Assert(0);
    }

    for (int i = 0; i < 3; i++) {
        temp   = ((int16_t)((uint16_t)buffer[2 * i] << 8)
                  + buffer[2 * i + 1]) * 1000 / sensitivity;
        out[i] = temp;
    }
    // Data reads out as X,Z,Y
    temp   = out[2];
    out[2] = out[1];
    out[1] = temp;

    // This should not be necessary but for some reason it is coming out of continuous conversion mode
    dev_cfg->Driver->Write(PIOS_HMC5x83_MODE_REG, PIOS_HMC5x83_MODE_CONTINUOUS);

    return 0;
}


/**
 * @brief Read the identification bytes from the HMC5x83 sensor
 * \param[out] uint8_t array of size 4 to store HMC5x83 ID.
 * \return 0 if successful, -1 if not
 */
uint8_t PIOS_HMC5x83_ReadID(uint8_t out[4])
{
    uint8_t retval = dev_cfg->Driver->Read(PIOS_HMC5x83_DATAOUT_IDA_REG, out, 3);

    out[3] = '\0';
    return retval;
}

/**
 * @brief Tells whether new magnetometer readings are available
 * \return true if new data is available
 * \return false if new data is not available
 */
bool PIOS_HMC5x83_NewDataAvailable(void)
{
    return pios_hmc5x83_data_ready;
}

/**
 * @brief Run self-test operation.  Do not call this during operational use!!
 * \return 0 if success, -1 if test failed
 */
int32_t PIOS_HMC5x83_Test(void)
{
    int32_t failed = 0;
    uint8_t registers[3] = { 0, 0, 0 };
    uint8_t status;
    uint8_t ctrl_a_read;
    uint8_t ctrl_b_read;
    uint8_t mode_read;
    int16_t values[3];


    /* Verify that ID matches (HMC5x83 ID is null-terminated ASCII string "H43") */
    char id[4];

    PIOS_HMC5x83_ReadID((uint8_t *)id);
    if ((id[0] != 'H') || (id[1] != '4') || (id[2] != '3')) { // Expect H43
        return -1;
    }

    /* Backup existing configuration */
    if (dev_cfg->Driver->Read(PIOS_HMC5x83_CONFIG_REG_A, registers, 3) != 0) {
        return -1;
    }

    /* Stop the device and read out last value */
    PIOS_DELAY_WaitmS(10);
    if (dev_cfg->Driver->Write(PIOS_HMC5x83_MODE_REG, PIOS_HMC5x83_MODE_IDLE) != 0) {
        return -1;
    }
    if (dev_cfg->Driver->Read(PIOS_HMC5x83_DATAOUT_STATUS_REG, &status, 1) != 0) {
        return -1;
    }
    if (PIOS_HMC5x83_ReadMag(values) != 0) {
        return -1;
    }

    /*
     * Put HMC5x83 into self test mode
     * This is done by placing measurement config into positive (0x01) or negative (0x10) bias
     * and then placing the mode register into single-measurement mode.  This causes the HMC5x83
     * to create an artificial magnetic field of ~1.1 Gauss.
     *
     * If gain were PIOS_HMC5x83_GAIN_2_5, for example, X and Y will read around +766 LSB
     * (1.16 Ga * 660 LSB/Ga) and Z would read around +713 LSB (1.08 Ga * 660 LSB/Ga)
     *
     * Changing measurement config back to PIOS_HMC5x83_MEASCONF_NORMAL will leave self-test mode.
     */
    PIOS_DELAY_WaitmS(10);
    if (dev_cfg->Driver->Write(PIOS_HMC5x83_CONFIG_REG_A, PIOS_HMC5x83_MEASCONF_BIAS_POS | PIOS_HMC5x83_ODR_15) != 0) {
        return -1;
    }
    PIOS_DELAY_WaitmS(10);
    if (dev_cfg->Driver->Write(PIOS_HMC5x83_CONFIG_REG_B, PIOS_HMC5x83_GAIN_8_1) != 0) {
        return -1;
    }
    PIOS_DELAY_WaitmS(10);
    if (dev_cfg->Driver->Write(PIOS_HMC5x83_MODE_REG, PIOS_HMC5x83_MODE_SINGLE) != 0) {
        return -1;
    }

    /* Must wait for value to be updated */
    PIOS_DELAY_WaitmS(200);

    if (PIOS_HMC5x83_ReadMag(values) != 0) {
        return -1;
    }

    /*
       if(abs(values[0] - 766) > 20)
       failed |= 1;
       if(abs(values[1] - 766) > 20)
       failed |= 1;
       if(abs(values[2] - 713) > 20)
       failed |= 1;
     */

    dev_cfg->Driver->Read(PIOS_HMC5x83_CONFIG_REG_A, &ctrl_a_read, 1);
    dev_cfg->Driver->Read(PIOS_HMC5x83_CONFIG_REG_B, &ctrl_b_read, 1);
    dev_cfg->Driver->Read(PIOS_HMC5x83_MODE_REG, &mode_read, 1);
    dev_cfg->Driver->Read(PIOS_HMC5x83_DATAOUT_STATUS_REG, &status, 1);


    /* Restore backup configuration */
    PIOS_DELAY_WaitmS(10);
    if (dev_cfg->Driver->Write(PIOS_HMC5x83_CONFIG_REG_A, registers[0]) != 0) {
        return -1;
    }
    PIOS_DELAY_WaitmS(10);
    if (dev_cfg->Driver->Write(PIOS_HMC5x83_CONFIG_REG_B, registers[1]) != 0) {
        return -1;
    }
    PIOS_DELAY_WaitmS(10);
    if (dev_cfg->Driver->Write(PIOS_HMC5x83_MODE_REG, registers[2]) != 0) {
        return -1;
    }

    return failed;
}

/**
 * @brief IRQ Handler
 */
bool PIOS_HMC5x83_IRQHandler(void)
{
    pios_hmc5x83_data_ready = true;

    return false;
}

#ifdef PIOS_INCLUDE_SPI
int32_t PIOS_HMC5x83_SPI_Read(uint8_t address, uint8_t *buffer, uint8_t len);
int32_t PIOS_HMC5x83_SPI_Write(uint8_t address, uint8_t buffer);

const struct pios_hmc5x83_io_driver PIOS_HMC5x83_SPI_DRIVER = {
    .Read  = PIOS_HMC5x83_SPI_Read,
    .Write = PIOS_HMC5x83_SPI_Write,
};

static int32_t pios_hmc5x83_spi_claim_bus()
{
    if (PIOS_SPI_ClaimBus(dev_port_id) < 0) {
        return -1;
    }
    PIOS_SPI_RC_PinSet(dev_port_id, dev_slave_num, 0);
    return 0;
}

static void pios_hmc5x83_spi_release_bus()
{
    PIOS_SPI_RC_PinSet(dev_port_id, dev_slave_num, 1);
    PIOS_SPI_ReleaseBus(dev_port_id);
}
/**
 * @brief Reads one or more bytes into a buffer
 * \param[in] address HMC5x83 register address (depends on size)
 * \param[out] buffer destination buffer
 * \param[in] len number of bytes which should be read
 * \return 0 if operation was successful
 * \return -1 if error during I2C transfer
 * \return -2 if unable to claim i2c device
 */
int32_t PIOS_HMC5x83_SPI_Read(uint8_t address, uint8_t *buffer, uint8_t len)
{
    if (pios_hmc5x83_spi_claim_bus() < 0) {
        return -1;
    }

    memset(buffer, 0xA5, len);
    PIOS_SPI_TransferByte(dev_port_id, address | PIOS_HMC5x83_SPI_AUTOINCR_FLAG | PIOS_HMC5x83_SPI_READ_FLAG);

    // buffer[0] = address | PIOS_HMC5x83_SPI_AUTOINCR_FLAG | PIOS_HMC5x83_SPI_READ_FLAG;
    /* Copy the transfer data to the buffer */
    if (PIOS_SPI_TransferBlock(dev_port_id, NULL, buffer, len, NULL) < 0) {
        pios_hmc5x83_spi_release_bus();
        return -3;
    }
    pios_hmc5x83_spi_release_bus();
    return 0;
}

/**
 * @brief Writes one or more bytes to the HMC5x83
 * \param[in] address Register address
 * \param[in] buffer source buffer
 * \return 0 if operation was successful
 * \return -1 if error during I2C transfer
 * \return -2 if unable to claim spi device
 */
int32_t PIOS_HMC5x83_SPI_Write(uint8_t address, uint8_t buffer)
{
    if (pios_hmc5x83_spi_claim_bus() < 0) {
        return -1;
    }
    uint8_t data[] = {
        address | PIOS_HMC5x83_SPI_AUTOINCR_FLAG,
        buffer,
    };

    if (PIOS_SPI_TransferBlock(dev_port_id, data, NULL, sizeof(data), NULL) < 0) {
        pios_hmc5x83_spi_release_bus();
        return -2;
    }

    pios_hmc5x83_spi_release_bus();
    return 0;
}
#endif /* ifdef PIOS_INCLUDE_SPI */
#ifdef PIOS_INCLUDE_I2C

int32_t PIOS_HMC5x83_I2C_Read(uint8_t address, uint8_t *buffer, uint8_t len);
int32_t PIOS_HMC5x83_I2C_Write(uint8_t address, uint8_t buffer);

const struct pios_hmc5x83_io_driver PIOS_HMC5x83_I2C_DRIVER = {
    .Read  = PIOS_HMC5x83_I2C_Read,
    .Write = PIOS_HMC5x83_I2C_Write,
};

/**
 * @brief Reads one or more bytes into a buffer
 * \param[in] address HMC5x83 register address (depends on size)
 * \param[out] buffer destination buffer
 * \param[in] len number of bytes which should be read
 * \return 0 if operation was successful
 * \return -1 if error during I2C transfer
 * \return -2 if unable to claim i2c device
 */
int32_t PIOS_HMC5x83_I2C_Read(uint8_t address, uint8_t *buffer, uint8_t len)
{
    uint8_t addr_buffer[] = {
        address,
    };

    const struct pios_i2c_txn txn_list[] = {
        {
            .info = __func__,
            .addr = PIOS_HMC5x83_I2C_ADDR,
            .rw   = PIOS_I2C_TXN_WRITE,
            .len  = sizeof(addr_buffer),
            .buf  = addr_buffer,
        }
        ,
        {
            .info = __func__,
            .addr = PIOS_HMC5x83_I2C_ADDR,
            .rw   = PIOS_I2C_TXN_READ,
            .len  = len,
            .buf  = buffer,
        }
    };

    return PIOS_I2C_Transfer(dev_port_id, txn_list, NELEMENTS(txn_list));
}

/**
 * @brief Writes one or more bytes to the HMC5x83
 * \param[in] address Register address
 * \param[in] buffer source buffer
 * \return 0 if operation was successful
 * \return -1 if error during I2C transfer
 * \return -2 if unable to claim i2c device
 */
int32_t PIOS_HMC5x83_I2C_Write(uint8_t address, uint8_t buffer)
{
    uint8_t data[] = {
        address,
        buffer,
    };

    const struct pios_i2c_txn txn_list[] = {
        {
            .info = __func__,
            .addr = PIOS_HMC5x83_I2C_ADDR,
            .rw   = PIOS_I2C_TXN_WRITE,
            .len  = sizeof(data),
            .buf  = data,
        }
        ,
    };

    ;
    return PIOS_I2C_Transfer(dev_port_id, txn_list, NELEMENTS(txn_list));
}
#endif /* ifdef PIOS_INCLUDE_I2C */


#endif /* PIOS_INCLUDE_HMC5x83 */

/**
 * @}
 * @}
 */