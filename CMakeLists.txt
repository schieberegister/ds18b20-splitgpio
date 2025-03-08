idf_component_register(
    SRCS "ds18b20_splitgpio.c"
    INCLUDE_DIRS "include"
    REQUIRES driver freertos
)
