TARGET      = rlm_mongo
SRCS        = bson.c gridfs.c md5.c mongo.c numbers.c rlm_mongo.c
RLM_CFLAGS  = --std=c99

include ../rules.mak
