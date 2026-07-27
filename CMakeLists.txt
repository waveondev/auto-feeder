# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

if("${IDF_TARGET}" STREQUAL "linux")
    set(COMPONENTS main)
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(auto_feeder)

# Create a SPIFFS image from the contents of the 'spiffs_image' directory
# that fits the partition named 'storage'. FLASH_IN_PROJECT indicates that
# the generated image should be flashed when the entire project is flashed to
# the target with 'idf.py -p PORT flash'.
spiffs_create_partition_image(spiffs_storage ./spiffs_image FLASH_IN_PROJECT)

# ==========================================
# [맥OS 빌드 에러 해결을 위한 최종 강제 제거 코드]
# ==========================================
# 모든 컴포넌트 등록이 끝난 후, 전역 컴파일 옵션에서 -D__linux__를 강제로 도려냅니다.
get_property(current_compile_options DIRECTORY PROPERTY COMPILE_OPTIONS)
if("-D__linux__" IN_LIST current_compile_options)
    list(REMOVE_ITEM current_compile_options "-D__linux__")
    set_property(DIRECTORY PROPERTY COMPILE_OPTIONS "${current_compile_options}")
endif()

# 컴파일러가 매크로를 읽을 때 __linux__ 정의를 강제로 해제(Undefined)합니다.
add_compile_options("-U__linux__")
# ==========================================