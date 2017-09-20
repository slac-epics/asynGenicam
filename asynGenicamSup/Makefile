TOP=..
include $(TOP)/configure/CONFIG
#=======================================

# Build asynGenicam as a library for an IOC:
LIBRARY_IOC += asynInterposeGenicam

# Library Source files
asynInterposeGenicam_SRCS += asynInterposeGenicam.c

# Link with the asyn and base libraries
asynInterposeGenicam_LIBS += asyn
asynInterposeGenicam_LIBS += $(EPICS_BASE_IOC_LIBS)

# Install .dbd and .db files
DBD += asynInterposeGenicam.dbd

#=======================================
include $(TOP)/configure/RULES
