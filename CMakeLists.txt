cmake_minimum_required(VERSION 3.16)
set(ADDON_NAME tcxNozzle)

set(NOZZLE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/nozzle)

file(GLOB ADDON_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
file(GLOB ADDON_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h)

if(APPLE)
    list(APPEND ADDON_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/tcxNozzle_blit.mm)
endif()

if(NOT ADDON_SOURCES)
    add_library(${ADDON_NAME} INTERFACE)
    target_include_directories(${ADDON_NAME} INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${nozzle_SOURCE_DIR}/include
    )
    target_link_libraries(${ADDON_NAME} INTERFACE
        TrussC
        nozzle
    )
else()
    add_library(${ADDON_NAME} STATIC ${ADDON_SOURCES} ${ADDON_HEADERS})
    target_include_directories(${ADDON_NAME} PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${nozzle_SOURCE_DIR}/include
    )
    target_link_libraries(${ADDON_NAME} PUBLIC
        TrussC
        nozzle
    )
    if(APPLE)
        target_link_libraries(${ADDON_NAME} PUBLIC
            "-framework Metal"
            "-framework IOSurface"
            "-framework Foundation"
        )
    endif()
endif()

add_library(tc::${ADDON_NAME} ALIAS ${ADDON_NAME})
