set(CMAKE_C_STANDARD 11)

set(CMAKE_C_COMPILER ${aarch64_gcc})
set(CMAKE_ASM_COMPILER ${aarch64_gcc})

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../libc/obj/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../libc/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../libc/arch/aarch64)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../libc/arch/generic)

set(compiler_warnings "-Wall -Wextra \
    -Wconversion -Wsign-conversion")
set(compiler_flags "${compiler_warnings} \
    -fno-pie -fno-pic -fno-stack-protector \
    -fno-zero-initialized-in-bss \
    -static -fno-builtin -nostdlib -nostdinc -ffreestanding -nostartfiles \
    -mgeneral-regs-only \
    -MMD -MP \
    -mlittle-endian -mcmodel=small -mno-outline-atomics \
    -mcpu=cortex-a53 -mtune=cortex-a53")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${compiler_flags}")
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} ${compiler_flags}")

set(linker_script "${CMAKE_CURRENT_SOURCE_DIR}/linker.ld")
set(LINK_DEPENDS "${LINK_DEPENDS} ${linker_script}")

# "--build-id=none": remove ".note.gnu.build-id" section.
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} \
    -T ${linker_script} \
    -Wl,--build-id=none")

add_subdirectory(aarch64)
add_subdirectory(asm)
add_subdirectory(common)
add_subdirectory(core)
add_subdirectory(driver)
add_subdirectory(user)
add_subdirectory(fs)

set(kernel_name kernel8)
add_executable(${kernel_name}.elf main.c start.S)
target_link_libraries(${kernel_name}.elf core fs asm driver aarch64 common user)

# because there's a cyclic dependency between core and fs, we need to link them
# again to resolve "undefined reference" issues.
target_link_libraries(${kernel_name}.elf core fs)

set(kernel_prefix "${CMAKE_CURRENT_BINARY_DIR}/${kernel_name}")
set(kernel_elf "${kernel_prefix}.elf")
set(kernel_asm "${kernel_prefix}.asm")
set(kernel_hdr "${kernel_prefix}.hdr")
set(kernel_img "${kernel_prefix}.img")

add_custom_command(
    OUTPUT ${kernel_asm}
    COMMAND ${aarch64_objdump} -S -d ${kernel_elf} > ${kernel_asm}
    DEPENDS ${kernel_elf})

add_custom_command(
    OUTPUT ${kernel_hdr}
    COMMAND ${aarch64_objdump} -x ${kernel_elf} > ${kernel_hdr}
    DEPENDS ${kernel_elf})

add_custom_command(
    OUTPUT ${kernel_img}
    COMMAND ${aarch64_objcopy} -O binary ${kernel_elf} ${kernel_img}
    DEPENDS ${kernel_elf})

add_custom_target(kernel ALL DEPENDS ${kernel_asm} ${kernel_hdr} ${kernel_img})

set_property(GLOBAL PROPERTY kernel_elf_path ${kernel_elf})
set_property(GLOBAL PROPERTY kernel_image_path ${kernel_img})
