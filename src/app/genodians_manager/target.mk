TARGET := genodians_manager
SRC_CC := component.cc
LIBS   := base

SCULPT_MANAGER__DIR := $(call select_from_repositories,src/app/sculpt_manager)
INC_DIR += $(PRG_DIR) $(SCULPT_MANAGER__DIR)

# musl-libc contrib sources
MUSL_TM := $(call select_from_repositories,src/lib/musl_tm)
SRC_C   := secs_to_tm.c tm_to_secs.c
INC_DIR += $(MUSL_TM)

vpath %.c $(MUSL_TM)
