#ifndef __FSUTILS_H__
#define __FSUTILS_H__
#include "main.h"
#include "w25q64.h"
#include "w25q64_lfs.h"

lfs_t setup_lfs();
const char* getcwd();

#endif //__FSUTILS_H__
