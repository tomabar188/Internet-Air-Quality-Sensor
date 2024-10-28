#include <stdio.h>
//#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_http_client.h"
//#include "esp_http_server.h"
#include "ledmatrix7219.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#define WIFI_SSID "AP354"
#define WIFI_PASS "VFR$5tgb"
#define SERVER_URL_POST "http://157.158.168.83:8085/post"
#define SERVER_URL_CHCK_CONN "http://157.158.168.83:8085/check-connection"
#define DISPLAY_TIME 3000
#define MEASUREMENT_TIME 6000
#define RXD_PIN (GPIO_NUM_16)
#define TXD_PIN (GPIO_NUM_17)
#define LOST_CONN_ARRAY_ELEM 288



static const int PMS_BUF = 256;
static bool isDissconnected=true;



uint64_t plusTime; //Czas który upłynął od startu urządzenia do ostatniej synchronizacji czasu z serwerem
static int DataPM[2]={-1};

// ----------------------------------------- Synchronizacja i sprawdzenie połączenia z serwerem node js -----------------------------------------
esp_err_t conn_handler(esp_http_client_event_handle_t evt)
{
    switch (evt->event_id)
    {
    case HTTP_EVENT_ON_DATA:{// w przypadku połączenia
        
        plusTime=(esp_timer_get_time() / 1000000); // ustaw znacznik czasu w którym wykonywana była synchronizacja z serwerem
        isDissconnected=false;
        break;

    }
        
    case HTTP_EVENT_ERROR: // w przypadku braku połączenia z serwerem node js
    {
        printf("brak połączenia");
        isDissconnected=true; 
        break;}

    default:
        break;
    }
    return ESP_OK;
}


static void check_conn() 
{
    esp_http_client_config_t config_get = {
        .url = SERVER_URL_CHCK_CONN,
        .method = HTTP_METHOD_GET,
        .event_handler = conn_handler};
        
    esp_http_client_handle_t client_get = esp_http_client_init(&config_get);

    printf("check_conn\n");
    esp_http_client_perform(client_get);//otwarcie połączenia, wymiana danych 
    printf("2check_conn\n\n");
    esp_http_client_cleanup(client_get);//zamkniecie połączenia
}



// ----------------------------------------- Obsługa zdarzeń WIFI -----------------------------------------
static void wifi_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    switch (event_id)
    {
    case WIFI_EVENT_STA_START:
        esp_wifi_connect(); // połączenie do punktu dostępowego
        break;
    
    /*case WIFI_EVENT_STA_CONNECTED:
        printf("WiFi connected ... \n");
        break;
    */
    case WIFI_EVENT_STA_DISCONNECTED:{//w przypadku rozłączenia 
            isDissconnected=true;
            esp_wifi_connect(); //ponów próbę połączenia
            break;}
    case IP_EVENT_STA_GOT_IP:{
         check_conn(); //uzyskano połączenie z wifi, więc sprawdź czy istnieje połączenie z serwerem, jeśli tak synchronizacja czasu
        break;}
    default:
        break;
    }
}

// ----------------------------------------- Połączenie do  WIFI -----------------------------------------
void wifi_connection()
{
    
    esp_netif_init();                    //inicjowanie stosu TCP/IP
    esp_event_loop_create_default();     // tworzenie domyślnej pętli zdarzeń
    esp_netif_create_default_wifi_sta(); // netif+wifi dla obsługi zdarzeń WiFi przez ESP w trybie klienta
    wifi_init_config_t wifi_initiation = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&wifi_initiation); //inicjowanie sterownika
    

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_handler, NULL); 
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_handler, NULL); 
    wifi_config_t wifi_configuration = {   // skonfigurowanie sterownika WiFi
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS}};
            
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_configuration);
    
    esp_wifi_start();
   
    
}

