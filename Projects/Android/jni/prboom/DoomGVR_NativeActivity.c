/************************************************************************************

Filename	:	DoomGVR.c based on VrCubeWorld_SurfaceView.c
Content		:	This sample uses a plain Android SurfaceView and handles all
				Activity and Surface life cycle events in native code. This sample
				does not use the application framework and also does not use LibOVR.
				This sample only uses the VrApi.
Created		:	March, 2015
Authors		:	J.M.P. van Waveren / Simon Brown

Copyright	:	Copyright 2015 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <android/log.h>
#include <android/native_window_jni.h>	// for native window JNI
#include <android/input.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>

#include "doomdef.h"
#include "doomstat.h"
#include "d_event.h"

#if !defined( EGL_OPENGL_ES3_BIT_KHR )
#define EGL_OPENGL_ES3_BIT_KHR		0x0040
#endif

#define PITCH 0
#define YAW 1
#define ROLL 2

#include "VrApi.h"
#include "VrApi_Helpers.h"

#include "SystemActivities.h"

#define DEBUG 1
#define LOG_TAG "DoomGVR"

#define ALOGE(...) __android_log_print( ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__ )
#if DEBUG
#define ALOGV(...) __android_log_print( ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__ )
#else
#define ALOGV(...)
#endif

static const int CPU_LEVEL			= 3;
static const int GPU_LEVEL			= 2;

static bool doom_initialised = false;

static JavaVM *jVM;

extern void jni_new_frame (float yaw, float pitch, float roll);
extern void jni_draw_eye (int eye, int width, int height, float* eyeView, float* centerEyeView, float* projection);
extern void jni_finish_frame ();
extern void D_PostEvent (event_t* ev);

//Timing stuff for joypad control
static long oldtime=0;
long delta=0;
float last_joystick_x=0;
float last_joystick_y=0;

int curtime;
int Sys_Milliseconds (void)
{
	struct timeval tp;
	struct timezone tzp;
	static int		secbase;

	gettimeofday(&tp, &tzp);

	if (!secbase)
	{
		secbase = tp.tv_sec;
		return tp.tv_usec/1000;
	}

	curtime = (tp.tv_sec - secbase)*1000 + tp.tv_usec/1000;

	return curtime;
}

int returnvalue = -1;

float hmdorientation[3];

static void UnEscapeQuotes( char *arg )
{
	char *last = NULL;
	while( *arg ) {
		if( *arg == '"' && *last == '\\' ) {
			char *c_curr = arg;
			char *c_last = last;
			while( *c_curr ) {
				*c_last = *c_curr;
				c_last = c_curr;
				c_curr++;
			}
			*c_last = '\0';
		}
		last = arg;
		arg++;
	}
}

static int ParseCommandLine(char *cmdline, char **argv)
{
	char *bufp;
	char *lastp = NULL;
	int argc, last_argc;
	argc = last_argc = 0;
	for ( bufp = cmdline; *bufp; ) {
		while ( isspace(*bufp) ) {
			++bufp;
		}
		if ( *bufp == '"' ) {
			++bufp;
			if ( *bufp ) {
				if ( argv ) {
					argv[argc] = bufp;
				}
				++argc;
			}
			while ( *bufp && ( *bufp != '"' || *lastp == '\\' ) ) {
				lastp = bufp;
				++bufp;
			}
		} else {
			if ( *bufp ) {
				if ( argv ) {
					argv[argc] = bufp;
				}
				++argc;
			}
			while ( *bufp && ! isspace(*bufp) ) {
				++bufp;
			}
		}
		if ( *bufp ) {
			if ( argv ) {
				*bufp = '\0';
			}
			++bufp;
		}
		if( argv && last_argc != argc ) {
			UnEscapeQuotes( argv[last_argc] );
		}
		last_argc = argc;
	}
	if ( argv ) {
		argv[argc] = NULL;
	}
	return(argc);
}

/*
================================================================================

OpenGL-ES Utility Functions

================================================================================
*/

#if 0
	#define GL( func )		func; EglCheckErrors();
#else
	#define GL( func )		func;
#endif

static const char * EglErrorString( const EGLint error )
{
	switch ( error )
	{
		case EGL_SUCCESS:				return "EGL_SUCCESS";
		case EGL_NOT_INITIALIZED:		return "EGL_NOT_INITIALIZED";
		case EGL_BAD_ACCESS:			return "EGL_BAD_ACCESS";
		case EGL_BAD_ALLOC:				return "EGL_BAD_ALLOC";
		case EGL_BAD_ATTRIBUTE:			return "EGL_BAD_ATTRIBUTE";
		case EGL_BAD_CONTEXT:			return "EGL_BAD_CONTEXT";
		case EGL_BAD_CONFIG:			return "EGL_BAD_CONFIG";
		case EGL_BAD_CURRENT_SURFACE:	return "EGL_BAD_CURRENT_SURFACE";
		case EGL_BAD_DISPLAY:			return "EGL_BAD_DISPLAY";
		case EGL_BAD_SURFACE:			return "EGL_BAD_SURFACE";
		case EGL_BAD_MATCH:				return "EGL_BAD_MATCH";
		case EGL_BAD_PARAMETER:			return "EGL_BAD_PARAMETER";
		case EGL_BAD_NATIVE_PIXMAP:		return "EGL_BAD_NATIVE_PIXMAP";
		case EGL_BAD_NATIVE_WINDOW:		return "EGL_BAD_NATIVE_WINDOW";
		case EGL_CONTEXT_LOST:			return "EGL_CONTEXT_LOST";
		default:						return "unknown";
	}
}

static const char * EglFrameBufferStatusString( GLenum status )
{
	switch ( status )
	{
		case GL_FRAMEBUFFER_UNDEFINED:						return "GL_FRAMEBUFFER_UNDEFINED";
		case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:			return "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT";
		case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:	return "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT";
		case GL_FRAMEBUFFER_UNSUPPORTED:					return "GL_FRAMEBUFFER_UNSUPPORTED";
		case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:			return "GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE";
		default:											return "unknown";
	}
}

static void EglCheckErrors()
{
	for ( int i = 0; i < 10; i++ )
	{
		const EGLint error = eglGetError();
		if ( error == EGL_SUCCESS )
		{
			break;
		}
		ALOGE( "EGL error: %s", EglErrorString( error ) );
	}
}

/*
================================================================================

ovrEgl

================================================================================
*/

typedef struct
{
	EGLint		MajorVersion;
	EGLint		MinorVersion;
	EGLDisplay	Display;
	EGLConfig	Config;
	EGLSurface	TinySurface;
	EGLSurface	MainSurface;
	EGLContext	Context;
} ovrEgl;

static void ovrEgl_Clear( ovrEgl * egl )
{
	egl->MajorVersion = 0;
	egl->MinorVersion = 0;
	egl->Display = 0;
	egl->Config = 0;
	egl->TinySurface = EGL_NO_SURFACE;
	egl->MainSurface = EGL_NO_SURFACE;
	egl->Context = EGL_NO_CONTEXT;
}

