idf_component_register(SRCS "st7789.c" "hydroponic_board.c" "fontx.c"
                    INCLUDE_DIRS "."
                    REQUIRES kinetic_os lvgl driver vfs)
