FILE(TO_CMAKE_PATH "$ENV{GSL_DIR}" TRY1_DIR)
FILE(TO_CMAKE_PATH "${GSL_DIR}" TRY2_DIR)
FILE(GLOB GSL_DIR ${TRY1_DIR} ${TRY2_DIR})

FIND_PATH(GSL_INCLUDE_DIR gsl/gsl_blas.h
                            PATHS ${GSL_DIR}/include ${GSL_DIR}
                            ENV INCLUDE DOC "Directory containing gsl_blas.h include file")

FIND_LIBRARY(GSL_ERR_LIBRARY NAMES gslerr
                                     PATHS ${GSL_DIR}/lib
                                     ENV LIB
                                     DOC "gslerr library to link with"
                                     NO_SYSTEM_ENVIRONMENT_PATH)
                            
FIND_LIBRARY(GSL_INTERPOLATION_LIBRARY NAMES gslinterpolation
                                     PATHS ${GSL_DIR}/lib
                                     ENV LIB
                                     DOC "gslinterpolation library to link with"
                                     NO_SYSTEM_ENVIRONMENT_PATH)
                                     
FIND_LIBRARY(GSL_SYS_LIBRARY NAMES gslsys
                                     PATHS ${GSL_DIR}/lib
                                     ENV LIB
                                     DOC "gslsys library to link with"
                                     NO_SYSTEM_ENVIRONMENT_PATH)
                            
IF (GSL_INCLUDE_DIR AND GSL_INTERPOLATION_LIBRARY AND GSL_ERROR_LIBRARY AND GSL_SYS_LIBRARY)
  SET(GSL_FOUND TRUE)
ENDIF (GSL_INCLUDE_DIR AND GSL_INTERPOLATION_LIBRARY AND GSL_ERROR_LIBRARY AND GSL_SYS_LIBRARY)
