#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include <math.h>
#include "ledmatrix7219.h"


#define MOSI_PIN 23
#define CS_PIN 5
#define CLK_PIN 18

#define DECODE     0x09
#define INTENSITY       0x0A
#define SCAN_LIMIT      0x0B
#define SHUTDOWN        0x0C
#define DISPLAY_TEST    0x0F
#define ALL_SEGM 5

spi_device_handle_t spi;

uint64_t Symbol(int num){ 

switch (num) //funkcja zwracająca kod litery/cyfry
{
case 0:{
    return 0x0018242c34241800;
    break;
    }
case 1:{
    return 0x0038101010181000;
    break;
    }
case 2:{
    return 0x003c081024241800;
    break;
    }
case 3:{
    return 0x0018242010241800;
    break;
    }
case 4:{
    return 0x0010103c14181000;
    break;
    }
case 5:{
    return 0x001c20201c043c00;
    break;
    }
case 6:{
    return 0x0018241c04241800;
    break;
    }
case 7:{
    return 0x0008081020203c00;
    break;
    }
case 8:{
    return 0x0018242418241800;
    break;
    }
case 9:{
    return 0x0018203824241800;
    break;
    }
case 10:{
    return 0x0004041c24241c00; //P
    break;
    }
case 11:{
    return 0x004242425a664200; //M
    break;
    }
case 12:{
    return 0x00bc881024241800; //2.
    break;
    }
case 13:{
    return 0x001d21201c043c00; //.5
    break;
    }

default:
    return 0x0;
    break;
}
return 0x0;
}




esp_err_t spi_init() { //inicjalizacja SPI
    
    
    
    spi_bus_config_t spibus={
        .miso_io_num = -1,
        .mosi_io_num = MOSI_PIN,
        .sclk_io_num = CLK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 32,
    };

    
        
    spi_device_interface_config_t spidev={
        .clock_speed_hz = 1000000,  // 1 MHz
        .mode = 0,                  //SPI mode 0
        .spics_io_num = CS_PIN,     
        .queue_size = 1,
        .flags = SPI_DEVICE_HALFDUPLEX,
        .pre_cb = NULL,
        .post_cb = NULL,
    };

   spi_bus_initialize(SPI2_HOST, &spibus, SPI_DMA_CH_AUTO);
   return spi_bus_add_device(SPI2_HOST, &spidev, &spi);
};




void set_row(int segm,uint8_t reg, uint8_t value) {
    uint8_t tx_data[8] ={ 0 };
    
    if(segm==ALL_SEGM){
        for(int i=0;i<4;i++){
            tx_data[i*2]=reg;
            tx_data[i*2+1]=value;
        }
    }
    else{
        tx_data[segm*2]=reg;
            tx_data[segm*2+1]=value;
    }

    spi_transaction_t print = {
        .tx_buffer = tx_data,
        .length = 16*4
    };
    ESP_ERROR_CHECK(spi_device_transmit(spi, &print));
}



esp_err_t clear() {
   for (int i = 0; i < 8; i++) {
    set_row(ALL_SEGM,i + 1, 0x0000);
  }

  return ESP_OK;
}

esp_err_t max7219_init() {
    set_row(ALL_SEGM,DISPLAY_TEST, 0); //
    set_row(ALL_SEGM,SCAN_LIMIT, 7); //ze względu na wyświetlacz matrycowy
    set_row(ALL_SEGM,DECODE, 0);
    set_row(ALL_SEGM,SHUTDOWN, 1);
    set_row(ALL_SEGM,INTENSITY, 1);
    
    clear();
    

    return ESP_OK;
}

esp_err_t DisplayNumber(int num){

    int Numbers = (int)log10(num) + 1; //ilość cyfr w liczbie
    

    switch (Numbers)
    {
    case 1: //jedna cyfra
        {
         for (int j=0;j<8;j++) 
            set_row(0,8-j, (Symbol(num)>>(8*j))); //na ostatnim segmencie wyświetl wartość 
            
        
        break;}
    case 2:
        {
            for (int j=0;j<8;j++){
            set_row(0,8-j, (Symbol(num%10)>>(8*j)));
            set_row(1,8-j, (Symbol((num-(num%10))/10)>>(8*j)));
            }
        
        break;}
    case 3:
        {
            for (int j=0;j<8;j++){
            set_row(0,8-j, (Symbol(num%10)>>(8*j)));
            set_row(1,8-j, (Symbol(((num%100)-(num%10))/10)>>(8*j)));
            set_row(2,8-j, (Symbol((num-(num%100))/100)>>(8*j)));
            }
        
        break;}
    case 4:
        {
            for (int j=0;j<8;j++){
            set_row(0,8-j, (Symbol(num%10)>>(8*j)));
            set_row(1,8-j, (Symbol(((num%100)-(num%10))/10)>>(8*j)));
            set_row(2,8-j, (Symbol(((num-(num%100))/100)%10)>>(8*j)));
            set_row(3,8-j, (Symbol((num-(num%1000))/1000)>>(8*j)));
            }
        
        break;}
    

    default:
        break;
    }
    

    return ESP_OK;
}




esp_err_t showPM10() {

clear();

for (int j=0;j<8;j++){
    
    set_row(0,8-j, (Symbol(0)>>(8*j)));
    set_row(1,8-j, (Symbol(1)>>(8*j)));
    set_row(2,8-j, (Symbol(11)>>(8*j)));
    set_row(3,8-j, (Symbol(10)>>(8*j)));
    }

return ESP_OK;
}


esp_err_t showPM25() {

clear();

for (int j=0;j<8;j++){
    
    set_row(0,8-j, (Symbol(13)>>(8*j)));
    set_row(1,8-j, (Symbol(12)>>(8*j)));
    set_row(2,8-j, (Symbol(11)>>(8*j)));
    set_row(3,8-j, (Symbol(10)>>(8*j)));
    }

return ESP_OK;
}


