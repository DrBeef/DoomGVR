package com.drbeef.doomgvr;

import android.content.Context;
import android.content.res.AssetManager;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.Typeface;
import android.opengl.GLES20;
import android.opengl.Matrix;

import java.io.File;
import java.io.InputStream;
import java.nio.IntBuffer;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import doom.util.DoomTools;

/**
 * Created by Simon on 04/03/2016.
 */
public class WADChooser {

	OpenGL openGL = null;
	List<String> wads = new ArrayList<String>();
	Map<String, String> wadThumbnails = new HashMap<String, String>();
	Typeface type;
	private boolean initialised = false;

	enum Transition
	{
		ready,
		move_left,
		moving_left,
		move_right,
		moving_right
	};

	Transition mCurrentTransition = Transition.ready;
	long mTransitionStart = -1;

	private int selectedWAD = 0;

	WADChooser(OpenGL openGL) {
		this.openGL = openGL;
	}

	public void Initialise(AssetManager assets)
	{
		wadThumbnails.put(new String("doom.wad"), new String("d1.png"));
		wadThumbnails.put(new String("doom2.wad"), new String("d2.png"));
		wadThumbnails.put(new String("freedoom.wad"), new String("fd.png"));
		wadThumbnails.put(new String("freedoom1.wad"), new String("fd1.png"));
		wadThumbnails.put(new String("freedoom2.wad"), new String("fd2.png"));

		type = Typeface.createFromAsset(assets, "fonts/DooM.ttf");

		File[] files = new File(DoomTools.GetDVRFolder()).listFiles();

		for (File file : files) {
			if (file.isFile() &&
					file.getName().toUpperCase().compareTo("PRBOOM.WAD") != 0 &&
					file.getName().toUpperCase().endsWith("WAD"))
				wads.add(file.getName());
		}

		//No point choosing a wad if there's only one!
		if (wads.size() == 1) mChoosingWAD = false;

		initialised = true;
	}

	private boolean mChoosingWAD = true;

	public boolean choosingWAD() {
		return mChoosingWAD;
	}

	public void SelectWAD()
	{
		mChoosingWAD = false;
	}

	public String GetSelectedWADName() {
		if (wads.isEmpty())
			return "NO WADS";

		return wads.get(selectedWAD);
	}

	public void MoveNext()
	{
		if (mCurrentTransition == Transition.ready) {
			mCurrentTransition = Transition.move_right;
			mTransitionStart = System.currentTimeMillis();
		}
	}

	public void MovePrev()
	{
		if (mCurrentTransition == Transition.ready) {
			mCurrentTransition = Transition.move_left;
			mTransitionStart = System.currentTimeMillis();
		}
	}

	void DrawWADName(Context ctx)
	{
// Create an empty, mutable bitmap
		Bitmap bitmap = Bitmap.createBitmap(256, 256, Bitmap.Config.ARGB_4444);

// get a canvas to paint over the bitmap
		Canvas canvas = new Canvas(bitmap);
		bitmap.eraseColor(0);

		Paint paint = new Paint();
		paint.setTextSize(20);
		paint.setTypeface(type);
		paint.setAntiAlias(true);
		paint.setARGB(0xff, 0xff, 0x20, 0x00);

		if (wadThumbnails.containsKey(GetSelectedWADName().toLowerCase())) {

			try {
				AssetManager assets = ctx.getAssets();
				InputStream in = assets.open("thumbnails/" + wadThumbnails.get(GetSelectedWADName().toLowerCase()));
				Bitmap thumbnail = BitmapFactory.decodeStream(in);
				in.close();

				canvas.drawBitmap(thumbnail, null, new Rect(36, 36, 218, 214), paint);
			} catch (Exception e) {
				e.printStackTrace();
			}

		}
		else
		{
			canvas.drawText("no thumbnail", 42, 114, paint);
		}

// Draw the text
		if (mCurrentTransition == Transition.ready) {
			canvas.drawText("Choose Wad", 32, 24, paint);
			canvas.drawText(GetSelectedWADName(), 32, 256, paint);

			//Draw arrows
			paint.setTextSize(36);
			paint.setARGB(0xff, 0x20, 0x20, 0xff);
			canvas.drawText("<", 0, 116, paint);
			canvas.drawText(">", 228, 116, paint);
		}

		openGL.CopyBitmapToTexture(bitmap, openGL.fbo.ColorTexture[0]);
	}

