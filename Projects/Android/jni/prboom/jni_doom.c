#include <stdio.h>
#include "include/doom_jni_Natives.h"
#include "include/jni_doom.h"
#include "doomdef.h"
#include "doomstat.h"
#include "d_event.h"


// Global env ref (for callbacks)

//static JNIEnv *g_jniEnv = 0;
JavaVM *g_VM = 0;

/**
 * Used for image update
 */
jclass jNativesCls;
jmethodID jSendImageMethod = 0;
jmethodID jStartSoundMethod = 0;
jmethodID jSendInfoMessage = 0;
jmethodID jSetMusicVolume = 0;
jmethodID jStartMusic = 0;
jmethodID jStopMusic = 0;
jmethodID jInitGraphics = 0;
jmethodID jSendStr = 0;
jmethodID jQuit = 0;

jmethodID jOnNewFrame = 0;
jmethodID jOnDrawEye = 0;
jmethodID jOnFinishFrame = 0;

// Java image pixels: int ARGB
jintArray jImage = 0;
int iSize;


int JNI_OnLoad(JavaVM* vm, void* reserved)
{
    JNIEnv *env;
    g_VM = vm;
    if((*vm)->GetEnv(vm, (void**) &env, JNI_VERSION_1_4) != JNI_OK)
    {
        return -1;
    }

    loadJNIMethods(env);

    return JNI_VERSION_1_4;
}

int loadJNIMethods(JNIEnv * env)
{
	/*
	 * Load the Image update class (called many times)
	 */
	jobject tmp = (*env)->FindClass(env, CB_CLASS);
	jNativesCls = (jobject)(*env)->NewGlobalRef(env, tmp);

	if ( jNativesCls == 0 ) {
		jni_printf("Unable to find class: %s", CB_CLASS);
		return -1;
	}

	jOnNewFrame = (*env)->GetStaticMethodID(env, jNativesCls
			, "onNewFrame"
			, "(FFF)V");

	if ( jOnNewFrame == 0 ) {
		jni_printf("Unable to find method onNewFrame");
		return -1;
	}

	jOnDrawEye = (*env)->GetStaticMethodID(env, jNativesCls
			, "onDrawEye"
			, "(III[F[F[F)V");

	if ( jOnDrawEye == 0 ) {
		jni_printf("Unable to find method onDrawEye");
		return -1;
	}

	jOnFinishFrame = (*env)->GetStaticMethodID(env, jNativesCls
			, "onFinishFrame"
			, "()V");

	if ( jOnFinishFrame == 0 ) {
		jni_printf("Unable to find method onFinishFrame");
		return -1;
	}

	// Load doom.util.Natives.OnImageUpdate(char[])
	jSendImageMethod = (*env)->GetStaticMethodID(env, jNativesCls
			, CB_CLASS_IU_CB
			, CB_CLASS_IU_SIG);

	if ( jSendImageMethod == 0 ) {
		jni_printf("Unable to find method OnImageUpdate(char[]): %s", CB_CLASS);
		return -1;
	}

	// Load OnStartSound(String name, int vol)
	jStartSoundMethod = (*env)->GetStaticMethodID(env, jNativesCls
			, CB_CLASS_SS_CB
			, CB_CLASS_SS_SIG);

	if ( jStartSoundMethod == 0 ) {
		jni_printf("Unable to find method OnStartSound sig: %s ", CB_CLASS_SS_SIG);
		return -1;
	}

	jStartMusic = (*env)->GetStaticMethodID(env, jNativesCls
			, CB_CLASS_SM_CB
			, CB_CLASS_SM_SIG);
	if ( jStartMusic  == 0 ) {
		jni_printf("Unable to find method OnStartMusic sig: %s ", CB_CLASS_SM_SIG);
		return -1;
	}

	jStopMusic = (*env)->GetStaticMethodID(env, jNativesCls
			, CB_CLASS_STOPM_CB
			, CB_CLASS_STOPM_SIG);
	if ( jStopMusic  == 0 ) {
		jni_printf("Unable to find method OnStopMusic sig: %s ", CB_CLASS_STOPM_SIG);
		return -1;
	}


	jSetMusicVolume = (*env)->GetStaticMethodID(env, jNativesCls
			, CB_CLASS_SETMV_CB
			, CB_CLASS_SETMV_SIG);

	if ( jSetMusicVolume == 0 ) {
		jni_printf("Unable to find method SetMusicVolume sig: %s ", CB_CLASS_SETMV_SIG);
		return -1;
	}

	jQuit = (*env)->GetStaticMethodID(env, jNativesCls
			, CB_CLASS_QUIT_CB
			, CB_CLASS_QUIT_SIG);

	if ( jQuit == 0 ) {
		jni_printf("Unable to find method OnQuit sig: %s ", CB_CLASS_QUIT_SIG);
		return -1;
	}

	// Load OnInfoMessage(String name, boolean longDisplay)
	jSendInfoMessage = (*env)->GetStaticMethodID(env, jNativesCls
			, CB_CLASS_INFMSG_CB
			, CB_CLASS_INFMSG_SIG);

	if ( jSendInfoMessage == 0 ) {
		jni_printf("Unable to find method OnInfoMessage sig: %s ", CB_CLASS_INFMSG_SIG);
		return -1;
	}

	jInitGraphics = (*env)->GetStaticMethodID(env, jNativesCls
			, CB_CLASS_IG_CB
			, CB_CLASS_IG_SIG);
	if ( jInitGraphics == 0 ) {
		jni_printf("Unable to find method OnInitGraphics sig: %s ", CB_CLASS_IG_SIG);
		return -1;
	}

	jSendStr = (*env)->GetStaticMethodID(env, jNativesCls
			, CB_CLASS_MSG_CB
			, CB_CLASS_MSG_SIG);
	if ( jSendStr == 0 ) {
		jni_printf("Unable to find method OnSendStr sig: %s ", CB_CLASS_MSG_SIG);
		return -1;
	}


	return 0;
}