static void ovrEgl_CreateContext( ovrEgl * egl, const ovrEgl * shareEgl )
{
	if ( egl->Display != 0 )
	{
		return;
	}

	egl->Display = eglGetDisplay( EGL_DEFAULT_DISPLAY );
	ALOGV( "        eglInitialize( Display, &MajorVersion, &MinorVersion )" );
	eglInitialize( egl->Display, &egl->MajorVersion, &egl->MinorVersion );
	// Do NOT use eglChooseConfig, because the Android EGL code pushes in multisample
	// flags in eglChooseConfig if the user has selected the "force 4x MSAA" option in
	// settings, and that is completely wasted for our warp target.
	const int MAX_CONFIGS = 1024;
	EGLConfig configs[MAX_CONFIGS];
	EGLint numConfigs = 0;
	if ( eglGetConfigs( egl->Display, configs, MAX_CONFIGS, &numConfigs ) == EGL_FALSE )
	{
		ALOGE( "        eglGetConfigs() failed: %s", EglErrorString( eglGetError() ) );
		return;
	}
	const EGLint configAttribs[] =
	{
		EGL_BLUE_SIZE,  8,
		EGL_GREEN_SIZE, 8,
		EGL_RED_SIZE,   8,
		EGL_DEPTH_SIZE, 0,
		EGL_SAMPLES,	0,
		EGL_NONE
	};
	egl->Config = 0;
	for ( int i = 0; i < numConfigs; i++ )
	{
		EGLint value = 0;

		eglGetConfigAttrib( egl->Display, configs[i], EGL_RENDERABLE_TYPE, &value );
		if ( ( value & EGL_OPENGL_ES3_BIT_KHR ) != EGL_OPENGL_ES3_BIT_KHR )
		{
			continue;
		}

		// The pbuffer config also needs to be compatible with normal window rendering
		// so it can share textures with the window context.
		eglGetConfigAttrib( egl->Display, configs[i], EGL_SURFACE_TYPE, &value );
		if ( ( value & ( EGL_WINDOW_BIT | EGL_PBUFFER_BIT ) ) != ( EGL_WINDOW_BIT | EGL_PBUFFER_BIT ) )
		{
			continue;
		}

		int	j = 0;
		for ( ; configAttribs[j] != EGL_NONE; j += 2 )
		{
			eglGetConfigAttrib( egl->Display, configs[i], configAttribs[j], &value );
			if ( value != configAttribs[j + 1] )
			{
				break;
			}
		}
		if ( configAttribs[j] == EGL_NONE )
		{
			egl->Config = configs[i];
			break;
		}
	}
	if ( egl->Config == 0 )
	{
		ALOGE( "        eglChooseConfig() failed: %s", EglErrorString( eglGetError() ) );
		return;
	}
	EGLint contextAttribs[] =
	{
		EGL_CONTEXT_CLIENT_VERSION, 3,
		EGL_NONE
	};
	ALOGV( "        Context = eglCreateContext( Display, Config, EGL_NO_CONTEXT, contextAttribs )" );
	egl->Context = eglCreateContext( egl->Display, egl->Config, ( shareEgl != NULL ) ? shareEgl->Context : EGL_NO_CONTEXT, contextAttribs );
	if ( egl->Context == EGL_NO_CONTEXT )
	{
		ALOGE( "        eglCreateContext() failed: %s", EglErrorString( eglGetError() ) );
		return;
	}
	const EGLint surfaceAttribs[] =
	{
		EGL_WIDTH, 16,
		EGL_HEIGHT, 16,
		EGL_NONE
	};
	ALOGV( "        TinySurface = eglCreatePbufferSurface( Display, Config, surfaceAttribs )" );
	egl->TinySurface = eglCreatePbufferSurface( egl->Display, egl->Config, surfaceAttribs );
	if ( egl->TinySurface == EGL_NO_SURFACE )
	{
		ALOGE( "        eglCreatePbufferSurface() failed: %s", EglErrorString( eglGetError() ) );
		eglDestroyContext( egl->Display, egl->Context );
		egl->Context = EGL_NO_CONTEXT;
		return;
	}
	ALOGV( "        eglMakeCurrent( Display, TinySurface, TinySurface, Context )" );
	if ( eglMakeCurrent( egl->Display, egl->TinySurface, egl->TinySurface, egl->Context ) == EGL_FALSE )
	{
		ALOGE( "        eglMakeCurrent() failed: %s", EglErrorString( eglGetError() ) );
		eglDestroySurface( egl->Display, egl->TinySurface );
		eglDestroyContext( egl->Display, egl->Context );
		egl->Context = EGL_NO_CONTEXT;
		return;
	}
}

static void ovrEgl_DestroyContext( ovrEgl * egl )
{
	if ( egl->Display != 0 )
	{
		ALOGE( "        eglMakeCurrent( Display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT )" );
		if ( eglMakeCurrent( egl->Display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT ) == EGL_FALSE )
		{
			ALOGE( "        eglMakeCurrent() failed: %s", EglErrorString( eglGetError() ) );
		}
	}
	if ( egl->Context != EGL_NO_CONTEXT )
	{
		ALOGE( "        eglDestroyContext( Display, Context )" );
		if ( eglDestroyContext( egl->Display, egl->Context ) == EGL_FALSE )
		{
			ALOGE( "        eglDestroyContext() failed: %s", EglErrorString( eglGetError() ) );
		}
		egl->Context = EGL_NO_CONTEXT;
	}
	if ( egl->TinySurface != EGL_NO_SURFACE )
	{
		ALOGE( "        eglDestroySurface( Display, TinySurface )" );
		if ( eglDestroySurface( egl->Display, egl->TinySurface ) == EGL_FALSE )
		{
			ALOGE( "        eglDestroySurface() failed: %s", EglErrorString( eglGetError() ) );
		}
		egl->TinySurface = EGL_NO_SURFACE;
	}
	if ( egl->Display != 0 )
	{
		ALOGE( "        eglTerminate( Display )" );
		if ( eglTerminate( egl->Display ) == EGL_FALSE )
		{
			ALOGE( "        eglTerminate() failed: %s", EglErrorString( eglGetError() ) );
		}
		egl->Display = 0;
	}
}

static void ovrEgl_CreateSurface( ovrEgl * egl, ANativeWindow * nativeWindow )
{
	if ( egl->MainSurface != EGL_NO_SURFACE )
	{
		return;
	}
	ALOGV( "        MainSurface = eglCreateWindowSurface( Display, Config, nativeWindow, attribs )" );
	const EGLint surfaceAttribs[] = { EGL_NONE };
	egl->MainSurface = eglCreateWindowSurface( egl->Display, egl->Config, nativeWindow, surfaceAttribs );
	if ( egl->MainSurface == EGL_NO_SURFACE )
	{
		ALOGE( "        eglCreateWindowSurface() failed: %s", EglErrorString( eglGetError() ) );
		return;
	}
	ALOGV( "        eglMakeCurrent( display, MainSurface, MainSurface, Context )" );
	if ( eglMakeCurrent( egl->Display, egl->MainSurface, egl->MainSurface, egl->Context ) == EGL_FALSE )
	{
		ALOGE( "        eglMakeCurrent() failed: %s", EglErrorString( eglGetError() ) );
		return;
	}
}

static void ovrEgl_DestroySurface( ovrEgl * egl )
{
	if ( egl->Context != EGL_NO_CONTEXT )
	{
		ALOGV( "        eglMakeCurrent( display, TinySurface, TinySurface, Context )" );
		if ( eglMakeCurrent( egl->Display, egl->TinySurface, egl->TinySurface, egl->Context ) == EGL_FALSE )
		{
			ALOGE( "        eglMakeCurrent() failed: %s", EglErrorString( eglGetError() ) );
		}
	}
	if ( egl->MainSurface != EGL_NO_SURFACE )
	{
		ALOGV( "        eglDestroySurface( Display, MainSurface )" );
		if ( eglDestroySurface( egl->Display, egl->MainSurface ) == EGL_FALSE )
		{
			ALOGE( "        eglDestroySurface() failed: %s", EglErrorString( eglGetError() ) );
		}
		egl->MainSurface = EGL_NO_SURFACE;
	}
}


/*
================================================================================

ovrFramebuffer

================================================================================
*/

typedef struct
{
	int						Width;
	int						Height;
	int						Multisamples;
	int						TextureSwapChainLength;
	int						TextureSwapChainIndex;
	ovrTextureSwapChain *	ColorTextureSwapChain;
	GLuint *				DepthBuffers;
	GLuint *				FrameBuffers;
} ovrFramebuffer;

