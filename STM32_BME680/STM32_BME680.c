#include "STM32_BME680.h"

// Handler I2C
static I2C_HandleTypeDef *s_bme680_i2c_handle = NULL;

// Fonctions de communication I2C
static int8_t stm32_bme680_i2c_read(uint8_t dev_id, uint8_t reg_addr, uint8_t *data, uint16_t len)
{
    if (s_bme680_i2c_handle == NULL)
        return BME680_E_NULL_PTR;
    if (HAL_I2C_Mem_Read(s_bme680_i2c_handle, (uint16_t)(dev_id << 1), reg_addr, I2C_MEMADD_SIZE_8BIT, data, len, HAL_MAX_DELAY) == HAL_OK)
    {
        return BME680_OK;
    }
    return BME680_E_COM_FAIL;
}
static int8_t stm32_bme680_i2c_write(uint8_t dev_id, uint8_t reg_addr, uint8_t *data, uint16_t len)
{
    if (s_bme680_i2c_handle == NULL)
        return BME680_E_NULL_PTR;
    if (HAL_I2C_Mem_Write(s_bme680_i2c_handle, (uint16_t)(dev_id << 1), reg_addr, I2C_MEMADD_SIZE_8BIT, data, len, HAL_MAX_DELAY) == HAL_OK)
    {
        return BME680_OK;
    }
    return BME680_E_COM_FAIL;
}

// Fonctions utilitaires générales
static int8_t null_ptr_check(const struct bme680_dev *dev)
{
    int8_t status;

    if ((dev == NULL) || (dev->read == NULL) || (dev->write == NULL) || (dev->delay_ms == NULL))
    {
        status = BME680_E_NULL_PTR;
    }
    else
    {
        status = BME680_OK;
    }

    return status;
}
static int8_t boundary_check(uint8_t *value, uint8_t min, uint8_t max, struct bme680_dev *dev)
{
    int8_t status = BME680_OK;

    if (value != NULL)
    {
        if (*value < min)
        {
            *value = min;
            dev->info_msg |= BME680_I_MIN_CORRECTION;
        }
        if (*value > max)
        {
            *value = max;
            dev->info_msg |= BME680_I_MAX_CORRECTION;
        }
    }
    else
    {
        status = BME680_E_NULL_PTR;
    }

    return status;
}
static uint16_t calculate_tph_measurement_duration(const struct bme680_dev *dev)
{
    uint32_t tph_dur_us; /* Duration in microseconds */
    uint32_t meas_cycles;
    uint8_t os_to_meas_cycles[6] = {0, 1, 2, 4, 8, 16}; /* Values from Bosch BME680 driver */

    if (dev == NULL)
    {
        return 0; /* Or handle error appropriately */
    }

    meas_cycles = os_to_meas_cycles[dev->tph_sett.os_temp];
    meas_cycles += os_to_meas_cycles[dev->tph_sett.os_pres];
    meas_cycles += os_to_meas_cycles[dev->tph_sett.os_hum];

    /* Calculation based on BME680 datasheet section 3.2.4 "Measurement duration" and Bosch driver logic */
    tph_dur_us = meas_cycles * UINT32_C(1963); /* Base measurement time per cycle in us */
    tph_dur_us += UINT32_C(477 * 4);           /* Typical ADC setup time for T & P (477us * 4 for oversampling factors) - approximation */
    tph_dur_us += UINT32_C(477 * 5);           /* Typical ADC setup time for H (477us * 5 for oversampling factors) - approximation */
    tph_dur_us += UINT32_C(500);               /* Typical filter processing time in us */

    /* Convert to milliseconds, rounding up */
    return (uint16_t)((tph_dur_us + 999) / 1000 + 1); /* Add 1ms as a safety margin or for rounding, as in original code */
}

