#include "bh1750_handler.h"

#include <string.h>

#include "bh1750.h"

static const char* TAG = "bh1750_handler";

#define CAPS_I2C_BUS I2C_NUM_1
#define CAPS_BH1750_I2C CAPS_I2C_BUS
#define CAPS_BH1750_SCL GPIO_NUM_22
#define CAPS_BH1750_SDA GPIO_NUM_21
#define CAPS_BH1750_ADDR BH1750_ADDR_LO

TaskHandle_t bh1750_task = NULL;

i2c_dev_t dev;
uint16_t lux;

uint16_t caps_bh1750_measure(void) {
    if (bh1750_read(&dev, &lux) == ESP_OK) {
        return lux;
    }
    return 0;
}

void caps_bh1750_i2c_init(void) { ESP_ERROR_CHECK(i2cdev_init()); }

void caps_bh1750_init(void) {
    memset(&dev, 0, sizeof(i2c_dev_t));  // Zero descriptor
    ESP_ERROR_CHECK(bh1750_init_desc(&dev, CAPS_BH1750_ADDR, CAPS_BH1750_I2C, CAPS_BH1750_SDA, CAPS_BH1750_SCL));
    ESP_ERROR_CHECK(bh1750_setup(&dev, BH1750_MODE_CONTINUOUS, BH1750_RES_HIGH));
}