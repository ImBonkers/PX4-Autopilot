
# Cortex-M55 (ARMv8.1-M Mainline)
# +nomve is critical — without it GCC generates Helium instructions that fault
# +dsp enables DSP extension
set(cpu_flags "-mcpu=cortex-m55+nomve -mthumb -mfpu=fpv5-d16 -mfloat-abi=hard")

set(CMAKE_C_FLAGS "${cpu_flags}" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS "${cpu_flags}" CACHE STRING "" FORCE)
set(CMAKE_ASM_FLAGS "${cpu_flags} -D__ASSEMBLY__" CACHE STRING "" FORCE)