// Fonctions de calcul
static float calc_temperature(uint32_t temp_adc, struct bme680_dev *dev)
{
    float var1 = 0;
    float var2 = 0;
    float calc_temp = 0;

    var1 = ((((float)temp_adc / 16384.0f) - ((float)dev->calib.par_t1 / 1024.0f)) * ((float)dev->calib.par_t2));

    var2 = (((((float)temp_adc / 131072.0f) - ((float)dev->calib.par_t1 / 8192.0f)) *
             (((float)temp_adc / 131072.0f) - ((float)dev->calib.par_t1 / 8192.0f))) *
            ((float)dev->calib.par_t3 * 16.0f));

    dev->calib.t_fine = (var1 + var2);

    calc_temp = ((dev->calib.t_fine) / 5120.0f);

    return calc_temp;
}
static float calc_pressure(uint32_t pres_adc, const struct bme680_dev *dev)
{
    float var1 = 0;
    float var2 = 0;
    float var3 = 0;
    float calc_pres = 0;

    var1 = (((float)dev->calib.t_fine / 2.0f) - 64000.0f);
    var2 = var1 * var1 * (((float)dev->calib.par_p6) / (131072.0f));
    var2 = var2 + (var1 * ((float)dev->calib.par_p5) * 2.0f);
    var2 = (var2 / 4.0f) + (((float)dev->calib.par_p4) * 65536.0f);
    var1 = (((((float)dev->calib.par_p3 * var1 * var1) / 16384.0f) + ((float)dev->calib.par_p2 * var1)) / 524288.0f);
    var1 = ((1.0f + (var1 / 32768.0f)) * ((float)dev->calib.par_p1));
    calc_pres = (1048576.0f - ((float)pres_adc));

    if ((int)var1 != 0)
    {
        calc_pres = (((calc_pres - (var2 / 4096.0f)) * 6250.0f) / var1);
        var1 = (((float)dev->calib.par_p9) * calc_pres * calc_pres) / 2147483648.0f;
        var2 = calc_pres * (((float)dev->calib.par_p8) / 32768.0f);
        var3 = ((calc_pres / 256.0f) * (calc_pres / 256.0f) * (calc_pres / 256.0f) * (dev->calib.par_p10 / 131072.0f));
        calc_pres = (calc_pres + (var1 + var2 + var3 + ((float)dev->calib.par_p7 * 128.0f)) / 16.0f);
    }
    else
    {
        calc_pres = 0;
    }

    return calc_pres;
}
static float calc_humidity(uint16_t hum_adc, const struct bme680_dev *dev)
{
    float calc_hum = 0;
    float var1 = 0;
    float var2 = 0;
    float var3 = 0;
    float var4 = 0;
    float temp_comp;

    temp_comp = ((dev->calib.t_fine) / 5120.0f);

    var1 = (float)((float)hum_adc) - (((float)dev->calib.par_h1 * 16.0f) + (((float)dev->calib.par_h3 / 2.0f) * temp_comp));

    var2 = var1 * ((float)(((float)dev->calib.par_h2 / 262144.0f) * (1.0f + (((float)dev->calib.par_h4 / 16384.0f) * temp_comp) + (((float)dev->calib.par_h5 / 1048576.0f) * temp_comp * temp_comp))));

    var3 = (float)dev->calib.par_h6 / 16384.0f;

    var4 = (float)dev->calib.par_h7 / 2097152.0f;

    calc_hum = var2 + ((var3 + (var4 * temp_comp)) * var2 * var2);

    if (calc_hum > 100.0f)
        calc_hum = 100.0f;
    else if (calc_hum < 0.0f)
        calc_hum = 0.0f;

    return calc_hum;
}
static float calc_gas_resistance(uint16_t gas_res_adc, uint8_t gas_range, const struct bme680_dev *dev)
{
    float calc_gas_res;
    float var1 = 0;
    float var2 = 0;
    float var3 = 0;

    const float lookup_k1_range[16] = {
        0.0, 0.0, 0.0, 0.0, 0.0, -1.0, 0.0, -0.8,
        0.0, 0.0, -0.2, -0.5, 0.0, -1.0, 0.0, 0.0};
    const float lookup_k2_range[16] = {
        0.0, 0.0, 0.0, 0.0, 0.1, 0.7, 0.0, -0.8,
        -0.1, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};

    var1 = (1340.0f + (5.0f * dev->calib.range_sw_err));
    var2 = (var1) * (1.0f + lookup_k1_range[gas_range] / 100.0f);
    var3 = 1.0f + (lookup_k2_range[gas_range] / 100.0f);

    calc_gas_res = 1.0f / (float)(var3 * (0.000000125f) * (float)(1 << gas_range) * (((((float)gas_res_adc) - 512.0f) / var2) + 1.0f));

    return calc_gas_res;
}
static float calc_heater_res(uint16_t temp, const struct bme680_dev *dev)
{
    float var1 = 0;
    float var2 = 0;
    float var3 = 0;
    float var4 = 0;
    float var5 = 0;
    float res_heat = 0;

    if (temp > 400)
        temp = 400;

    var1 = (((float)dev->calib.par_gh1 / (16.0f)) + 49.0f);
    var2 = ((((float)dev->calib.par_gh2 / (32768.0f)) * (0.0005f)) + 0.00235f);
    var3 = ((float)dev->calib.par_gh3 / (1024.0f));
    var4 = (var1 * (1.0f + (var2 * (float)temp)));
    var5 = (var4 + (var3 * (float)dev->amb_temp));
    res_heat = (uint8_t)(3.4f * ((var5 * (4 / (4 + (float)dev->calib.res_heat_range)) *
                                  (1 / (1 + ((float)dev->calib.res_heat_val * 0.002f)))) -
                                 25));

    return res_heat;
}
static uint8_t calc_heater_dur(uint16_t dur)
{
    uint8_t factor = 0;
    uint8_t durval;

    if (dur >= 0xfc0)
    {
        durval = 0xff;
    }
    else
    {
        while (dur > 0x3F)
        {
            dur = dur / 4;
            factor += 1;
        }
        durval = (uint8_t)(dur + (factor * 64));
    }

    return durval;
}

