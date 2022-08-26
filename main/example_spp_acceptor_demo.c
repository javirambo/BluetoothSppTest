/*
 This example code is in the Public Domain (or CC0 licensed, at your option.)

 Unless required by applicable law or agreed to in writing, this
 software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 CONDITIONS OF ANY KIND, either express or implied.
 */

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "classicBtSpp.h"

static const char *SPP_TAG = "DEMO";

void OnEachLine(char *line)
{
	// hacer algo con los datos.......parse de JSON o comandos AT

	ESP_LOGI(SPP_TAG, "%s", line);
}

void app_main(void)
{
	// abre una especie de servidor BT para aceptar clientes que se conecten con una terminal serie virtual.
	// se llama al callback por cada linea que ingresa (terminada en \n\r).

	startClassicBtSpp(OnEachLine);
}
