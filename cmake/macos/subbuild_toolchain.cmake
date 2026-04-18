# Toolchain file for OBS sub-build to fix linker language issues
set(CMAKE_LANG_NONE_INIT CXX)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
enable_language(OBJC OBJCXX)
