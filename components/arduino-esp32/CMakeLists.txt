cmake_minimum_required(VERSION 3.16)

# ============================================================
# Arduino-ESP32 component
# ============================================================

if(NOT IDF_VERSION)
  message(FATAL_ERROR "IDF_VERSION is not defined")
endif()

if(IDF_VERSION VERSION_LESS "5.3.0" OR IDF_VERSION VERSION_GREATER_EQUAL "6.2.0")
  message(FATAL_ERROR
    "Arduino-ESP32 requires ESP-IDF >= 5.3.0 and < 6.2.0. "
    "Detected IDF_VERSION=${IDF_VERSION}"
  )
endif()

# ============================================================
# Core Arduino sources
# ============================================================

set(CORE_SRCS
  cores/esp32/base64.cpp
  cores/esp32/cbuf.cpp
  cores/esp32/esp32-hal-adc.c
  cores/esp32/esp32-hal-bt.c
  cores/esp32/esp32-hal-cpu.c
  cores/esp32/esp32-hal-dac.c
  cores/esp32/esp32-hal-gpio.c
  cores/esp32/esp32-hal-i2c.c
  cores/esp32/esp32-hal-ledc.c
  cores/esp32/esp32-hal-matrix.c
  cores/esp32/esp32-hal-misc.c
  cores/esp32/esp32-hal-psram.c
  cores/esp32/esp32-hal-rmt.c
  cores/esp32/esp32-hal-sigmadelta.c
  cores/esp32/esp32-hal-spi.c
  cores/esp32/esp32-hal-time.c
  cores/esp32/esp32-hal-timer.c
  cores/esp32/esp32-hal-touch.c
  cores/esp32/esp32-hal-uart.c
  cores/esp32/esp32-hal.c
  cores/esp32/esp32-hal-cdc.c
  cores/esp32/esp32-hal-jpeg.c
  cores/esp32/esp32-hal-i2s.c
  cores/esp32/esp32-hal-camera.c
  cores/esp32/esp32-hal-sd.c
  cores/esp32/esp32-hal-spi-master.c
  cores/esp32/esp32-hal-spi-slave.c
  cores/esp32/esp32-hal-wdt.c
  cores/esp32/main.cpp
  cores/esp32/Print.cpp
  cores/esp32/Stream.cpp
  cores/esp32/StreamString.cpp
  cores/esp32/Tone.cpp
  cores/esp32/WString.cpp
  cores/esp32/HardwareSerial.cpp
  cores/esp32/HardwareSerial.cpp
  cores/esp32/IPAddress.cpp
  cores/esp32/MD5Builder.cpp
  cores/esp32/PluggableUSB.cpp
  cores/esp32/USBCDC.cpp
  cores/esp32/USB.cpp
  cores/esp32/USBMSC.cpp
  cores/esp32/USBHID.cpp
  cores/esp32/USBCDC.cpp
)

# ============================================================
# Arduino libraries
#
# These lists mirror the upstream Arduino-ESP32 component.
# ============================================================

set(ARDUINO_ALL_LIBRARIES
  AsyncUDP
  BluetoothSerial
  DNSServer
  EEPROM
  ESP32
  ESPmDNS
  Ethernet
  FirmwareMSC
  FS
  HTTPClient
  HTTPUpdate
  LittleFS
  NetBIOS
  Network
  NetworkClientSecure
  Preferences
  SD
  SD_MMC
  SPI
  SPIFFS
  Ticker
  Update
  USB
  WebServer
  WiFi
  WiFiClientSecure
  WiFiProv
  Wire
  Zigbee
)

# ============================================================
# NetworkClientSecure
#
# IMPORTANT:
# Both files MUST be part of the same Arduino component:
#
#   ssl_client.cpp
#   NetworkClientSecure.cpp
#
# NetworkClientSecure.cpp references functions implemented by
# ssl_client.cpp.
# ============================================================

set(ARDUINO_LIBRARY_NetworkClientSecure_SRCS
  libraries/NetworkClientSecure/src/ssl_client.cpp
  libraries/NetworkClientSecure/src/NetworkClientSecure.cpp
)

# ============================================================
# NetworkClientSecure include directories
# ============================================================

set(ARDUINO_LIBRARY_NetworkClientSecure_INCLUDEDIRS
  libraries/NetworkClientSecure/src
)

# ============================================================
# Other Arduino library source definitions
# ============================================================

