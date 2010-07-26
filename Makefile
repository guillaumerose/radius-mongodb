#######################################################################
#
# TARGET should be set by autoconf only.  Don't touch it.
#
# The SRCS definition should list ALL source files.
#
# The HEADERS definition should list ALL header files
#
# RLM_CFLAGS defines addition C compiler flags.  You usually don't
# want to modify this, though.  Get it from autoconf.
#
# The RLM_LIBS definition should list ALL required libraries.
# These libraries really should be pulled from the 'config.mak'
# definitions, if at all possible.  These definitions are also
# echoed into another file in ../lib, where they're picked up by
# ../main/Makefile for building the version of the server with
# statically linked modules.  Get it from autoconf.
#
# RLM_INSTALL is the names of additional rules you need to install
# some particular portion of the module.  Usually, leave it blank.
#
#######################################################################
TARGET      = rlm_mongo
SRCS        = bson.c gridfs.c md5.c mongo.c numbers.c rlm_mongo.c
HEADERS     = bson.h config.h gridfs.h md5.h mongo_except.h mongo.h platform_hacks.h
RLM_CFLAGS  =  -I/usr/include --std=c99
RLM_LIBS    =  -lc
RLM_INSTALL = install-mongo

## this uses the RLM_CFLAGS and RLM_LIBS and SRCS defs to make TARGET.
include ../rules.mak

$(LT_OBJS): $(HEADERS)

## the rule that RLM_INSTALL tells the parent rules.mak to use.
install-mongo:
	touch .