extern int doom_main(int argc, char **argv, char *wadDir);

/*
 * Class:     doom_util_Natives
 * Method:    DoomMain
 * Signature: ([Ljava/lang/String;)V
 */
JNIEXPORT jint JNICALL Java_doom_util_Natives_DoomInit
  (JNIEnv * env, jclass class, jobjectArray jargv, jstring jDoomWADDir)
{
	(*env)->GetJavaVM(env, &g_VM);

	if (loadJNIMethods(env) != 0)
		return -1;

	// Extract char ** args from Java array
	jsize clen =  getArrayLen(env, jargv);

	char ** args = (char**)malloc((int)clen * sizeof(char*));

	int i;
	jstring jrow;
	for (i = 0; i < clen; i++)
	{
	    jrow = (jstring)(*env)->GetObjectArrayElement(env, jargv, i);
	    const char *row  = (*env)->GetStringUTFChars(env, jrow, 0);

	    args[i] = malloc( strlen(row) + 1);
	    strcpy (args[i], row);

	    jni_printf("Main argv[%d]=%s", i, args[i]);

	    // free java string jrow
	    (*env)->ReleaseStringUTFChars(env, jrow, row);
	}

	const char *row  = (*env)->GetStringUTFChars(env, jDoomWADDir, 0);

	char *doomWADDir = malloc( strlen(row) + 1);
	strcpy (doomWADDir, row);

	jni_printf("Main argv[%d]=%s", i, args[i]);

	// free java string jrow
	(*env)->ReleaseStringUTFChars(env, jrow, row);

	doom_main (clen, args, doomWADDir);

	return 0;
}

/*
 * Class:     doom_util_Natives
 * Method:    DoomInit
 * Signature: ()I
 */

extern void D_DoomStartFrame(float pitch, float yaw, float roll);

JNIEXPORT jint JNICALL Java_doom_util_Natives_DoomStartFrame(JNIEnv * env, jclass class, float pitch, float yaw, float roll )
{
	D_DoomStartFrame(pitch, yaw, roll);
}

/*
 * Class:     doom_util_Natives
 * Method:    DoomInit
 * Signature: (I)I
 */

extern void D_Display(int eye);
extern int texUnit;
JNIEXPORT jint JNICALL Java_doom_util_Natives_DoomDrawEye(JNIEnv * env, jclass class, jint eye, jint textureUnit)
{
	texUnit = (int)textureUnit;
	D_Display((int)eye);
}

/*
 * Class:     doom_util_Natives
 * Method:    DoomInit
 * Signature: ()I
 */

extern void D_DoomEndFrame(void);

JNIEXPORT jint JNICALL Java_doom_util_Natives_DoomEndFrame(JNIEnv * env, jclass class)
{
	D_DoomEndFrame();
}


/*
 * Class:     doom_util_Natives
 * Method:    keyEvent
 * Signature: (II)V
 */
extern void D_PostEvent (event_t* ev);

JNIEXPORT jint JNICALL Java_doom_util_Natives_keyEvent
  (JNIEnv * env, jclass cls, jint type, jint key)
{
	event_t event;
	event.type = (int)type;
	event.data1 = (int)key;
	D_PostEvent(&event);

	return type + key;
}

