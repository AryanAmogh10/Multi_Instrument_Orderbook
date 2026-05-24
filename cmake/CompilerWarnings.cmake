function(velox_set_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE
            /W4 /WX /permissive- /Zc:__cplusplus
            /wd4458 # declaration hides class member (noisy with intrusive types)
        )
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic -Werror
            -Wshadow -Wnon-virtual-dtor -Wcast-align
            -Wunused -Woverloaded-virtual -Wconversion
            -Wsign-conversion -Wdouble-promotion -Wformat=2
            -Wnull-dereference -Wold-style-cast
        )
    endif()
endfunction()
