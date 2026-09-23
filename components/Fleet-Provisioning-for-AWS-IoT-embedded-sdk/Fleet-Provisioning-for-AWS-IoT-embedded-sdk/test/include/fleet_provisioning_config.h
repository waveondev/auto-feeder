/*
 * AWS IoT Fleet Provisioning v1.2.1
 * Copyright (C) 2021 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/**
 * @file fleet_provisioning_config.h
 * @brief Config values for testing the AWS IoT Fleet Provisioning Library.
 */

#ifndef FLEET_PROVISIONING_CONFIG_H_
#define FLEET_PROVISIONING_CONFIG_H_

#include <stdio.h>
#include "esp_log.h"
#ifdef DISABLE_LOGGING
    #ifndef LogError
        #define LogError( message )
    #endif
    #ifndef LogWarn
        #define LogWarn( message )
    #endif

    #ifndef LogInfo
        #define LogInfo( message )
    #endif

    #ifndef LogDebug
        #define LogDebug( message )
    #endif

#else /* ! DISABLE_LOGGING */
    #define LogError( message )    ESP_LOGE(__FILE__,message)

    #define LogWarn( message )     ESP_LOGW(__FILE__,message)

    #define LogInfo( message )     ESP_LOGI(__FILE__,message)

    #define LogDebug( message )    ESP_LOGD(__FILE__,message)
#endif /* DISABLE_LOGGING */

#endif /* FLEET_PROVISIONING_CONFIG_H_ */
