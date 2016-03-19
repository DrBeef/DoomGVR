#############################################
# Android Makefile for Doom
#############################################
CC = agcc
MACROS = -DNORMALUNIX -DLINUX -DHAVE_NET -DUSE_SDL_NET -DHAVE_CONFIG_H
FLAGS = -O2 -ffast-math -Wall -nostdinc
LINKER = ald
LFLAGS = 
LIBS =

MAIN_OBJS = \
	am_map.o m_cheat.o p_lights.o p_user.o sounds.o \
	hu_lib.o md5.o p_map.o r_bsp.o s_sound.o \
	d_deh.o hu_stuff.o m_menu.o p_maputl.o r_data.o st_lib.o \
	d_items.o i_main.o m_misc.o p_mobj.o r_demo.o st_stuff.o \
	d_main.o info.o p_plats.o r_draw.o tables.o \
	doomdef.o i_sound.o m_random.o p_pspr.o r_filter.o version.o \
	doomstat.o i_system.o p_ceilng.o p_saveg.o r_fps.o v_video.o \
	p_checksum.o p_setup.o r_main.o wi_stuff.o \
	dstrings.o p_doors.o p_sight.o r_patch.o w_memcache.o \
	f_finale.o jni_doom.o p_enemy.o p_spec.o r_plane.o w_mmap.o \
	f_wipe.o lprintf.o p_floor.o p_switch.o r_segs.o w_wad.o \
	g_game.o m_argv.o p_genlin.o p_telept.o r_sky.o z_bmalloc.o \
	m_bbox.o p_inter.o p_tick.o r_things.o z_zone.o \
	d_client.o i_video.o i_network.o d_server.o

LIB = ../bin/libdoom_jni.so
DYN = ../bin/doom-cli

all: lib

jni:
	@echo "Creating JNI C headers..."
	javah -jni -classpath ../../bin -d include doom.jni.Natives

# DSO
lib: $(MAIN_OBJS)
	@echo
	$(LINKER) -shared $(LFLAGS) $(LIB_PATHS) $(LIBS) -o $(LIB) \
		$(MAIN_OBJS) $(LIBRARIES)
	@echo
	@echo Done. Out file is $(LIB)
	@echo
.c.o:
	@echo
	$(CC) -fpic -c $(FLAGS) $(MACROS) $(INCLUDES) $<

# Deploy lib
deploy-lib: lib
	adb push $(LIB) /data/data/org.doom/files
clean:
	rm -f *.o $(EXE)

