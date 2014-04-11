
ADD_LIBRARY(path-util STATIC
	path-util.cpp
	path-util.h
)

IF(WIN32)
	TARGET_LINK_LIBRARIES(path-util shlwapi userenv)
ENDIF()
