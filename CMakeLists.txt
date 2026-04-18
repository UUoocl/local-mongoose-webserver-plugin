# Replace lines 57-81 with:

foreach(target_name libobs-metal libobs-metal-objc libobs-metal-objcxx)
    if(TARGET ${target_name})
        set_target_properties(${target_name} PROPERTIES LINKER_LANGUAGE CXX)
    endif()
endforeach()