static void ovrFramebuffer_Clear( ovrFramebuffer * frameBuffer )
{
	frameBuffer->Width = 0;
	frameBuffer->Height = 0;
	frameBuffer->Multisamples = 0;
	frameBuffer->TextureSwapChainLength = 0;
	frameBuffer->TextureSwapChainIndex = 0;
	frameBuffer->ColorTextureSwapChain = NULL;
	frameBuffer->DepthBuffers = NULL;
	frameBuffer->FrameBuffers = NULL;
}

static bool ovrFramebuffer_Create( ovrFramebuffer * frameBuffer, const ovrTextureFormat colorFormat, const int width, const int height, const int multisamples )
{
	frameBuffer->Width = width;
	frameBuffer->Height = height;
	frameBuffer->Multisamples = multisamples;

	frameBuffer->ColorTextureSwapChain = vrapi_CreateTextureSwapChain( VRAPI_TEXTURE_TYPE_2D, colorFormat, width, height, 1, true );
	frameBuffer->TextureSwapChainLength = vrapi_GetTextureSwapChainLength( frameBuffer->ColorTextureSwapChain );
	frameBuffer->DepthBuffers = (GLuint *)malloc( frameBuffer->TextureSwapChainLength * sizeof( GLuint ) ); 
	frameBuffer->FrameBuffers = (GLuint *)malloc( frameBuffer->TextureSwapChainLength * sizeof( GLuint ) );

	for ( int i = 0; i < frameBuffer->TextureSwapChainLength; i++ )
	{
		// Create the color buffer texture.
		const GLuint colorTexture = vrapi_GetTextureSwapChainHandle( frameBuffer->ColorTextureSwapChain, i );
		GL( glBindTexture( GL_TEXTURE_2D, colorTexture ) );
		GL( glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE ) );
		GL( glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE ) );
		GL( glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR ) );
		GL( glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR ) );
		GL( glBindTexture( GL_TEXTURE_2D, 0 ) );

		// Create depth buffer.
		GL( glGenRenderbuffers( 1, &frameBuffer->DepthBuffers[i] ) );
		GL( glBindRenderbuffer( GL_RENDERBUFFER, frameBuffer->DepthBuffers[i] ) );
		GL( glRenderbufferStorage( GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height ) );
		GL( glBindRenderbuffer( GL_RENDERBUFFER, 0 ) );

		// Create the frame buffer.
		GL( glGenFramebuffers( 1, &frameBuffer->FrameBuffers[i] ) );
		GL( glBindFramebuffer( GL_FRAMEBUFFER, frameBuffer->FrameBuffers[i] ) );
		GL( glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, frameBuffer->DepthBuffers[i] ) );
		GL( glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture, 0 ) );
		GL( GLenum renderFramebufferStatus = glCheckFramebufferStatus( GL_FRAMEBUFFER ) );
		GL( glBindFramebuffer( GL_FRAMEBUFFER, 0 ) );
		if ( renderFramebufferStatus != GL_FRAMEBUFFER_COMPLETE )
		{
			ALOGE( "Incomplete frame buffer object: %s", EglFrameBufferStatusString( renderFramebufferStatus ) );
			return false;
		}
	}

	return true;
}

static void ovrFramebuffer_Destroy( ovrFramebuffer * frameBuffer )
{
	GL( glDeleteFramebuffers( frameBuffer->TextureSwapChainLength, frameBuffer->FrameBuffers ) );
	GL( glDeleteRenderbuffers( frameBuffer->TextureSwapChainLength, frameBuffer->DepthBuffers ) );
	vrapi_DestroyTextureSwapChain( frameBuffer->ColorTextureSwapChain );

	free( frameBuffer->DepthBuffers );
	free( frameBuffer->FrameBuffers );

	ovrFramebuffer_Clear( frameBuffer );
}

static void ovrFramebuffer_SetCurrent( ovrFramebuffer * frameBuffer )
{
	GL( glBindFramebuffer( GL_FRAMEBUFFER, frameBuffer->FrameBuffers[frameBuffer->TextureSwapChainIndex] ) );
}

static void ovrFramebuffer_SetNone()
{
	GL( glBindFramebuffer( GL_FRAMEBUFFER, 0 ) );
}

static void ovrFramebuffer_Resolve( ovrFramebuffer * frameBuffer )
{
	// Discard the depth buffer, so the tiler won't need to write it back out to memory.
	const GLenum depthAttachment[1] = { GL_DEPTH_ATTACHMENT };
	glInvalidateFramebuffer( GL_FRAMEBUFFER, 1, depthAttachment );

	// Flush this frame worth of commands.
	glFlush();
}

static void ovrFramebuffer_Advance( ovrFramebuffer * frameBuffer )
{
	// Advance to the next texture from the set.
	frameBuffer->TextureSwapChainIndex = ( frameBuffer->TextureSwapChainIndex + 1 ) % frameBuffer->TextureSwapChainLength;
}

static void ovrFramebuffer_ClearEdgeTexels( ovrFramebuffer * frameBuffer )
{
	GL( glEnable( GL_SCISSOR_TEST ) );
	GL( glViewport( 0, 0, frameBuffer->Width, frameBuffer->Height ) );

	// Explicitly clear the border texels to black because OpenGL-ES does not support GL_CLAMP_TO_BORDER.
	// Clear to fully opaque black.
	GL( glClearColor( 0.0f, 0.0f, 0.0f, 1.0f ) );

	// bottom
	GL( glScissor( 0, 0, frameBuffer->Width, 1 ) );
	GL( glClear( GL_COLOR_BUFFER_BIT ) );
	// top
	GL( glScissor( 0, frameBuffer->Height - 1, frameBuffer->Width, 1 ) );
	GL( glClear( GL_COLOR_BUFFER_BIT ) );
	// left
	GL( glScissor( 0, 0, 1, frameBuffer->Height ) );
	GL( glClear( GL_COLOR_BUFFER_BIT ) );
	// right
	GL( glScissor( frameBuffer->Width - 1, 0, 1, frameBuffer->Height ) );
	GL( glClear( GL_COLOR_BUFFER_BIT ) );

	GL( glScissor( 0, 0, 0, 0 ) );
	GL( glDisable( GL_SCISSOR_TEST ) );
}

/*
================================================================================

ovrRenderer

================================================================================
*/

#define NUM_EYES			VRAPI_FRAME_LAYER_EYE_MAX
#define NUM_BUFFERS			3
#define NUM_MULTI_SAMPLES	1

typedef struct
{
	ovrFramebuffer	FrameBuffer[VRAPI_FRAME_LAYER_EYE_MAX];
	ovrFramebuffer	DoomFrameBuffer;
	ovrMatrix4f		ProjectionMatrix;
	ovrMatrix4f		TexCoordsTanAnglesMatrix;
} ovrRenderer;

static void ovrRenderer_Clear( ovrRenderer * renderer )
{
	for ( int i = 0; i < NUM_BUFFERS; i++ )
	{
		for ( int eye = 0; eye < NUM_EYES; eye++ )
		{
			ovrFramebuffer_Clear( &renderer->FrameBuffer[eye] );
		}
	}
	ovrFramebuffer_Clear( &renderer->DoomFrameBuffer );
	renderer->ProjectionMatrix = ovrMatrix4f_CreateIdentity();
	renderer->TexCoordsTanAnglesMatrix = ovrMatrix4f_CreateIdentity();
}

