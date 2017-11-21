TOP=..
include $(TOP)/configure/CONFIG
#=======================================

# Build asynGenicam as a library for an IOC:
LIBRARY_IOC += asynGenicam

# Library Source files
asynGenicam_SRCS += asynGenicam.cpp
asynGenicam_SRCS += GenCpPacket.cpp
#asynGenicam_SRCS += GenCpTool.cpp

# Link with the asyn and base libraries
asynGenicam_LIBS += asyn
asynGenicam_LIBS += $(EPICS_BASE_IOC_LIBS)

# Install .dbd and .db files
DBD += asynGenicam.dbd

#=======================================
include $(TOP)/configure/RULES
