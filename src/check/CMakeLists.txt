cmake_minimum_required (VERSION 3.18)

include (../utils/bdj4macros.cmake)

add_executable (check_all
  check_all.c
  # libcommon
  check_libcommon.c
  check_bdjmsg.c
  check_bdjstring.c
  check_bdjvars.c
  check_colorutils.c
  check_dirop.c
  check_filedata.c
  check_filemanip.c
  check_fileop.c
  check_lock.c
  check_osprocess.c
  check_osrandom.c
  check_ossignal.c
  check_pathbld.c
  check_pathutil.c
  check_procutil.c
  check_queue.c
  check_rafile.c
  check_sock.c
  check_tmutil.c
  # libbasic
  check_libbasic.c
  check_datafile.c
  check_dirlist.c
  check_ilist.c
  check_istring.c
  check_nlist.c
  check_progstate.c
  check_slist.c
  # libbdj4
  check_libbdj4.c
  check_autosel.c
  check_bdjvarsdf.c
  check_bdjvarsdfload.c
  check_dance.c
  check_dancesel.c
  check_dispsel.c
  check_dnctypes.c
  check_genre.c
  check_level.c
  check_musicdb.c
  check_orgopt.c
  check_orgutil.c
  check_rating.c
  check_samesong.c
  check_sequence.c
  check_song.c
  check_songfav.c
  check_songfilter.c
  check_songlist.c
  check_songutil.c
  check_sortopt.c
  check_status.c
  check_tagdef.c
  check_templateutil.c
  check_validate.c
)
target_link_libraries (check_all
  libbdj4 libbdj4basic libbdj4common
  ${ICU_LDFLAGS}
  ${CURL_LDFLAGS} ${OPENSSL_LDFLAGS}
  ${CHECK_LDFLAGS}
  pthread
)
target_compile_options (check_all PRIVATE -pthread)
addIntlLibrary (check_all)
addWinSockLibrary (check_all)

add_executable (chkprocess chkprocess.c)

add_executable (testsuite
  testsuite.c
)
target_link_libraries (testsuite
  libbdj4 libbdj4basic libbdj4common
  ${ICU_LDFLAGS}
  ${CURL_LDFLAGS} ${OPENSSL_LDFLAGS}
  ${CHECK_LDFLAGS}
)

if (APPLE)
  macUpdateRPath (check_all)
  macUpdateRPath (testsuite)
endif()

install (TARGETS
  check_all chkprocess testsuite
  DESTINATION ${PROJECT_SOURCE_DIR}/../bin
)

