
# Create the project.
cmake_minimum_required(VERSION 3.30.8)
project(khess VERSION 0.0.1)

# Compile Flags
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_FLAGS_RELEASE "-O3")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_EXPORT_COMPILE_COMMANDS True)

add_compile_options(-mavx2)

# Add the chessboard library.
add_library(cblib
	src/cblib/cb_magical.c
	src/cblib/cb_normal.c
	src/cblib/cb_gen.c
	src/cblib/cb_lib.c
	src/cblib/cb_const.c
  src/cblib/cb_dbg.c
)
target_include_directories(cblib
	PRIVATE
		${PROJECT_SOURCE_DIR}/include
		${PROJECT_SOURCE_DIR}/include/cblib
		${PROJECT_SOURCE_DIR}/include/utils
)

# Add the debug executable.
add_executable(debug
	src/debug/debug.c
  src/debug/perft.c
)
target_include_directories(debug
	PRIVATE
		${PROJECT_SOURCE_DIR}/include
		${PROJECT_SOURCE_DIR}/include/cblib
		${PROJECT_SOURCE_DIR}/include/debug
		${PROJECT_SOURCE_DIR}/include/utils
)
target_link_libraries(debug
	cblib
)

# Create the engine executable.
add_executable(cibyl
	src/cibyl/cibyl.c
	src/cibyl/uci.c
	src/cibyl/engine.c
)
target_include_directories(cibyl
	PRIVATE
		${PROJECT_SOURCE_DIR}/include
		${PROJECT_SOURCE_DIR}/include/cibyl
		${PROJECT_SOURCE_DIR}/include/cblib
)
target_link_libraries(cibyl
	cblib
)

# Create the server executable.
add_executable(kcsrv
	src/kcsrv/kcsrv.c
)
target_include_directories(kcsrv
	PRIVATE
		${PROJECT_SOURCE_DIR}/include
		${PROJECT_SOURCE_DIR}/include/kcsrv
)

# Create the chess executable.
add_executable(khess
	src/khess/khess.c
)
target_include_directories(khess
	PRIVATE
		${PROJECT_SOURCE_DIR}/include
		${PROJECT_SOURCE_DIR}/include/khess
)
