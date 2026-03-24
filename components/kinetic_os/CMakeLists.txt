idf_component_register(SRCS "flask_img.c" "lightbulb_img.c" "thermometer_img.c" "bolt_img.c" "kinetic_os.c"
                       INCLUDE_DIRS "."
                       REQUIRES lvgl)
target_compile_definitions(${COMPONENT_LIB} PUBLIC "-DLV_COLOR_16_SWAP=1")
