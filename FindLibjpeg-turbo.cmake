# Copyright (C) 2019  Christian Berger
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

###########################################################################
# Find libjpeg-turbo.
FIND_PATH(JPEG_TURBO_INCLUDE_DIR
          NAMES turbojpeg.h
          PATHS /usr/local/include/
                /usr/include/)
MARK_AS_ADVANCED(JPEG_TURBO_INCLUDE_DIR)
FIND_LIBRARY(JPEG_TURBO_LIBRARY
             NAMES turbojpeg
             PATHS ${LIBYUVDIR}/lib/
                    /usr/lib/arm-linux-gnueabihf/
                    /usr/lib/arm-linux-gnueabi/
                    /usr/lib/x86_64-linux-gnu/
                    /usr/local/lib64/
                    /usr/lib64/
                    /usr/lib/)
MARK_AS_ADVANCED(JPEG_TURBO_LIBRARY)

###########################################################################
IF (JPEG_TURBO_INCLUDE_DIR
    AND JPEG_TURBO_LIBRARY)
    SET(JPEG_TURBO_FOUND 1)
    SET(JPEG_TURBO_LIBRARIES ${JPEG_TURBO_LIBRARY})
    SET(JPEG_TURBO_INCLUDE_DIRS ${JPEG_TURBO_INCLUDE_DIR})
ENDIF()

MARK_AS_ADVANCED(JPEG_TURBO_LIBRARIES)
MARK_AS_ADVANCED(JPEG_TURBO_INCLUDE_DIRS)

IF (JPEG_TURBO_FOUND)
    MESSAGE(STATUS "Found libjpeg-turbo: ${JPEG_TURBO_INCLUDE_DIRS}, ${JPEG_TURBO_LIBRARIES}")
ELSE ()
    MESSAGE(STATUS "Could not find libjpeg-turbo")
ENDIF()
