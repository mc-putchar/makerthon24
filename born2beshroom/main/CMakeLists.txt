idf_component_register(SRCS "server.c" "main.c" "init_wifi.c"
                    INCLUDE_DIRS "include"
                    EMBED_FILES "include/favicon.ico")
spiffs_create_partition_image(storage ../spiffs_data FLASH_IN_PROJECT)