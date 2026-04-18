# ============================================================
# Top-level CMakeLists.txt
# Selects which application to build via -DAPPLICATION=<name>
# Example: -DAPPLICATION=personal
# ============================================================
cmake_minimum_required(VERSION 3.20)
project(PFE2 C ASM)

# List of valid applications
set(AVAILABLE_APPLICATIONS
    LoRaWAN_End_Node
    LoRaWAN_AT_Slave
    personal
    static
    static1
    static2
    scanner
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

add_subdirectory(Apps/${APPLICATION})