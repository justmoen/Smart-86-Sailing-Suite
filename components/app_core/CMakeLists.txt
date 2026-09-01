file(GLOB APP_CORE_SRCS
    "${CMAKE_CURRENT_LIST_DIR}/../../main/ui/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../main/screens/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../main/net/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../main/app/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../main/*.cpp"
)

list(FILTER APP_CORE_SRCS EXCLUDE REGEX ".*/main\\.cpp$")

idf_component_register(
    SRCS
        ${APP_CORE_SRCS}

    INCLUDE_DIRS
        "${CMAKE_CURRENT_LIST_DIR}/../../main"
        "${CMAKE_CURRENT_LIST_DIR}/../../main/ui"
        "${CMAKE_CURRENT_LIST_DIR}/../../main/screens"
        "${CMAKE_CURRENT_LIST_DIR}/../../main/net"
        "${CMAKE_CURRENT_LIST_DIR}/../../main/app"


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