/*
 * Class:     doom_util_Natives
 * Method:    motionEvent
 * Signature: (II)I
 */
JNIEXPORT jint JNICALL Java_doom_util_Natives_motionEvent
  (JNIEnv * env, jclass cls, jint x, jint y, jint z)
{
	event_t event;
	event.type = ev_joystick;
	event.data1 = x;
	event.data2 = y;
	event.data3 = z;
	D_PostEvent(&event);
	return 0;
}

/*
 * Class:     doom_util_Natives
 * Method:    setVideoMode
 * Signature: (II)I
 */
extern void I_UpdateVideoMode(void);

JNIEXPORT jint JNICALL Java_doom_util_Natives_setVideoMode
  (JNIEnv * env, jclass cls, jint w, jint h)
{
	extern int SCREENWIDTH, SCREENHEIGHT, desired_fullscreen;

	SCREENWIDTH = (int)w;
	SCREENHEIGHT = (int)h;
	desired_fullscreen = 0;

	I_UpdateVideoMode();
}

extern  gamestate_t  gamestate;
extern boolean menuactive;
extern boolean demoplayback;

JNIEXPORT jint JNICALL Java_doom_util_Natives_gameState
		(JNIEnv * env, jclass cls)
{
	return (int)gamestate +
			(int) automapmode +
			menuactive ? 1 : 0 +
			demoplayback ? 1 : 0;
}


JNIEXPORT jint JNICALL Java_doom_util_Natives_isMapShowing
		(JNIEnv * env, jclass cls)
{
	return (int) automapmode;
}

JNIEXPORT jint JNICALL Java_doom_util_Natives_isMenuShowing
		(JNIEnv * env, jclass cls)
{
	return menuactive ? 1 : 0;
}

/**
 * Get java array length
 */
const int getArrayLen(JNIEnv * env, jobjectArray jarray)
{
	return (*env)->GetArrayLength(env, jarray);
}



/***********************************************************
 * Doom Callbacks - Send stuff back to Java
 ***********************************************************/


/**
 * Fires when Doom graphics are initialized.
 * params: img width, height
 */
void jni_init_graphics(int width, int height)
{
	if ( !g_VM) {
		return;
	}
	JNIEnv *env;
	if (((*g_VM)->GetEnv(g_VM, (void**) &env, JNI_VERSION_1_4))<0)
	{
		(*g_VM)->AttachCurrentThread(g_VM, &env, NULL);
	}

	iSize = width * height;

	// call doom.util.Natives.OnInitGraphics(w, h);
	if (jInitGraphics) {
	    (*env)->CallStaticVoidMethod(env, jNativesCls
	    		, jInitGraphics
	    		, width, height);
	}
}

/**
 * Image update Java callback. Gets called many times per sec.
 * It must not lookup JNI classes/methods or create any objects, otherwise
 * the local JNI ref table Will overflow & the app will crash
 */
void jni_send_pixels(int * data, int eye)
{
	if ( !g_VM) {
		return;
	}
	JNIEnv *env;
	if (((*g_VM)->GetEnv(g_VM, (void**) &env, JNI_VERSION_1_4))<0)
	{
		(*g_VM)->AttachCurrentThread(g_VM, &env, NULL);
	}

	if (jImage == 0)
	{
		jbyteArray temp_buffer = (*env)-> NewIntArray(env, iSize);
		jImage = (jbyteArray)(*env)->NewGlobalRef(env, temp_buffer);
	}

	// Send img back to java.
	if (jSendImageMethod) {

		(*env)->SetIntArrayRegion(env, jImage, 0, iSize, (jint *) data);

		// Call Java method
	    (*env)->CallStaticVoidMethod(env, jNativesCls
	    		, jSendImageMethod
	    		, jImage
				, eye);
	}
}


/**
 * Send a string back to Java
 */
void jni_send_str( const char * text, int level) {
	if ( !g_VM) {
		return;
	}
	JNIEnv *env;
	if (((*g_VM)->GetEnv(g_VM, (void**) &env, JNI_VERSION_1_4))<0)
	{
		(*g_VM)->AttachCurrentThread(g_VM, &env, NULL);
	}

	// Call doom.jni.Natives.OnMessage(String)
	if (jSendStr) {
	    (*env)->CallStaticVoidMethod(env, jNativesCls
	    	, jSendStr
	    	, (*env)->NewStringUTF(env, text)
			, (jint) level );
	}
}

/**
 * Printf into the java layer
 * does a varargs printf into a temp buffer
 * and calls jni_sebd_str
 */