static void ovrRenderer_Create( ovrRenderer * renderer, const ovrJava * java )
{
	// Create the render Textures.
	for ( int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++ )
	{
		ovrFramebuffer_Create( &renderer->FrameBuffer[eye],
								VRAPI_TEXTURE_FORMAT_8888,
								vrapi_GetSystemPropertyInt( java, VRAPI_SYS_PROP_SUGGESTED_EYE_TEXTURE_WIDTH ),
								vrapi_GetSystemPropertyInt( java, VRAPI_SYS_PROP_SUGGESTED_EYE_TEXTURE_HEIGHT ),
								NUM_MULTI_SAMPLES );
	}

	ovrFramebuffer_Create( &renderer->DoomFrameBuffer,
							VRAPI_TEXTURE_FORMAT_8888,
							vrapi_GetSystemPropertyInt( java, VRAPI_SYS_PROP_SUGGESTED_EYE_TEXTURE_WIDTH ),
							vrapi_GetSystemPropertyInt( java, VRAPI_SYS_PROP_SUGGESTED_EYE_TEXTURE_HEIGHT ),
							NUM_MULTI_SAMPLES );

	// Setup the projection matrix.
	renderer->ProjectionMatrix = ovrMatrix4f_CreateProjectionFov(
										vrapi_GetSystemPropertyFloat( java, VRAPI_SYS_PROP_SUGGESTED_EYE_FOV_DEGREES_X ),
										vrapi_GetSystemPropertyFloat( java, VRAPI_SYS_PROP_SUGGESTED_EYE_FOV_DEGREES_Y ),
										0.0f, 0.0f, 1.0f, 0.0f );
	renderer->TexCoordsTanAnglesMatrix = ovrMatrix4f_TanAngleMatrixFromProjection( &renderer->ProjectionMatrix );
}

static void ovrRenderer_Destroy( ovrRenderer * renderer )
{
	for ( int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++ )
	{
		ovrFramebuffer_Destroy( &renderer->FrameBuffer[eye] );
	}
	ovrFramebuffer_Destroy( &renderer->DoomFrameBuffer );
	renderer->ProjectionMatrix = ovrMatrix4f_CreateIdentity();
	renderer->TexCoordsTanAnglesMatrix = ovrMatrix4f_CreateIdentity();
}

void QuatToYawPitchRoll(ovrQuatf q, float* out) {
	float sqw = q.w*q.w;
	float sqx = q.x*q.x;
	float sqy = q.y*q.y;
	float sqz = q.z*q.z;
	float test = q.x*q.y + q.z*q.w;
	out[PITCH] = -atan2(2*q.x*q.w-2*q.y*q.z , 1 - 2*sqx - 2*sqz);
	if( test > 0.49999 ) { // singularity at north pole
		out[YAW] = (2 * atan2(q.x,q.w));
		out[ROLL] = 0;
	}
	else if( test < -0.49999 ) { // singularity at south pole
		out[YAW] = (-2 * atan2(q.x,q.w));
		out[ROLL] = 0;
	}
	else {
		out[YAW] = atan2(2*q.y*q.w-2*q.x*q.z , 1 - 2*sqy - 2*sqz);
		out[ROLL] = -asin(2*q.x*q.y+2*q.z*q.w);
	}
	
	//Convert to degrees
	out[YAW] /= (M_PI / 180.0f);
	out[PITCH] /= (M_PI / 180.0f);
	out[ROLL] /= (M_PI / 180.0f);
}

static ovrFrameParms ovrRenderer_RenderFrame( ovrRenderer * renderer, const ovrJava * java, long long frameIndex, 
											int minimumVsyncs, const ovrPerformanceParms * perfParms,
											const ovrTracking * tracking )
{
	ovrFrameParms parms = vrapi_DefaultFrameParms( java, VRAPI_FRAME_INIT_DEFAULT, vrapi_GetTimeInSeconds(), NULL );

	parms.FrameIndex = frameIndex;
	parms.MinimumVsyncs = minimumVsyncs;
	parms.PerformanceParms = *perfParms;
	parms.Layers[VRAPI_FRAME_LAYER_TYPE_WORLD].Flags |= VRAPI_FRAME_LAYER_FLAG_CHROMATIC_ABERRATION_CORRECTION;

	long t=Sys_Milliseconds();
	delta=t-oldtime;
	oldtime=t;
	if (delta>1000)
		delta=1000;

	//Send right-stick info to Doom
	event_t event;
	event.type = ev_joystick;
	event.data1 = 0;
	event.data2 = last_joystick_x;
	event.data3 = last_joystick_y;
	D_PostEvent(&event);

	// Calculate the center view matrix.
	const ovrHeadModelParms headModelParms = vrapi_DefaultHeadModelParms();
	const ovrMatrix4f centerEyeViewMatrix = vrapi_GetCenterEyeViewMatrix( &headModelParms, tracking, NULL );

	//Get orientation
	// We extract Yaw, Pitch, Roll instead of directly using the orientation
	// to allow "additional" yaw manipulation with mouse/controller.
	const ovrQuatf quat = tracking->HeadPose.Pose.Orientation;
	QuatToYawPitchRoll(quat, hmdorientation);

	//Set move information - if showing menu, don't pass head orientation through
	jni_new_frame(hmdorientation[YAW], hmdorientation[PITCH], hmdorientation[ROLL]);

	// Render the eye images.
	for ( int eye = 0; eye < NUM_EYES; eye++ )
	{
		ovrFramebuffer * frameBuffer = &renderer->FrameBuffer[eye];
		ovrFramebuffer_SetCurrent( frameBuffer );

		// Apply the projection and view transformation
		ovrMatrix4f eyeViewMatrix = vrapi_GetEyeViewMatrix( &headModelParms, &centerEyeViewMatrix, eye );

		jni_draw_eye(eye, frameBuffer->Width,
                          frameBuffer->Height,
                          (float*)(eyeViewMatrix.M[0]),
                          (float*)(centerEyeViewMatrix.M[0]),
                          (float*)(renderer->ProjectionMatrix.M[0]));


		ovrFramebuffer_ClearEdgeTexels( frameBuffer );

		ovrFramebuffer_Resolve( frameBuffer );

		parms.Layers[VRAPI_FRAME_LAYER_TYPE_WORLD].Textures[eye].ColorTextureSwapChain = frameBuffer->ColorTextureSwapChain;
		parms.Layers[VRAPI_FRAME_LAYER_TYPE_WORLD].Textures[eye].TextureSwapChainIndex = frameBuffer->TextureSwapChainIndex;
		parms.Layers[VRAPI_FRAME_LAYER_TYPE_WORLD].Textures[eye].TexCoordsFromTanAngles = renderer->TexCoordsTanAnglesMatrix;
		parms.Layers[VRAPI_FRAME_LAYER_TYPE_WORLD].Textures[eye].HeadPose = tracking->HeadPose;

		ovrFramebuffer_Advance( frameBuffer );
	}

	jni_finish_frame();

	ovrFramebuffer_SetNone();

	return parms;
}


/*================================================================================

ovrApp

================================================================================
*/

typedef enum
{
	BACK_BUTTON_STATE_NONE,
	BACK_BUTTON_STATE_PENDING_DOUBLE_TAP,
	BACK_BUTTON_STATE_PENDING_SHORT_PRESS,
	BACK_BUTTON_STATE_SKIP_UP
} ovrBackButtonState;

typedef struct
{
	ovrJava				Java;
	ovrEgl				Egl;
	ANativeWindow *		NativeWindow;
	bool				Resumed;
	ovrMobile *			Ovr;
	long long			FrameIndex;
	int					MinimumVsyncs;
	ovrBackButtonState	BackButtonState;
	bool				BackButtonDown;
	double				BackButtonDownStartTime;
	ovrRenderer			Renderer;
	bool				WasMounted;
} ovrApp;

static void ovrApp_Clear( ovrApp * app )
{
	app->Java.Vm = NULL;
	app->Java.Env = NULL;
	app->Java.ActivityObject = NULL;
	app->NativeWindow = NULL;
	app->Resumed = false;
	app->Ovr = NULL;
	app->FrameIndex = 1;
	app->MinimumVsyncs = 1;
	app->BackButtonState = BACK_BUTTON_STATE_NONE;
	app->BackButtonDown = false;
	app->BackButtonDownStartTime = 0.0;
	app->WasMounted = false;

	ovrEgl_Clear( &app->Egl );
	ovrRenderer_Clear( &app->Renderer );
}