// ----------------------------------------- Przesyłanie odczytów na serwer node js -----------------------------------------
esp_err_t post_handler(esp_http_client_event_handle_t evt)
{
    switch (evt->event_id)
    {
    case HTTP_EVENT_ON_DATA:
        printf("HTTP_EVENT_ON_DATA: %.*s\n", evt->data_len, (char *)evt->data);
        break;

        default:
        break;
    }
    return ESP_OK;
}




static void post_data(int valuePM10, int valuePM25,uint64_t post_time)
{
   
    esp_http_client_config_t config_post = {
        .url = SERVER_URL_POST,
        .method = HTTP_METHOD_POST,
        .event_handler = post_handler};
        
    
    
   
    esp_http_client_handle_t client_post = esp_http_client_init(&config_post);
   
    

    char post_array[1000]; //Tablica przechowywująca dane, które zostaną wysłane (POST)
    

    

    sprintf(post_array, "{\"valuepm10\" : \"%d\",\"valuepm25\" : \"%d\",\"time\" : \"%lld\" \n}",valuePM10,valuePM25, post_time); //setting array with data and channel
    printf("\n %s \n",post_array);
    esp_http_client_set_post_field(client_post, post_array, strlen(post_array)); //setting 
    
    esp_http_client_set_header(client_post, "Content-Type", "application/json");
    
    esp_http_client_perform(client_post);
     
    esp_http_client_cleanup(client_post);
    
}

