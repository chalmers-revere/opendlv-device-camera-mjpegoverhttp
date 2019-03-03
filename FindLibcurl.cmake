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
# Find libcurl.
FIND_PATH(CURL_INCLUDE_DIR
          NAMES curl/curl.h
          PATHS /usr/local/include/
                /usr/include/)
MARK_AS_ADVANCED(CURL_INCLUDE_DIR)
FIND_LIBRARY(CURL_LIBRARY
             NAMES curl
             PATHS ${LIBYUVDIR}/lib/
                    /usr/lib/arm-linux-gnueabihf/
                    /usr/lib/arm-linux-gnueabi/
                    /usr/lib/x86_64-linux-gnu/
                    /usr/local/lib64/
                    /usr/lib64/
                    /usr/lib/)
MARK_AS_ADVANCED(CURL_LIBRARY)

###########################################################################
IF (CURL_INCLUDE_DIR
    AND CURL_LIBRARY)
    SET(CURL_FOUND 1)
    SET(CURL_LIBRARIES ${CURL_LIBRARY})
    SET(CURL_INCLUDE_DIRS ${CURL_INCLUDE_DIR})
ENDIF()

MARK_AS_ADVANCED(CURL_LIBRARIES)
MARK_AS_ADVANCED(CURL_INCLUDE_DIRS)

IF (CURL_FOUND)
    MESSAGE(STATUS "Found libcurl: ${CURL_INCLUDE_DIRS}, ${CURL_LIBRARIES}")
ELSE ()
    MESSAGE(STATUS "Could not find libcurl")
ENDIF()