static void ovrApp_HandleVrModeChanges( ovrApp * app )
{
	if ( app->NativeWindow != NULL && app->Egl.MainSurface == EGL_NO_SURFACE )
	{
		ovrEgl_CreateSurface( &app->Egl, app->NativeWindow );
	}

	if ( app->Resumed != false && app->NativeWindow != NULL )
	{
		if ( app->Ovr == NULL )
		{
			ovrModeParms parms = vrapi_DefaultModeParms( &app->Java );
			parms.ResetWindowFullscreen = false;	// No need to reset the FLAG_FULLSCREEN window flag when using a View

#if EXPLICIT_GL_OBJECTS == 1
			parms.Display = (size_t)app->Egl.Display;
			parms.WindowSurface = (size_t)app->Egl.MainSurface;
			parms.ShareContext = (size_t)app->Egl.Context;
#else
			ALOGV( "        eglGetCurrentSurface( EGL_DRAW ) = %p", eglGetCurrentSurface( EGL_DRAW ) );
#endif

			app->Ovr = vrapi_EnterVrMode( &parms );

			ALOGV( "        vrapi_EnterVrMode()" );

#if EXPLICIT_GL_OBJECTS == 0
			ALOGV( "        eglGetCurrentSurface( EGL_DRAW ) = %p", eglGetCurrentSurface( EGL_DRAW ) );
#endif
		}
	}
	else
	{
		if ( app->Ovr != NULL )
		{
			ALOGV( "        eglGetCurrentSurface( EGL_DRAW ) = %p", eglGetCurrentSurface( EGL_DRAW ) );

			vrapi_LeaveVrMode( app->Ovr );
			app->Ovr = NULL;

			ALOGV( "        vrapi_LeaveVrMode()" );
			ALOGV( "        eglGetCurrentSurface( EGL_DRAW ) = %p", eglGetCurrentSurface( EGL_DRAW ) );
		}
	}

	if ( app->NativeWindow == NULL && app->Egl.MainSurface != EGL_NO_SURFACE )
	{
		ovrEgl_DestroySurface( &app->Egl );
	}
}

static void ovrApp_BackButtonAction( ovrApp * app, const ovrPerformanceParms * perfParms )
{
	if ( app->BackButtonState == BACK_BUTTON_STATE_PENDING_DOUBLE_TAP )
	{
		app->BackButtonState = BACK_BUTTON_STATE_SKIP_UP;
	}
	else if ( app->BackButtonState == BACK_BUTTON_STATE_PENDING_SHORT_PRESS && !app->BackButtonDown )
	{
		if ( ( vrapi_GetTimeInSeconds() - app->BackButtonDownStartTime ) > BACK_BUTTON_DOUBLE_TAP_TIME_IN_SECONDS )
		{
			//Send ESC to doom
			//QGVR_KeyEvent(1, 27, 0);
			//QGVR_KeyEvent(0, 27, 0);
			app->BackButtonState = BACK_BUTTON_STATE_NONE;
		}
	}
	else if ( app->BackButtonState == BACK_BUTTON_STATE_NONE && app->BackButtonDown )
	{
		if ( ( vrapi_GetTimeInSeconds() - app->BackButtonDownStartTime ) > BACK_BUTTON_LONG_PRESS_TIME_IN_SECONDS )
		{
			SystemActivities_StartSystemActivity( &app->Java, PUI_GLOBAL_MENU, NULL );
			app->BackButtonState = BACK_BUTTON_STATE_SKIP_UP;
		}
	}
}

static int ovrApp_HandleKeyEvent( ovrApp * app, const int keyCode, const int action, const int character  )
{
	// Handle GearVR back button.
	if ( keyCode == AKEYCODE_BACK )
	{
		if ( action == AKEY_EVENT_ACTION_DOWN )
		{
			if ( !app->BackButtonDown )
			{
				if ( ( vrapi_GetTimeInSeconds() - app->BackButtonDownStartTime ) < BACK_BUTTON_DOUBLE_TAP_TIME_IN_SECONDS )
				{
					app->BackButtonState = BACK_BUTTON_STATE_PENDING_DOUBLE_TAP;
				}
				app->BackButtonDownStartTime = vrapi_GetTimeInSeconds();
			}
			app->BackButtonDown = true;
		}
		else if ( action == AKEY_EVENT_ACTION_UP )
		{
			if ( app->BackButtonState == BACK_BUTTON_STATE_NONE )
			{
				if ( ( vrapi_GetTimeInSeconds() - app->BackButtonDownStartTime ) < BACK_BUTTON_SHORT_PRESS_TIME_IN_SECONDS )
				{
					app->BackButtonState = BACK_BUTTON_STATE_PENDING_SHORT_PRESS;
				}
			}
			else if ( app->BackButtonState == BACK_BUTTON_STATE_SKIP_UP )
			{
				app->BackButtonState = BACK_BUTTON_STATE_NONE;
			}
			app->BackButtonDown = false;
		}
		return 1;
	}
	else
	{
		//Dispatch to doom
		event_t event;
		event.type = action;
		event.data1 = (int)keyCode;
		D_PostEvent(&event);
	}
	return 0;
}

#define SOURCE_GAMEPAD 	0x00000401
#define SOURCE_JOYSTICK 0x01000010
static int ovrApp_HandleTouchEvent( ovrApp * app, const int source, const int action, const float x, const float y )
{
	// Handle GVR touch pad.
	if ( app->Ovr != NULL )
	{
//		if (source == SOURCE_JOYSTICK || source == SOURCE_GAMEPAD)
//			QGVR_Analog(true, x, y);
	}
	return 1;
}

static int ovrApp_HandleMotionEvent( ovrApp * app, const int source, const int action, const float x, const float y )
{
	static bool down = false;
	if (source == SOURCE_JOYSTICK || source == SOURCE_GAMEPAD)
	{
		last_joystick_x=x;
		last_joystick_y=y;
	}
	return 1;
}

int resetHMDOrientation = 0;
void ResetHMDOrientation()
{
	resetHMDOrientation = 1;
}

void DGVR_exit(int exitCode)
{
	returnvalue = exitCode;
}

static void ovrApp_HandleSystemEvents( ovrApp * app )
{
	SystemActivitiesAppEventList_t appEvents;
	SystemActivities_Update( app->Ovr, &app->Java, &appEvents );

	// if you want to reorient on mount, check mount state here
	const bool isMounted = ( vrapi_GetSystemStatusInt( &app->Java, VRAPI_SYS_STATUS_MOUNTED ) != VRAPI_FALSE );
	if ( resetHMDOrientation == 1 || ( isMounted && !app->WasMounted ))
	{
		// We just mounted so push a reorient event to be handled at the app level (if desired)
		char reorientMessage[1024];
		SystemActivities_CreateSystemActivitiesCommand( "", SYSTEM_ACTIVITY_EVENT_REORIENT, "", "", reorientMessage, sizeof( reorientMessage ) );
		SystemActivities_AppendAppEvent( &appEvents, reorientMessage );
		
		//Ensure this is reset so we don't continuously reorient
		resetHMDOrientation = 0;
	}
	app->WasMounted = isMounted;

	// TODO: any app specific events should be handled right here by looping over appEvents list

	SystemActivities_PostUpdate( app->Ovr, &app->Java, &appEvents );
}

/*
================================================================================

ovrMessageQueue

================================================================================
*/

typedef enum
{
	MQ_WAIT_NONE,		// don't wait
	MQ_WAIT_RECEIVED,	// wait until the consumer thread has received the message
	MQ_WAIT_PROCESSED	// wait until the consumer thread has processed the message
} ovrMQWait;

#define MAX_MESSAGE_PARMS	8
#define MAX_MESSAGES		1024

typedef struct
{
	int			Id;
	ovrMQWait	Wait;
	long long	Parms[MAX_MESSAGE_PARMS];
} ovrMessage;

static void ovrMessage_Init( ovrMessage * message, const int id, const int wait )
{
	message->Id = id;
	message->Wait = wait;
	memset( message->Parms, 0, sizeof( message->Parms ) );
}

