# Designed to be included from an IDF app's CMakeLists.txt file
#
cmake_minimum_required(VERSION 3.5)

# Set IDF_PATH, as nothing else will work without this
set(IDF_PATH "$ENV{IDF_PATH}")
if(NOT IDF_PATH)
    # Documentation says you should set IDF_PATH in your environment, but we
    # can infer it relative to tools/cmake directory if it's not set.
    get_filename_component(IDF_PATH "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
endif()
file(TO_CMAKE_PATH "${IDF_PATH}" IDF_PATH)
set(ENV{IDF_PATH} ${IDF_PATH})


#
# Load cmake modules
#
set(CMAKE_MODULE_PATH
    "${IDF_PATH}/tools/cmake"
    "${IDF_PATH}/tools/cmake/third_party"
    ${CMAKE_MODULE_PATH})
include(GetGitRevisionDescription)
include(utilities)
include(components)
include(targets)
include(kconfig)
include(git_submodules)
include(idf_functions)
include(ldgen)

set_default(PYTHON "python")

if(NOT PYTHON_DEPS_CHECKED AND NOT BOOTLOADER_BUILD)
    message(STATUS "Checking Python dependencies...")
    execute_process(COMMAND "${PYTHON}" "${IDF_PATH}/tools/check_python_dependencies.py"
        RESULT_VARIABLE result)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Some Python dependencies must be installed. Check above message for details.")
    endif()
endif()

# project
#
# This macro wraps the cmake 'project' command to add
# all of the IDF-specific functionality required
#
# Implementation Note: This macro wraps 'project' on purpose, because cmake has
# some backwards-compatible magic where if you don't call "project" in the
# top-level CMakeLists file, it will call it implicitly. However, the implicit
# project will not have CMAKE_TOOLCHAIN_FILE set and therefore tries to
# create a native build project.
#
# Therefore, to keep all the IDF "build magic", the cleanest way is to keep the
# top-level "project" call but customize it to do what we want in the IDF build.
#
macro(project name)
    # Determine the build target
    idf_set_target()

    # Set global variables used by rest of the build
    idf_set_global_variables()

    # Sort the components list, as it may be found via filesystem
    # traversal and therefore in a non-deterministic order
    list(SORT COMPONENTS)

    execute_process(COMMAND "${CMAKE_COMMAND}"
        -D "COMPONENTS=${COMPONENTS}"
        -D "COMPONENT_REQUIRES_COMMON=${COMPONENT_REQUIRES_COMMON}"
        -D "EXCLUDE_COMPONENTS=${EXCLUDE_COMPONENTS}"
        -D "TEST_COMPONENTS=${TEST_COMPONENTS}"
        -D "TEST_EXCLUDE_COMPONENTS=${TEST_EXCLUDE_COMPONENTS}"
        -D "TESTS_ALL=${TESTS_ALL}"
        -D "DEPENDENCIES_FILE=${CMAKE_BINARY_DIR}/component_depends.cmake"
        -D "COMPONENT_DIRS=${COMPONENT_DIRS}"
        -D "BOOTLOADER_BUILD=${BOOTLOADER_BUILD}"
        -D "IDF_PATH=${IDF_PATH}"
        -D "IDF_TARGET=${IDF_TARGET}"
        -D "DEBUG=${DEBUG}"
        -P "${IDF_PATH}/tools/cmake/scripts/expand_requirements.cmake"
        WORKING_DIRECTORY "${PROJECT_PATH}")
    include("${CMAKE_BINARY_DIR}/component_depends.cmake")

    # We now have the following component-related variables:
    # COMPONENTS is the list of initial components set by the user (or empty to include all components in the build).
    # BUILD_COMPONENTS is the list of components to include in the build.
    # BUILD_COMPONENT_PATHS is the paths to all of these components.

    # Print list of components
    string(REPLACE ";" " " BUILD_COMPONENTS_SPACES "${BUILD_COMPONENTS}")
    message(STATUS "Component names: ${BUILD_COMPONENTS_SPACES}")
    unset(BUILD_COMPONENTS_SPACES)
    message(STATUS "Component paths: ${BUILD_COMPONENT_PATHS}")

    # Print list of test components
    if(TESTS_ALL EQUAL 1 OR TEST_COMPONENTS)
        string(REPLACE ";" " " BUILD_TEST_COMPONENTS_SPACES "${BUILD_TEST_COMPONENTS}")
        message(STATUS "Test component names: ${BUILD_TEST_COMPONENTS_SPACES}")
        unset(BUILD_TEST_COMPONENTS_SPACES)
        message(STATUS "Test component paths: ${BUILD_TEST_COMPONENT_PATHS}")
    endif()

    kconfig_set_variables()

    kconfig_process_config()

    # Include sdkconfig.cmake so rest of the build knows the configuration
    include(${SDKCONFIG_CMAKE})

    # Check that the targets set in cache, sdkconfig, and in environment all match
    idf_check_config_target()

    # Now the configuration is loaded, set the toolchain appropriately
    idf_set_toolchain()

    # Declare the actual cmake-level project
    _project(${name} ASM C CXX)

    # generate compile_commands.json (needs to come after project)
    set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

    # Verify the environment is configured correctly
    idf_verify_environment()

    # Add some idf-wide definitions
    idf_set_global_compiler_options()

    # Check git revision (may trigger reruns of cmake)
    ##  sets IDF_VER to IDF git revision
    idf_get_git_revision()
    ## if project uses git, retrieve revision
    git_describe(PROJECT_VER "${CMAKE_CURRENT_SOURCE_DIR}")

    #
    # Add the app executable to the build (has name of PROJECT.elf)
    #
    idf_add_executable()

    #
    # Setup variables for linker script generation
    #
    ldgen_set_variables()

    # Include any top-level project_include.cmake files from components
    foreach(component ${BUILD_COMPONENT_PATHS})
        set(COMPONENT_PATH "${component}")
        include_if_exists("${component}/project_include.cmake")
        unset(COMPONENT_PATH)
    endforeach()

    #
    # Add each component to the build as a library
    #
    foreach(COMPONENT_PATH ${BUILD_COMPONENT_PATHS})
        list(FIND BUILD_TEST_COMPONENT_PATHS ${COMPONENT_PATH} idx)

        if(NOT idx EQUAL -1)
            list(GET BUILD_TEST_COMPONENTS ${idx} test_component)
            set(COMPONENT_NAME ${test_component})
        else()
            get_filename_component(COMPONENT_NAME ${COMPONENT_PATH} NAME)
        endif()

        add_subdirectory(${COMPONENT_PATH} ${COMPONENT_NAME})
    endforeach()
    unset(COMPONENT_NAME)
    unset(COMPONENT_PATH)

    # At this point the fragment files have been collected, generate
    # the commands needed to generate the output linker scripts
    ldgen_add_dependencies(${PROJECT_NAME}.elf)

    # Write project description JSON file
    make_json_list("${BUILD_COMPONENTS}" build_components_json)
    make_json_list("${BUILD_COMPONENT_PATHS}" build_component_paths_json)
    configure_file("${IDF_PATH}/tools/cmake/project_description.json.in"
        "${CMAKE_BINARY_DIR}/project_description.json")
    unset(build_components_json)
    unset(build_component_paths_json)

    #
    # Finish component registration (add cross-dependencies, make
    # executable dependent on all components)
    #
    components_finish_registration()

endmacro()
