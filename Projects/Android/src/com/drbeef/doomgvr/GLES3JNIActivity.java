
package com.drbeef.doomgvr;


import java.io.BufferedReader;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.ByteBuffer;
import java.nio.IntBuffer;
import java.util.concurrent.ScheduledThreadPoolExecutor;
import java.util.concurrent.TimeUnit;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.res.AssetManager;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.media.MediaPlayer;
import android.net.Uri;
import android.opengl.GLES20;
import android.opengl.Matrix;
import android.os.Bundle;
import android.os.Vibrator;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.Display;
import android.view.InputDevice;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.WindowManager;
import android.view.KeyEvent;
import android.view.MotionEvent;

import doom.util.DoomTools;
import doom.util.Natives;


public class GLES3JNIActivity extends Activity implements SurfaceHolder.Callback, Natives.EventListener
{
	OpenGL openGL = null;

	// Audio Cache Manager
	private doom.audio.AudioManager mAudioMgr;

	// width of mBitmap
	private int mDoomWidth;
	// height of mBitmap
	private int mDoomHeight;

	//Head orientation
	private float[] eulerAngles = new float[3];
	private float hmdYaw;
	private float hmdPitch;
	private float hmdRoll;

	//-1 means start button isn't pressed
	private long startButtonDownCounter = -1;
	//Don't allow the trigger to fire more than once per 200ms
	private long triggerTimeout = 0;

	private Vibrator vibrator;
	private float M_PI = 3.14159265358979323846f;

	//Read these from a file and pass through
	String commandLineParams = new String("");

	private WADChooser  mWADChooser = null;

	public static boolean mDVRInitialised = false;

	//Can't rebuild eye buffers until surface changed flag recorded
	public static boolean mSurfaceChanged = false;
	public static boolean mSurfaceCreated = false;
	public static boolean initOpenGL = false;

	float lensCentreOffset = -1.0f;

	private boolean mShowingSpashScreen = true;
	private int[] splashTexture = new int[1];
	private MediaPlayer mPlayer;
	private int mPlayerVolume = 100;

	// Load the gles3jni library right away to make sure JNI_OnLoad() gets called as the very first thing.
	static {
		try {
			System.loadLibrary("doomgvr");
		} catch (UnsatisfiedLinkError ule) {
			Log.e("JNI", "WARNING: Could not load libdoomgvr.so");
		}
	}

	private static final String TAG = "DoomGVR";

	private SurfaceView mView;
	private SurfaceHolder mSurfaceHolder;
	private long mNativeHandle;
	
	@Override protected void onCreate( Bundle icicle )
	{
		Log.v( TAG, "----------------------------------------------------------------" );
		Log.v( TAG, "GLES3JNIActivity::onCreate()" );
		super.onCreate( icicle );

		mView = new SurfaceView( this );
		setContentView( mView );
		mView.getHolder().addCallback( this );

		// Force the screen to stay on, rather than letting it dim and shut off
		// while the user is watching a movie.
		getWindow().addFlags( WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON );

		// Force screen brightness to stay at maximum
		WindowManager.LayoutParams params = getWindow().getAttributes();
		params.screenBrightness = 1.0f;
		getWindow().setAttributes(params);

		openGL = new OpenGL();
		openGL.onCreate();

		vibrator = (Vibrator) getSystemService(Context.VIBRATOR_SERVICE);

		//At the very least ensure we have a directory containing a config file
		copy_asset("DVR.cfg", DoomTools.GetDVRFolder() + File.separator);
		copy_asset("prboom.wad", DoomTools.GetDVRFolder() + File.separator);
		copy_asset("extraparams.txt", DoomTools.GetDVRFolder() + File.separator);

		File folder = new File(DoomTools.GetDVRFolder() + File.separator + "sound" + File.separator);
		if(!folder.exists())
			folder.mkdirs();

		//Clean up sound folder
		DoomTools.deleteSounds();

		//See if user is trying to use command line params
		BufferedReader br;
		try {
			br = new BufferedReader(new FileReader(DoomTools.GetDVRFolder() + File.separator + "extraparams.txt"));
			String s;
			StringBuilder sb=new StringBuilder(0);
			while ((s=br.readLine())!=null)
				sb.append(s + " ");
			br.close();

			commandLineParams = new String(sb.toString());
		} catch (FileNotFoundException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		} catch (IOException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}

		mWADChooser = new WADChooser(openGL);

		// Audio?
		mAudioMgr = doom.audio.AudioManager.getInstance(this);

		// Listen for Doom events
		Natives.setListener(this);

		mNativeHandle = Natives.onCreate( this );
	}

