package doom.util;

import android.app.Activity;
import android.view.Surface;

/**
 * Doom for android JNI natives
 *
 * @author vsilva
 */
public class Natives {
	private static EventListener listener;

	public static final int EV_KEYDOWN = 0;
	public static final int EV_KEYUP = 1;
	public static final int EV_MOUSE = 2;

	public static interface EventListener {
		void OnMessage(String text);

		void OnInitGraphics(int w, int h);

		void OnImageUpdate(int[] pixels, int eye);

		void OnFatalError(String text);

		void OnQuit(int code);

		void OnStartSound(String name, int vol);

		void OnStartMusic(String name, int loop);

		void OnStopMusic(String name);

		void OnSetMusicVolume(int volume);

		void OnInfoMessage(String msg, int displayType);

		void onNewFrame(float yaw, float pitch, float roll);

		void onDrawEye(int eye, int width, int height, float[] eyeView, float[] centerEyeView, float[] projection);

		void onFinishFrame();
	}

	// Activity lifecycle
	public static native long onCreate(Activity obj);

	public static native void onStart(long handle, String commandLineParams);

	public static native void onResume(long handle);

	public static native void onPause(long handle);

	public static native void onStop(long handle);

	public static native void onDestroy(long handle);

	// Surface lifecycle
	public static native void onSurfaceCreated(long handle, Surface s);

	public static native void onSurfaceChanged(long handle, Surface s);

	public static native void onSurfaceDestroyed(long handle);


	public static void setListener(EventListener l) {
		listener = l;
	}

	/**
	 * Native Main Doom Initialisation
	 *
	 * @param argv
	 * @return
	 */
	public static native int DoomInit(String[] argv, String wadDir);

	/**
	 * Start frame rendering
	 */
	public static native int DoomStartFrame(float pitch, float yaw, float roll);

	/**
	 * Draw Eye: 0 = left, 1 = right
	 */
	public static native int DoomDrawEye(int eye, int textureUnit);

	/**
	 * Finish frame rendering
	 */
	public static native int DoomEndFrame();


	// Input
	public static native void onKeyEvent(long handle, int keyCode, int action, int character);

	public static native void onTouchEvent(long handle, int source, int action, float x, float y);

	public static native void onMotionEvent(long handle, int source, int action, float x, float y);

	//Little game state getters
	public static native int gameState();

	public static native int isMapShowing();

	public static native int isMenuShowing();

	/***********************************************************
	 * C - Callbacks
	 ***********************************************************/

	/**
	 * This fires on messages from the C layer
	 *
	 * @param text
	 */
	@SuppressWarnings("unused")
	private static void OnMessage(String text) {
		if (listener != null)
			listener.OnMessage(text);
	}

	@SuppressWarnings("unused")
	private static void OnInfoMessage(String msg, int displayType) {
		if (listener != null)
			listener.OnInfoMessage(msg, displayType);
	}

	@SuppressWarnings("unused")
	private static void OnInitGraphics(int w, int h) {
		if (listener != null)
			listener.OnInitGraphics(w, h);
	}

	@SuppressWarnings("unused")
	private static void OnImageUpdate(int[] pixels, int eye) {
		if (listener != null)
			listener.OnImageUpdate(pixels, eye);

	}

	/**
	 * Fires when the C lib calls exit()
	 *
	 * @param message
	 */
	@SuppressWarnings("unused")
	private static void OnFatalError(String message) {
		if (listener != null)
			listener.OnFatalError(message);
	}

	/**
	 * Fires when Doom Quits
	 *
	 * @param code
	 */
	@SuppressWarnings("unused")
	private static void OnQuit(int code) {
		if (listener != null)
			listener.OnQuit(code);
	}

	/**
	 * Fires when a sound is played in the C layer.
	 */
	@SuppressWarnings("unused")
	private static void OnStartSound(byte[] name, int vol) {
		if (listener != null)
			listener.OnStartSound(new String(name), vol);
	}

	/**
	 * Start background music callback
	 *
	 * @param name
	 * @param loop
	 */
	@SuppressWarnings("unused")
	private static void OnStartMusic(byte[] name, int loop) {
		if (listener != null)
			listener.OnStartMusic(new String(name), loop);
	}

	/**
	 * Stop bg music
	 *
	 * @param name
	 */
	@SuppressWarnings("unused")
	private static void OnStopMusic(byte[] name) {
		if (listener != null)
			listener.OnStopMusic(new String(name));
	}


	/**
	 * Set bg music volume
	 *
	 * @param volume Range: (0-15)
	 */
	@SuppressWarnings("unused")
	private static void OnSetMusicVolume(int volume) {
		if (listener != null)
			listener.OnSetMusicVolume((int) (volume * 100.0 / 15.0));
	}


	private static void onNewFrame(float yaw, float pitch, float roll) {
		if (listener != null)
			listener.onNewFrame(yaw, pitch, roll);
	}

	private static void onDrawEye(int eye, int width, int height, float[] eyeView, float[] centerEyeView, float[] projection) {
		if (listener != null)
			listener.onDrawEye(eye, width, height, eyeView, centerEyeView, projection);
	}

	private static void onFinishFrame() {
		if (listener != null)
			listener.onFinishFrame();
	}
}
