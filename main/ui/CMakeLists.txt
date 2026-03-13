file(GLOB SCREEN_SRCS
    "screens/*.c"
    "screens/*.cpp"
)

idf_component_register(
    SRCS
        "ui_manager.cpp"
        "navigation.cpp"
        "ui_event_bus.cpp"
        ${SCREEN_SRCS}

    INCLUDE_DIRS
        "."
        "screens"
)