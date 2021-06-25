macro(set_Options)

if(MSVC)
    # enable RTTI
    SET (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc /GR")
else()
    #SET (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-switch -Wno-unused-function -Wno-unused-result -Wno-unused-variable -Wno-unused-private-field -Wno-sign-compare -Wno-implicit-fallthrough -Wno-logical-op-parentheses -Wno-parentheses -Wno-unused-command-line-argument -frtti -lstdc++fs")
    SET (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-switch -Wno-unused-function -Wno-unused-result -Wno-unused-variable -Wno-sign-compare -Wno-implicit-fallthrough -Wno-parentheses -Wno-type-limits -Wno-unused-but-set-variable")
    # enable RTTI
    SET (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -frtti -fexceptions")
    SET (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -lstdc++fs")
    link_libraries(stdc++fs)
endif()

endmacro()