// ----------------------------------------- Obsługa poprawności danych wysyłanych na serwer -----------------------------------------
static void send_data(void){

    static int lostConnData[LOST_CONN_ARRAY_ELEM][2] = {0}; //Tablica przechowywująca zaległe dane
    
    static uint64_t lostConnTime[LOST_CONN_ARRAY_ELEM]={0};//Tablica przechowująca czas w którym powinien odbywać się zapis do bazy danych
    

    static int pos=0; //pozycja w tabeli offline
    static int revpos=0; //w przypadku dłuższego bycia off-line - pozycja danych odwróconych w tabeli (By nie przesuwać całej tabeli cały czas)
    static bool tablehelp=false; //czy istnieją zaległe dane

    while(1){
    if(DataPM[0]!=-1 && DataPM[1]!=-1){ //sprawdzenie czy pojawiły się nowe dane z czujnika
        check_conn();//sprawdzenie synchronizacji czasu by uniknąć ewentualnych błędów
    if(isDissconnected==false){ //w przypadku połączenia z serwerem
        if(tablehelp==true){ //w przypadku braku zaległych wpisów (w wyniku utraty połączenia)
            
            if(revpos==0){//jeśli istnieją zaległe dane i nie jest ich więcej niż 288 (24-godziny)
               
                       
                for(int i=0;i<pos;i++){ //pos jest maxymalna wartoscia w tablicy z najaktualniejsza data
                    post_data(lostConnData[i][1],lostConnData[i][0],lostConnTime[i]-plusTime); //żądanie wysłania danych (odczyt PM10, PM2.5, znacznika czasowego)
                                       
                }
                
            }
            else{ //w przypadku kiedy utracone zostanie połączenie na więcej niż 24 godziny tablica będzie wypłeniana od początku
               
                static int lostConnDataHelp[LOST_CONN_ARRAY_ELEM][2]; //utworzenie danych pomocniczych w celu odwrócenia finalnych danych
                static uint64_t lostConnTimeHelp[LOST_CONN_ARRAY_ELEM];
                int x=0;
                for(int i=revpos;i<LOST_CONN_ARRAY_ELEM;i++){//dane najstarsze przekładane są na początek tablicy pomocniczej
                    lostConnDataHelp[x][0]=lostConnData[i][0];
                    lostConnDataHelp[x][1]=lostConnData[i][1];
                    lostConnTimeHelp[x]=lostConnTime[i];
                    x++;
                }
                for(int i=0;i<revpos;i++){ //dane najmłodsze ustawiane są na koniec tablicy pomocniczej
                    
                    lostConnDataHelp[x][0]=lostConnData[i][0];
                    lostConnDataHelp[x][1]=lostConnData[i][1];
                    lostConnTimeHelp[x]=lostConnTime[i];
                    x++;
                }

                for(int i=0;i<LOST_CONN_ARRAY_ELEM;i++){ //żadanie wysłania każdego elementu listy wraz ze znacznikiem czasowym
                post_data(lostConnDataHelp[i][1],lostConnDataHelp[i][0],lostConnTimeHelp[i]-plusTime); 
                
                }
                
                
            }
            
            pos=0; // po wysłaniu wszystkich wartości zmienne pomocnicze ustawiane są na wartości domyślne
            revpos=0;
            tablehelp=false;
        }
        
        
        post_data(DataPM[1],DataPM[0], (esp_timer_get_time() / 1000000)-plusTime);//po wysłaniu zaległych danych, następuje żądanie wysłania aktualnych
        DataPM[0]=-1; //PM 2.5 - brak nowych danych
        DataPM[1]=-1; // PM 10 - brak nowych danych
    }
    else if(pos<LOST_CONN_ARRAY_ELEM){ //w przypadku braku połączenia
        tablehelp=true;//informacja o tym, że w tablicy zaległych danych znajdują się zaległe odczyty
        lostConnData[pos][0]=DataPM[0];//pomiar PM2.5 zapisany do tablicy
        lostConnData[pos][1]=DataPM[1];//pomiar PM10 zapisany do tablicy
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        uint64_t time1 =(esp_timer_get_time() / 1000000); //znacznik czasu dla zaległych odczytów
        lostConnTime[pos]=time1; //zapis znacznika do tablicy znaczników, o odpowiadajacym indeksie
        pos++;//inkrementacja pozycji w tablicy
                
    }
    else if(revpos<LOST_CONN_ARRAY_ELEM){ // w przypadku braku połączenia i przekroczenia limitu danych w tablicy
        lostConnData[revpos][0]=DataPM[0];//pomiar PM2.5 zapisany do tablicy
        lostConnData[revpos][1]=DataPM[1];//pomiar PM10 zapisany do tablicy
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        uint64_t time2 =(esp_timer_get_time() / 1000000);//znacznik czasu dla zaległych odczytów
        lostConnTime[revpos]=time2;//zapis znacznika do tablicy znaczników, o odpowiadajacym indeksie
        revpos++;//inkrementacja pozycji w tablicy
        
    }
    else {//tablica znów przekroczy wartość maksymalną, pozycja znów będzie pozycją startową
        pos=0;
        revpos=0;

        lostConnData[pos][0]=DataPM[0];//pomiar PM2.5 zapisany do tablicy
        lostConnData[pos][1]=DataPM[1];//pomiar PM10 zapisany do tablicy
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        lostConnTime[pos]=(esp_timer_get_time() / 1000000); //znacznik czasu dla zaległych odczytów
        pos++;//inkrementacja pozycji w tablicy
    }

    
    vTaskDelay(294000 / portTICK_PERIOD_MS);
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

// ----------------------------------------- Inicjalizacja Interfejsu UART do obsługi czujnika PMS 5003  -----------------------------------------
void init(void) 
{
    
    const uart_config_t uart_config = { //obiekt konfiguracyjny dla interfejsu UART
        .baud_rate = 9600, //prędkość transmisji - zgodna z prędkością transmisji pms5003
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        
    };

    
    uart_driver_install(UART_NUM_2, PMS_BUF * 2, 0, 0, NULL, 0); //instalacja sterownika interfejsu UART, wraz z alokacją pamięci danych przychodzących
    uart_param_config(UART_NUM_2, &uart_config); //konfiguracja interfejsu UART zgodnie z danymi obiektu konfiguracyjnego
    uart_set_pin(UART_NUM_2, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE); //ustawienie pinów wykorzystywanych w transmisji - z racji użytego pinu RX2 - UART = UART_NUM_2
    
    
    
    
}

// ----------------------------------------- Sprawdzenie sumy kontrolnej i poprawności danych otrzymanych poprzez UART -----------------------------------------
static int checkSum(uint8_t* UART_Data)
{
    

    if (UART_Data[0] != 0x42 || UART_Data[1] != 0x4D || UART_Data[2] != 0x00 || UART_Data[3] != 0x1C){ //sprawdzenie poprawności otrzymanych danych - pierwszy bajt powinien zawsze wynosić 0x42, drugi 0x4d, dwa następne są długością ramek - w tym przypadku stałe 28
        uart_flush_input( UART_NUM_2 ); //w przypadku błędnego odczytu danych występuje opróżnienie bufora danych wejściowych interfejsu UART
       
       vTaskDelay(500 / portTICK_PERIOD_MS);


        for(int i=0;i<32;i++){
             printf(" %x ", UART_Data[i]);
        }
       printf(" \n\n ");
       
       
        return 0;
    }

    
    int checksum = 0;//zmienne pomocnicze do obliczenia i sprawdzenia sumy kontrolnej
    int checksum1; //31 paczka bitów - powinna być równa pierwszemu bajtowi sumy kontrolnej
    int checksum2; //32 paczka bitów - powinna być równa drugiemu bajtowi sumy kontrolnej
    for (int i = 0; i < 30; i++){
        checksum += UART_Data[i]; //Dodanie każdej wartości otrzymanych danych z wyłączeniem dwóch ostatnich (suma kontrolna)
    }
    
    checksum2 = (checksum >> 8) & 0xFF; //wyodrębnienie wartości dla porównania pierwszej ramki sumy kontrolnej 
    checksum1 = checksum & 0xFF; //wyodrębnienie wartości dla porównania drugiej ramki sumy kontrolnej
        
    if (UART_Data[30] != checksum2 || UART_Data[31] != checksum1){ // porównanie otrzymanych bajtów z obliczoną sumą kontrolną
        
        uart_flush_input( UART_NUM_2 ); // w przypadku niezgodności sumy kontrolnej nastepuje opróżnienie bufora danych wejściowych interfejsu UART
       
       vTaskDelay(500 / portTICK_PERIOD_MS);
        return 0;
    }
    else return 1;
   
}
// ----------------------------------------- Pobieranie danych z czujnika -----------------------------------------
static void pm5003_task(void *arg)
{
    
    uint8_t* data = (uint8_t*) malloc(PMS_BUF+1); //przydzielenie bloku pamięci 257 bajtów - 256 bajtów danych z magistrali UART + końcowy 
    
    while (1) {
        const int rxBytes = uart_read_bytes(UART_NUM_2, data, PMS_BUF, 20000 / portTICK_RATE_MS);// iczba bajtów odczytanych z magistrali UART
        if (rxBytes > 0) {
            data[rxBytes] = 0; //końcowy bajt w buforze danych jest ustawiany na zero w celu oznaczenia końca danych

            if(checkSum(data)==1){ //wywołanie funkcji sprawdzenia poprawności otrzymanych danych

                for(int i=0;i<32;i++)
                {
                    printf(" '%d 0x%x' ",i, data[i]);
                    
                }
                
                DataPM[0]=((data[12] << 8) + data[13]); //Przypisanie odczytanych wartości PM2.5 (under atmospheric environment) zgodnie z dokumentacją czujnika - 12 bajt łączony jest z 13'stym 
                DataPM[1]=((data[14] << 8) + data[15]); //Przypisanie odczytanych wartości PM10 (under atmospheric environment) zgodnie z dokumentacją czujnika - 14 bajt łączony jest z 15'stym 
                printf("\n Data 5 indicates PM2.5 concentration : '%d' \n", DataPM[0]);
                printf("\n Data 6 indicates PM10 concentration : '%d' \n", DataPM[1]);
                printf("\n Data 2 indicates PM2.5 concentration : '%d' \n", (data[6] << 8) + data[7]);
                printf("\n Data 3 indicates PM10 concentration : '%d' \n", (data[8] << 8) + data[9]);

            }

            else printf("\n Data Error! \n");
            
           
             
           
        }
        vTaskDelay(MEASUREMENT_TIME / portTICK_PERIOD_MS);
    }
    free(data);
}

// ----------------------------------------- Obsługa matrycowego wyświetlacza led -----------------------------------------
static void matrix_task(void *arg) 
{
    int disp[2]={-1};
    while(1){
    if(DataPM[0]!=-1 && DataPM[1]!=-1) //jeśli pojawiły się nowe dane - wyswietl je
    {
    disp[0]=DataPM[0]; //zapis danych w razie zastąpienia ich nowymi/usunięciem
    disp[1]=DataPM[1];
    ESP_ERROR_CHECK(clear()); // wyczyść wyświetlacz
    ESP_ERROR_CHECK(showPM25()); // pokaż napis "PM 2.5"
    vTaskDelay(DISPLAY_TIME / portTICK_PERIOD_MS);
    ESP_ERROR_CHECK(clear());// wyczyść wyświetlacz
    ESP_ERROR_CHECK(DisplayNumber(disp[0])); // wyświetl wartość odczytaną z czujnika dla PM 2.5
    vTaskDelay(DISPLAY_TIME / portTICK_PERIOD_MS);
    ESP_ERROR_CHECK(clear());// wyczyść wyświetlacz
    ESP_ERROR_CHECK(showPM10());// pokaż napis "PM 10"
    vTaskDelay(DISPLAY_TIME / portTICK_PERIOD_MS);
    ESP_ERROR_CHECK(clear());// wyczyść wyświetlacz
    ESP_ERROR_CHECK(DisplayNumber(disp[1])); // wyświetl wartość odczytaną z czujnika dla PM 10
    
    }
    else if(disp[0]!=-1 && disp[1]!=-1){ //jeśli nie ma nowych wyświetl stary odczyt jeszcze raz
    ESP_ERROR_CHECK(clear()); // wyczyść wyświetlacz
    ESP_ERROR_CHECK(showPM25());// pokaż napis "PM 2.5"
    vTaskDelay(DISPLAY_TIME / portTICK_PERIOD_MS);
    ESP_ERROR_CHECK(clear());// wyczyść wyświetlacz
    ESP_ERROR_CHECK(DisplayNumber(disp[0]));// wyświetl wartość odczytaną z czujnika dla PM 2.5
    vTaskDelay(DISPLAY_TIME / portTICK_PERIOD_MS);
    ESP_ERROR_CHECK(clear());// wyczyść wyświetlacz
    ESP_ERROR_CHECK(showPM10());// pokaż napis "PM 10"
    vTaskDelay(DISPLAY_TIME / portTICK_PERIOD_MS);
    ESP_ERROR_CHECK(clear());// wyczyść wyświetlacz
    ESP_ERROR_CHECK(DisplayNumber(disp[1]));// wyświetl wartość odczytaną z czujnika dla PM 10

    }
    
    vTaskDelay(DISPLAY_TIME / portTICK_PERIOD_MS);
    
    
    }
}



// ----------------------------------------- Main wywołanie inicjalizacji i podzielenie programu na funkcje  -----------------------------------------
void app_main(void)
{
    ESP_ERROR_CHECK(spi_init());
    ESP_ERROR_CHECK(max7219_init());
    nvs_flash_init();
    wifi_connection();
    
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    printf("WIFI was initiated ...........\n\n");
    
    init();  

    xTaskCreate(pm5003_task, "pm5003_uart_task", 1024*2, NULL, configMAX_PRIORITIES-1, NULL); 
    xTaskCreate(matrix_task, "matrix_task_display", 1024*2, NULL, configMAX_PRIORITIES-2, NULL);
    
    send_data();   
    
}