void jni_printf(char *format, ...)
{
/*
	va_list         argptr;
	static char             string[1024];

	va_start (argptr, format);
	vsprintf (string, format,argptr);
	va_end (argptr);

	jni_send_str (string, 0);
*/
}

void jni_message(int level, char *format, ...)
{
	va_list         argptr;
	static char             string[1024];

	va_start (argptr, format);
	vsprintf (string, format,argptr);
	va_end (argptr);

	jni_send_str (string, level);
}


/**
 * Called when a fatal error has occurred.
 * The receiver should terminate
 */
void jni_fatal_error(const char * text) {
	JNIEnv *env;
	if (((*g_VM)->GetEnv(g_VM, (void**) &env, JNI_VERSION_1_4))<0)
	{
		(*g_VM)->AttachCurrentThread(g_VM, &env, NULL);
	}

	if ( !env) {
		printf("JNI FATAL: Unable to attach to cuur thread: %s.\n", text);
		exit(-1);
	}

	if ( !jNativesCls ) {
		jNativesCls = (*env)->FindClass(env, CB_CLASS);

		if ( jNativesCls == 0 ) {
	    		ERROR1("JNI FATAL: Unable to find class: %s", CB_CLASS);
	    		exit(-1);
		}
	}
	jmethodID mid = (*env)->GetStaticMethodID(env, jNativesCls
		, CB_CLASS_FATAL_CB
		, CB_CLASS_FATAL_SIG);

	if (mid) {
	    (*env)->CallStaticVoidMethod(env, jNativesCls
	    		, mid
	    		, (*env)->NewStringUTF(env, text) );
	}
	else {
	    printf("JNI FATAL: Unable to find method: %s, signature: %s\n"
	    		, CB_CLASS_MSG_CB, CB_CLASS_MSG_SIG );
	    exit (-1);
	}
}


/**
 * Fires multiple times when a sound is played
 * @param name Sound name
 * @param volume
 */
void jni_start_sound (const char * name, int vol)
{
	if ( !g_VM) {
		return;
	}
	JNIEnv *env;
	if (((*g_VM)->GetEnv(g_VM, (void**) &env, JNI_VERSION_1_4))<0)
	{
		(*g_VM)->AttachCurrentThread(g_VM, &env, NULL);
	}

	if ( jStartSoundMethod == 0 ) {
		//jni_printf("BUG: Invalid Doom JNI method method OnStartSound %s", CB_CLASS_SS_SIG);
	    return ;
	}

	// Create a new char[] used by jni_send_pixels
	// Used to prevent JNI ref table overflows
	int iSize = strlen(name);
	jbyteArray jSound = (*env)-> NewByteArray(env, iSize);

	(*env)->SetByteArrayRegion(env, jSound, 0, iSize, (jbyte *) name);

	// Call Java method
	(*env)->CallStaticVoidMethod(env, jNativesCls
			, jStartSoundMethod
			, jSound //(*env)->NewStringUTF(env, name)
			, (jint) vol);

	(*env)->DeleteLocalRef(env,jSound);
}

/**
 * Fires multiple times when a sound is played
 * @param name Sound name
 * @param volume
 */
void jni_info_msg (const char * msg, int type)
{
	/*
	 * Attach to the curr thread otherwise we get JNI WARNING:
	 * threadid=3 using env from threadid=15 which aborts the VM
	 */
	JNIEnv *env;

	if ( !g_VM) {
		//ERROR0("I_JNI: jni_start_sound No JNI Environment available.\n");
		return;
	}

	(*g_VM)->AttachCurrentThread (g_VM, (void **) &env, NULL);

	if ( jSendInfoMessage == 0 ) {
		//jni_printf("BUG: Invalid Doom JNI method method OnStartSound %s", CB_CLASS_SS_SIG);
	    return ;
	}

	// Call Java method
	(*env)->CallStaticVoidMethod(env, jNativesCls
			, jSendInfoMessage
			, (*env)->NewStringUTF(env, msg)
			, (jint) type);
}
/**
 * Fires when a background song is requested
 */
void jni_start_music (const char * name, int loop)
{
	if (!name || strlen(name) == 0)
		return;

	JNIEnv *env;
	if (((*g_VM)->GetEnv(g_VM, (void**) &env, JNI_VERSION_1_4))<0)
	{
		(*g_VM)->AttachCurrentThread(g_VM, &env, NULL);
	}

	int iSize = strlen(name);
	jbyteArray jSound = (*env)-> NewByteArray(env, iSize);

	(*env)->SetByteArrayRegion(env, jSound, 0, iSize, (jbyte *) name);

	if (jStartMusic) {
		(*env)->CallStaticVoidMethod(env, jNativesCls
				, jStartMusic
				, jSound
				, (jint) loop );
	}

	(*env)->DeleteLocalRef(env,jSound);
}

