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

# Partially based on back-ported upstream code.
function(version_from_git_tags VERSION_MAJOR VERSION_MINOR VERSION_PATCH VERSION_REVISION)

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
    COMMAND ${GIT_EXECUTABLE} rev-parse --short=8 HEAD
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    OUTPUT_VARIABLE git_sha_short
    ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  message(VERBOSE "Git SHA short: " ${git_sha_short})

  execute_process(
    COMMAND ${GIT_EXECUTABLE} rev-list --tags --count
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    OUTPUT_VARIABLE git_tag_count
    ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  message(VERBOSE "Git tag count: " ${git_tag_count})

  if(git_tag_count EQUAL 0)
    message(FATAL_ERROR "No tags found in Git repository")
  endif()

  # This differs from upstream in that we use `git describe master` to count changes
  # since last tag only on master. This is because we use squash merge instead of rebasing;
  # using describe on the branch would give us a superficially higher revision number.
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

  string(FIND ${git_describe} "v" tag_version_found)
  if(tag_version_found EQUAL -1)
    message(FATAL_ERROR "No version tags found in the Git repository")
  endif()

  string(REGEX MATCH "[0-9]+" match_major ${git_describe})
  string(REGEX MATCH "\\.[0-9]+" minor_match ${git_describe})
  string(REPLACE "." "" minor_match "${minor_match}")
  string(REGEX MATCH "[0-9]+-" patch_match ${git_describe})
  string(REPLACE "-" "" patch_match "${patch_match}")
  string(REGEX MATCH "-[0-9]+-" rev_match ${git_describe})
  string(REPLACE "-" "" rev_match "${rev_match}")

  set(${VERSION_MAJOR} ${match_major} PARENT_SCOPE)
  set(${VERSION_MINOR} ${minor_match} PARENT_SCOPE)
  set(${VERSION_PATCH} ${patch_match} PARENT_SCOPE)
  set(${VERSION_REVISION} ${rev_match} PARENT_SCOPE)

endfunction()
