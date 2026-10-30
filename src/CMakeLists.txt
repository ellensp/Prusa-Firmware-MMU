target_sources(firmware PRIVATE application.cpp debug.cpp main.cpp registers.cpp version.cpp)

if(NOT MMU_BOARD STREQUAL "MMU2_DIP")
  target_link_libraries(firmware LUFA)
endif()

add_subdirectory(hal)
add_subdirectory(logic)
add_subdirectory(modules)
