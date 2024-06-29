idf_component_register(SRCS "main.c" "init_wifi.c" "init_tcp.c"
                    INCLUDE_DIRS "include")
spiffs_create_partition_image(storage ../spiffs_data FLASH_IN_PROJECT)