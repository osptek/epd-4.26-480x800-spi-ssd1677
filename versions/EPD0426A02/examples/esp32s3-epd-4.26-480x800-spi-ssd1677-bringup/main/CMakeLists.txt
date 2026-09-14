file(GLOB_RECURSE SOURCES
    ./*.c
    ./app_init/*.c
)

idf_component_register(
    SRCS
        ${SOURCES}
    INCLUDE_DIRS
        "."
        "./app_init"
        "./images"
        "./fonts"
)
