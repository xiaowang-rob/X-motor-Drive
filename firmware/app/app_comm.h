#ifndef __APP_COMM_H
#define __APP_COMM_H
#include "parameter_list.h"

bool comm_sup_init(void);
bool comm_main_init(tParameter *param);
void comm_process(void);

#endif