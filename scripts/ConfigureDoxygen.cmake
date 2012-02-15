# Authors: Frank Stappers 
# Copyright: see the accompanying file COPYING or copy at
# https://svn.win.tue.nl/trac/MCRL2/browser/trunk/COPYING
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

# Random documentation requires Doxygen 

find_package(Doxygen)
if(DOXYGEN_FOUND)

  # Print version Doxygen
  execute_process(COMMAND ${DOXYGEN_EXECUTABLE} --version
      OUTPUT_VARIABLE DOXYGEN_VERSION
      OUTPUT_STRIP_TRAILING_WHITESPACE
  )		
  message( STATUS "Doxygen ${DOXYGEN_VERSION}" )

endif(DOXYGEN_FOUND)
