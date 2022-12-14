cmake_minimum_required (VERSION 3.18)

include (../utils/bdj4macros.cmake)

add_library (libbdj4common SHARED
  bdjmsg.c
  bdjstring.c
  bdjvars.c
  colorutils.c
  conn.c
  dirop.c
  filedata.c
  filemanip.c
  fileop.c
  fileutil.c
  log.c
  osdir.c
  oslocale.c
  osnetutils.c
  osprocess.c
  osrandom.c
  ossignal.c
  osutils.c
  oslinuxutils.c
  osmacutils.m
  oswinprocess.c
  oswinutils.c
  pathbld.c
  pathutil.c
  queue.c
  sock.c
  sockh.c
  strlcat.c
  strlcpy.c
  sysvars.c
  tmutil.c
)
target_include_directories (libbdj4common
  PUBLIC "${GLIB_INCLUDE_DIRS}"
)
target_link_libraries (libbdj4common
  ${GLIB_LDFLAGS}
)
addWinSockLibrary (libbdj4common)
# for RtlGetVersion
addWinNtdllLibrary (libbdj4common)

install (TARGETS
  libbdj4common
  DESTINATION ${PROJECT_SOURCE_DIR}/../bin
)
