#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "gpio_util.h"
#include "isd2360.h"

spi_device_handle_t spi_dev;
static const char *TAG = __FILE__;

/*---------------------------------------------------------------------------*/
/* CS (Chip Select) 제어 함수 수동 구현                                         */
/*---------------------------------------------------------------------------*/
static void ISD3800_CS_Low(void)
{
    gpio_set_level(ISD_CS_PIN, 0);
}

static void ISD3800_CS_High(void)
{
    gpio_set_level(ISD_CS_PIN, 1);
}

/*---------------------------------------------------------------------------*/
/* ESP32 SPI 단일 바이트 송수신 전송 함수                                        */
/*---------------------------------------------------------------------------*/
static uint8_t esp32_spi_transfer_byte(uint8_t data)
{
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    
    t.flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA;
    t.length = 8; 
    t.tx_data[0] = data;

    esp_err_t ret = spi_device_polling_transmit(spi_dev, &t);
    if (ret != ESP_OK) {
        return 0;
    }

    return t.rx_data[0];
}

//==============================================================ISD3800/3900 함수들

uint16_t ISD3800_READ_STATUS(void)
{
    uint8_t Temp0, Temp1;
    uint16_t Temp;

    ISD3800_CS_Low();
    Temp0 = esp32_spi_transfer_byte(READ_STATUS);
    Temp1 = esp32_spi_transfer_byte(0x00);
    ISD3800_CS_High();

    Temp = ((Temp0 & 0xFF) << 8) | (Temp1 & 0xFF);
    return Temp;     
}

void ISD3800_PWR_UP(void)
{
    ISD3800_CS_Low();
    esp32_spi_transfer_byte(PWR_UP);
    ISD3800_CS_High();   
}

void ISD3800_PWR_DN(void)
{
    ISD3800_CS_Low();
    esp32_spi_transfer_byte(PWR_DN);
    ISD3800_CS_High();
}
void ISD3800_WaitReady(uint8_t Status)
{
    uint32_t spiread;
    int timeout = 0;
    
    while(1)
    {
        spiread = ISD3800_READ_STATUS() >> 8;
        
        // 1. 칩이 정상적으로 깨어있고(PD == 0), 타겟 비트가 만족되는지 확인
        // 만약 비트 매핑 상 0x40(DBUF_RDY)을 기다린다면 해당 비트 체크
        if ((spiread & 0x80) == 0 && (spiread & Status) == Status) {
            break;
        }
        
        // 2. 무한 루프 방지를 위한 타임아웃 (약 250ms)
        timeout++;
        if(timeout > 50) { 
            ESP_LOGW("DEBUG", "WaitReady Timeout Force Pass! Status: 0x%02X", (unsigned int)spiread);
            break; 
        }
        
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

void ISD3800_PLAY_VP(uint16_t u16Index)
{
    ISD3800_WaitReady(PD | DBUF_RDY | VM_BSY | CBUF_FUL);

    ISD3800_CS_Low();
    esp32_spi_transfer_byte(PLAY_VP);
    esp32_spi_transfer_byte((uint8_t)(u16Index >> 8));
    esp32_spi_transfer_byte((uint8_t)(u16Index & 0xFF));
    ISD3800_CS_High();   
}

void ISD3800_SET_CLK_CFG(uint8_t u8ClkReg)
{
    ISD3800_WaitReady(PD | DBUF_RDY | VM_BSY | CBUF_FUL | CMD_BSY);

    ISD3800_CS_Low();
    esp32_spi_transfer_byte(SET_CLK_CFG);
    esp32_spi_transfer_byte(u8ClkReg);
    ISD3800_CS_High();

    ISD3800_WaitReady(PD | DBUF_RDY | VM_BSY | CBUF_FUL | CMD_BSY);
}

void ISD3800_WR_CFG_REG(uint8_t u8Reg, uint8_t u8Data)
{
    ISD3800_WaitReady(PD | DBUF_RDY);

    ISD3800_CS_Low();
    esp32_spi_transfer_byte(WR_CFG_REG);
    esp32_spi_transfer_byte(u8Reg);
    esp32_spi_transfer_byte(u8Data);
    ISD3800_CS_High();
}

void Set_ISD3800_Playing_Path(void)
{  
    ISD3800_WR_CFG_REG(0x01, 0x20);  
    ISD3800_WR_CFG_REG(0x02, 0x44); 
    ISD3800_WR_CFG_REG(0x03, 0x00);  
    ISD3800_WR_CFG_REG(0x05, 0x00);  
    ISD3800_WR_CFG_REG(0x06, 0x00);  
}

void isd2360_read_id_accurate(void) {
    // 수동 CS 제어 방식으로 변경
    uint8_t tx_data[5] = { 0x48, 0x00, 0x00, 0x00, 0x00 };
    uint8_t rx_data[5] = { 0 };

    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = 40; 
    t.tx_buffer = tx_data;
    t.rx_buffer = rx_data;

    ISD3800_CS_Low();
    esp_err_t ret = spi_device_polling_transmit(spi_dev, &t);
    ISD3800_CS_High();

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "--- ISD2360 Read ID Result ---");
        ESP_LOGI(TAG, "Status Byte : 0x%02X", rx_data[0]);
        ESP_LOGI(TAG, "PART ID     : 0x%02X", rx_data[1]);
        ESP_LOGI(TAG, "MAN ID      : 0x%02X", rx_data[2]);
        ESP_LOGI(TAG, "MEM TYPE    : 0x%02X", rx_data[3]);
        ESP_LOGI(TAG, "DEV ID      : 0x%02X", rx_data[4]);
    }
}

