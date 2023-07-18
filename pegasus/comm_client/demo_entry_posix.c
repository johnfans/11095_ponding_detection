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

#include <stdio.h>
#include <stdlib.h>
#include "net_demo.h"
#include "net_params.h"

int main(int argc, char* argv[])
{
    printf("Usage: %s [port] [host]\n", argv[0]);

    short port = argc > 1 ? atoi(argv[1]) : PARAM_SERVER_PORT; /* 1 */
    char* host = argc > 2 ? argv[2] : PARAM_SERVER_ADDR; /* 2,2 */

    NetDemoTest(port, host);
    return 0;
}