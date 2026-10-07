# Copies the default mod config ini file to the deploy folder if none exists.

# usage: cmake -DSRC=<default ini> -DDST=<deployed ini> -P deploy_ini.cmake

if(NOT EXISTS "${DST}")
	file(COPY_FILE "${SRC}" "${DST}")
	message(STATUS "Deployed default INI to ${DST}")
endif()
