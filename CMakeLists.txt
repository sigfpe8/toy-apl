cmake_minimum_required(VERSION 3.20)
project(toyapl C)

if(APPLE)
	add_compile_options(-fno-signed-char)
elseif(UNIX)
	add_compile_options(-fno-signed-char)
endif()

add_executable(apl
	src/apl.c
	src/aplio.c
	src/editor.c
	src/eval.c
	src/function.c
	src/lexer.c
	src/linalg.c
	src/syscmmd.c
	src/token.c
	src/utf8.c
)

if(UNIX)
	set_target_properties(apl PROPERTIES COMPILE_DEFINITIONS _UNIX_)
endif()

target_compile_features(apl PUBLIC c_std_99)

if(APPLE)
	# Line edit
	target_link_libraries(apl edit)
elseif(UNIX)
	# Math functions
	target_link_libraries(apl m)
endif()

# Supported compilers: AppleClang, Clang, GNU, MSVC, SunPro, Intel
#if (AppleClang)
#    add_compile_options(-fno-signed-chat)
#endif()
