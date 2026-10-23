# The CAN matrix and E2E sources are shared with the STM32 firmware
idf_component_register(
    SRCS "main.c" "can_if.c" "sim.c" "console.c"
         "../../common/src/can_matrix.c"
         "../../common/src/e2e.c"
    INCLUDE_DIRS "." "../../common/inc"
    REQUIRES driver esp_timer
)
