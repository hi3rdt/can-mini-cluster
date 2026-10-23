# The CAN matrix, E2E and ISO-TP sources are shared with the STM32 firmware
idf_component_register(
    SRCS "main.c" "can_if.c" "sim.c" "tester.c" "console.c"
         "../../common/src/can_matrix.c"
         "../../common/src/e2e.c"
         "../../common/src/isotp.c"
    INCLUDE_DIRS "." "../../common/inc"
    REQUIRES driver esp_timer
)
