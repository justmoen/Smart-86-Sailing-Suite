idf_component_register(
    SRCS "TinyGPSPlus.cpp"
    INCLUDE_DIRS "."
    REQUIRES arduino-esp32
)

target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wno-stringop-truncation
)