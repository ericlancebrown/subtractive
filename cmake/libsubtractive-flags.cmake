macro(set_compiler_flags cxxStandard pedanticMode)

  set(CMAKE_CXX_STANDARD ${cxxStandard})
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_CXX_EXTENSIONS OFF)

  if(MSVC)
    set(${PROJECT_NAME}_WARNING_FLAGS "/bigobj")
  else()
    set(${PROJECT_NAME}_WARNING_FLAGS "-W -Wall -Wextra -pedantic")
  endif()

  if(${CMAKE_CXX_COMPILER_ID} MATCHES GNU)
    set(
      ${PROJECT_NAME}_WARNING_FLAGS
      "${${PROJECT_NAME}_WARNING_FLAGS} -Wcast-align -Wctor-dtor-privacy -Wdouble-promotion -Wduplicated-branches -Wduplicated-cond -Weffc++ -Wformat=2 -Winit-self -Wlogical-op -Wmissing-declarations -Wmissing-include-dirs -Wnull-dereference -Wold-style-cast -Woverloaded-virtual -Wrestrict -Wshadow -Wstrict-null-sentinel -Wswitch-default -Wswitch-enum -Wundef -Wunused-macros"
    )
  elseif(${CMAKE_CXX_COMPILER_ID} MATCHES Clang)
    set(
      ${PROJECT_NAME}_WARNING_FLAGS
      "${${PROJECT_NAME}_WARNING_FLAGS} -Weverything -Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-covered-switch-default -Wno-exit-time-destructors -Wno-global-constructors -Wno-missing-prototypes -Wno-missing-variable-declarations -Wno-padded -Wno-redundant-parens -Wno-undefined-func-template -Wno-unknown-warning-option -Wno-weak-template-vtables -Wno-weak-vtables -Wno-reserved-id-macro -Wno-poison-system-directories -Wno-documentation-unknown-command -Wno-zero-as-null-pointer-constant -Wno-shadow-field -Wno-undef -Wno-newline-eof -Wno-float-equal"
    )
  elseif(MSVC)
    set(
      ${PROJECT_NAME}_WARNING_FLAGS
      "${${PROJECT_NAME}_WARNING_FLAGS} /W3 /wd4068 /wd4250"
    )
  endif()

  if(NOT MSVC)
    set(
      ${PROJECT_NAME}_WARNING_FLAGS
      "${${PROJECT_NAME}_WARNING_FLAGS} -Wno-pragmas -Wno-unknown-pragmas"
    )
  endif()

  if(${pedanticMode})
    if(MSVC)
      set(${PROJECT_NAME}_PEDANTIC_FLAG "/WX")
      set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /WX")
      set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /WX")
      set(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS} /WX")
    else()
      set(${PROJECT_NAME}_PEDANTIC_FLAG "-Werror -pedantic-errors")
    endif()
  else()
    set(${PROJECT_NAME}_PEDANTIC_FLAG "")
  endif()

  set(
    CMAKE_CXX_FLAGS
    "${CMAKE_CXX_FLAGS} ${${PROJECT_NAME}_PEDANTIC_FLAG} ${${PROJECT_NAME}_WARNING_FLAGS} -pthread -fPIC"
  )
endmacro()
