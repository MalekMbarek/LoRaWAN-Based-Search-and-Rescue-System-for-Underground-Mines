# ============================================================
# Top-level CMakeLists.txt
# Selects which application to build via -DAPPLICATION=<name>
# Example: -DAPPLICATION=PersonalNode
# ============================================================
cmake_minimum_required(VERSION 3.20)
project(PFE2 C ASM)

# Set default build type if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type (Debug/Release)" FORCE)
endif()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# List of valid applications
set(AVAILABLE_APPLICATIONS
    LoRaWAN_End_Node
    LoRaWAN_AT_Slave
    PersonalNode
    StaticNode
    ScannerNode
)

# Default app
if(NOT DEFINED APPLICATION)
    set(APPLICATION "LoRaWAN_End_Node" CACHE STRING "Application to build")
endif()

# Validate selection
list(FIND AVAILABLE_APPLICATIONS ${APPLICATION} APP_INDEX)
if(APP_INDEX EQUAL -1)
    message(FATAL_ERROR 
        "Invalid APPLICATION: ${APPLICATION}\n"
        "Available options are: ${AVAILABLE_APPLICATIONS}"
    )
endif()

message(STATUS "Building application: ${APPLICATION}")

# Include the selected application
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/Apps/${APPLICATION})

# Custom target to list available applications
add_custom_target(list-apps
    COMMAND ${CMAKE_COMMAND} -E echo "============================================"
    COMMAND ${CMAKE_COMMAND} -E echo "Available applications:"
    COMMAND ${CMAKE_COMMAND} -E echo "============================================"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "  ${AVAILABLE_APPLICATIONS}"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "============================================"
    COMMAND ${CMAKE_COMMAND} -E echo "Usage: cmake -DAPPLICATION=<name> .."
    COMMAND ${CMAKE_COMMAND} -E echo "============================================"
    COMMENT "List all available applications"
)