// Fonctions liées à la configuration
static int8_t read_field_data(struct bme680_field_data *data, struct bme680_dev *dev)
{
    int8_t status;
    uint8_t buff[BME680_FIELD_LENGTH] = {0};
    uint8_t gas_range;
    uint32_t adc_temp;
    uint32_t adc_pres;
    uint16_t adc_hum;
    uint16_t adc_gas_res;
    uint8_t tries = 10;

    status = null_ptr_check(dev);
    do
    {
        if (status == BME680_OK)
        {
            status = bme680_get_regs(((uint8_t)(BME680_FIELD0_ADDR)), buff, (uint16_t)BME680_FIELD_LENGTH,
                                     dev);

            data->status = buff[0] & BME680_NEW_DATA_MSK;
            data->gas_index = buff[0] & BME680_GAS_INDEX_MSK;
            data->meas_index = buff[1];

            adc_pres = (uint32_t)(((uint32_t)buff[2] * 4096) | ((uint32_t)buff[3] * 16) | ((uint32_t)buff[4] / 16));
            adc_temp = (uint32_t)(((uint32_t)buff[5] * 4096) | ((uint32_t)buff[6] * 16) | ((uint32_t)buff[7] / 16));
            adc_hum = (uint16_t)(((uint32_t)buff[8] * 256) | (uint32_t)buff[9]);
            adc_gas_res = (uint16_t)((uint32_t)buff[13] * 4 | (((uint32_t)buff[14]) / 64));
            gas_range = buff[14] & BME680_GAS_RANGE_MSK;

            data->status |= buff[14] & BME680_GASM_VALID_MSK;
            data->status |= buff[14] & BME680_HEAT_STAB_MSK;

            if (data->status & BME680_NEW_DATA_MSK)
            {
                data->temperature = calc_temperature(adc_temp, dev);
                data->pressure = calc_pressure(adc_pres, dev);
                data->humidity = calc_humidity(adc_hum, dev);
                data->gas_resistance = calc_gas_resistance(adc_gas_res, gas_range, dev);
                break;
            }
            dev->delay_ms(BME680_POLL_PERIOD_MS);
        }
        tries--;
    } while (tries);

    if (!tries)
        status = BME680_W_NO_NEW_DATA;

    return status;
}
static int8_t get_calib_data(struct bme680_dev *dev)
{
    int8_t status;
    uint8_t coeff_array[BME680_COEFF_SIZE] = {0};
    uint8_t temp_var = 0;

    status = null_ptr_check(dev);
    if (status == BME680_OK)
    {
        status = bme680_get_regs(BME680_COEFF_ADDR1, coeff_array, BME680_COEFF_ADDR1_LEN, dev);
        if (status == BME680_OK)
            status = bme680_get_regs(BME680_COEFF_ADDR2, &coeff_array[BME680_COEFF_ADDR1_LEN], BME680_COEFF_ADDR2_LEN, dev);

        dev->calib.par_t1 = (uint16_t)(BME680_CONCAT_BYTES(coeff_array[BME680_T1_MSB_REG],
                                                           coeff_array[BME680_T1_LSB_REG]));
        dev->calib.par_t2 = (int16_t)(BME680_CONCAT_BYTES(coeff_array[BME680_T2_MSB_REG],
                                                          coeff_array[BME680_T2_LSB_REG]));
        dev->calib.par_t3 = (int8_t)(coeff_array[BME680_T3_REG]);

        dev->calib.par_p1 = (uint16_t)(BME680_CONCAT_BYTES(coeff_array[BME680_P1_MSB_REG],
                                                           coeff_array[BME680_P1_LSB_REG]));
        dev->calib.par_p2 = (int16_t)(BME680_CONCAT_BYTES(coeff_array[BME680_P2_MSB_REG],
                                                          coeff_array[BME680_P2_LSB_REG]));
        dev->calib.par_p3 = (int8_t)coeff_array[BME680_P3_REG];
        dev->calib.par_p4 = (int16_t)(BME680_CONCAT_BYTES(coeff_array[BME680_P4_MSB_REG],
                                                          coeff_array[BME680_P4_LSB_REG]));
        dev->calib.par_p5 = (int16_t)(BME680_CONCAT_BYTES(coeff_array[BME680_P5_MSB_REG],
                                                          coeff_array[BME680_P5_LSB_REG]));
        dev->calib.par_p6 = (int8_t)(coeff_array[BME680_P6_REG]);
        dev->calib.par_p7 = (int8_t)(coeff_array[BME680_P7_REG]);
        dev->calib.par_p8 = (int16_t)(BME680_CONCAT_BYTES(coeff_array[BME680_P8_MSB_REG],
                                                          coeff_array[BME680_P8_LSB_REG]));
        dev->calib.par_p9 = (int16_t)(BME680_CONCAT_BYTES(coeff_array[BME680_P9_MSB_REG],
                                                          coeff_array[BME680_P9_LSB_REG]));
        dev->calib.par_p10 = (uint8_t)(coeff_array[BME680_P10_REG]);

        dev->calib.par_h1 = (uint16_t)(((uint16_t)coeff_array[BME680_H1_MSB_REG] << BME680_HUM_REG_SHIFT_VAL) | (coeff_array[BME680_H1_LSB_REG] & BME680_BIT_H1_DATA_MSK));
        dev->calib.par_h2 = (uint16_t)(((uint16_t)coeff_array[BME680_H2_MSB_REG] << BME680_HUM_REG_SHIFT_VAL) | ((coeff_array[BME680_H2_LSB_REG]) >> BME680_HUM_REG_SHIFT_VAL));
        dev->calib.par_h3 = (int8_t)coeff_array[BME680_H3_REG];
        dev->calib.par_h4 = (int8_t)coeff_array[BME680_H4_REG];
        dev->calib.par_h5 = (int8_t)coeff_array[BME680_H5_REG];
        dev->calib.par_h6 = (uint8_t)coeff_array[BME680_H6_REG];
        dev->calib.par_h7 = (int8_t)coeff_array[BME680_H7_REG];

        dev->calib.par_gh1 = (int8_t)coeff_array[BME680_GH1_REG];
        dev->calib.par_gh2 = (int16_t)(BME680_CONCAT_BYTES(coeff_array[BME680_GH2_MSB_REG],
                                                           coeff_array[BME680_GH2_LSB_REG]));
        dev->calib.par_gh3 = (int8_t)coeff_array[BME680_GH3_REG];

        if (status == BME680_OK)
        {
            status = bme680_get_regs(BME680_ADDR_RES_HEAT_RANGE_ADDR, &temp_var, 1, dev);

            dev->calib.res_heat_range = ((temp_var & BME680_RHRANGE_MSK) / 16);
            if (status == BME680_OK)
            {
                status = bme680_get_regs(BME680_ADDR_RES_HEAT_VAL_ADDR, &temp_var, 1, dev);

                dev->calib.res_heat_val = (int8_t)temp_var;
                if (status == BME680_OK)
                    status = bme680_get_regs(BME680_ADDR_RANGE_SW_ERR_ADDR, &temp_var, 1, dev);
            }
        }
        dev->calib.range_sw_err = ((int8_t)temp_var & (int8_t)BME680_RSERROR_MSK) / 16;
    }

    return status;
}
static int8_t set_gas_config(struct bme680_dev *dev)
{
    int8_t status;

    status = null_ptr_check(dev);
    if (status == BME680_OK)
    {

        uint8_t reg_addr[2] = {0};
        uint8_t reg_data[2] = {0};

        if (dev->power_mode == BME680_FORCED_MODE)
        {
            reg_addr[0] = BME680_RES_HEAT0_ADDR;
            reg_data[0] = calc_heater_res(dev->gas_sett.heatr_temp, dev);
            reg_addr[1] = BME680_GAS_WAIT0_ADDR;
            reg_data[1] = calc_heater_dur(dev->gas_sett.heatr_dur);
            dev->gas_sett.nb_conv = 0;
        }
        else
        {
            status = BME680_W_DEFINE_PWR_MODE;
        }
        if (status == BME680_OK)
            status = bme680_set_regs(reg_addr, reg_data, 2, dev);
    }

    return status;
}
static int8_t get_gas_config(struct bme680_dev *dev)
{
    int8_t status;
    uint8_t reg_addr1 = BME680_ADDR_SENS_CONF_START;
    uint8_t reg_addr2 = BME680_ADDR_GAS_CONF_START;
    uint8_t reg_data = 0;

    status = null_ptr_check(dev);
    if (status == BME680_OK)
    {
        status = bme680_get_regs(reg_addr1, &reg_data, 1, dev);
        if (status == BME680_OK)
        {
            dev->gas_sett.heatr_temp = reg_data;
            status = bme680_get_regs(reg_addr2, &reg_data, 1, dev);
            if (status == BME680_OK)
            {
                dev->gas_sett.heatr_dur = reg_data;
            }
        }
    }

    return status;
}
static int8_t apply_initial_configurations(struct bme680_dev *dev, const BME680_InitialSettings *initial_conf)
{
    int8_t status;
    uint16_t desired_settings_mask = BME680_OST_SEL | BME680_OSP_SEL | BME680_OSH_SEL | BME680_FILTER_SEL | BME680_GAS_SENSOR_SEL;

    if (initial_conf == NULL)
    {
        dev->power_mode = BME680_FORCED_MODE;
    }
    else
    {
        dev->tph_sett = initial_conf->tph_sett;
        dev->gas_sett = initial_conf->gas_sett;
        dev->power_mode = initial_conf->power_mode;
    }

    status = bme680_set_sensor_settings(desired_settings_mask, dev);
    if (status != BME680_OK)
    {
        return status;
    }

    return bme680_set_sensor_mode(dev);
}
static int8_t sensor_init(struct bme680_dev *dev, uint8_t i2c_addr, const BME680_InitialSettings *settings)
{
    int8_t status = BME680_E_DEV_NOT_FOUND;
    int attempt_count = 0;

    dev->dev_id = i2c_addr;
    while (attempt_count < 3)
    {
        status = bme680_init(dev);
        if (status == BME680_OK)
        {
            /* Sensor found and basic init OK, now apply further configurations */
            status = apply_initial_configurations(dev, settings);
            return status; /* Return status of apply_initial_configurations */
        }
        attempt_count++;
        dev->delay_ms(100); /* Wait before retrying */
    }
    return status; /* Return last error status after all attempts */
}

