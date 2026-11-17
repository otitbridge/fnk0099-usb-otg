# Keep dependencies unconditional: IDF also evaluates this file before sdkconfig
# is available while collecting component requirements.
if(CONFIG_FNK0099_MSC_PROBE)
    set(probe_source "msc_main.c" "msc_file_probe.c")
else()
    set(probe_source "main.c")
endif()

idf_component_register(SRCS "${probe_source}" INCLUDE_DIRS "."
                       REQUIRES usb esp_timer fatfs esp_driver_uart mbedtls)
