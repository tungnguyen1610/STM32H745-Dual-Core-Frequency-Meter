target_sources(${CM7_TARGET} PUBLIC
    main.c
    syscall.c
    sysmem.c
    gc9a01.c
    stm32h7xx_hal_msp.c
    stm32h7xx_it.c
)

add_subdirectory(standard_output)