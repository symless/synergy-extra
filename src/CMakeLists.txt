# Synergy -- mouse and keyboard sharing utility
# Copyright (C) 2024 Symless Ltd.
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

add_definitions(-DDESKFLOW_WEBSITE_URL="${DESKFLOW_WEBSITE_URL}")

include_directories(./lib)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/lib)

# Include parent repository source tree (e.g. for utils, etc).
include_directories(${CMAKE_SOURCE_DIR}/src/lib)

add_subdirectory(lib)
#add_subdirectory(test)
