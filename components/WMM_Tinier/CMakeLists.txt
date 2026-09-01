idf_component_register(
    SRCS "WMM_Tinier.cpp"
    INCLUDE_DIRS "."
    REQUIRES arduino
)

target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wno-stringop-truncation
)