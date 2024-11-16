# Define the source and include directories for the module
set(MODULE_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(MODULE_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Collect all source and header files for the module
file(GLOB MODULE_SOURCES "${MODULE_SRC_DIR}/*.cc" "${MODULE_SRC_DIR}/*.c")
file(GLOB MODULE_HEADERS "${MODULE_INCLUDE_DIR}/*.h")

# Define the library for the module
add_library(module_library STATIC ${MODULE_SOURCES} ${MODULE_HEADERS})

# Define the FERS to pick the correct files
target_compile_definitions(module_library PRIVATE FERS_5202)

# Add include directories
target_include_directories(module_library PRIVATE ${MODULE_INCLUDE_DIR})
target_include_directories(module_library PRIVATE ${HARDWARE_INCLUDE_DIR})
target_include_directories(module_library PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../hardware/include)

# Ensure the hardware library is linked to this module
target_link_libraries(module_library PRIVATE ${USER_HARDWARE_LIBRARY})

# Define any additional compiler definitions if needed
# target_compile_definitions(module_library PRIVATE SOME_MODULE_SPECIFIC_MACRO)

# Define the preprocessor directive for the module library

# Optional: If there are external dependencies for the module, link them here
# target_link_libraries(module_library PRIVATE <additional libraries>)
