#ifndef __FSUTILS_H__
#define __FSUTILS_H__
#include "main.h"
#include "w25q64.h"
#include "w25q64_lfs.h"

enum FIO_ERR{
    TIMEOUT_ERR = 0xF1F1F1F1,
    EXISTS_ERR = 0xF2F2F2F2,
    CREATE_ERR = 0xF3F3F3F3,
    NOEXSITS_ERR = 0xF4F4F4F4,
    NOFILE_ERR = 0xF5F5F5F5,
    NOSPACE_ERR = 0xF6F6F6F6,
};

lfs_t setup_lfs(GPIO_TypeDef *cs_port, uint16_t cs_pin);
const char* getcwd();
void listdir(lfs_t *lfs, const char* dir);
void makedir(lfs_t *lfs, const char *dirname);
void pathjoin(char* buf, const char* a, const char* b);
void changedir(lfs_t *lfs, const char* dir);
void cat(lfs_t *lfs, const char *path);
void touch(lfs_t *lfs, const char *path);
void diskinfo(lfs_t *lfs);
int rm_recursive(lfs_t *lfs, const char *path);
void lsr_print_callback(const char* path, void* param);
void ls_recursive(lfs_t *lfs, const char *path, void(*callback)(const char*, void*), void* param);
void receive_file(lfs_t *lfs, UART_HandleTypeDef *huart1);
void send_file(lfs_t *lfs, UART_HandleTypeDef *huart);


#endif //__FSUTILS_H__
