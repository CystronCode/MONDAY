# cmake/Sanitizers.cmake
set(MONDAY_SANITIZER "" CACHE STRING
    "Sanitizer to enable: asan | tsan | ubsan | asan-ubsan | none")

if(MONDAY_SANITIZER STREQUAL "asan")
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=address)
elseif(MONDAY_SANITIZER STREQUAL "tsan")
    add_compile_options(-fsanitize=thread -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=thread)
elseif(MONDAY_SANITIZER STREQUAL "ubsan")
    add_compile_options(-fsanitize=undefined -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=undefined)
elseif(MONDAY_SANITIZER STREQUAL "asan-ubsan")
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=address,undefined)
endif()
