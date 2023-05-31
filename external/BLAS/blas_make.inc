####################################################################
#  BLAS make include file.                                         #
#  March 2007                                                      #
####################################################################
#
SHELL = /bin/sh
#
#  The machine (platform) identifier to append to the library names
#
PLAT = _ARM
#
#  Modify the FORTRAN and OPTS definitions to refer to the
#  compiler and desired compiler options for your machine.  NOOPT
#  refers to the compiler options desired when NO OPTIMIZATION is
#  selected.  Define LOADER and LOADOPTS to refer to the loader and
#  desired load options for your machine.
#
FFLAGS = -O2 -frecursive -march=armv7-a -mthumb -mfloat-abi=hard -mtune=cortex-a7 -mfpu=neon
FFLAGS_DRV = $(FFLAGS)
FFLAGS_NOOPT = -O0 -frecursive -march=armv7-a -mthumb -mfloat-abi=hard -mtune=cortex-a7 -mfpu=neon
#  Define LDFLAGS to the desired linker options for your machine.
#
LDFLAGS =

#  The archiver and the flag(s) to use when building an archive
#  (library).  If your system has no ranlib, set RANLIB = echo.
#
ARFLAGS = cr
#
#  The location and name of the Reference BLAS library.
#
BLASLIB      = ../CBLAS/lib/blas$(PLAT).a
