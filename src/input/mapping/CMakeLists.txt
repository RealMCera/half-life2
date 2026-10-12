# Opt-in utility: existing xbox_input readers do not link or use it.
add_library(xbox_input_map STATIC EXCLUDE_FROM_ALL ../input_map.c)
target_include_directories(xbox_input_map PUBLIC ${CMAKE_CURRENT_LIST_DIR}/..)
target_compile_features(xbox_input_map PUBLIC c_std_11)
if(NOT MSVC)
    target_link_libraries(xbox_input_map PUBLIC m)
endif()
