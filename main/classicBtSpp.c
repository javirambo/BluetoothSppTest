/*
 * classicBtSpp.c
 *
 *  Created on: Aug 26, 2022
 *      Author: Javier
 */
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include "nvs.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_bt_device.h"
#include "esp_spp_api.h"
#include "classicBtSpp.h"

static const char *SPP_TAG = "BT_SPP";

bool passwordIngresada = false;
esp_bd_addr_t remoteDeviceAddress;
char myDeviceName[] = "EffiCast XXXXX";

onEachLineCallback_t onEachLineCallback = NULL;

static void GetDeviceName()
{
	char *p = myDeviceName + 9;
	uint32_t tmp;
	uint8_t mac[7];
	esp_efuse_mac_get_default(mac);
	uint32_t N = (mac[3] * 256 * 256 + mac[4] * 256 + mac[5]) & 0xFFFFFF;
	do
	{
		tmp = N / 34;
		*p++ = "QWERTYUIPASDFGHJKLZXCVBNM123456789"[N - (tmp * 34)];
		N = tmp;
	} while (N);
	*p = 0;
	ESP_LOGW(SPP_TAG, "HOLA, YO SOY \"%s\"", myDeviceName);
}

static void esp_spp_cb(esp_spp_cb_event_t event, esp_spp_cb_param_t *param)
{
	switch (event) {
	case ESP_SPP_INIT_EVT:
		ESP_LOGI(SPP_TAG, "ESP_SPP_INIT_EVT");
		esp_spp_start_srv(ESP_SPP_SEC_AUTHENTICATE, ESP_SPP_ROLE_SLAVE, 0, "SPP_SERVER");
		break;
	case ESP_SPP_DISCOVERY_COMP_EVT:
		ESP_LOGI(SPP_TAG, "ESP_SPP_DISCOVERY_COMP_EVT");
		break;
	case ESP_SPP_OPEN_EVT:
		ESP_LOGI(SPP_TAG, "ESP_SPP_OPEN_EVT");

		uint8_t *c = (uint8_t*) "Hello";
		esp_spp_write(param->srv_open.handle, 6, c);

		break;

	case ESP_SPP_CLOSE_EVT:
		// cuando se cierra la terminal del celu...
		passwordIngresada = false;
		ESP_LOGI(SPP_TAG, "ESP_SPP_CLOSE_EVT");
		break;

	case ESP_SPP_START_EVT:
		ESP_LOGI(SPP_TAG, "ESP_SPP_START_EVT");
		esp_bt_dev_set_device_name(myDeviceName);
		esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
		break;
	case ESP_SPP_CL_INIT_EVT:
		ESP_LOGI(SPP_TAG, "ESP_SPP_CL_INIT_EVT");
		break;

	case ESP_SPP_DATA_IND_EVT: {
		char texto[128];
		int texlen;

		// aca aparecen los datos que transmite el celu por bluetooth:

		ESP_LOGI(SPP_TAG, "ESP_SPP_DATA_IND_EVT len=%d handle=%d", param->data_ind.len, param->data_ind.handle);
		esp_log_buffer_hex("", param->data_ind.data, param->data_ind.len);

		// si es una terminal común, siempre viene un \n\r al final. Se los quito.
		int dataLen = param->data_ind.len;
		while (dataLen > 0)
		{
			char c = param->data_ind.data[dataLen - 1];
			if (c == '\n' || c == '\r')
			{
				dataLen--;
				param->data_ind.data[dataLen] = 0;
			} else
				break;
		}

		if (!passwordIngresada)
		{
			if (strcmp((char*) param->data_ind.data, BT_PASSWORD) == 0)
			{
				passwordIngresada = true;
				ESP_LOGW(SPP_TAG, "PASSWORD CORRECTA");
				texlen = sprintf(texto, "Clave ok\n\r");
			} else
			{
				ESP_LOGE(SPP_TAG, "PASSWORD INCORRECTA");
				texlen = sprintf(texto, "Clave incorrecta\n\r");
			}
			esp_spp_write(param->srv_open.handle, texlen, (uint8_t*) texto);
		} else
		{
			// hacer algo con los datos.......parse de JSON o comandos AT
			if (onEachLineCallback)
				onEachLineCallback((char*) param->data_ind.data);
		}

		break;
	}

	case ESP_SPP_CONG_EVT:
		ESP_LOGI(SPP_TAG, "ESP_SPP_CONG_EVT");
		break;
	case ESP_SPP_WRITE_EVT:
		ESP_LOGI(SPP_TAG, "ESP_SPP_WRITE_EVT");
		break;

	case ESP_SPP_SRV_OPEN_EVT:
		// esto aparece al conectar la terminal bluetooth desde el celu:
		ESP_LOGI(SPP_TAG, "ESP_SPP_SRV_OPEN_EVT");

		//********************************************
		// PEDIR CONTRASEÑA
		//********************************************
		char data[128];
		int len = sprintf(data, "Para comenzar, ingrese la clave para %s:\n\r", myDeviceName);
		esp_spp_write(param->srv_open.handle, len, (uint8_t*) data);
		passwordIngresada = false;
		break;

	case ESP_SPP_SRV_STOP_EVT:
		ESP_LOGI(SPP_TAG, "ESP_SPP_SRV_STOP_EVT");
		break;
	case ESP_SPP_UNINIT_EVT:
		ESP_LOGI(SPP_TAG, "ESP_SPP_UNINIT_EVT");
		break;
	default:
		break;
	}
}