static void		ovrMessage_SetPointerParm( ovrMessage * message, int index, void * ptr ) { *(void **)&message->Parms[index] = ptr; }
static void *	ovrMessage_GetPointerParm( ovrMessage * message, int index ) { return *(void **)&message->Parms[index]; }
static void		ovrMessage_SetIntegerParm( ovrMessage * message, int index, int value ) { message->Parms[index] = value; }
static int		ovrMessage_GetIntegerParm( ovrMessage * message, int index ) { return (int)message->Parms[index]; }
static void		ovrMessage_SetFloatParm( ovrMessage * message, int index, float value ) { *(float *)&message->Parms[index] = value; }
static float	ovrMessage_GetFloatParm( ovrMessage * message, int index ) { return *(float *)&message->Parms[index]; }

// Cyclic queue with messages.
typedef struct
{
	ovrMessage	 		Messages[MAX_MESSAGES];
	volatile int		Head;	// dequeue at the head
	volatile int		Tail;	// enqueue at the tail
	volatile bool		Enabled;
	ovrMQWait			Wait;
	pthread_mutex_t		Mutex;
	pthread_cond_t		Posted;
	pthread_cond_t		Received;
	pthread_cond_t		Processed;
} ovrMessageQueue;

static void ovrMessageQueue_Create( ovrMessageQueue * messageQueue )
{
	messageQueue->Head = 0;
	messageQueue->Tail = 0;
	messageQueue->Enabled = false;
	messageQueue->Wait = MQ_WAIT_NONE;

	pthread_mutexattr_t	attr;
	pthread_mutexattr_init( &attr );
	pthread_mutexattr_settype( &attr, PTHREAD_MUTEX_ERRORCHECK );
	pthread_mutex_init( &messageQueue->Mutex, &attr );
	pthread_mutexattr_destroy( &attr );
	pthread_cond_init( &messageQueue->Posted, NULL );
	pthread_cond_init( &messageQueue->Received, NULL );
	pthread_cond_init( &messageQueue->Processed, NULL );
}

static void ovrMessageQueue_Destroy( ovrMessageQueue * messageQueue )
{
	pthread_mutex_destroy( &messageQueue->Mutex );
	pthread_cond_destroy( &messageQueue->Posted );
	pthread_cond_destroy( &messageQueue->Received );
	pthread_cond_destroy( &messageQueue->Processed );
}

static void ovrMessageQueue_Enable( ovrMessageQueue * messageQueue, const bool set )
{
	messageQueue->Enabled = set;
}

static void ovrMessageQueue_PostMessage( ovrMessageQueue * messageQueue, const ovrMessage * message )
{
	if ( !messageQueue->Enabled )
	{
		return;
	}
	while ( messageQueue->Tail - messageQueue->Head >= MAX_MESSAGES )
	{
		usleep( 1000 );
	}
	pthread_mutex_lock( &messageQueue->Mutex );
	messageQueue->Messages[messageQueue->Tail & ( MAX_MESSAGES - 1 )] = *message;
	messageQueue->Tail++;
	pthread_cond_broadcast( &messageQueue->Posted );
	if ( message->Wait == MQ_WAIT_RECEIVED )
	{
		pthread_cond_wait( &messageQueue->Received, &messageQueue->Mutex );
	}
	else if ( message->Wait == MQ_WAIT_PROCESSED )
	{
		pthread_cond_wait( &messageQueue->Processed, &messageQueue->Mutex );
	}
	pthread_mutex_unlock( &messageQueue->Mutex );
}

static void ovrMessageQueue_SleepUntilMessage( ovrMessageQueue * messageQueue )
{
	if ( messageQueue->Wait == MQ_WAIT_PROCESSED )
	{
		pthread_cond_broadcast( &messageQueue->Processed );
		messageQueue->Wait = MQ_WAIT_NONE;
	}
	pthread_mutex_lock( &messageQueue->Mutex );
	if ( messageQueue->Tail > messageQueue->Head )
	{
		pthread_mutex_unlock( &messageQueue->Mutex );
		return;
	}
	pthread_cond_wait( &messageQueue->Posted, &messageQueue->Mutex );
	pthread_mutex_unlock( &messageQueue->Mutex );
}

static bool ovrMessageQueue_GetNextMessage( ovrMessageQueue * messageQueue, ovrMessage * message, bool waitForMessages )
{
	if ( messageQueue->Wait == MQ_WAIT_PROCESSED )
	{
		pthread_cond_broadcast( &messageQueue->Processed );
		messageQueue->Wait = MQ_WAIT_NONE;
	}
	if ( waitForMessages )
	{
		ovrMessageQueue_SleepUntilMessage( messageQueue );
	}
	pthread_mutex_lock( &messageQueue->Mutex );
	if ( messageQueue->Tail <= messageQueue->Head )
	{
		pthread_mutex_unlock( &messageQueue->Mutex );
		return false;
	}
	*message = messageQueue->Messages[messageQueue->Head & ( MAX_MESSAGES - 1 )];
	messageQueue->Head++;
	pthread_mutex_unlock( &messageQueue->Mutex );
	if ( message->Wait == MQ_WAIT_RECEIVED )
	{
		pthread_cond_broadcast( &messageQueue->Received );
	}
	else if ( message->Wait == MQ_WAIT_PROCESSED )
	{
		messageQueue->Wait = MQ_WAIT_PROCESSED;
	}
	return true;
}

/*
================================================================================

ovrAppThread

================================================================================
*/

enum
{
	MESSAGE_ON_CREATE,
	MESSAGE_ON_START,
	MESSAGE_ON_RESUME,
	MESSAGE_ON_PAUSE,
	MESSAGE_ON_STOP,
	MESSAGE_ON_DESTROY,
	MESSAGE_ON_SURFACE_CREATED,
	MESSAGE_ON_SURFACE_DESTROYED,
	MESSAGE_ON_KEY_EVENT,
	MESSAGE_ON_TOUCH_EVENT,
	MESSAGE_ON_MOTION_EVENT
};

typedef struct
{
	JavaVM *		JavaVm;
	jobject			ActivityObject;
	pthread_t		Thread;
	ovrMessageQueue	MessageQueue;
	ANativeWindow * NativeWindow;
} ovrAppThread;

