# Authors: Frank Stappers 
# Copyright: see the accompanying file COPYING or copy at
# https://svn.win.tue.nl/trac/MCRL2/browser/trunk/COPYING
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

# Raise warning if MCRL2_COPYRIGHT_TEXT is not set.
if( NOT(MCRL2_COPYRIGHT_TEXT) )
  message(WARNING "MCRL2_COPYRIGHT_TEXT is not set.")
endif( NOT(MCRL2_COPYRIGHT_TEXT) )

# Raise warning if MCRL2_VERSION is not set.
if( NOT(MCRL2_VERSION) )
  message(WARNING "MCRL2_VERSION is not set." )
endif( NOT(MCRL2_VERSION) )

# Set default bundle information
SET(MACOSX_BUNDLE_NAME ${PROJECT_NAME})
SET(MACOSX_BUNDLE_ICON_FILE ${PROJECT_NAME}.icns)
SET(MACOSX_BUNDLE_COPYRIGHT ${MCRL2_COPYRIGHT_TEXT})
SET(MACOSX_BUNDLE_BUNDLE_VERSION ${MCRL2_VERSION}) 
