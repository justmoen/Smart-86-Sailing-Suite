file(GLOB UI_SRCS
    "ui/*.cpp"
    "screens/*.cpp"
    "net/*.cpp"
    "*.cpp"
)

idf_component_register(
    SRCS
        ${UI_SRCS}

    INCLUDE_DIRS
        "."
        "ui"
        "screens"
        "net"
    REQUIRES
        ReactESP
        arduino
        esp-tls
        mbedtls
        arduino-mqtt
        TinyGPSPlus
        WMM_Tinier
        nvs_flash
        waveshare__esp32_p4_wifi6_touch_lcd_4b
        ArduinoJson
        WebSockets
)