// Fonctions de lecture et d'écriture des registres
int8_t bme680_get_regs(uint8_t reg_addr, uint8_t *reg_data, uint16_t len, struct bme680_dev *dev)
{
    int8_t status;
    status = null_ptr_check(dev);
    if (status == BME680_OK)
    {
        dev->com_status = dev->read(dev->dev_id, reg_addr, reg_data, len);
        if (dev->com_status != 0)
            status = BME680_E_COM_FAIL;
    }
    return status;
}
int8_t bme680_set_regs(const uint8_t *reg_addr, const uint8_t *reg_data, uint8_t len, struct bme680_dev *dev)
{
    int8_t status;
    uint16_t index;
    status = null_ptr_check(dev);
    if (status == BME680_OK)
    {
        if (len > 0)
        {
            /* Iterate and write each register-value pair */
            for (index = 0; index < len; index++)
            {
                dev->com_status = dev->write(dev->dev_id, reg_addr[index], (uint8_t *)&reg_data[index], 1);
                if (dev->com_status != BME680_OK)
                {
                    status = BME680_E_COM_FAIL;
                    break; /* Exit loop on first error */
                }
            }
        }
        else
        {
            /* Or handle len = 0 as no-op or error, BME680_E_INVALID_LENGTH if len is 0 or too large */
            /* Current Bosch API does not specify behavior for len = 0, assuming it's an invalid length */
            status = BME680_E_COM_FAIL;
        }
    }
    return status;
}

