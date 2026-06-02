cmake_minimum_required(VERSION 3.15)
project(myos
    VERSION 1.0.0
    DESCRIPTION "A modern CMake project which to build a snake"
    LANGUAGES CXX C  # 支持C和C++
)
if(MSVC)
    # 强制关闭所有安全警告
    add_compile_definitions(
        _CRT_SECURE_NO_WARNINGS
        _CRT_NONSTDC_NO_WARNINGS
        _SCL_SECURE_NO_WARNINGS
        _WINSOCK_DEPRECATED_NO_WARNINGS
    )
    
    # 禁用特定错误警告
    add_compile_options(/wd4996)  # 不安全函数警告
    add_compile_options(/wd4710)  # 函数未内联
    add_compile_options(/wd4820)  # 字节填充
    
    # 关键：禁用MSVC的“安全生命周期”检查
    add_compile_options(/sdl-)
    
    # 强制使用C99（尽可能接近）
    add_compile_options(/TC)  # 编译C代码（不是C++）
    
    # 设置C标准（MSVC支持有限）
    set(CMAKE_C_STANDARD 99)
    set(CMAKE_C_STANDARD_REQUIRED ON)
    set(CMAKE_C_EXTENSIONS OFF)
    
    # 禁用MSVC扩展
    add_compile_options(/Za)  # 禁用语言扩展
    
    # 或者用更暴力的方式
    string(REPLACE "/W3" "/W0" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
endif()
# 现代策略设置
cmake_policy(SET CMP0077 NEW)  # 目标名称的选项
cmake_policy(SET CMP0091 NEW)  # MSVC运行时库设置

# 编译特性
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 99)

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
)

# 编译选项
target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive->
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
)

