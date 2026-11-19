# Crack the Safe -- a safe-cracking game for the Waveshare
# ESP32-S3-Knob-Touch-LCD-1.8 (360x360 round touch LCD, detent knob, DRV2605
# haptics).
#
# The GUI is authored in EEZ Studio (GUI/CrackTheSafe.eez-project) and
# exported to main/ui/, which can be deleted and re-exported at any time.
# Board support lives in components/knob_board.
#
#   idf.py set-target esp32s3     # first build on a fresh clone
#   idf.py build
cmake_minimum_required(VERSION 3.16)
# The firmware version the knob reports and compares against GitHub releases
# (main/net.c): the release workflow writes the tag to fw_version.txt; a
# local build takes `git describe` (e.g. v1.2.0-3-gabc1234-dirty, which
# counts as 1.2.0).
if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/fw_version.txt")
    file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/fw_version.txt" PROJECT_VER LIMIT_COUNT 1)
else()
    execute_process(COMMAND git describe --tags --always --dirty
                    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
                    OUTPUT_VARIABLE GIT_VER OUTPUT_STRIP_TRAILING_WHITESPACE
                    ERROR_QUIET RESULT_VARIABLE GIT_RES)
    if(GIT_RES EQUAL 0 AND GIT_VER MATCHES "^v[0-9]")
        set(PROJECT_VER "${GIT_VER}")
    else()
        set(PROJECT_VER "v0.0.0-dev")
    endif()
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(crack_the_safe)
