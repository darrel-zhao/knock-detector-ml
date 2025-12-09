cmake_minimum_required(VERSION 3.13)
project(ei_infer)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Grab ALL Edge Impulse + TFLM sources
file(GLOB_RECURSE EI_SOURCES
    "edge-impulse-sdk/*.cpp"
    "edge-impulse-sdk/*.cc"
    "edge-impulse-sdk/*.c"
    "tflite-model/*.cpp"
)

add_executable(ei_infer
    live_inference.cpp        
    ${EI_SOURCES}
)

target_include_directories(ei_infer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/edge-impulse-sdk
    ${CMAKE_CURRENT_SOURCE_DIR}/model-parameters
    ${CMAKE_CURRENT_SOURCE_DIR}/tflite-model
)
