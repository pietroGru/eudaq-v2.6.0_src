option(USER_FERS_BUILD "Build user/fers" ON)
if(NOT USER_FERS_BUILD)
    message(STATUS "user/fers is NOT to be built (USER_FERS_BUILD=OFF)")
    return()  # exit early if not building
endif()
message(STATUS "user/fers is to be built (USER_FERS_BUILD=ON)")

get_filename_component(EUDAQ_USERNAME ${CMAKE_CURRENT_LIST_DIR} NAME)
# Set the library name for hardware
set(EUDAQ_MODULE ${EUDAQ_PREFIX}module_${EUDAQ_USERNAME})
set(USER_FERSHW_LIBRARY ${EUDAQ_MODULE}_hardware)


# Add hardware and module subdirectories
include_directories(${EUDAQ_INCLUDE_DIRS})
include_directories(hardware/include)
add_subdirectory(hardware)  # Adds the hardware subdirectory
add_subdirectory(module)    # Adds the module subdirectory

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
target_compile_definitions(${EUDAQ_MODULE} PRIVATE ${FERS_VERSION_DEFINE})
target_compile_definitions(${USER_FERSHW_LIBRARY} PRIVATE ${FERS_VERSION_DEFINE})