	public void onDrawEye(int eye, int width, int height, float[] eyeView, float[] projection, Context ctx) {

		if (!initialised)
			return;

		if (System.currentTimeMillis() - mTransitionStart > 250) {
			if (mCurrentTransition == Transition.move_right) {
				selectedWAD++;
				if (selectedWAD == wads.size())
					selectedWAD = 0;
				mCurrentTransition = Transition.moving_right;
			}
			if (mCurrentTransition == Transition.move_left) {
				selectedWAD--;
				if (selectedWAD < 0)
					selectedWAD = wads.size() - 1;
				mCurrentTransition = Transition.moving_left;
			}
		}

		if (System.currentTimeMillis() - mTransitionStart > 500)
		{
			mTransitionStart = -1;
			mCurrentTransition = Transition.ready;
		}


		DrawWADName(ctx);

		GLES20.glEnable(GLES20.GL_SCISSOR_TEST);
		GLES20.glScissor(0, 0,	width, height);
		GLES20.glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT | GLES20.GL_DEPTH_BUFFER_BIT);

		GLES20.glViewport(0, 0,	width, height);

		//Clear the viewport
		GLES20.glEnable(GLES20.GL_SCISSOR_TEST);
		GLES20.glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		GLES20.glScissor(0,	0,	width,	height);
		GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT);

		GLES20.glUseProgram(openGL.sp_Image);

		// Object first appears directly in front of user.
		float modelScreen[] = new float[16];
		Matrix.setIdentityM(modelScreen, 0);
		Matrix.scaleM(modelScreen, 0, openGL.screenScale, openGL.screenScale, 1.0f);
		Matrix.translateM(modelScreen, 0, 0, 0, openGL.screenDistance);

		if (mTransitionStart != -1) {
			long transVal = System.currentTimeMillis() - mTransitionStart;
			if (mCurrentTransition == Transition.moving_left ||
					mCurrentTransition == Transition.move_left)
				transVal = 500 - transVal;
			float mAngle = 180.0f * (float) (((float)transVal) / 500.0f);
			if (mAngle > 90.0f)
				mAngle += 180.0f;
			Matrix.rotateM(modelScreen, 0, mAngle, 0.0f, 1.0f, 0.0f);
		}

		float eyeViewTransposed[] = new float[16];
		Matrix.transposeM(eyeViewTransposed, 0, eyeView, 0);
		float projectionTransposed[] = new float[16];
		Matrix.transposeM(projectionTransposed, 0, projection, 0);

		Matrix.multiplyMM(openGL.modelView, 0, eyeViewTransposed, 0, modelScreen, 0);
		Matrix.multiplyMM(openGL.modelViewProjection, 0, projectionTransposed, 0, openGL.modelView, 0);
		GLES20.glVertexAttribPointer(openGL.positionParam, 3, GLES20.GL_FLOAT, false, 0, openGL.screenVertices);

		// Prepare the texturecoordinates
		GLES20.glVertexAttribPointer(openGL.texCoordParam, 2, GLES20.GL_FLOAT, false, 0, openGL.uvBuffer);

		// Apply the projection and view transformation
		GLES20.glUniformMatrix4fv(openGL.modelViewProjectionParam, 1, false, openGL.modelViewProjection, 0);

		// Bind texture to fbo's color texture
		GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
		IntBuffer activeTex0 = IntBuffer.allocate(2);
		GLES20.glGetIntegerv(GLES20.GL_TEXTURE_BINDING_2D, activeTex0);

		GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, openGL.fbo.ColorTexture[0]);

		// Set the sampler texture unit to our fbo's color texture
		GLES20.glUniform1i(openGL.samplerParam, 0);

		// Draw the triangles
		GLES20.glDrawElements(GLES20.GL_TRIANGLES, 6, GLES20.GL_UNSIGNED_SHORT, openGL.listBuffer);
		GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, activeTex0.get(0));
	}
}
