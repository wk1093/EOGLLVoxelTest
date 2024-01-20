cmake_minimum_required(VERSION 3.26)
project(eoglltest C)

# use the FetchContent module to download and build eogll
include(FetchContent)

# declare where to download eogll from
FetchContent_Declare(
        eogll
        GIT_REPOSITORY https://github.com/wk1093/eogll.git
        GIT_TAG        master
)

# download and build eogll
FetchContent_MakeAvailable(eogll)

# our code
add_executable(eoglltest main.c)

# our code uses eogll
target_link_libraries(eoglltest eogll)