/**
 * Fires when a background song is stopped
 */
void jni_stop_music (const char * name)
{
	if (!name || strlen(name) == 0)
		return;

	JNIEnv *env;
	if (((*g_VM)->GetEnv(g_VM, (void**) &env, JNI_VERSION_1_4))<0)
	{
		(*g_VM)->AttachCurrentThread(g_VM, &env, NULL);
	}

	int iSize = strlen(name);
	jbyteArray jSound = (*env)-> NewByteArray(env, iSize);

	(*env)->SetByteArrayRegion(env, jSound, 0, iSize, (jbyte *) name);

	if (jStopMusic) {
		(*env)->CallStaticVoidMethod(env, jNativesCls
				, jStopMusic
				, jSound
				);
	}

	(*env)->DeleteLocalRef(env,jSound);
}

/**
 * Set bg msic vol callback
 */
void jni_set_music_volume (int vol) {

	JNIEnv *env;
	if (((*g_VM)->GetEnv(g_VM, (void**) &env, JNI_VERSION_1_4))<0)
	{
		(*g_VM)->AttachCurrentThread(g_VM, &env, NULL);
	}

	if (jSetMusicVolume) {
		(*env)->CallStaticVoidMethod(env, jNativesCls
				, jSetMusicVolume
				, (jint) vol);
	}
}

/**
 * Set bg msic vol callback
 */
void jni_quit (int code) {

	JNIEnv *env;
	if (((*g_VM)->GetEnv(g_VM, (void**) &env, JNI_VERSION_1_4))<0)
	{
		(*g_VM)->AttachCurrentThread(g_VM, &env, NULL);
	}

	if (jQuit) {
		(*env)->CallStaticVoidMethod(env, jNativesCls
				, jQuit
				, (jint) code);
	}
}


void jni_new_frame (float yaw, float pitch, float roll)
{
	ALOGV("jni_new_frame: yaw=%f, pitch=%f, roll=%f", yaw, pitch, roll);
	JNIEnv *env;
	if (((*g_VM)->GetEnv(g_VM, (void**) &env, JNI_VERSION_1_4))<0)
	{
		(*g_VM)->AttachCurrentThread(g_VM, &env, NULL);
	}

	if (jOnNewFrame) {
		(*env)->CallStaticVoidMethod(env, jNativesCls
				, jOnNewFrame
				, yaw
				, pitch
				, roll
				);
	}
}

void jni_draw_eye (int eye, int width, int height, float* eyeView, float *centerEyeView, float* projection)
{
	JNIEnv *env;
	if (((*g_VM)->GetEnv(g_VM, (void**) &env, JNI_VERSION_1_4))<0)
	{
		(*g_VM)->AttachCurrentThread(g_VM, &env, NULL);
	}

	jfloatArray jEyeView = (*env)-> NewFloatArray(env, 16);
	jfloatArray jCenterEye = (*env)-> NewFloatArray(env, 16);
	jfloatArray jProjectionMatrix = (*env)-> NewFloatArray(env, 16);

	(*env)->SetFloatArrayRegion(env, jEyeView, 0, 16, eyeView);
	(*env)->SetFloatArrayRegion(env, jCenterEye, 0, 16, centerEyeView);
	(*env)->SetFloatArrayRegion(env, jProjectionMatrix, 0, 16, projection);

	if (jOnDrawEye) {
		(*env)->CallStaticVoidMethod(env, jNativesCls
				, jOnDrawEye
				, eye
				, width
				, height
				, jEyeView
				, jCenterEye
				, jProjectionMatrix
				);
	}

	(*env)->DeleteLocalRef(env,jEyeView);
	(*env)->DeleteLocalRef(env,jCenterEye);
	(*env)->DeleteLocalRef(env,jProjectionMatrix);
}

void jni_finish_frame ()
{
	ALOGV("jni_finish_frame: ");
	JNIEnv *env;
	if (((*g_VM)->GetEnv(g_VM, (void**) &env, JNI_VERSION_1_4))<0)
	{
		(*g_VM)->AttachCurrentThread(g_VM, &env, NULL);
	}

	if (jOnFinishFrame) {
		(*env)->CallStaticVoidMethod(env, jNativesCls
				, jOnFinishFrame
				);
	}
}