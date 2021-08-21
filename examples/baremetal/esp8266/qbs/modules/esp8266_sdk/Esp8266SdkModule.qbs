Module {
    Depends { name: "cpp" }

    Esp8266SdkProbe { id: esp_sdk_probe }

    validate: {
        if (!esp_sdk_probe.found)
            throw "ESP8266 NON OS SDK not found. Please set the ESP8266_NON_OS_SDK_ROOT env variable."
    }

    cpp.systemIncludePaths: [esp_sdk_probe.includesPath]
    cpp.libraryPaths: [esp_sdk_probe.libsPath]
    cpp.driverFlags: ["-nostdlib"]

    cpp.cFlags: [
        "-Wpointer-arith",
        "-Wundef",
        "-fno-inline-functions",
        "-mlongcalls",
        "-mtext-section-literals",
        "-ffunction-sections",
        "-fdata-sections",
        "-fno-builtin-printf",
        "-fno-guess-branch-probability",
        "-freorder-blocks-and-partition",
        "-fno-cse-follow-jumps"
    ]

    cpp.linkerFlags: [
        "--no-check-sections",
        "--gc-sections",
        "--start-group"
    ]

    cpp.defines: [
        "ICACHE_FLASH",
        "USE_OPTIMIZE_PRINTF",

        // Target specific defines for the external 40MHz DIO
        // SPI 16 Mbit FLASH (2048 KByte + 2048 KByte) FOTA.
        "SPI_FLASH_SIZE_MAP=5",
        "ESP_PART_BL_ADDR=0x000000",
        "ESP_PART_APP1_ADDR=0x001000",
        "ESP_PART_APP2_ADDR=0x101000",
        "ESP_PART_RF_CAL_ADDR=0x1FB000",
        "ESP_PART_PHY_DATA_ADDR=0x1FC000",
        "ESP_PART_SYS_PARAM_ADDR=0x1FD000",
        "ESP_PART_BOOTLOADER_SIZE=0x001000",
        "ESP_PART_APPLICATION_SIZE=0x0E0000",
        "ESP_PART_RF_CAL_SIZE=0x001000",
        "ESP_PART_PHY_DATA_SIZE=0x001000",
        "ESP_PART_SYS_PARAM_SIZE=0x003000"
    ]

    property string _linker_scripts_path: esp_sdk_probe.linkerScriptsPath + "/"

    Group {
        name: "ESP8266 Linker Scripts"
        prefix: esp8266_sdk._linker_scripts_path + "/"
        fileTags: ["linkerscript"]
        files: [
            "eagle.rom.addr.v6.ld",
            "eagle.app.v6.new.2048.ld"
        ]
    }
}
