file(GLOB UI_SRCS "*.cpp")
file(GLOB SCREEN_SRCS "screens/*.c")

idf_component_register(
    SRCS
        ${UI_SRCS}
        ${SCREEN_SRCS}
    INCLUDE_DIRS
        "."
        "screens"
)