void isd2360_spi_init(void) {
    esp_err_t ret;

    // CS 핀을 일반 GPIO 출력 모드로 초기화 (수동 제어용)
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << ISD_CS_PIN),
        .pull_down_en = 0,
        .pull_up_en = 1
    };
    gpio_config(&io_conf);
    ISD3800_CS_High(); // 초기 상태 High 유지

    spi_bus_config_t buscfg = {
        .miso_io_num = ISD_MISO_PIN,
        .mosi_io_num = ISD_MOSI_PIN,
        .sclk_io_num = ISD_SCLK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 32
    };

    spi_device_interface_config_t devcfg = {
        .command_bits = 0,
        .address_bits = 0,
        .dummy_bits = 0,
        .mode = 3, 
        .duty_cycle_pos = 0, 
        .cs_ena_pretrans = 0,
        .cs_ena_posttrans = 0,
        .clock_speed_hz = 500000,
        .input_delay_ns = 0,
        .spics_io_num = -1, // ⭐ -1로 설정하여 하드웨어 자동 CS 해제
        .flags = 0,
        .queue_size = 7,
        .pre_cb = NULL,
        .post_cb = NULL,
    };

    ret = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    ESP_ERROR_CHECK(ret);

    ret = spi_bus_add_device(SPI2_HOST, &devcfg, &spi_dev);
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "SPI Initialized Successfully.");
}

void isd2360_task(void* arg)
{
    // 1. 전원 켜기 및 안정화 대기
    ESP_LOGI(TAG, "Powering up ISD2360...");
    ISD3800_PWR_UP();
    vTaskDelay(pdMS_TO_TICKS(100)); 

    // 2. 통신 확인용 ID 읽기
    isd2360_read_id_accurate();
    
    // 3. 클럭 설정 및 재생 오디오 패스 설정
    ESP_LOGI(TAG, "Configuring Clock and Playing Path...");
    ISD3800_SET_CLK_CFG(0x34);   
    Set_ISD3800_Playing_Path();
    
    // 4. 5번 매크로(음원) 재생 명령 전송
    ESP_LOGI(TAG, "Executing PLAY_VP(5)...");
    ISD3800_PLAY_VP(5);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void isd2360_taskinit(void) {
    // SPI 초기화만 진행
    isd2360_spi_init();

    TaskHandle_t xHandle = NULL;
    static uint8_t ucParameterToPass;
    
    if (xTaskCreatePinnedToCore(
            isd2360_task,                  
            "isd2360_task",                
            4096,       // 스택 크기가 타이트할 수 있어 4096으로 상향 조정
            &ucParameterToPass,        
            tskIDLE_PRIORITY + 1,      
            &xHandle,                  
            1                          
        ) != pdPASS) { 
        ESP_LOGE(TAG, "Error creating isd2360_task on Core 1");
    }
}