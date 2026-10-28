# Overrideable project defaults: the recorder must see the effective Make
# command-line values, not these defaults or unrelated environment contents.
CC = project-default-cc
CFLAGS = -O0
LDFLAGS = project-default-linker
AR = project-default-ar
RANLIB = project-default-ranlib
