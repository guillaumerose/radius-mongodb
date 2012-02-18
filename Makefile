TARGET      = rlm_mongo
SRCS        = bson.c encoding.c md5.c mongo.c net.c numbers.c rlm_mongo.c
RLM_CFLAGS  = --std=c99

include ../rules.mak
