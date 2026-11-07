cmake_minimum_required(VERSION 3.16)

project(lvgl_two C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(CARMW_BUILD_TESTS "Build the carmw unit tests" ON)
option(CARMW_BUILD_UI "Build the LVGL dashboard UI" ON)

# 平台切换：PC = SDL2 模拟器；BOARD = 板上 Linux framebuffer(/dev/fb0)
set(CARMW_PLATFORM "PC" CACHE STRING "UI target platform: PC or BOARD")
set_property(CACHE CARMW_PLATFORM PROPERTY STRINGS PC BOARD)
if(NOT CARMW_PLATFORM MATCHES "^(PC|BOARD)$")
    message(FATAL_ERROR "CARMW_PLATFORM 只能是 PC 或 BOARD，当前值为: ${CARMW_PLATFORM}")
endif()
if(CARMW_PLATFORM STREQUAL "BOARD" AND NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "BOARD 平台使用 Linux framebuffer，目标系统必须是 Linux")
endif()

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    set(CARMW_WARN_FLAGS -Wall -Wextra -Wpedantic -Wshadow -Wstrict-prototypes)
else()
    set(CARMW_WARN_FLAGS)
endif()

# ---------------------------------------------------------------------------
# 核心库：与平台无关（parser / decode / registry / state / command / session）
# ---------------------------------------------------------------------------
add_library(carmw STATIC
    src/car_protocol.c
    src/car_parser.c
    src/car_decode.c
    src/car_registry.c
    src/car_state.c
    src/car_color.c
    src/car_command.c
    src/car_session.c
    src/car_link.c
    src/car_port.c
    src/car_settings.c
    src/port/car_port_memory.c
)

target_include_directories(carmw PUBLIC include)
target_compile_options(carmw PRIVATE ${CARMW_WARN_FLAGS})

# ---------------------------------------------------------------------------
# POSIX 串口实现 + 线程支持（板子和 Linux PC 用；MCU 移植时可以不编译这一段）
# ---------------------------------------------------------------------------
if(UNIX)
    target_sources(carmw PRIVATE src/port/car_port_posix.c
                                src/car_settings_saver.c)
    find_package(Threads REQUIRED)
    target_link_libraries(carmw PUBLIC Threads::Threads)
endif()

# ---------------------------------------------------------------------------
# 测试与演示（交叉编译时不构建，避免把测试带上板子）
# ---------------------------------------------------------------------------
if(CARMW_BUILD_TESTS AND NOT CMAKE_CROSSCOMPILING)
    enable_testing()

    add_executable(carmw_tests
        tests/main.c
        tests/test_parser.c
        tests/test_registry.c
        tests/test_decode.c
        tests/test_state.c
        tests/test_session.c
        tests/test_command.c
        tests/test_link.c
    )

    # src 用于白盒测试（car_decode_internal.h）
    target_include_directories(carmw_tests PRIVATE src tests)
    target_link_libraries(carmw_tests PRIVATE carmw)
    target_compile_options(carmw_tests PRIVATE ${CARMW_WARN_FLAGS})

    add_test(NAME carmw COMMAND carmw_tests)

    add_executable(carmw_settings_tests tests/test_settings.c)
    target_link_libraries(carmw_settings_tests PRIVATE carmw)
    target_compile_options(carmw_settings_tests PRIVATE ${CARMW_WARN_FLAGS})
    add_test(NAME carmw_settings COMMAND carmw_settings_tests)

    if(UNIX)
        add_executable(carmw_demo examples/demo.c)
        target_link_libraries(carmw_demo PRIVATE carmw)
    endif()
endif()

# ---------------------------------------------------------------------------
# 可选：LVGL 9.5 仪表 UI
#   CARMW_PLATFORM=PC    -> SDL2 窗口（桌面调试）
#   CARMW_PLATFORM=BOARD -> /dev/fb0（板上运行，可交叉编译）
# 平台差异只体现在 lv_conf 与链接库，源码通过 CARMW_PLATFORM_PC/BOARD 宏切换。
# ---------------------------------------------------------------------------
if(CARMW_BUILD_UI)
    if(CARMW_PLATFORM STREQUAL "PC")
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
        set(CARMW_LV_CONF "${CMAKE_CURRENT_SOURCE_DIR}/config/lv_conf.h")
    else()
        set(CARMW_LV_CONF "${CMAKE_CURRENT_SOURCE_DIR}/config/lv_conf_board.h")
    endif()

    set(LV_BUILD_CONF_PATH "${CARMW_LV_CONF}" CACHE PATH "" FORCE)
    set(LV_BUILD_CONF_DIR "" CACHE PATH "" FORCE)
    set(LV_BUILD_SET_CONFIG_OPTS OFF CACHE BOOL "" FORCE)
    set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
    set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

    add_subdirectory(third_party/lvgl "${CMAKE_BINARY_DIR}/lvgl" EXCLUDE_FROM_ALL)

    if(CARMW_PLATFORM STREQUAL "PC")
        # lv_fs_stdio.c 使用 dirent 的 DT_DIR，严格 C11 下需要 glibc 的 _DEFAULT_SOURCE
        target_compile_definitions(lvgl PRIVATE _POSIX_C_SOURCE=200809L _DEFAULT_SOURCE)
        target_link_libraries(lvgl PUBLIC PkgConfig::SDL2 m)
    else()
        # fbdev 驱动用到 clock_gettime/CLOCK_MONOTONIC，要求 POSIX 2008 声明
        target_compile_definitions(lvgl PRIVATE _POSIX_C_SOURCE=200809L _DEFAULT_SOURCE)
        target_link_libraries(lvgl PUBLIC m)
    endif()
    
    add_library(carmw_ui STATIC
        src/ui/ui_common.c
        src/ui/ui_assets.c
        src/ui/ui_app.c
        src/ui/car_ui_key.c
        src/ui/ui_session.c
        src/ui/ui_style_registry.c
        src/ui/widgets/ui_lamp.c
        src/ui/widgets/ui_dial.c
        src/ui/SpeedDial/speed_forester1.c
        src/ui/RotationDial/rotate_forester1.c
        src/ui/widgets/ui_tick.c
        src/ui/FuelTank/fuel_forester1.c
        src/ui/WaterTemp/water_forester1.c
        src/ui/ComControl/ui_gear.c
        src/ui/ComControl/ui_mileage.c
        src/ui/ComControl/ui_range.c
        src/ui/ComControl/ui_info_row.c
        src/ui/ComControl/ui_clock.c
        src/ui/ComControl/ui_outside_temp.c
        src/ui/ComControl/ui_turn_signal.c
        src/ui/ComControl/ui_cruise.c
        src/ui/ComControl/ui_small_door.c
        src/ui/ComControl/ui_lamp_row.c
        src/ui/ComControl/ui_lamp_group.c
        src/ui/PageModel/page_subaru_forester1.c
        src/ui/PageModel/page_itinerary.c
        src/ui/PageModel/page_tire.c
        src/ui/PageModel/page_chassis.c
        src/ui/PageModel/page_4i.c
        src/ui/PageModel/page_short_term.c
        src/ui/PageModel/page_instant_fuel.c
        src/ui/PageModel/page_instant_fuel.c
        src/ui/PageModel/page_eco.c
        src/ui/PageModel/page_music.c
        src/ui/PageModel/page_connect.c
        src/ui/PageModel/page_general1.c
        src/ui/overlays/ov_door.c
        src/ui/overlays/ov_tire.c
        src/ui/overlays/ov_radar.c
        src/ui/overlays/ov_cruise.c
        src/ui/overlays/ov_tips.c
        src/ui/settings/ui_settings_page.c
    )
    target_include_directories(carmw_ui PUBLIC src)
    target_link_libraries(carmw_ui PUBLIC carmw lvgl::lvgl)
    target_compile_options(carmw_ui PRIVATE ${CARMW_WARN_FLAGS})
    target_compile_definitions(carmw_ui PUBLIC "CARMW_PLATFORM_${CARMW_PLATFORM}=1")

    # PC 专用离屏截图（ui_png 写 PNG + ui_snapshot 重绘屏幕），用于 D1 视觉对比
    if(CARMW_PLATFORM STREQUAL "PC")
        find_package(ZLIB REQUIRED)
        target_sources(carmw_ui PRIVATE src/ui/ui_png.c src/ui/ui_snapshot.c)
        target_link_libraries(carmw_ui PUBLIC ZLIB::ZLIB)
    endif()

    if(CARMW_BUILD_TESTS AND NOT CMAKE_CROSSCOMPILING)
        add_executable(carmw_overlay_tests tests/test_overlays.c)
        target_link_libraries(carmw_overlay_tests PRIVATE carmw_ui)
        target_compile_options(carmw_overlay_tests PRIVATE ${CARMW_WARN_FLAGS})
        add_test(NAME carmw_overlays COMMAND carmw_overlay_tests)
        set_tests_properties(carmw_overlays PROPERTIES
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

        add_executable(carmw_turn_signal_tests tests/test_turn_signal.c)
        target_link_libraries(carmw_turn_signal_tests PRIVATE carmw_ui)
        target_compile_options(carmw_turn_signal_tests PRIVATE ${CARMW_WARN_FLAGS})
        add_test(NAME carmw_turn_signal COMMAND carmw_turn_signal_tests)
        set_tests_properties(carmw_turn_signal PROPERTIES
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

        add_executable(carmw_dial_tests tests/test_dial.c)
        target_link_libraries(carmw_dial_tests PRIVATE carmw_ui)
        target_compile_options(carmw_dial_tests PRIVATE ${CARMW_WARN_FLAGS})
        add_test(NAME carmw_dial COMMAND carmw_dial_tests)
        set_tests_properties(carmw_dial PROPERTIES
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

        add_executable(carmw_steering_key_tests tests/test_steering_key.c)
        target_link_libraries(carmw_steering_key_tests PRIVATE carmw_ui)
        target_compile_options(carmw_steering_key_tests PRIVATE ${CARMW_WARN_FLAGS})
        add_test(NAME carmw_steering_key COMMAND carmw_steering_key_tests)
        set_tests_properties(carmw_steering_key PROPERTIES
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
    endif()

    add_executable(carmw_ui_demo main.c)
    target_link_libraries(carmw_ui_demo PRIVATE carmw_ui)
    target_compile_options(carmw_ui_demo PRIVATE ${CARMW_WARN_FLAGS})
    set_target_properties(carmw_ui_demo PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endif()