void * AppThreadFunction( void * parm )
{
	ovrAppThread * appThread = (ovrAppThread *)parm;

	static ovrJava java;
	java.Vm = appThread->JavaVm;
	(*java.Vm)->AttachCurrentThread( java.Vm, &java.Env, NULL );
	java.ActivityObject = appThread->ActivityObject;
	SystemActivities_Init( &java );

	const ovrInitParms initParms = vrapi_DefaultInitParms( &java );
	vrapi_Initialize( &initParms );

	ovrApp appState;
	ovrApp_Clear( &appState );
	appState.Java = java;

	ovrEgl_CreateContext( &appState.Egl, NULL );

	ovrPerformanceParms perfParms = vrapi_DefaultPerformanceParms();
	perfParms.CpuLevel = CPU_LEVEL;
	perfParms.GpuLevel = GPU_LEVEL;
	perfParms.MainThreadTid = gettid();

	ovrRenderer_Create( &appState.Renderer, &java );

	//Always use this folder
	chdir("/sdcard/DVR");

	for ( bool destroyed = false; destroyed == false; )
	{
		for ( ; ; )
		{
			ovrMessage message;
			const bool waitForMessages = ( appState.Ovr == NULL && destroyed == false );
			if ( !ovrMessageQueue_GetNextMessage( &appThread->MessageQueue, &message, waitForMessages ) )
			{
				break;
			}

			switch ( message.Id )
			{
				case MESSAGE_ON_CREATE:
				{
					break;
				}
				case MESSAGE_ON_START:
				{
					returnvalue = -1;
					break;
				}
				case MESSAGE_ON_RESUME:
				{
					//If we get here, then user has opted not to quit
					returnvalue = -1;
					appState.Resumed = true;
					break;
				}
				case MESSAGE_ON_PAUSE:
				{
					appState.Resumed = false;
					break;
				}
				case MESSAGE_ON_STOP:
				{
					break;
				}
				case MESSAGE_ON_DESTROY:
				{
					appState.NativeWindow = NULL;
					destroyed = true;
					break;
				}
				case MESSAGE_ON_SURFACE_CREATED:	{ appState.NativeWindow = (ANativeWindow *)ovrMessage_GetPointerParm( &message, 0 ); break; }
				case MESSAGE_ON_SURFACE_DESTROYED:	{ appState.NativeWindow = NULL; break; }
				case MESSAGE_ON_KEY_EVENT:			{ ovrApp_HandleKeyEvent( &appState,
														ovrMessage_GetIntegerParm( &message, 0 ),
														ovrMessage_GetIntegerParm( &message, 1 ),
														ovrMessage_GetIntegerParm( &message, 2 )); break; }
				case MESSAGE_ON_TOUCH_EVENT:		{ ovrApp_HandleTouchEvent( &appState,
						ovrMessage_GetIntegerParm( &message, 0 ),
						ovrMessage_GetIntegerParm( &message, 1 ),
						ovrMessage_GetFloatParm( &message, 2 ),
						ovrMessage_GetFloatParm( &message, 3 ) ); break; }
				case MESSAGE_ON_MOTION_EVENT:		{ ovrApp_HandleMotionEvent( &appState,
						ovrMessage_GetIntegerParm( &message, 0 ),
						ovrMessage_GetIntegerParm( &message, 1 ),
						ovrMessage_GetFloatParm( &message, 2 ),
						ovrMessage_GetFloatParm( &message, 3 ) ); break; }
			}

			ovrApp_HandleVrModeChanges( &appState );
		}

		ovrApp_BackButtonAction( &appState, &perfParms );
		ovrApp_HandleSystemEvents( &appState );

		if ( appState.Ovr == NULL )
		{
			continue;
		}

		// This is the only place the frame index is incremented, right before
		// calling vrapi_GetPredictedDisplayTime().
		appState.FrameIndex++;

		// Get the HMD pose, predicted for the middle of the time period during which
		// the new eye images will be displayed. The number of frames predicted ahead
		// depends on the pipeline depth of the engine and the synthesis rate.
		// The better the prediction, the less black will be pulled in at the edges.
		const double predictedDisplayTime = vrapi_GetPredictedDisplayTime( appState.Ovr, appState.FrameIndex );
		const ovrTracking tracking = vrapi_GetPredictedTracking( appState.Ovr, predictedDisplayTime );

		// Render eye images and setup ovrFrameParms using ovrTracking.
		const ovrFrameParms parms = ovrRenderer_RenderFrame( &appState.Renderer, &appState.Java,
															appState.FrameIndex, appState.MinimumVsyncs, &perfParms,
															&tracking );

		// Hand over the eye images to the time warp.
		vrapi_SubmitFrame( appState.Ovr, &parms );

		//User is thinking about quitting
		if (returnvalue != -1)
		{
			SystemActivities_StartSystemActivity( &appState.Java, PUI_CONFIRM_QUIT, NULL );
		}
	}

	ovrRenderer_Destroy( &appState.Renderer );

	ovrEgl_DestroyContext( &appState.Egl );
	vrapi_Shutdown();

	(*java.Vm)->DetachCurrentThread( java.Vm );

	SystemActivities_ReturnToHome( &appState.Java );
}

int texUnit;
extern int SCREENWIDTH;
extern int SCREENHEIGHT;

void fill_texture(int *pixels)
{
		// Bind texture to texturename
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, texUnit);

		// Set filtering
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

		// Set wrapping mode
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		// Load the bitmap into the bound texture.
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, SCREENWIDTH, SCREENHEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

		//unbind
		glBindTexture(GL_TEXTURE_2D, 0);

}

static void ovrAppThread_Create( ovrAppThread * appThread, JNIEnv * env, jobject activityObject, jclass activityClass)
{
	(*env)->GetJavaVM( env, &appThread->JavaVm );
	appThread->ActivityObject = (*env)->NewGlobalRef( env, activityObject );
	appThread->Thread = 0;
	appThread->NativeWindow = NULL;
	ovrMessageQueue_Create( &appThread->MessageQueue );

	const int createErr = pthread_create( &appThread->Thread, NULL, AppThreadFunction, appThread );
	if ( createErr != 0 )
	{
		ALOGE( "pthread_create returned %i", createErr );
	}
}

static void ovrAppThread_Destroy( ovrAppThread * appThread, JNIEnv * env )
{
	pthread_join( appThread->Thread, NULL );
	(*env)->DeleteGlobalRef( env, appThread->ActivityObject );
	ovrMessageQueue_Destroy( &appThread->MessageQueue );
}

/*
================================================================================

Activity lifecycle

================================================================================
*/

JNIEXPORT jlong JNICALL Java_doom_util_Natives_onCreate( JNIEnv * env, jclass activityClass, jobject activity)
{
	ALOGV( "    GLES3JNILib::onCreate()" );

	ovrAppThread * appThread = (ovrAppThread *) malloc( sizeof( ovrAppThread ) );
	ovrAppThread_Create( appThread, env, activity, activityClass );

	ovrMessageQueue_Enable( &appThread->MessageQueue, true );
	ovrMessage message;
	ovrMessage_Init( &message, MESSAGE_ON_CREATE, MQ_WAIT_PROCESSED );
	ovrMessageQueue_PostMessage( &appThread->MessageQueue, &message );

	return (jlong)((size_t)appThread);
}

JNIEXPORT void JNICALL Java_doom_util_Natives_onStart( JNIEnv * env, jobject obj, jlong handle,
		jstring commandLineParams )
{
	ALOGV( "    GLES3JNILib::onStart()" );

	jboolean iscopy;
    const char *arg = (*env)->GetStringUTFChars(env, commandLineParams, &iscopy);

    char *cmdLine = NULL;
    if (arg && strlen(arg))
    {
    	cmdLine = strdup(arg);
    }

	(*env)->ReleaseStringUTFChars(env, commandLineParams, arg);


	ovrAppThread * appThread = (ovrAppThread *)((size_t)handle);
	ovrMessage message;
	ovrMessage_Init( &message, MESSAGE_ON_START, MQ_WAIT_PROCESSED );
	ovrMessage_SetPointerParm( &message, 0, cmdLine );
	ovrMessageQueue_PostMessage( &appThread->MessageQueue, &message );
}

JNIEXPORT void JNICALL Java_doom_util_Natives_onResume( JNIEnv * env, jobject obj, jlong handle )
{
	ALOGV( "    GLES3JNILib::onResume()" );
	ovrAppThread * appThread = (ovrAppThread *)((size_t)handle);
	ovrMessage message;
	ovrMessage_Init( &message, MESSAGE_ON_RESUME, MQ_WAIT_PROCESSED );
	ovrMessageQueue_PostMessage( &appThread->MessageQueue, &message );
}

JNIEXPORT void JNICALL Java_doom_util_Natives_onPause( JNIEnv * env, jobject obj, jlong handle )
{
	ALOGV( "    GLES3JNILib::onPause()" );
	ovrAppThread * appThread = (ovrAppThread *)((size_t)handle);
	ovrMessage message;
	ovrMessage_Init( &message, MESSAGE_ON_PAUSE, MQ_WAIT_PROCESSED );
	ovrMessageQueue_PostMessage( &appThread->MessageQueue, &message );
}

JNIEXPORT void JNICALL Java_doom_util_Natives_onStop( JNIEnv * env, jobject obj, jlong handle )
{
	ALOGV( "    GLES3JNILib::onStop()" );
	ovrAppThread * appThread = (ovrAppThread *)((size_t)handle);
	ovrMessage message;
	ovrMessage_Init( &message, MESSAGE_ON_STOP, MQ_WAIT_PROCESSED );
	ovrMessageQueue_PostMessage( &appThread->MessageQueue, &message );
}

