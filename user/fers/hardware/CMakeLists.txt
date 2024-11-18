# Set the source and include directories
set(HARDWARE_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(HARDWARE_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Collect all source and header files
file(GLOB HARDWARE_SOURCES "${HARDWARE_SRC_DIR}/*.cc" "${HARDWARE_SRC_DIR}/*.c")
file(GLOB HARDWARE_HEADERS "${HARDWARE_INCLUDE_DIR}/*.h")

# Define the library for the hardware module
add_library(${USER_FERSHW_LIBRARY} STATIC ${HARDWARE_SOURCES} ${HARDWARE_HEADERS})

# Add include directories
target_include_directories(${USER_FERSHW_LIBRARY} PRIVATE ${HARDWARE_INCLUDE_DIR})
target_include_directories(${USER_FERSHW_LIBRARY} SYSTEM PRIVATE /usr/include/libusb-1.0)

# Set the FERS version with a CACHE entry to allow selection in ccmake
set(USER_FERS_VERSION "5202" CACHE STRING "Select the FERS version (5202 or 5215)")

# Provide options for the user to choose from in ccmake
set_property(CACHE USER_FERS_VERSION PROPERTY STRINGS "5202;5215")

# Set the preprocessor definition based on the selected version
if(USER_FERS_VERSION STREQUAL "5202")
    set(FERS_VERSION_DEFINE "FERS_5202")
elseif(USER_FERS_VERSION STREQUAL "5215")
    set(FERS_VERSION_DEFINE "FERS_5215")
else()
    message(FATAL_ERROR "Invalid version selected for FERS. Please choose either 5202 or 5215.")
endif()

# Apply the preprocessor definition to the hardware module
target_compile_definitions(${USER_FERSHW_LIBRARY} PRIVATE ${FERS_VERSION_DEFINE})

# Link necessary libraries (libusb, pthread, rt)
target_link_libraries(${USER_FERSHW_LIBRARY} PRIVATE pthread rt usb-1.0)

install(TARGETS
  ${USER_FERSHW_LIBRARY}
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib)