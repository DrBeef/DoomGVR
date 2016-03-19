
#ifndef JNI_DOOM_H
#define JNI_DOOM_H

/*
 *-------------------------------------
 * Standard library includes
 *-------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* JNI Includes */
#include <jni.h>


/*
 *----------------------------------------------------------------------------
 * globals
 *----------------------------------------------------------------------------
 */
#define TT_SHORT_DELAY 0
#define TT_LONG_DELAY 1
#define TT_COLOR_RED 2
#define TT_COLOR_GREEN 4
#define TT_COLOR_BLUE 8
#define TT_COLOR_YELLOW 6
#define TT_COLOR_MAGENTA 10
#define TT_COLOR_CYAN 12
#define TT_COLOR_WHITE 14

#define CB_CLASS "doom/util/Natives"

/**
 * OnMessage callback
 */
#define CB_CLASS_MSG_CB  "OnMessage"
#define CB_CLASS_MSG_SIG  "(Ljava/lang/String;)V"

#define CB_CLASS_INFMSG_CB  "OnInfoMessage"
#define CB_CLASS_INFMSG_SIG  "(Ljava/lang/String;I)V"

// Used before exit()
#define CB_CLASS_FATAL_CB  "OnFatalError"
#define CB_CLASS_FATAL_SIG  "(Ljava/lang/String;)V"

/**
 * prboom4android.jni.Natives.OnImageUpdate callback
 */
#define CB_CLASS_IU_CB  "OnImageUpdate"
#define CB_CLASS_IU_SIG  "([II)V"

/**
 * prboom4android.jni.Natives.OnInitGraphics(int, int) callback
 */
#define CB_CLASS_IG_CB  "OnInitGraphics"
#define CB_CLASS_IG_SIG  "(II)V"

/**
 * prboom4android.jni.Natives.OnStartSound callback
 */
#define CB_CLASS_SS_CB  "OnStartSound"
//#define CB_CLASS_SS_SIG  "(Ljava/lang/String;I)V"
#define CB_CLASS_SS_SIG  "([BI)V"

/**
 * prboom4android.jni.Natives.OnStartMusic (String name , int loop)
 */
#define CB_CLASS_SM_CB  "OnStartMusic"
#define CB_CLASS_SM_SIG  "([BI)V"

/**
 * prboom4android.jni.Natives.OnStopMusic (String name )
 */
#define CB_CLASS_STOPM_CB  "OnStopMusic"
#define CB_CLASS_STOPM_SIG  "([B)V"

/**
 * prboom4android.jni.Natives.OnSetMusicVolume (int volume)
 */
#define CB_CLASS_SETMV_CB  "OnSetMusicVolume"
#define CB_CLASS_SETMV_SIG  "(I)V"

/**
 * prboom4android.jni.Natives.OnQuit (int code)
 */
#define CB_CLASS_QUIT_CB  "OnQuit"
#define CB_CLASS_QUIT_SIG  "(I)V"

/* debug macros */
#define DEBUG_PREFIX printf("JNI__DEBUG ");
#define DEBUG0(X) if (debug_jni) {DEBUG_PREFIX printf(X);}
#define DEBUG1(X,Y) if (debug_jni) {DEBUG_PREFIX printf(X,Y);}
#define DEBUG2(X,Y,Z) if (debug_jni) {DEBUG_PREFIX printf(X,Y,Z);}
#define DEBUG3(X,Y,Z,P) if (debug_jni) {DEBUG_PREFIX printf(X,Y,Z,P);}

#define ERROR0(X) printf(X);
#define ERROR1(X,Y) printf(X,Y);
#define ERROR2(X,Y,Z) printf(X,Y,Z);
#define ERROR3(X,Y,Z,P) printf(X,Y,Z,P);

// Send a fatal error msg, before exi
void jni_fatal_error(const char * text);

// printf str messages back to java
void jni_printf(char *format, ...);

void jni_info_msg(const char * msg, int type);
void jni_send_str( const char * text, int level);

// For sending img updates back to Java
void jni_init_graphics(int width, int height);
void jni_send_pixels(int * data, int eye);
void jni_start_sound (const char * name, int vol);

void jni_start_music (const char * name, int loop);
void jni_stop_music (const char * name);
void jni_set_music_volume (int vol);

const int getArrayLen(JNIEnv * env, jobjectArray jarray);


#endif
