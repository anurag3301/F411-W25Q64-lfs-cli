#include "main.h"
#include <stdio.h>
#include "fsutils.h"

UART_HandleTypeDef huart1;
SPI_HandleTypeDef  spi1;


int main(){
    HAL_Init();
    SystemClock_Config();
    enable_gpio();
    setup_hardfault_led();
    setup_uart1();
    setup_spi1();
    setvbuf(stdout, NULL, _IONBF, 0);
    lfs_t lfs = setup_lfs();

    char s[100];
 
    printf("CWD: %s\n\r", getcwd());

    while (1) { 
        printf("Enter Something: ");
        fgets(s, sizeof(s), stdin);
        printf("You entered: %s\n\r", s);
    }

}

