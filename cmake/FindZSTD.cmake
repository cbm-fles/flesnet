find_path(ZSTD_INCLUDE_DIR
  NAMES zstd.h
)

find_library(ZSTD_LIBRARY
  NAMES zstd
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ZSTD REQUIRED_VARS ZSTD_LIBRARY ZSTD_INCLUDE_DIR)

