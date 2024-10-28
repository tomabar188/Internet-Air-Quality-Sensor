
#include "driver/spi_master.h"


uint64_t Symbol(int num);//

esp_err_t spi_init();

void set_row(int segm,uint8_t reg, uint8_t value);

esp_err_t clear();

esp_err_t max7219_init();

esp_err_t DisplayNumber(int num);

esp_err_t showPM10();

esp_err_t showPM25();


