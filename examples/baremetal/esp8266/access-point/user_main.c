/****************************************************************************
**
** Copyright (C) 2020 Denis Shienkov <denis.shienkov@gmail.com>
** Contact: https://www.qt.io/licensing/
**
** This file is part of the examples of Qbs.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "user_config.h"

// From ESP8266 SDK.
#include <osapi.h>

static const partition_item_t esp_partitions[] = {
    {SYSTEM_PARTITION_BOOTLOADER, ESP_PART_BL_ADDR, ESP_PART_BOOTLOADER_SIZE},
    {SYSTEM_PARTITION_OTA_1, ESP_PART_APP1_ADDR, ESP_PART_APPLICATION_SIZE},
    {SYSTEM_PARTITION_OTA_2, ESP_PART_APP2_ADDR, ESP_PART_APPLICATION_SIZE},
    {SYSTEM_PARTITION_RF_CAL, ESP_PART_RF_CAL_ADDR, ESP_PART_RF_CAL_SIZE},
    {SYSTEM_PARTITION_PHY_DATA, ESP_PART_PHY_DATA_ADDR, ESP_PART_PHY_DATA_SIZE},
    {SYSTEM_PARTITION_SYSTEM_PARAMETER, ESP_PART_SYS_PARAM_ADDR, ESP_PART_SYS_PARAM_SIZE}
};

enum {
    ESP_PART_COUNT = sizeof(esp_partitions) / sizeof(esp_partitions[0])
};

LOCAL void ICACHE_FLASH_ATTR user_init_done(void)
{
    os_printf("esp: initialization completed\n");
}

void ICACHE_FLASH_ATTR user_pre_init(void)
{
    const bool ok = system_partition_table_regist(esp_partitions, ESP_PART_COUNT, SPI_FLASH_SIZE_MAP);
    if (!ok) {
        os_printf("esp: partitions registration failed\n");
        while (true);
    }
}

void ICACHE_FLASH_ATTR user_init(void)
{
    os_printf("esp: SDK version: %s\n", system_get_sdk_version());
    os_printf("esp: reset reason: %u\n", system_get_rst_info()->reason);

    const bool ok = wifi_set_opmode(SOFTAP_MODE);
    if (!ok) {
        os_printf("esp: set softap mode failed\n");
    } else {
        struct softap_config ap_cfg = {0};
        ap_cfg.authmode = ESP_AUTO_MODE_CFG;
        ap_cfg.beacon_interval = ESP_BEACON_INTERVAL_MS_CFG;
        ap_cfg.channel = ESP_CHANNEL_NO_CFG;
        ap_cfg.max_connection = 0;
        ap_cfg.ssid_hidden = false;
        os_memcpy(&ap_cfg.ssid, ESP_SSID_CFG, sizeof(ESP_SSID_CFG));
        const bool ok = wifi_softap_set_config(&ap_cfg);
        if (!ok)
            os_printf("esp: set softap configuration failed\n");
    }

    system_init_done_cb(user_init_done);
}
