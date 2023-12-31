vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO ithewei/libhv
    REF v${VERSION} #v1.3.0
    SHA512 7d8f552947eb464a8dd644515b9543a7961631e427033de4e1f51f1e9d2b9ca1801553a478b0a20935b50dbc274632e38a6f0438732efed9fcbf0738738ddac5
    HEAD_REF master
)

string(COMPARE EQUAL "${VCPKG_LIBRARY_LINKAGE}" "static" BUILD_STATIC)
string(COMPARE EQUAL "${VCPKG_LIBRARY_LINKAGE}" "dynamic" BUILD_SHARED)

vcpkg_check_features(OUT_FEATURE_OPTIONS FEATURE_OPTIONS
    FEATURES
        ssl WITH_OPENSSL
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    DISABLE_PARALLEL_CONFIGURE
    OPTIONS
        -DBUILD_EXAMPLES=OFF
        -DBUILD_UNITTEST=OFF
        -DBUILD_STATIC=${BUILD_STATIC}
        -DBUILD_SHARED=${BUILD_SHARED}
        ${FEATURE_OPTIONS}
)

vcpkg_cmake_install()

vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/libhv)
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

file(INSTALL "${SOURCE_PATH}/LICENSE" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright)