	public void copy_asset(String name, String folder) {
		File f = new File(folder + name);
		if (!f.exists()) {
			//Ensure we have an appropriate folder
			new File(folder).mkdirs();
			_copy_asset(name, folder + name);
		}
	}

	public void _copy_asset(String name_in, String name_out) {
		AssetManager assets = this.getAssets();

		try {
			InputStream in = assets.open(name_in);
			OutputStream out = new FileOutputStream(name_out);

			copy_stream(in, out);

			out.close();
			in.close();

		} catch (Exception e) {

			e.printStackTrace();
		}

	}

	public static void copy_stream(InputStream in, OutputStream out)
			throws IOException {
		byte[] buf = new byte[1024];
		while (true) {
			int count = in.read(buf);
			if (count <= 0)
				break;
			out.write(buf, 0, count);
		}
	}


	@Override protected void onStart()
	{
		Log.v( TAG, "GLES3JNIActivity::onStart()" );
		super.onStart();

		Natives.onStart(mNativeHandle, commandLineParams);
	}

	@Override protected void onResume()
	{
		Log.v( TAG, "GLES3JNIActivity::onResume()" );
		super.onResume();
		Natives.onResume(mNativeHandle);
	}

	@Override protected void onPause()
	{
		Log.v(TAG, "GLES3JNIActivity::onPause()");
		Natives.onPause(mNativeHandle);
		super.onPause();
	}

	@Override protected void onStop()
	{
		Log.v(TAG, "GLES3JNIActivity::onStop()");
		Natives.onStop(mNativeHandle);
		super.onStop();
	}

	@Override protected void onDestroy()
	{
		Log.v( TAG, "GLES3JNIActivity::onDestroy()" );
		if ( mSurfaceHolder != null )
		{
			Natives.onSurfaceDestroyed( mNativeHandle );
		}
		Natives.onDestroy(mNativeHandle);
		super.onDestroy();
		mNativeHandle = 0;
	}

	@Override public void surfaceCreated( SurfaceHolder holder )
	{
		Log.v(TAG, "GLES3JNIActivity::surfaceCreated()");
		if ( mNativeHandle != 0 )
		{
			Natives.onSurfaceCreated(mNativeHandle, holder.getSurface());
			mSurfaceHolder = holder;

			mSurfaceCreated = true;
		}
	}

	@Override public void surfaceChanged( SurfaceHolder holder, int format, int width, int height )
	{
		Log.v(TAG, "GLES3JNIActivity::surfaceChanged()");
		if ( mNativeHandle != 0 )
		{
			Natives.onSurfaceChanged(mNativeHandle, holder.getSurface());
			mSurfaceHolder = holder;
			mSurfaceChanged = true;
		}
	}
	
	@Override public void surfaceDestroyed( SurfaceHolder holder )
	{
		Log.v( TAG, "GLES3JNIActivity::surfaceDestroyed()" );
		if ( mNativeHandle != 0 )
		{
			Natives.onSurfaceDestroyed( mNativeHandle );
			mSurfaceHolder = null;
		}
	}

