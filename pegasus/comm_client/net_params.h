/*
 * Copyright (c) 2022 HiSilicon (Shanghai) Technologies CO., LIMITED.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef NET_PARAMS_H
#define NET_PARAMS_H

#ifndef PARAM_HOTSPOT_SSID
#define PARAM_HOTSPOT_SSID "johnfzm"   // your AP SSID
#endif

#ifndef PARAM_HOTSPOT_PSK
#define PARAM_HOTSPOT_PSK  "12345678"  // your AP PSK
#endif

#ifndef PARAM_HOTSPOT_TYPE
#define PARAM_HOTSPOT_TYPE WIFI_SEC_TYPE_PSK // defined in wifi_device_config.h
#endif

#ifndef PARAM_SERVER_ADDR
#define PARAM_SERVER_ADDR "192.168.243.138" // your PC IP address
#endif

#ifndef PARAM_SERVER_PORT
#define PARAM_SERVER_PORT 8888
#endif

#endif  // NET_PARAMS_H