JNIEXPORT void JNICALL Java_doom_util_Natives_onDestroy( JNIEnv * env, jobject obj, jlong handle )
{
	ALOGV( "    GLES3JNILib::onDestroy()" );
	ovrAppThread * appThread = (ovrAppThread *)((size_t)handle);
	ovrMessage message;
	ovrMessage_Init( &message, MESSAGE_ON_DESTROY, MQ_WAIT_PROCESSED );
	ovrMessageQueue_PostMessage( &appThread->MessageQueue, &message );
	ovrMessageQueue_Enable( &appThread->MessageQueue, false );

	ovrAppThread_Destroy( appThread, env );
	free( appThread );
}

/*
================================================================================

Surface lifecycle

================================================================================
*/

JNIEXPORT void JNICALL Java_doom_util_Natives_onSurfaceCreated( JNIEnv * env, jobject obj, jlong handle, jobject surface )
{
	ALOGV( "    GLES3JNILib::onSurfaceCreated()" );
	ovrAppThread * appThread = (ovrAppThread *)((size_t)handle);

	ANativeWindow * newNativeWindow = ANativeWindow_fromSurface( env, surface );
	if ( ANativeWindow_getWidth( newNativeWindow ) < ANativeWindow_getHeight( newNativeWindow ) )
	{
		// An app that is relaunched after pressing the home button gets an initial surface with
		// the wrong orientation even though android:screenOrientation="landscape" is set in the
		// manifest. The choreographer callback will also never be called for this surface because
		// the surface is immediately replaced with a new surface with the correct orientation.
		ALOGE( "        Surface not in landscape mode!" );
	}

	ALOGV( "        NativeWindow = ANativeWindow_fromSurface( env, surface )" );
	appThread->NativeWindow = newNativeWindow;
	ovrMessage message;
	ovrMessage_Init( &message, MESSAGE_ON_SURFACE_CREATED, MQ_WAIT_PROCESSED );
	ovrMessage_SetPointerParm( &message, 0, appThread->NativeWindow );
	ovrMessageQueue_PostMessage( &appThread->MessageQueue, &message );
}

JNIEXPORT void JNICALL Java_doom_util_Natives_onSurfaceChanged( JNIEnv * env, jobject obj, jlong handle, jobject surface )
{
	ALOGV( "    GLES3JNILib::onSurfaceChanged()" );
	ovrAppThread * appThread = (ovrAppThread *)((size_t)handle);

	ANativeWindow * newNativeWindow = ANativeWindow_fromSurface( env, surface );
	if ( ANativeWindow_getWidth( newNativeWindow ) < ANativeWindow_getHeight( newNativeWindow ) )
	{
		// An app that is relaunched after pressing the home button gets an initial surface with
		// the wrong orientation even though android:screenOrientation="landscape" is set in the
		// manifest. The choreographer callback will also never be called for this surface because
		// the surface is immediately replaced with a new surface with the correct orientation.
		ALOGE( "        Surface not in landscape mode!" );
	}

	if ( newNativeWindow != appThread->NativeWindow )
	{
		if ( appThread->NativeWindow != NULL )
		{
			ovrMessage message;
			ovrMessage_Init( &message, MESSAGE_ON_SURFACE_DESTROYED, MQ_WAIT_PROCESSED );
			ovrMessageQueue_PostMessage( &appThread->MessageQueue, &message );
			ALOGV( "        ANativeWindow_release( NativeWindow )" );
			ANativeWindow_release( appThread->NativeWindow );
			appThread->NativeWindow = NULL;
		}
		if ( newNativeWindow != NULL )
		{
			ALOGV( "        NativeWindow = ANativeWindow_fromSurface( env, surface )" );
			appThread->NativeWindow = newNativeWindow;
			ovrMessage message;
			ovrMessage_Init( &message, MESSAGE_ON_SURFACE_CREATED, MQ_WAIT_PROCESSED );
			ovrMessage_SetPointerParm( &message, 0, appThread->NativeWindow );
			ovrMessageQueue_PostMessage( &appThread->MessageQueue, &message );
		}
	}
	else if ( newNativeWindow != NULL )
	{
		ANativeWindow_release( newNativeWindow );
	}
}

JNIEXPORT void JNICALL Java_doom_util_Natives_onSurfaceDestroyed( JNIEnv * env, jobject obj, jlong handle )
{
	ALOGV( "    GLES3JNILib::onSurfaceDestroyed()" );
	ovrAppThread * appThread = (ovrAppThread *)((size_t)handle);
	ovrMessage message;
	ovrMessage_Init( &message, MESSAGE_ON_SURFACE_DESTROYED, MQ_WAIT_PROCESSED );
	ovrMessageQueue_PostMessage( &appThread->MessageQueue, &message );
	ALOGV( "        ANativeWindow_release( NativeWindow )" );
	ANativeWindow_release( appThread->NativeWindow );
	appThread->NativeWindow = NULL;
}

/*
================================================================================

Input

================================================================================
*/

JNIEXPORT void JNICALL Java_doom_util_Natives_onKeyEvent( JNIEnv * env, jobject obj, jlong handle, int keyCode, int action, int character )
{
	if ( action == AKEY_EVENT_ACTION_UP )
	{
		ALOGV( "    GLES3JNILib::onKeyEvent( %d, %d )", keyCode, action );
	}
	ovrAppThread * appThread = (ovrAppThread *)((size_t)handle);
	ovrMessage message;
	ovrMessage_Init( &message, MESSAGE_ON_KEY_EVENT, MQ_WAIT_NONE );
	ovrMessage_SetIntegerParm( &message, 0, keyCode );
	ovrMessage_SetIntegerParm( &message, 1, action );
	ovrMessage_SetIntegerParm( &message, 2, character );
	ovrMessageQueue_PostMessage( &appThread->MessageQueue, &message );
}

JNIEXPORT void JNICALL Java_doom_util_Natives_onTouchEvent( JNIEnv * env, jobject obj, jlong handle, int source, int action, float x, float y )
{
	if ( action == AMOTION_EVENT_ACTION_UP )
	{
		ALOGV( "    GLES3JNILib::onTouchEvent( %d, %d, %1.0f, %1.0f )", source, action, x, y );
	}
	ovrAppThread * appThread = (ovrAppThread *)((size_t)handle);
	ovrMessage message;
	ovrMessage_Init( &message, MESSAGE_ON_TOUCH_EVENT, MQ_WAIT_NONE );
	ovrMessage_SetIntegerParm( &message, 0, source );
	ovrMessage_SetIntegerParm( &message, 1, action );
	ovrMessage_SetFloatParm( &message, 2, x );
	ovrMessage_SetFloatParm( &message, 3, y );
	ovrMessageQueue_PostMessage( &appThread->MessageQueue, &message );
}

JNIEXPORT void JNICALL Java_doom_util_Natives_onMotionEvent( JNIEnv * env, jobject obj, jlong handle, int source, int action, float x, float y )
{
	if ( action == AMOTION_EVENT_ACTION_UP )
	{
		ALOGV( "    GLES3JNILib::onMotionEvent( %d, %d, %1.0f, %1.0f )", source, action, x, y );
	}
	ovrAppThread * appThread = (ovrAppThread *)((size_t)handle);
	ovrMessage message;
	ovrMessage_Init( &message, MESSAGE_ON_MOTION_EVENT, MQ_WAIT_NONE );
	ovrMessage_SetIntegerParm( &message, 0, source );
	ovrMessage_SetIntegerParm( &message, 1, action );
	ovrMessage_SetFloatParm( &message, 2, x );
	ovrMessage_SetFloatParm( &message, 3, y );
	ovrMessageQueue_PostMessage( &appThread->MessageQueue, &message );
}