	long prevState = 0;
	public void onNewFrame(float yaw, float pitch, float roll) {

		if (!mSurfaceCreated)
			return;

		if (!initOpenGL)
		{
			openGL.onSurfaceCreated();
			openGL.SetupUVCoords();

			//Start intro music
			mPlayer = MediaPlayer.create(this, R.raw.m010912339);
			mPlayer.start();

			//Load bitmap for splash screen
			splashTexture[0] = 0;
			GLES20.glGenTextures(1, splashTexture, 0);

			Bitmap bmp = null;
			try {
				AssetManager assets = this.getAssets();
				InputStream in = assets.open("splash.png");
				bmp = BitmapFactory.decodeStream(in);
				in.close();
			} catch (Exception e) {
				e.printStackTrace();
			}

			// Bind texture to texturename
			GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
			GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, splashTexture[0]);
			openGL.CopyBitmapToTexture(bmp, splashTexture[0]);

			bmp.recycle();

			initOpenGL = true;
		}

		hmdYaw = yaw;
		hmdPitch = pitch;
		hmdRoll = roll;

		if (!mShowingSpashScreen && mWADChooser.choosingWAD())
		{
			return;
		}

		if (!mDVRInitialised) {
			if (!mSurfaceChanged)
				return;

			if (!mShowingSpashScreen) {
				final String[] argv;
				String args = new String();
				args = "doom -iwad " + mWADChooser.GetSelectedWADName() + " " + commandLineParams;
				argv = args.split(" ");
				String dvr= DoomTools.GetDVRFolder();
				Natives.DoomInit(argv, dvr);

				mDVRInitialised = true;

			}
		}