set(ARDUINO_LIBRARY_AsyncUDP_SRCS
  libraries/AsyncUDP/src/AsyncUDP.cpp
)

set(ARDUINO_LIBRARY_BluetoothSerial_SRCS
  libraries/BluetoothSerial/src/BluetoothSerial.cpp
)

set(ARDUINO_LIBRARY_DNSServer_SRCS
  libraries/DNSServer/src/DNSServer.cpp
)

set(ARDUINO_LIBRARY_EEPROM_SRCS
  libraries/EEPROM/src/EEPROM.cpp
)

set(ARDUINO_LIBRARY_ESPmDNS_SRCS
  libraries/ESPmDNS/src/ESPmDNS.cpp
)

set(ARDUINO_LIBRARY_Ethernet_SRCS
  libraries/Ethernet/src/ETH.cpp
)

set(ARDUINO_LIBRARY_FirmwareMSC_SRCS
  libraries/FirmwareMSC/src/FirmwareMSC.cpp
)

set(ARDUINO_LIBRARY_FS_SRCS
  libraries/FS/src/FS.cpp
  libraries/FS/src/vfs_api.cpp
)

set(ARDUINO_LIBRARY_HTTPClient_SRCS
  libraries/HTTPClient/src/HTTPClient.cpp
)

set(ARDUINO_LIBRARY_HTTPUpdate_SRCS
  libraries/HTTPUpdate/src/HTTPUpdate.cpp
)

set(ARDUINO_LIBRARY_LittleFS_SRCS
  libraries/LittleFS/src/LittleFS.cpp
)

set(ARDUINO_LIBRARY_NetBIOS_SRCS
  libraries/NetBIOS/src/NetBIOS.cpp
)

set(ARDUINO_LIBRARY_Network_SRCS
  libraries/Network/src/NetworkClient.cpp
  libraries/Network/src/NetworkEvents.cpp
  libraries/Network/src/NetworkInterface.cpp
  libraries/Network/src/NetworkManager.cpp
  libraries/Network/src/NetworkServer.cpp
  libraries/Network/src/NetworkUdp.cpp
)

set(ARDUINO_LIBRARY_Preferences_SRCS
  libraries/Preferences/src/Preferences.cpp
)

set(ARDUINO_LIBRARY_SD_SRCS
  libraries/SD/src/SD.cpp
  libraries/SD/src/sd_diskio.cpp
  libraries/SD/src/sd_diskio.cpp
)

set(ARDUINO_LIBRARY_SD_MMC_SRCS
  libraries/SD_MMC/src/SD_MMC.cpp
)

set(ARDUINO_LIBRARY_SPI_SRCS
  libraries/SPI/src/SPI.cpp
)

set(ARDUINO_LIBRARY_SPIFFS_SRCS
  libraries/SPIFFS/src/SPIFFS.cpp
)

set(ARDUINO_LIBRARY_Ticker_SRCS
  libraries/Ticker/src/Ticker.cpp
)

set(ARDUINO_LIBRARY_Update_SRCS
  libraries/Update/src/Updater.cpp
)

set(ARDUINO_LIBRARY_USB_SRCS
  libraries/USB/src/USB.cpp
  libraries/USB/src/USBMSC.cpp
)

set(ARDUINO_LIBRARY_WebServer_SRCS
  libraries/WebServer/src/WebServer.cpp
  libraries/WebServer/src/Parsing.cpp
)

set(ARDUINO_LIBRARY_WiFi_SRCS
  libraries/WiFi/src/AP.cpp
  libraries/WiFi/src/STA.cpp
  libraries/WiFi/src/WiFi.cpp
  libraries/WiFi/src/WiFiAP.cpp
  libraries/WiFi/src/WiFiClient.cpp
  libraries/WiFi/src/WiFiGeneric.cpp
  libraries/WiFi/src/WiFiMulti.cpp
  libraries/WiFi/src/WiFiScan.cpp
  libraries/WiFi/src/WiFiServer.cpp
  libraries/WiFi/src/WiFiSTA.cpp
  libraries/WiFi/src/WiFiUdp.cpp
)

set(ARDUINO_LIBRARY_WiFiClientSecure_SRCS
  libraries/WiFiClientSecure/src/WiFiClientSecure.cpp
)

set(ARDUINO_LIBRARY_WiFiProv_SRCS
  libraries/WiFiProv/src/WiFiProv.cpp
)

