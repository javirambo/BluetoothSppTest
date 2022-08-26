/*
 * classicBtSpp.h
 *
 *  Created on: Aug 26, 2022
 *      Author: Javier
 */

#ifndef MAIN_CLASSICBTSPP_H_
#define MAIN_CLASSICBTSPP_H_

typedef void (*BtStringCallback_t)(char *line);

void btspp_init(const char * password, BtStringCallback_t onLineCallback, BtStringCallback_t onDeviceAuth);
int btspp_printf(const char *format, ...);

#endif /* MAIN_CLASSICBTSPP_H_ */
