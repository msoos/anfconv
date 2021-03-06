cmake_minimum_required(VERSION 2.6 FATAL_ERROR)

IF(DEFINED CMAKE_BUILD_TYPE)
    SET(CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE} CACHE STRING "Choose the type of build, options are: None(CMAKE_CXX_FLAGS or CMAKE_C_FLAGS used) Debug Release RelWithDebInfo MinSizeRel.")
ELSE()
  SET(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Choose the type of build, options are: None(CMAKE_CXX_FLAGS or CMAKE_C_FLAGS used) Debug Release RelWithDebInfo MinSizeRel.")
ENDIF()

PROJECT(anfconv)
SET(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)
macro(add_cxx_flag flagname)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${flagname}")
endmacro()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -g")

SET(CMAKE_C_FLAGS_RELWITHDEBINFO "-O3 -g")
SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O3 -g")
SET(CMAKE_C_FLAGS_DEBUG "-O0 -g")
SET(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")

include(CheckCXXCompilerFlag)
macro(add_cxx_flag_if_supported flagname)
  check_cxx_compiler_flag("${flagname}" HAVE_FLAG_${flagname})

  if(HAVE_FLAG_${flagname})
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${flagname}")
    message(STATUS "flag ${flagname} supported")
  else()
    message(STATUS "flag ${flagname} NOT supported")
  endif()
endmacro()

add_cxx_flag_if_supported("-Wall")
add_cxx_flag_if_supported("-Wextra")
add_cxx_flag_if_supported("-Wunused")
add_cxx_flag_if_supported("-pedantic")
add_cxx_flag_if_supported("-Wsign-compare")
add_cxx_flag_if_supported("-fno-omit-frame-pointer")
add_cxx_flag_if_supported("-Wtype-limits")
add_cxx_flag_if_supported("-Wuninitialized")
add_cxx_flag_if_supported("-Wno-deprecated")
add_cxx_flag_if_supported("-Wstrict-aliasing")
add_cxx_flag_if_supported("-Wpointer-arith")
add_cxx_flag_if_supported("-Wheader-guard")

option(STATICCOMPILE "Compile to static executable" OFF)
if ((${CMAKE_SYSTEM_NAME} MATCHES "Linux") OR (${CMAKE_SYSTEM_NAME} MATCHES "Darwin"))
    if(STATICCOMPILE)
        MESSAGE(STATUS "Compiling for static library use")
        if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
            #set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static -Wl,--whole-archive -ldl -lpthread -Wl,--no-whole-archive -static ")
            set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}  -static ")
        endif()

        SET(CMAKE_FIND_LIBRARY_SUFFIXES ".a")

        # removing -rdynamic that's automatically added
        foreach (language CXX C)
            set(VAR_TO_MODIFY "CMAKE_SHARED_LIBRARY_LINK_${language}_FLAGS")
            string(REGEX REPLACE "(^| )-rdynamic($| )"
                                 " "
                                 replacement
                                 "${${VAR_TO_MODIFY}}")
            #message("Original (${VAR_TO_MODIFY}) is ${${VAR_TO_MODIFY}} replacement is ${replacement}")
            set(${VAR_TO_MODIFY} "${replacement}" CACHE STRING "Default flags for ${build_config} configuration" FORCE)
        endforeach()
    else()
        add_definitions(-DBOOST_TEST_DYN_LINK)
        MESSAGE(STATUS "Compiling for dynamic library use")
    endif()
endif()

find_package( Boost 1.34 REQUIRED COMPONENTS program_options)
link_directories ( ${Boost_LIBRARY_DIRS} )
include_directories ( ${Boost_INCLUDE_DIRS} )

find_package(ZLIB  REQUIRED)
link_directories( ${ZLIB_LIBRARY} )
include_directories( ${ZLIB_INCLUDE_DIR} )

find_package(cryptominisat5 REQUIRED)
if (cryptominisat4_FOUND)
    message(STATUS "OK, CryptoMiniSat5 found")
endif()

include(GetGitRevisionDescription)
get_git_head_revision(GIT_REFSPEC GIT_SHA1)

find_package(M4RI REQUIRED)
include_directories(${M4RI_INCLUDE_DIRS})
link_directories(${anfconv_BINARY_DIR}/espresso)

set(ANFCONV_EXPORT_NAME "anfconvTargets")

add_subdirectory(espresso)
add_subdirectory(src)

# -----------------------------------------------------------------------------
# Add uninstall target for makefiles
# -----------------------------------------------------------------------------
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)

add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)

# -----------------------------------------------------------------------------
# Export our targets so that other CMake based projects can interface with
# the build of anfconv in the build-tree
# -----------------------------------------------------------------------------
set(ANFCONV_TARGETS_FILENAME "anfconvTargets.cmake")
set(ANFCONV_CONFIG_FILENAME "anfconvConfig.cmake")

# Export targets
export(TARGETS libanfconv anfconv FILE "${PROJECT_BINARY_DIR}/${ANFCONV_TARGETS_FILENAME}")

# Create anfconvConfig file
set(EXPORT_TYPE "Build-tree")
set(CONF_INCLUDE_DIRS "${CMAKE_BINARY_DIR}/include")
configure_file(anfconvConfig.cmake.in
    "${CMAKE_BINARY_DIR}/${ANFCONV_CONFIG_FILENAME}" @ONLY
)

# Export this package to the CMake user package registry
# Now the user can just use find_package(anfconv) on their system
export(PACKAGE anfconv)

set(DEF_INSTALL_CMAKE_DIR lib/cmake/anfconv)
set(ANFCONV_INSTALL_CMAKE_DIR ${DEF_INSTALL_CMAKE_DIR} CACHE PATH
    "Installation directory for anfconv CMake files")

# Create anfconvConfig file
set(EXPORT_TYPE "installed")
set(CONF_INCLUDE_DIRS "${CMAKE_INSTALL_PREFIX}/include")
configure_file(anfconvConfig.cmake.in
   "${CMAKE_BINARY_DIR}/${CMAKE_FILES_DIRECTORY}/${ANFCONV_CONFIG_FILENAME}" @ONLY
)

install(FILES
    "${CMAKE_BINARY_DIR}/${CMAKE_FILES_DIRECTORY}/${ANFCONV_CONFIG_FILENAME}"
    DESTINATION "${ANFCONV_INSTALL_CMAKE_DIR}"
)

# Install the export set for use with the install-tree
install(EXPORT ${ANFCONV_EXPORT_NAME} DESTINATION
    "${ANFCONV_INSTALL_CMAKE_DIR}"
)

set(CPACK_PACKAGE_NAME "anfconv")
set(CPACK_PACKAGE_VENDOR "")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "An ANF minimizerr")
set(CPACK_PACKAGE_VERSION "1.0.0")
set(CPACK_PACKAGE_VERSION_MAJOR "1")
set(CPACK_PACKAGE_VERSION_MINOR "0")
set(CPACK_PACKAGE_VERSION_PATCH "0")
set(CPACK_SOURCE_GENERATOR "TBZ2")
set(CPACK_SOURCE_PACKAGE_FILE_NAME
  "${CMAKE_PROJECT_NAME}-${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")
set(CPACK_SOURCE_IGNORE_FILES
  "/build/"
  "/.git/"
  "~$"
  "${CPACK_SOURCE_IGNORE_FILES}"
  "/m4/"
  "/.kdev4/"
  ".gitignore"
  ".git$"
  "tar.gz$"
  "tar.bz2$"
  ".kdev4$"
  ".directory$"
)
include(CPack)

add_custom_target(dist COMMAND ${CMAKE_MAKE_PROGRAM} package_source)