// Fonctions de configuration du capteur
int8_t bme680_set_sensor_settings(uint16_t desired_settings, struct bme680_dev *dev)
{
    int8_t status;
    uint8_t reg_addr;
    uint8_t data = 0;
    uint8_t count = 0;
    uint8_t reg_array[BME680_REG_BUFFER_LENGTH] = {0};
    uint8_t data_array[BME680_REG_BUFFER_LENGTH] = {0};
    uint8_t intended_power_mode = dev->power_mode;
    status = null_ptr_check(dev);
    if (status == BME680_OK)
    {
        if (desired_settings & BME680_GAS_MEAS_SEL)
            status = set_gas_config(dev);
        dev->power_mode = BME680_SLEEP_MODE;
        if (status == BME680_OK)
            status = bme680_set_sensor_mode(dev);
        if (desired_settings & BME680_FILTER_SEL)
        {
            status = boundary_check(&dev->tph_sett.filter, BME680_FILTER_SIZE_0, BME680_FILTER_SIZE_127, dev);
            reg_addr = BME680_CONF_ODR_FILT_ADDR;
            if (status == BME680_OK)
                status = bme680_get_regs(reg_addr, &data, 1, dev);
            if (desired_settings & BME680_FILTER_SEL)
                data = BME680_SET_BITS(data, BME680_FILTER, dev->tph_sett.filter);
            reg_array[count] = reg_addr;
            data_array[count] = data;
            count++;
        }
        if (desired_settings & BME680_HCNTRL_SEL)
        {
            status = boundary_check(&dev->gas_sett.heatr_ctrl, BME680_ENABLE_HEATER, BME680_DISABLE_HEATER, dev);
            reg_addr = BME680_CONF_HEAT_CTRL_ADDR;
            if (status == BME680_OK)
                status = bme680_get_regs(reg_addr, &data, 1, dev);
            data = BME680_SET_BITS_POS_0(data, BME680_HCTRL, dev->gas_sett.heatr_ctrl);
            reg_array[count] = reg_addr;
            data_array[count] = data;
            count++;
        }
        if (desired_settings & (BME680_OST_SEL | BME680_OSP_SEL))
        {
            status = boundary_check(&dev->tph_sett.os_temp, BME680_OS_NONE, BME680_OS_16X, dev);
            reg_addr = BME680_CONF_T_P_MODE_ADDR;
            if (status == BME680_OK)
                status = bme680_get_regs(reg_addr, &data, 1, dev);
            if (desired_settings & BME680_OST_SEL)
                data = BME680_SET_BITS(data, BME680_OST, dev->tph_sett.os_temp);
            if (desired_settings & BME680_OSP_SEL)
                data = BME680_SET_BITS(data, BME680_OSP, dev->tph_sett.os_pres);
            reg_array[count] = reg_addr;
            data_array[count] = data;
            count++;
        }
        if (desired_settings & BME680_OSH_SEL)
        {
            status = boundary_check(&dev->tph_sett.os_hum, BME680_OS_NONE, BME680_OS_16X, dev);
            reg_addr = BME680_CONF_OS_H_ADDR;
            if (status == BME680_OK)
                status = bme680_get_regs(reg_addr, &data, 1, dev);
            data = BME680_SET_BITS_POS_0(data, BME680_OSH, dev->tph_sett.os_hum);
            reg_array[count] = reg_addr;
            data_array[count] = data;
            count++;
        }
        if (desired_settings & (BME680_RUN_GAS_SEL | BME680_NBCONV_SEL))
        {
            status = boundary_check(&dev->gas_sett.run_gas, BME680_RUN_GAS_DISABLE, BME680_RUN_GAS_ENABLE, dev);
            if (status == BME680_OK)
            {
                status = boundary_check(&dev->gas_sett.nb_conv, BME680_NBCONV_MIN, BME680_NBCONV_MAX, dev);
            }
            reg_addr = BME680_CONF_ODR_RUN_GAS_NBC_ADDR;
            if (status == BME680_OK)
                status = bme680_get_regs(reg_addr, &data, 1, dev);
            if (desired_settings & BME680_RUN_GAS_SEL)
                data = BME680_SET_BITS(data, BME680_RUN_GAS, dev->gas_sett.run_gas);
            if (desired_settings & BME680_NBCONV_SEL)
                data = BME680_SET_BITS_POS_0(data, BME680_NBCONV, dev->gas_sett.nb_conv);
            reg_array[count] = reg_addr;
            data_array[count] = data;
            count++;
        }
        if (status == BME680_OK)
            status = bme680_set_regs(reg_array, data_array, count, dev);
        dev->power_mode = intended_power_mode;
    }
    return status;
}
int8_t bme680_get_sensor_settings(uint16_t desired_settings, struct bme680_dev *dev)
{
    int8_t status;
    /* starting address of the register array for burst read*/
    uint8_t reg_addr = BME680_CONF_HEAT_CTRL_ADDR;
    uint8_t data_array[BME680_REG_BUFFER_LENGTH] = {0};

    /* Check for null pointer in the device structure*/
    status = null_ptr_check(dev);
    if (status == BME680_OK)
    {
        status = bme680_get_regs(reg_addr, data_array, BME680_REG_BUFFER_LENGTH, dev);

        if (status == BME680_OK)
        {
            if (desired_settings & BME680_GAS_MEAS_SEL)
                status = get_gas_config(dev);

            /* get the T,P,H ,Filter,ODR settings here */
            if (desired_settings & BME680_FILTER_SEL)
                dev->tph_sett.filter = BME680_GET_BITS(data_array[BME680_REG_FILTER_INDEX],
                                                       BME680_FILTER);

            if (desired_settings & (BME680_OST_SEL | BME680_OSP_SEL))
            {
                dev->tph_sett.os_temp = BME680_GET_BITS(data_array[BME680_REG_TEMP_INDEX], BME680_OST);
                dev->tph_sett.os_pres = BME680_GET_BITS(data_array[BME680_REG_PRES_INDEX], BME680_OSP);
            }

            if (desired_settings & BME680_OSH_SEL)
                dev->tph_sett.os_hum = BME680_GET_BITS_POS_0(data_array[BME680_REG_HUM_INDEX],
                                                             BME680_OSH);

            /* get the gas related settings */
            if (desired_settings & BME680_HCNTRL_SEL)
                dev->gas_sett.heatr_ctrl = BME680_GET_BITS_POS_0(data_array[BME680_REG_HCTRL_INDEX],
                                                                 BME680_HCTRL);

            if (desired_settings & (BME680_RUN_GAS_SEL | BME680_NBCONV_SEL))
            {
                dev->gas_sett.nb_conv = BME680_GET_BITS_POS_0(data_array[BME680_REG_NBCONV_INDEX],
                                                              BME680_NBCONV);
                dev->gas_sett.run_gas = BME680_GET_BITS(data_array[BME680_REG_RUN_GAS_INDEX],
                                                        BME680_RUN_GAS);
            }
        }
    }
    else
    {
        status = BME680_E_NULL_PTR;
    }

    return status;
}
int8_t BME680_Config_Advanced(struct bme680_dev *dev, struct bme680_tph_sett *tph_settings, struct bme680_gas_sett *gas_settings, uint8_t power_mode)
{
    uint16_t desired_settings = BME680_OST_SEL | BME680_OSP_SEL | BME680_OSH_SEL | BME680_FILTER_SEL | BME680_GAS_SENSOR_SEL;

    dev->tph_sett = *tph_settings;
    dev->gas_sett = *gas_settings;
    dev->power_mode = power_mode;

    return bme680_set_sensor_settings(desired_settings, dev);
}
void bme680_set_profile_dur(uint16_t duration, struct bme680_dev *dev)
{
    uint16_t tph_profile_duration;

    tph_profile_duration = calculate_tph_measurement_duration(dev);
    dev->gas_sett.heatr_dur = duration - tph_profile_duration;
}
void bme680_get_profile_dur(uint16_t *duration, const struct bme680_dev *dev)
{
    uint16_t tph_profile_duration;

    if (duration == NULL || dev == NULL)
    {
        return; /* Or handle error appropriately */
    }

    tph_profile_duration = calculate_tph_measurement_duration(dev);

    *duration = tph_profile_duration;

    if (dev->gas_sett.run_gas)
    {
        *duration += dev->gas_sett.heatr_dur;
    }
}

