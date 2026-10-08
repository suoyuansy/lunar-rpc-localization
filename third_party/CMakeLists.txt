# 查找 OpenCV 和 libtiff
find_package(OpenCV REQUIRED COMPONENTS core imgcodecs imgproc highgui)
find_package(TIFF REQUIRED)

# 统一接口目标，业务模块以后只链接 third_party_deps
add_library(third_party_deps INTERFACE)

target_include_directories(
    third_party_deps
    INTERFACE
        ${OpenCV_INCLUDE_DIRS}
        ${TIFF_INCLUDE_DIRS}
)

# 将 OpenCV 和 libtiff 的库文件链接到统一目标
if(TARGET TIFF::TIFF)
    target_link_libraries(third_party_deps INTERFACE ${OpenCV_LIBS} TIFF::TIFF)
else()
    target_link_libraries(third_party_deps INTERFACE ${OpenCV_LIBS} ${TIFF_LIBRARIES})
endif()