cmake_minimum_required(VERSION 3.4.1)

# 添加game2048库
add_library(game2048 SHARED
            game2048_jni.c
            game2048_impl.c)

# 查找系统库
find_library(log-lib log)

# 链接库
target_link_libraries(game2048
                      ${log-lib}) 