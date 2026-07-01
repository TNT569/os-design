cmake_minimum_required(VERSION 3.15)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
project(myos
    VERSION 1.0.0
    DESCRIPTION "An OS with file system running on POSIX CLI"
    LANGUAGES C
  )

# 禁用 MSVC
if(MSVC)
    message(FATAL_ERROR "MSVC is not supported. Use GCC or Clang.")
endif()

# 设置严格的 C 标准
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)  # 使用标准 C，而非 GNU 扩展

# 可选：设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 查找源文件
file(GLOB_RECURSE SOURCES "src/*.cpp" "src/*.c" "utils/*.c")
file(GLOB_RECURSE HEADERS "include/*.hpp" "include/*.h" "utils/*.h")

# 添加可执行文件
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# 包含目录
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils
)

# 启用 POSIX 扩展（strdup, nanosleep 等需要）
target_compile_definitions(${PROJECT_NAME} PRIVATE _POSIX_C_SOURCE=200809L)

# 统一的编译选项
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wall -Wextra -Wpedantic    # 所有常见警告
        -Werror=implicit-function-declaration  # 隐式声明变错误
        -Werror=missing-prototypes  # 函数必须有原型
        -Wstrict-prototypes         # 空参数列表必须用 void
        -Wold-style-definition      # 禁止 K&R 风格函数定义
    )
endif()

# 🆕 集成 clang-tidy（可选但推荐）
find_program(CLANG_TIDY clang-tidy)
if(CLANG_TIDY AND CMAKE_EXPORT_COMPILE_COMMANDS)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        C_CLANG_TIDY   "${CLANG_TIDY}"
    )
    message(STATUS "clang-tidy enabled: ${CLANG_TIDY}")
else()
    message(STATUS "clang-tidy not found, skipping")
endif()