void esp_bt_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
	switch (event) {
	case ESP_BT_GAP_AUTH_CMPL_EVT: {
		if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS)
		{
			// aca se muestra el nombre del celular vinculado:

			ESP_LOGI(SPP_TAG, "authentication success: %s", param->auth_cmpl.device_name);

			// remote bluetooth device address:
			esp_log_buffer_hex(SPP_TAG, param->auth_cmpl.bda, ESP_BD_ADDR_LEN);

			// *******************************************************
			// LOGUEAR QUIEN SE VINCULO, SE PUEDE ENVIAR AL DASHBOARD
			// *******************************************************
			// guardo la mac del celu
			memcpy(remoteDeviceAddress, param->auth_cmpl.bda, ESP_BD_ADDR_LEN);

		} else
		{
			ESP_LOGE(SPP_TAG, "authentication failed, status:%d", param->auth_cmpl.stat);
		}
		break;
	}
	case ESP_BT_GAP_PIN_REQ_EVT: {
		ESP_LOGI(SPP_TAG, "ESP_BT_GAP_PIN_REQ_EVT min_16_digit:%d", param->pin_req.min_16_digit);
		if (param->pin_req.min_16_digit)
		{
			ESP_LOGI(SPP_TAG, "Input pin code: 0000 0000 0000 0000");
			esp_bt_pin_code_t pin_code = { 0 };
			esp_bt_gap_pin_reply(param->pin_req.bda, true, 16, pin_code);
		} else
		{
			ESP_LOGI(SPP_TAG, "Input pin code: 1234");
			esp_bt_pin_code_t pin_code;
			pin_code[0] = '1';
			pin_code[1] = '2';
			pin_code[2] = '3';
			pin_code[3] = '4';
			esp_bt_gap_pin_reply(param->pin_req.bda, true, 4, pin_code);
		}
		break;
	}

#if (CONFIG_BT_SSP_ENABLED == true)
	case ESP_BT_GAP_CFM_REQ_EVT:
		ESP_LOGI(SPP_TAG, "ESP_BT_GAP_CFM_REQ_EVT Please compare the numeric value: %d", param->cfm_req.num_val);
		esp_bt_gap_ssp_confirm_reply(param->cfm_req.bda, true);
		break;
	case ESP_BT_GAP_KEY_NOTIF_EVT:
		ESP_LOGI(SPP_TAG, "ESP_BT_GAP_KEY_NOTIF_EVT passkey:%d", param->key_notif.passkey);
		break;
	case ESP_BT_GAP_KEY_REQ_EVT:
		ESP_LOGI(SPP_TAG, "ESP_BT_GAP_KEY_REQ_EVT Please enter passkey!");
		/*
		 Si se usa ESP_BT_IO_CAP_IN (KeyboardOnly) en esp_bt_gap_set_security_param(...)
		 entonces pide aca ingresar la clave que aparece en el celular.

		 (Aparentemente con esta funcion se ingresa la clave para aceptar la vinculacion)

		 //esp_bt_gap_ssp_passkey_reply(bd_addr, accept, passkey);
		 */
		break;
#endif

	case ESP_BT_GAP_MODE_CHG_EVT:
		ESP_LOGI(SPP_TAG, "ESP_BT_GAP_MODE_CHG_EVT mode:%d", param->mode_chg.mode);
		break;

	default:
		ESP_LOGI(SPP_TAG, "event: %d", event);
		break;

	}
}

void startClassicBtSpp(onEachLineCallback_t onLineCallback)
{
	onEachLineCallback = onLineCallback;

	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
	{
		nvs_flash_erase();
		nvs_flash_init();
	}
	esp_bt_controller_mem_release(ESP_BT_MODE_BLE);

	esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT()
	;
	esp_bt_controller_init(&bt_cfg);
	esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT);
	esp_bluedroid_init();
	esp_bluedroid_enable();

	//*************************
	// OBTENGO NOMBRE CON LA MAC
	GetDeviceName();

	esp_bt_gap_register_callback(esp_bt_gap_cb);
	esp_spp_register_callback(esp_spp_cb);
	esp_spp_init(ESP_SPP_MODE_CB);

	//si no se usa la autenticacion, se vincula directamente.

#if (CONFIG_BT_SSP_ENABLED == true)
	// Set default parameters for Secure Simple Pairing
	esp_bt_sp_param_t param_type = ESP_BT_SP_IOCAP_MODE;
	//OUT  DisplayOnly  : solo pide aceptar en el celu para vincular (sin codigo)
	//IO   DisplayYesNo : muestra un codigo en el celu y en el log, el celu puede aceptar la vinculacion.
	//IN   KeyboardOnly : es para ingresar el codigo en el dispositivo (el que aparece en el celu) no sirve porque no tenemos teclado (ver ESP_BT_GAP_KEY_REQ_EVT)
	//NONE NoInputNoOutput : similar a OUT
	esp_bt_io_cap_t iocap = ESP_BT_IO_CAP_NONE;
	esp_bt_gap_set_security_param(param_type, &iocap, sizeof(uint8_t));
#endif

	/*
	 * Set default parameters for Legacy Pairing
	 * Use variable pin, input pin code when pairing
	 */
	esp_bt_pin_code_t pin_code;
	esp_bt_gap_set_pin(ESP_BT_PIN_TYPE_VARIABLE, 0, pin_code);
}
