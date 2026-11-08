idf_component_register(SRCS "audio_player.cpp"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES driver spiffs)

spiffs_create_partition_image(storage ../spiffs FLASH_IN_PROJECT)