// Fonction de mise en veille du capteur
int8_t bme680_set_sensor_mode(struct bme680_dev *dev)
{
    int8_t status;
    uint8_t tmp_pow_mode;
    uint8_t pow_mode = 0;
    uint8_t reg_addr = BME680_CONF_T_P_MODE_ADDR;

    status = null_ptr_check(dev);
    if (status == BME680_OK)
    {
        do
        {
            status = bme680_get_regs(BME680_CONF_T_P_MODE_ADDR, &tmp_pow_mode, 1, dev);
            if (status == BME680_OK)
            {
                pow_mode = (tmp_pow_mode & BME680_MODE_MSK);

                if (pow_mode != BME680_SLEEP_MODE)
                {
                    tmp_pow_mode = tmp_pow_mode & (~BME680_MODE_MSK);
                    status = bme680_set_regs(&reg_addr, &tmp_pow_mode, 1, dev);
                    dev->delay_ms(BME680_POLL_PERIOD_MS);
                }
            }
        } while (pow_mode != BME680_SLEEP_MODE);

        if (dev->power_mode != BME680_SLEEP_MODE)
        {
            tmp_pow_mode = (tmp_pow_mode & ~BME680_MODE_MSK) | (dev->power_mode & BME680_MODE_MSK);
            if (status == BME680_OK)
                status = bme680_set_regs(&reg_addr, &tmp_pow_mode, 1, dev);
        }
    }

    return status;
}
int8_t bme680_get_sensor_mode(struct bme680_dev *dev)
{
    int8_t status;
    uint8_t mode;

    status = null_ptr_check(dev);
    if (status == BME680_OK)
    {
        status = bme680_get_regs(BME680_CONF_T_P_MODE_ADDR, &mode, 1, dev);
        dev->power_mode = mode & BME680_MODE_MSK;
    }

    return status;
}

