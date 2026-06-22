#include "main.h"
#include <stdio.h>
#include "fsutils.h"

UART_HandleTypeDef huart1;
SPI_HandleTypeDef  spi1;

void print_help(){
    printf("Available commands:\n\r");
    printf("  ls [path]      List directory contents\n\r");
    printf("  lsr [path]     Recursively list all files\n\r");
    printf("  cat <file>     Print file contents\n\r");
    printf("  touch <file>   Create a new file (prompts for content)\n\r");
    printf("  mkdir <dir>    Create a directory\n\r");
    printf("  rm <path>      Recursively remove a file or directory\n\r");
    printf("  cd [dir]       Change directory (no arg goes to /)\n\r");
    printf("  pwd            Print current directory\n\r");
    printf("  info           Show filesystem disk usage\n\r");
    printf("  receive        Receive a file from the PC\n\r");
    printf("  send           Send a file to the PC\n\r");
    printf("  help           Show this message\n\r");
}



void runcmd(char* cmd, lfs_t *lfs){
    if(cmd[0] == '\0' || cmd[0] == '\n')return;

    int pos = 0;
    int len = strlen(cmd);
    while(pos < len && cmd[pos] != ' '&& cmd[pos] != '\n' && cmd[pos] != '\r'){pos++;}
    int pos2 = pos+1;
    while(pos2 < len && cmd[pos2] != ' '&& cmd[pos2] != '\n' && cmd[pos2] != '\r'){pos2++;}
    cmd[pos2] = '\0';
    char* cmd2 = cmd+pos+1;
    int cmd2len = strlen(cmd2);
    static char newpath[500];

    if(strncmp(cmd, "lsr", 3) == 0){
        if(cmd2len == 0){
            ls_recursive(lfs, getcwd(), lsr_print_callback, 0x0);
        }
        else{
            pathjoin(newpath, getcwd(), cmd2);
            ls_recursive(lfs, newpath, lsr_print_callback, 0x0);
        }
    }
    else if(strncmp(cmd, "ls", 2) == 0){
        if(cmd2len == 0){
            listdir(lfs, getcwd());
        }
        else{
            pathjoin(newpath, getcwd(), cmd2);
            listdir(lfs, newpath);
        }
    }
    else if(strncmp(cmd, "cat", 3) == 0){
        if(cmd2len == 0){
            printf("Usage cat <dir>\n\r");
        }
        else{
            pathjoin(newpath, getcwd(), cmd2);
            cat(lfs, newpath);
        }
    }
    else if(strncmp(cmd, "touch", 3) == 0){
        if(cmd2len == 0){
            printf("Usage touch <filename>\n\r");
        }
        else{
            pathjoin(newpath, getcwd(), cmd2);
            touch(lfs, newpath);
        }
    }
    else if(strncmp(cmd, "cd", 2) == 0){
        if(cmd2len == 0){
            changedir(lfs, "/");
        }
        else{
            changedir(lfs, cmd2);
        }
    }
    else if(strncmp(cmd, "rm", 2) == 0){
        if(cmd2len == 0){
            printf("Usage rm <dir/file>\n\r");
        }
        else{
            rm_recursive(lfs, cmd2);
        }
    }
    else if(strncmp(cmd, "pwd", 2) == 0){
        printf("CWD: %s\n\r", getcwd());
    }
    else if(strncmp(cmd, "mkdir", 2) == 0){
        if(cmd2len == 0){
            printf("Usage mkdir <dir>\n\r");
        }
        else{
            pathjoin(newpath, getcwd(), cmd2);
            makedir(lfs, newpath);
        }
    }
    else if(strncmp(cmd, "help", 4) == 0){
        print_help();
    }
    else if(strncmp(cmd, "info", 4) == 0){
        diskinfo(lfs);
    }
    else if(strncmp(cmd, "receive", 7) == 0){
        receive_file(lfs, &huart1);
    }
    else if(strncmp(cmd, "send", 4) == 0){
        send_file(lfs, &huart1);
    }
    else{
        printf("Unknown command \"%s\"! run \"help\"\n\r", cmd);
    }
}

void print_banner(void){
    printf(
        "\r\n\r\n"
        "    ___ __  __  __     ___________    ________    ____\n\r"
        "   / (_) /_/ /_/ /__  / ____/ ___/   / ____/ /   /  _/\n\r"
        "  / / / __/ __/ / _ \\/ /_   \\__ \\   / /   / /    / /  \n\r"
        " / / / /_/ /_/ /  __/ __/  ___/ /  / /___/ /____/ /   \n\r"
        "/_/_/\\__/\\__/_/\\___/_/    /____/   \\____/_____/___/  \n\r"
        "\n\r"
    );
}

int main(){
    /*
     * Pin connections
     * ---------------
     * UART1  PA9  → TX  (connect to RX of USB-UART adapter)
     *        PA10 → RX  (connect to TX of USB-UART adapter)
     *
     * SPI1   PA5  → SCK
     *        PA6  → MISO
     *        PA7  → MOSI
     *
     * W25Q64 PA4  → CS  (active low, software controlled)
     *
     * LED    PC13 → hardfault indicator (active low, onboard)
     */

    HAL_Init();
    SystemClock_Config();
    enable_gpio();
    setup_hardfault_led();
    setup_uart1();
    setup_spi1();
    setvbuf(stdout, NULL, _IONBF, 0);

    GPIO_InitTypeDef cs_gpio = {
        .Pin   = GPIO_PIN_4,
        .Mode  = GPIO_MODE_OUTPUT_PP,
        .Pull  = GPIO_NOPULL,
        .Speed = GPIO_SPEED_FREQ_VERY_HIGH,
    };
    HAL_GPIO_Init(GPIOA, &cs_gpio);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);

    print_banner();
    lfs_t lfs = setup_lfs(GPIOA, GPIO_PIN_4);
    char s[100];

    while (1) { 
        printf("%s# ", getcwd());
        fgets(s, sizeof(s), stdin);
        runcmd(s, &lfs);
    }
}

