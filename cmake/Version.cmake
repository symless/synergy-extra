# Synergy -- mouse and keyboard sharing utility
# Copyright (C) 2025 Symless Ltd.
#
# This package is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# found in the file LICENSE that should have accompanied this file.
#
# This package is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

#
# Partially based on back-ported upstream code but differs in two ways:
#
# 1. We use `git describe master` to count changes since last tag only on master.
#    This is because we use squash merge instead of rebasing; using describe on the branch would
#    give us a superficially higher revision number.
#
# 2. We use a different versioning strategy; we work on the version that is being released rather
#    than a revision of the last version, so we use the `VERSION` file as the source of truth
#    for the version number rather than the last tag.
#
function(version_from_git_tags VERSION VERSION_MAJOR VERSION_MINOR VERSION_PATCH VERSION_REVISION)

  set(version_file "${CMAKE_CURRENT_SOURCE_DIR}/VERSION")
  file(READ "${version_file}" version)
  string(STRIP "${version}" version)
  if ("${version}" STREQUAL "")
    message(FATAL_ERROR "No version found in: ${version_file}")
  endif()
  message(VERBOSE "Version from file: ${version}")

  string(REGEX MATCH "([0-9]+)\\.([0-9]+)\\.([0-9]+)" _ "${version}")
  set(match_major "${CMAKE_MATCH_1}")
  set(minor_match "${CMAKE_MATCH_2}")
  set(patch_match "${CMAKE_MATCH_3}")

  set(git_path "${CMAKE_CURRENT_SOURCE_DIR}/.git")
  if(NOT EXISTS ${git_path})
    message(FATAL_ERROR "Not a Git repository: ${git_path}")
  endif()
  
  find_package(Git)
  if(NOT GIT_FOUND)
    message(FATAL_ERROR "Git not found")
  endif()
  message(VERBOSE "Git repo: " ${CMAKE_CURRENT_SOURCE_DIR})

  execute_process(
    COMMAND ${GIT_EXECUTABLE} describe origin/master --long --match "v[0-9]*"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    OUTPUT_VARIABLE git_describe
    ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  if (NOT git_describe)
    message(FATAL_ERROR "No version tags found in the Git repository")
  endif()
  message(VERBOSE "Git describe: " ${git_describe})

  string(REGEX REPLACE ".*-([0-9]+)-g.*" "\\1" rev_count ${git_describe})
  if ("${rev_count}" STREQUAL "")
    message(FATAL_ERROR "No revision count found in Git describe output")
  endif()

  # Env var is normally controlled by CI, should be set when build triggered by a tag.
  # Strangely, even with GitHub actions `actions/checkout@v4` option `fetch-depth: 0` 
  # (gets all the tags), the `git describe` command only sometimes works as expected
  # and returns the tag name (bug only reproducible on CI).
  option(SYNERGY_RELEASE "Release version" OFF)
  if ("$ENV{SYNERGY_RELEASE}" STREQUAL "true")
    message(VERBOSE "Release env var is set")
    set(SYNERGY_RELEASE ON)
  endif()

  if ($SYNERGY_RELEASE)
    message(VERBOSE "Version is release")
  else()
    message(VERBOSE "Changes since last tag: " ${rev_count})

    # The `snapshot` stage is set here regardless of what the stage is in the version file,
    # which serves 2 purposes:
    # 1. It lets QA know that this is a snapshot and not a release.
    # 2. When uploading to the packages repo, files with `-snapshot` in the name are
    #    are uploaded to a separate directory, making packages easier for QA to find.
    set(version "${match_major}.${minor_match}.${patch_match}-snapshot+r${rev_count}")
  endif()

  set(${VERSION} "${version}" PARENT_SCOPE)
  set(${VERSION_MAJOR} ${match_major} PARENT_SCOPE)
  set(${VERSION_MINOR} ${minor_match} PARENT_SCOPE)
  set(${VERSION_PATCH} ${patch_match} PARENT_SCOPE)
  set(${VERSION_REVISION} ${rev_count} PARENT_SCOPE)
  
endfunction()