// Fonction de lecture des données du capteur
int8_t bme680_get_sensor_data(struct bme680_field_data *data, struct bme680_dev *dev)
{
    int8_t status;

    status = null_ptr_check(dev);
    if (status == BME680_OK)
    {
        status = read_field_data(data, dev);
        if (status == BME680_OK)
        {
            if (data->status & BME680_NEW_DATA_MSK)
                dev->new_fields = 1;
            else
                dev->new_fields = 0;
        }
    }

    return status;
}

// Fonction de démarrage du capteur
int8_t bme680_soft_reset(struct bme680_dev *dev)
{
    int8_t status;
    uint8_t reg_addr = BME680_SOFT_RESET_ADDR;
    uint8_t soft_rst_cmd = BME680_SOFT_RESET_CMD;
    status = null_ptr_check(dev);
    if (status == BME680_OK)
    {
        status = bme680_set_regs(&reg_addr, &soft_rst_cmd, 1, dev);
        dev->delay_ms(BME680_RESET_PERIOD);
    }
    return status;
}
int8_t bme680_init(struct bme680_dev *dev)
{
    int8_t status;
    status = null_ptr_check(dev);
    if (status == BME680_OK)
    {
        status = bme680_soft_reset(dev);
        if (status == BME680_OK)
        {
            status = bme680_get_regs(BME680_CHIP_ID_ADDR, &dev->chip_id, 1, dev);
            if (status == BME680_OK)
            {
                if (dev->chip_id == BME680_CHIP_ID)
                {
                    status = get_calib_data(dev);
                    if (status == BME680_OK)
                    {
                        dev->tph_sett.os_hum = BME680_OS_1X;
                        dev->tph_sett.os_pres = BME680_OS_1X;
                        dev->tph_sett.os_temp = BME680_OS_1X;
                        dev->tph_sett.filter = BME680_FILTER_SIZE_0;
                        dev->gas_sett.run_gas = BME680_RUN_GAS_DISABLE;
                        dev->gas_sett.nb_conv = 0;
                        dev->gas_sett.heatr_temp = 0;
                        dev->gas_sett.heatr_dur = 0;
                    }
                }
                else
                {
                    status = BME680_E_DEV_NOT_FOUND;
                }
            }
        }
    }
    return status;
}
int8_t BME680_Start(struct bme680_dev *dev, I2C_HandleTypeDef *i2c_handle,
                    bme680_delay_fptr_t delay_fptr, const BME680_InitialSettings *settings)
{
    int8_t status = BME680_E_DEV_NOT_FOUND;
    /* int attempt_count = 0; // No longer needed here */

    if (!dev || !i2c_handle || !delay_fptr)
    {
        return BME680_E_NULL_PTR;
    }

    s_bme680_i2c_handle = i2c_handle;
    dev->hi2c = i2c_handle;
    dev->read = stm32_bme680_i2c_read;
    dev->write = stm32_bme680_i2c_write;
    dev->delay_ms = delay_fptr;
    dev->intf = BME680_I2C_INTF;

    /* Try initializing with the primary I2C address */
    status = sensor_init(dev, BME680_I2C_ADDR_PRIMARY, settings);
    if (status == BME680_OK)
    {
        return BME680_OK;
    }

    /* If primary address fails, try the secondary I2C address */
    status = sensor_init(dev, BME680_I2C_ADDR_SECONDARY, settings);
    if (status == BME680_OK)
    {
        return BME680_OK;
    }

    /* If both addresses fail, cleanup and return the last error status */
    s_bme680_i2c_handle = NULL; /* Clear the global handle as initialization failed */
    return status;
}

