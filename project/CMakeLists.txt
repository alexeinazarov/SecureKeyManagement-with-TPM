#CMakeLists.txt
cmake_minimum_required(VERSION 3.10)
project(KMSApp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Find OpenSSL
find_package(OpenSSL REQUIRED)
if(OPENSSL_FOUND)
    add_compile_definitions(CPPHTTPLIB_OPENSSL_SUPPORT)
    message(STATUS "OpenSSL found: ${OPENSSL_VERSION}")
endif()

# Find TPM library
find_package(PkgConfig REQUIRED)
pkg_check_modules(TSS2_ESYS REQUIRED tss2-esys)

# Add include directories
include_directories(${PROJECT_SOURCE_DIR}/include ${TSS2_ESYS_INCLUDE_DIRS})

# Server executable
add_executable(kms_server 
    src/server_main.cpp 
    src/handlers.cpp 
    src/key_manager.cpp 
    src/utils.cpp 
    src/logger.cpp
)

target_compile_definitions(kms_server PRIVATE KMS_SERVER)
target_link_libraries(kms_server PUBLIC
    OpenSSL::SSL
    OpenSSL::Crypto
    ${TSS2_ESYS_LIBRARIES}
)

# Client executable
add_executable(kms_client 
    src/client_main.cpp 
    src/kms_client.cpp 
    src/key_manager.cpp 
    src/utils.cpp 
    src/logger.cpp
)

target_compile_definitions(kms_client PRIVATE KMS_CLIENT)
target_link_libraries(kms_client PUBLIC
    OpenSSL::SSL
    OpenSSL::Crypto
    ${TSS2_ESYS_LIBRARIES}
)

# Ensure linker can find TSS2 libraries
link_directories(${TSS2_ESYS_LIBRARY_DIRS})

# Ensure include directories are added
include_directories(${TSS2_ESYS_INCLUDE_DIRS})

