idf_component_register(
    SRCS
        "posix_compat/clock_esp.c"
        
    INCLUDE_DIRS
        "."
        "posix_compat"
    
    REQUIRES
        freertos
        esp_timer
)
