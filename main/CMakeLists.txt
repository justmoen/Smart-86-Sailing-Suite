file(GLOB UI_SRCS
    "ui/*.cpp"
    "screens/*.c"
    "screens/*.cpp"
    "*.cpp"
    "*.c"
)

idf_component_register(
    SRCS
        ${UI_SRCS}

    INCLUDE_DIRS
        "."
        "ui"
        "screens"
)