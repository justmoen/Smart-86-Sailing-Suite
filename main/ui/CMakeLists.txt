idf_component_register(
    SRCS
        "navigation.cpp"
        "ui_event_bus.cpp"

        "screens/ui_compass.c"
        "screens/ui_engine.c"
        "screens/ui_navigation.c"

    INCLUDE_DIRS
        "."
        "screens"
)