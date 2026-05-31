include(FetchContent)

# stb - header-only image loading/writing
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG master
)
FetchContent_MakeAvailable(stb)

# Copy stb headers to deps/stb for stable include path
file(COPY "${stb_SOURCE_DIR}/stb_image.h" "${stb_SOURCE_DIR}/stb_image_write.h"
     DESTINATION "${CMAKE_CURRENT_SOURCE_DIR}/deps/stb")

# Catch2 (tests only)
if(SEAMLINE_BUILD_TESTS)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.8.1
    )
    FetchContent_MakeAvailable(Catch2)
endif()