set(ARDUINO_LIBRARY_Wire_SRCS
  libraries/Wire/src/Wire.cpp
)

# ============================================================
# Zigbee
# ============================================================

set(ARDUINO_LIBRARY_Zigbee_SRCS
  libraries/Zigbee/src/Zigbee.cpp
  libraries/Zigbee/src/ZigbeeCore.cpp
  libraries/Zigbee/src/ZigbeeEP.cpp
  libraries/Zigbee/src/ZigbeeHandlers.cpp
  libraries/Zigbee/src/ZigbeeNode.cpp
  libraries/Zigbee/src/ZigbeeRemote.cpp
  libraries/Zigbee/src/ZigbeeZCL.cpp

  libraries/Zigbee/src/ep/ZigbeeColorDimmableLight.cpp
  libraries/Zigbee/src/ep/ZigbeeColorDimmableLight.cpp
  libraries/Zigbee/src/ep/ZigbeeDimmableLight.cpp
  libraries/Zigbee/src/ep/ZigbeeDoorWindow.cpp
  libraries/Zigbee/src/ep/ZigbeeFan.cpp
  libraries/Zigbee/src/ep/ZigbeeFlowSensor.cpp
  libraries/Zigbee/src/ep/ZigbeeHAOnOffLight.cpp
  libraries/Zigbee/src/ep/ZigbeeIASZone.cpp
  libraries/Zigbee/src/ep/ZigbeeLight.cpp
  libraries/Zigbee/src/ep/ZigbeeOccupancy.cpp
  libraries/Zigbee/src/ep/ZigbeePressure.cpp
  libraries/Zigbee/src/ep/ZigbeeRangeExtender.cpp
  libraries/Zigbee/src/ep/ZigbeeTempSensor.cpp
  libraries/Zigbee/src/ep/ZigbeeThermostat.cpp
  libraries/Zigbee/src/ep/ZigbeeTime.cpp
  libraries/Zigbee/src/ep/ZigbeeWindowCovering.cpp
)

# ============================================================
# Aggregate Arduino library sources
# ============================================================

set(ARDUINO_LIBRARIES_SRCS)
set(ARDUINO_LIBRARIES_REQUIRES)
set(ARDUINO_LIBRARIES_INCLUDEDIRS)

foreach(libname IN LISTS ARDUINO_ALL_LIBRARIES)

  if(NOT CONFIG_ARDUINO_SELECTIVE_COMPILATION OR CONFIG_ARDUINO_SELECTIVE_${libname})

    if(ARDUINO_LIBRARY_${libname}_SRCS)
      list(APPEND
        ARDUINO_LIBRARIES_SRCS
        ${ARDUINO_LIBRARY_${libname}_SRCS}
      )
    endif()

    if(ARDUINO_LIBRARY_${libname}_REQUIRES)
      list(APPEND
        ARDUINO_LIBRARIES_REQUIRES
        ${ARDUINO_LIBRARY_${libname}_REQUIRES}
      )
    endif()

    if(ARDUINO_LIBRARY_${libname}_INCLUDEDIRS)
      list(APPEND
        ARDUINO_LIBRARIES_INCLUDEDIRS
        ${ARDUINO_LIBRARY_${libname}_INCLUDEDIRS}
      )
    endif()

  endif()

endforeach()

# ============================================================
# Include directories
# ============================================================

set(includedirs
  cores/esp32
  variants/${CONFIG_ARDUINO_VARIANT}
  libraries
  ${ARDUINO_LIBRARIES_INCLUDEDIRS}
)

set(priv_includes
  cores/esp32
  variants/${CONFIG_ARDUINO_VARIANT}
)

# ============================================================
# Component requirements
# ============================================================

set(requires
  esp_adc
  esp_app_format
  esp_bootloader_format
  esp_common
  esp_event
  esp_hw_support
  esp_netif
  esp_partition
  esp_pm
  esp_rom
  esp_system
  esp_timer
  esp_wifi
  freertos
  hal
  heap
  log
  lwip
  mbedtls
  newlib
  nvs_flash
  pthread
  soc
  spi_flash
  usb
  vfs
  wear_levelling
)

set(priv_requires
  driver
)

# ============================================================
# FINAL SOURCE LIST
#
# IMPORTANT:
# Do NOT manually append ssl_client.cpp or
# NetworkClientSecure.cpp here.
#
# They already come from:
#
#   ARDUINO_LIBRARY_NetworkClientSecure_SRCS
#
# through:
#
#   ARDUINO_LIBRARIES_SRCS
# ============================================================