// Fonction pour le calcul de l'IAQ
float waterSatDensity(float temp)
{
    return (6.112 * 100 * exp((17.62 * temp) / (243.12 + temp))) / (461.52 * (temp + 273.15));
}
float getIAQ(IAQTracker *tracker, BME680Data bme_data)
{
    float temp = bme_data.temperature;
    float hum = bme_data.humidity;
    float R_gas = bme_data.gas_resistance;

    float rho_max = waterSatDensity(temp);
    float hum_abs = (hum / 100.0f) * 1000.0f * rho_max;

    float comp_gas = R_gas * expf(tracker->slope * hum_abs);

    tracker->gas_cal_data[tracker->gas_cal_data_idx] = comp_gas;
    tracker->gas_cal_data_idx = (tracker->gas_cal_data_idx + 1) % GAS_CAL_DATA_WINDOW_SIZE;
    if (tracker->gas_cal_data_filled_count < GAS_CAL_DATA_WINDOW_SIZE)
    {
        tracker->gas_cal_data_filled_count++;
    }

    if (tracker->gas_cal_data_filled_count > 0)
    {
        float sum = 0.0f;
        for (int i = 0; i < tracker->gas_cal_data_filled_count; i++)
        {
            sum += tracker->gas_cal_data[i];
        }
        tracker->gas_baseline = sum / tracker->gas_cal_data_filled_count;
    }
    else
    {
        tracker->gas_baseline = comp_gas;
    }

    if (tracker->burn_in_cycles_remaining > 0)
    {
        tracker->burn_in_cycles_remaining--;
        return NAN;
    }
    else
    {
        if (tracker->gas_baseline == 0.0f)
        {
            return (comp_gas == 0.0f) ? 0.0f : 100.0f;
        }

        float ratio = comp_gas / tracker->gas_baseline;
        float iaq_contribution_gas = powf(ratio, 2.0f);

        float AQ = (1.0f - fminf(iaq_contribution_gas, 1.0f)) * 100.0f;

        if (AQ < 0.0f)
            AQ = 0.0f;
        if (AQ > 100.0f)
            AQ = 100.0f;

        return AQ;
    }
}
const char *getIAQCategory(float iaq)
{
    if (iaq <= 20)
    {
        return "Excellent";
    }
    else if (iaq <= 40)
    {
        return "Bon";
    }
    else if (iaq <= 60)
    {
        return "Modéré";
    }
    else if (iaq <= 80)
    {
        return "Médiocre";
    }
    else
    {
        return "Mauvais";
    }
}
void initIAQTracker(IAQTracker *tracker, int burn_in_total_cycles, float ph_slope)
{
    tracker->slope = ph_slope;
    tracker->burn_in_cycles_remaining = burn_in_total_cycles;
    for (int i = 0; i < GAS_CAL_DATA_WINDOW_SIZE; ++i)
    {
        tracker->gas_cal_data[i] = 0.0f;
    }
    tracker->gas_cal_data_idx = 0;
    tracker->gas_cal_data_filled_count = 0;
    tracker->gas_baseline = 0.0f;
}

// Fonction de temporisation
void user_delay_ms(uint32_t period)
{
    HAL_Delay(period);
}
