LOCAL_PATH 	:= $(call my-dir)

# clear vars
include $(CLEAR_VARS)

# module name
LOCAL_MODULE 	:= doomgvr

#includes
LOCAL_C_INCLUDES:= $(LOCAL_PATH) $(LOCAL_PATH)/.. $(LOCAL_PATH)/include

#flags
PRBOOM_FLAGS 	:= -DNORMALUNIX -DLINUX -DHAVE_CONFIG_H -DHAVE_NET
LOCAL_CFLAGS 	:= -std=c99 $(PRBOOM_FLAGS) -O3 -ffast-math -fexpensive-optimizations

# sources
#LOCAL_SRC_FILES := $(notdir $(wildcard $(LOCAL_PATH)/*.c))
LOCAL_SRC_FILES := DoomGVR_NativeActivity.c \
	am_map.c m_cheat.c p_lights.c p_user.c sounds.c \
	hu_lib.c md5.c p_map.c r_bsp.c s_sound.c \
	d_deh.c hu_stuff.c m_menu.c p_maputl.c r_data.c st_lib.c \
	d_items.c m_misc.c p_mobj.c r_demo.c st_stuff.c \
	d_main.c info.c p_plats.c r_draw.c tables.c \
	doomdef.c m_random.c p_pspr.c r_filter.c version.c \
	doomstat.c p_ceilng.c p_saveg.c r_fps.c v_video.c \
	p_checksum.c p_setup.c r_main.c wi_stuff.c \
	dstrings.c p_doors.c p_sight.c r_patch.c w_memcache.c \
	f_finale.c p_enemy.c p_spec.c r_plane.c w_mmap.c \
	f_wipe.c lprintf.c p_floor.c p_switch.c r_segs.c w_wad.c \
	g_game.c m_argv.c p_genlin.c p_telept.c r_sky.c z_bmalloc.c \
	m_bbox.c p_inter.c p_tick.c r_things.c z_zone.c \
	d_client.c i_video.c i_network.c i_system.c \
	i_main.c i_sound.c jni_doom.c mmus2mid.c pcm2wav.c 

LOCAL_STATIC_LIBRARIES	:= systemutils libovrkernel
LOCAL_SHARED_LIBRARIES	:= vrapi
LOCAL_LDLIBS		:= -llog -landroid -lGLESv3 -lEGL		# include default libraries

include $(BUILD_SHARED_LIBRARY)

$(call import-module,VrApi/Projects/AndroidPrebuilt/jni)
$(call import-module,VrAppSupport/SystemUtils/Projects/AndroidPrebuilt/jni)

