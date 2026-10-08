# 当前测试入口只验证构建环境是否可用。
# 后续每个功能模块开发时，先增加失败测试，再实现代码。

add_test(
    NAME build_scaffold_available
    COMMAND ${CMAKE_COMMAND} -E echo "build scaffold available"
)
