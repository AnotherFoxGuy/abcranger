include(vcpkg_common_functions)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO BlueBrain/HighFive
    REF 30860496956873408847279eb60fc552bc243729
    SHA512 17a007ab51095815c6d395db34d6506eb185197649122231210533fb34fec6ddf20ff827c8ea14b97f4b6d9ece1eb8f02807eb0e5bdad8fffa25884808f6f65e
    HEAD_REF master
)

if(${VCPKG_LIBRARY_LINKAGE} MATCHES "static")
    set(HDF5_USE_STATIC_LIBRARIES ON)
endif()

vcpkg_configure_cmake(
    SOURCE_PATH ${SOURCE_PATH}
    PREFER_NINJA
    OPTIONS
        -DHIGHFIVE_UNIT_TESTS=OFF
        -DHIGHFIVE_EXAMPLES=OFF
        -DUSE_BOOST=OFF
        -DHIGH_FIVE_DOCUMENTATION=OFF
        -DUSE_EIGEN=ON
        -DHDF5_USE_STATIC_LIBRARIES=${HDF5_USE_STATIC_LIBRARIES}
)

vcpkg_install_cmake()

vcpkg_fixup_cmake_targets(CONFIG_PATH share/HighFive/CMake)

file(REMOVE_RECURSE ${CURRENT_PACKAGES_DIR}/debug)
if(NOT VCPKG_CMAKE_SYSTEM_NAME STREQUAL "WindowsStore")
  file(REMOVE_RECURSE ${CURRENT_PACKAGES_DIR}/share/HighFive)
endif()

# Handle copyright
file(INSTALL ${SOURCE_PATH}/LICENSE DESTINATION ${CURRENT_PACKAGES_DIR}/share/highfive-head RENAME copyright)
