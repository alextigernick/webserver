cmake_minimum_required (VERSION 3.0)
project (http_server)
find_package(Boost REQUIRED COMPONENTS system python37)
find_package (Python3 COMPONENTS Development)


set(CMAKE_CXX_COMPILER "g++-8")

#add_library(test SHARED src/test.h src/test.cpp )
#target_link_libraries(test )

add_executable(main src/main.cpp ${Python3_INCLUDE_DIRS})
target_link_libraries(main zmq stdc++fs ${Boost_LIBRARIES} ${Python3_LIBRARIES})
set_target_properties(main PROPERTIES CXX_STANDARD 17)


