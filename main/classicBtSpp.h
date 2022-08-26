/*
 * classicBtSpp.h
 *
 *  Created on: Aug 26, 2022
 *      Author: Javier
 */

#ifndef MAIN_CLASSICBTSPP_H_
#define MAIN_CLASSICBTSPP_H_

#define BT_PASSWORD "Notengoidea"
#define BT_PASSWORD_LEN 11

typedef void (*onEachLineCallback_t)(char *line);

void startClassicBtSpp(onEachLineCallback_t onLineCallback);

#endif /* MAIN_CLASSICBTSPP_H_ */