set(srcs
  ${CORE_SRCS}
  ${ARDUINO_LIBRARIES_SRCS}
)

# ============================================================
# Diagnostics
#
# These are intentionally limited to message()/list operations.
# Do not perform filesystem checks or target-property queries
# here because ESP-IDF evaluates this file during its component
# requirements scan as well as the normal configuration pass.
# ============================================================

message(STATUS "")
message(STATUS "============================================================")
message(STATUS "ARDUINO-ESP32 CMAKE DIAGNOSTICS")
message(STATUS "============================================================")

message(STATUS "IDF_VERSION       = ${IDF_VERSION}")
message(STATUS "IDF_TARGET        = ${IDF_TARGET}")
message(STATUS "CONFIG_ARDUINO_VARIANT = ${CONFIG_ARDUINO_VARIANT}")
message(STATUS "CONFIG_ARDUINO_SELECTIVE_COMPILATION = ${CONFIG_ARDUINO_SELECTIVE_COMPILATION}")
message(STATUS "CONFIG_ARDUINO_SELECTIVE_NetworkClientSecure = ${CONFIG_ARDUINO_SELECTIVE_NetworkClientSecure}")

message(STATUS "")
message(STATUS "--- NetworkClientSecure source variable ---")

foreach(src IN LISTS ARDUINO_LIBRARY_NetworkClientSecure_SRCS)
  message(STATUS "NCS declared source: ${src}")
endforeach()

list(LENGTH ARDUINO_LIBRARY_NetworkClientSecure_SRCS NCS_DECLARED_COUNT)
message(STATUS "NetworkClientSecure declared source count = ${NCS_DECLARED_COUNT}")

message(STATUS "")
message(STATUS "--- NetworkClientSecure aggregate sources ---")

set(NCS_AGGREGATE_COUNT 0)

foreach(src IN LISTS ARDUINO_LIBRARIES_SRCS)

  if(src MATCHES "libraries/NetworkClientSecure/src/")

    math(EXPR NCS_AGGREGATE_COUNT
      "${NCS_AGGREGATE_COUNT} + 1"
    )

    message(STATUS
      "NCS aggregate source ${NCS_AGGREGATE_COUNT}: ${src}"
    )

  endif()

endforeach()

message(STATUS
  "NetworkClientSecure aggregate source count = ${NCS_AGGREGATE_COUNT}"
)

message(STATUS "")
message(STATUS "--- Final Arduino source list: NetworkClientSecure entries ---")

set(NCS_FINAL_COUNT 0)

foreach(src IN LISTS srcs)

  if(src MATCHES "libraries/NetworkClientSecure/src/")

    math(EXPR NCS_FINAL_COUNT
      "${NCS_FINAL_COUNT} + 1"
    )

    message(STATUS
      "NCS final source ${NCS_FINAL_COUNT}: ${src}"
    )

  endif()

endforeach()

message(STATUS
  "NetworkClientSecure final source count = ${NCS_FINAL_COUNT}"
)

message(STATUS "")
message(STATUS "--- Final Arduino source list count ---")

list(LENGTH srcs FINAL_ARDUINO_SOURCE_COUNT)

message(STATUS
  "Final Arduino source count = ${FINAL_ARDUINO_SOURCE_COUNT}"
)

message(STATUS "")
message(STATUS "--- Final Arduino source list ---")

set(FINAL_ARDUINO_INDEX 0)

foreach(src IN LISTS srcs)

  math(EXPR FINAL_ARDUINO_INDEX
    "${FINAL_ARDUINO_INDEX} + 1"
  )

  message(STATUS
    "ARDUINO SRC ${FINAL_ARDUINO_INDEX}: ${src}"
  )

endforeach()

message(STATUS "")
message(STATUS "============================================================")
message(STATUS "END ARDUINO-ESP32 CMAKE DIAGNOSTICS")
message(STATUS "============================================================")
message(STATUS "")

# ============================================================
# Register component
# ============================================================

idf_component_register(
  INCLUDE_DIRS ${includedirs}
  PRIV_INCLUDE_DIRS ${priv_includes}
  SRCS ${srcs}
  REQUIRES ${requires}
  PRIV_REQUIRES ${priv_requires}
)