		if (mDVRInitialised) {
			//Fade out intro music
			if (mPlayer != null) {
				mPlayerVolume--;
				if (mPlayerVolume == 0) {
					mPlayer.stop();
					mPlayer.release();
					mPlayer = null;
				}
				else
				{
					float log1 = doom.audio.AudioManager.getLogVolume(mPlayerVolume);
					mPlayer.setVolume(log1, log1);
				}
			}

			long newState = Natives.gameState();
			if (newState == 0)
				Natives.DoomStartFrame(hmdPitch, hmdYaw, hmdRoll);
			else
				Natives.DoomStartFrame(0, 0, 0);
		}
	}

	public void onDrawEye(int eye, int width, int height, float[] eyeView, float[] centerEyeView, float[] projection) {

		if (!mSurfaceCreated)
			return;

		if (!mShowingSpashScreen && mWADChooser.choosingWAD())
		{
			mWADChooser.onDrawEye(eye, width, height, eyeView, projection, this);
		}
		else if (mDVRInitialised || mShowingSpashScreen) {

			GLES20.glViewport(0, 0,	width, height);

			//Clear the viewport
			GLES20.glEnable(GLES20.GL_SCISSOR_TEST);
			GLES20.glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
			GLES20.glScissor(0, 0,	width, height);
			GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT | GLES20.GL_DEPTH_BUFFER_BIT);

			GLES20.glUseProgram(openGL.sp_Image);

			float modelScreen[] = new float[16];
			Matrix.setIdentityM(modelScreen, 0);

			// Set the position of the screen
			if (mShowingSpashScreen)
			{
				Matrix.translateM(modelScreen, 0, 0, 0, openGL.splashScreenDistance);
				Matrix.scaleM(modelScreen, 0, openGL.screenScale, openGL.screenScale, 1.0f);

				float mAngle = 180.0f * (float)((System.currentTimeMillis() % 2000) / 2000.0f);
				if (mAngle > 90.0f) mAngle += 180.0f;
				Matrix.rotateM(modelScreen, 0, mAngle, 0.0f, 1.0f, 0.0f);
			}
			else if (Natives.gameState() != 0)
			{
				//Drawing Virtual Screen
				Matrix.translateM(modelScreen, 0, 0, 0, openGL.screenDistance);
				Matrix.scaleM(modelScreen, 0, openGL.screenScale, openGL.screenScale, 1.0f);
			}
			else
			{
				float screenDist = openGL.gameScreenDistance;
				float f = (hmdPitch / 90.0f);
				if (f > 0.125f)
					screenDist *= (1.0f + (f - 0.125f) * 2.0f);

				//In Game
				Matrix.translateM(modelScreen, 0, (float)(Math.sin(M_PI * (hmdYaw / 180f))) * screenDist, 0,
						(float)(Math.cos(M_PI * (hmdYaw / 180f))) * screenDist);
				Matrix.rotateM(modelScreen, 0, hmdYaw, 0.0f, 1.0f, 0.0f);
				Matrix.scaleM(modelScreen, 0, openGL.screenScale, openGL.screenScale, openGL.screenScale);
			}

			float projectionTransposed[] = new float[16];
			Matrix.transposeM(projectionTransposed, 0, projection, 0);

			float eyeViewTransposed[] = new float[16];
			if (Natives.gameState() != 0 || mShowingSpashScreen) {
				Matrix.transposeM(eyeViewTransposed, 0, eyeView, 0);
				Matrix.multiplyMM(openGL.modelView, 0, eyeViewTransposed, 0, modelScreen, 0);
				Matrix.multiplyMM(openGL.modelViewProjection, 0, projectionTransposed, 0, openGL.modelView, 0);
				GLES20.glVertexAttribPointer(openGL.positionParam, 3, GLES20.GL_FLOAT, false, 0, openGL.screenVertices);
			}
			else {
				Matrix.transposeM(eyeViewTransposed, 0, centerEyeView, 0);
				Matrix.multiplyMM(openGL.modelView, 0, eyeViewTransposed, 0, modelScreen, 0);
				Matrix.multiplyMM(openGL.modelViewProjection, 0, projectionTransposed, 0, openGL.modelView, 0);
				GLES20.glVertexAttribPointer(openGL.positionParam, 3, GLES20.GL_FLOAT, false, 0, openGL.gameScreenVertices);
			}

			// Prepare the texturecoordinates
			GLES20.glVertexAttribPointer(openGL.texCoordParam, 2, GLES20.GL_FLOAT, false, 0, openGL.uvBuffer);

			// Apply the projection and view transformation
			GLES20.glUniformMatrix4fv(openGL.modelViewProjectionParam, 1, false, openGL.modelViewProjection, 0);

			// Bind texture to fbo's color texture
			GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
			IntBuffer activeTex0 = IntBuffer.allocate(2);
			GLES20.glGetIntegerv(GLES20.GL_TEXTURE_BINDING_2D, activeTex0);

			if (mShowingSpashScreen) {
				GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, splashTexture[0]);
			}
			else  {
				//Actually Draw Doom
				Natives.DoomDrawEye(eye, openGL.fbo.ColorTexture[0]);
				GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, openGL.fbo.ColorTexture[0]);
			}

			// Set the sampler texture unit to our fbo's color texture
			GLES20.glUniform1i(openGL.samplerParam, 0);

			// Draw the triangles
			GLES20.glDrawElements(GLES20.GL_TRIANGLES, 6, GLES20.GL_UNSIGNED_SHORT, openGL.listBuffer);
			GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, activeTex0.get(0));
		}
	}

	public void onFinishFrame() {
		if (!mSurfaceCreated)
			return;

		if (mDVRInitialised) {
			Natives.DoomEndFrame();
		}
	}

	private void dismissSplashScreen()
	{
		if (mShowingSpashScreen) {
			mShowingSpashScreen = false;
			mWADChooser.Initialise(this);
		}
	}

	@Override public boolean dispatchKeyEvent( KeyEvent event )
	{
		int keyCode = event.getKeyCode();
		int action = event.getAction();

		//Following buttons must not be handled here
		if (keyCode == KeyEvent.KEYCODE_VOLUME_UP ||
				keyCode == KeyEvent.KEYCODE_VOLUME_DOWN
				)
			return false;

		if (!mShowingSpashScreen &&
				mWADChooser.choosingWAD())
		{
			if (action == KeyEvent.ACTION_UP &&
					keyCode == KeyEvent.KEYCODE_BUTTON_A) {
				if (hmdYaw > 15.0f)
					mWADChooser.MoveNext();
				else if (hmdYaw < -15.0f)
					mWADChooser.MovePrev();
				else
					mWADChooser.SelectWAD();
			}
			else if (action == KeyEvent.ACTION_UP &&
					keyCode == KeyEvent.KEYCODE_DPAD_LEFT) {
				mWADChooser.MovePrev();
			}
			else if (action == KeyEvent.ACTION_UP &&
					keyCode == KeyEvent.KEYCODE_DPAD_RIGHT) {
				mWADChooser.MoveNext();
			}

			return true;
		}

		if ( action != KeyEvent.ACTION_DOWN && action != KeyEvent.ACTION_UP )
		{
			return super.dispatchKeyEvent( event );
		}
		if ( action == KeyEvent.ACTION_UP )
		{
			dismissSplashScreen();
		}

		//Allow user to switch vr mode by holding the start button down
		if (keyCode == KeyEvent.KEYCODE_BUTTON_START)
		{
			if (action == KeyEvent.ACTION_DOWN &&
					startButtonDownCounter == -1)
			{
				startButtonDownCounter = System.currentTimeMillis();

			}
			else if (action == KeyEvent.ACTION_UP)
			{
				startButtonDownCounter = -1;
				gameMenu();
			}
		}

		if (startButtonDownCounter != -1)
		{
			if ((System.currentTimeMillis() - startButtonDownCounter) > 2000)
			{
				startButtonDownCounter = -1;
				//Now make sure dvr is aware!

			}
		}

		if (mDVRInitialised) {
			if (action == KeyEvent.ACTION_DOWN) {
				Natives.onKeyEvent(mNativeHandle, DoomTools.keyCodeToKeySym(keyCode), Natives.EV_KEYDOWN
						, 0);
			}
			else
			{
				Natives.onKeyEvent(mNativeHandle, DoomTools.keyCodeToKeySym(keyCode), Natives.EV_KEYUP
						, 0);
			}
		}

		return true;
	}

	/**
	 * Show the menu
	 */
	private void gameMenu() {
		Natives.onKeyEvent(mNativeHandle, DoomTools.KEY_ESCAPE, Natives.EV_KEYDOWN, 0);
		Natives.onKeyEvent(mNativeHandle, DoomTools.KEY_ESCAPE, Natives.EV_KEYUP, 0);
	}

	private static float getCenteredAxis(MotionEvent event,
										 int axis) {
		final InputDevice.MotionRange range = event.getDevice().getMotionRange(axis, event.getSource());
		if (range != null) {
			final float flat = range.getFlat();
			final float value = event.getAxisValue(axis);
			if (Math.abs(value) > flat) {
				return value;
			}
		}
		return 0;
	}


	//Save the game pad type once known:
	// 1 - Generic BT gamepad
	// 2 - Samsung gamepad that uses different axes for right stick
	int gamepadType = 0;
	int lTrigAction = KeyEvent.ACTION_UP;
	int rTrigAction = KeyEvent.ACTION_UP;

	@Override
	public boolean onGenericMotionEvent(MotionEvent event) {
		int source = event.getSource();
		int action = event.getAction();
		if ((source==InputDevice.SOURCE_JOYSTICK)||(event.getSource()==InputDevice.SOURCE_GAMEPAD))
		{
			if (event.getAction() == MotionEvent.ACTION_MOVE)
			{
				float z = getCenteredAxis(event, MotionEvent.AXIS_Z);
				float rz = -getCenteredAxis(event, MotionEvent.AXIS_RZ);
				//For the samsung game pad (uses different axes for the second stick)
				float rx = getCenteredAxis(event, MotionEvent.AXIS_RX);
				float ry = -getCenteredAxis(event, MotionEvent.AXIS_RY);

				//let's figure it out
				if (gamepadType == 0)
				{
					if (z != 0.0f || rz != 0.0f)
						gamepadType = 1;
					else if (rx != 0.0f || ry != 0.0f)
						gamepadType = 2;
				}

				switch (gamepadType)
				{
					case 0:
						break;
					case 1:
						Natives.onMotionEvent(mNativeHandle, source, action, (int) (z * 30), 0);
						break;
					case 2:
						Natives.onMotionEvent(mNativeHandle, source, action, (int) (rx * 30), 0);
						break;
				}

				//Fire weapon using shoulder trigger
				float axisRTrigger = max(event.getAxisValue(MotionEvent.AXIS_RTRIGGER),
						event.getAxisValue(MotionEvent.AXIS_GAS));
				int newRTrig = axisRTrigger > 0.6 ? Natives.EV_KEYDOWN : Natives.EV_KEYUP;
				if (rTrigAction != newRTrig)
				{
					Natives.onKeyEvent(mNativeHandle, DoomTools.KEY_RCTRL, newRTrig, 0);
					rTrigAction = newRTrig;
				}

				//Run using L shoulder
				float axisLTrigger = max(event.getAxisValue(MotionEvent.AXIS_LTRIGGER),
						event.getAxisValue(MotionEvent.AXIS_BRAKE));
				int newLTrig = axisLTrigger > 0.6 ? Natives.EV_KEYDOWN : Natives.EV_KEYUP;
				if (lTrigAction != newLTrig)
				{
					Natives.onKeyEvent(mNativeHandle, DoomTools.KEY_RSHIFT, newLTrig, 0);
					lTrigAction = newLTrig;
				}
			}
		}
		return false;
	}

	private float max(float axisValue, float axisValue2) {
		return (axisValue > axisValue2) ? axisValue : axisValue2;
	}

	@Override public boolean dispatchTouchEvent( MotionEvent event )
	{
		if ( mNativeHandle != 0 )
		{
			int source = event.getSource();
			int action = event.getAction();			
			float x = event.getRawX();
			float y = event.getRawY();
			if ( action == MotionEvent.ACTION_UP )
			{
				Log.v( TAG, "GLES3JNIActivity::dispatchTouchEvent( " + action + ", " + x + ", " + y + " )" );
			}
			Natives.onTouchEvent( mNativeHandle, source, action, x, y );
		}
		return true;
	}
	@Override
	public void OnQuit(int code) {
		try {
			Thread.sleep(500);
		}
		catch (InterruptedException ie){
		}

		System.exit(0);
	}

	/**
	 * Fires when there an image update from Doom lib
	 */
	@Override
	public void OnImageUpdate(int[] pixels, int eye) {
		//No longer used
	}

	/**
	 * Fires on LIB message
	 */
	@Override
	public void OnMessage(String text) {
		Log.d(TAG, "**Doom Message:  " + text);
	}

	@Override
	public void OnInfoMessage(String msg, final int type) {
		Log.i(TAG, "**Doom Message:  " + msg);
	}

	@Override
	public void OnInitGraphics(int w, int h) {
		Log.d(TAG, "OnInitGraphics creating Bitmap of " + w + " by " + h);
		mDoomWidth = w;
		mDoomHeight = h;
		openGL.CreateFBO(openGL.fbo, mDoomWidth, mDoomHeight);
	}

	@Override
	public void OnFatalError(final String text) {
		Log.e(TAG, "ERROR: " + text);
	}

	@Override
	public void OnStartSound(String name, int vol) {
		if (mAudioMgr == null) {
			Log.e(TAG, "Bug: Audio Mgr is NULL but sound is enabled!");
			return;
		}

		try {
			if (mAudioMgr != null)
				mAudioMgr.startSound(name, vol);

		} catch (Exception e) {
			Log.e(TAG, "OnStartSound: " + e.toString());
		}
	}

	/**
	 * Fires on background music
	 */
	@Override
	public void OnStartMusic(String name, int loop) {
		if (mAudioMgr != null)
			mAudioMgr.startMusic(GLES3JNIActivity.this, name, loop);
	}

	/**
	 * Stop bg music
	 */
	@Override
	public void OnStopMusic(String name) {
		if (mAudioMgr != null)
			mAudioMgr.stopMusic(name);
	}

	@Override
	public void OnSetMusicVolume(int volume) {
		if (mAudioMgr != null)
			mAudioMgr.setMusicVolume(volume);
	}
}
