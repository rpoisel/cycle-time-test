local_dir              := cycle-time
local_lib              := 
local_program          := $(addprefix $(DIR_BIN)/,cycle-time)
local_inc              := include
local_src              := $(addprefix $(DIR_SRC)/$(local_dir)/src/,\
    main.c)
local_objs             := $(addprefix $(DIR_OBJ)/,$(subst .c,.o,$(local_src)))
local_dep              := 
local_lib_dep          := -lrt -lm

libraries              += $(local_lib)
sources                += $(local_src)
includes               += $(local_inc)
objects                += $(local_objs)
programs               += $(local_program)

$(local_program): $(local_objs) $(local_dep)
	$(CC) $(LDFLAGS) -o $@ $^ $(local_lib_dep)
