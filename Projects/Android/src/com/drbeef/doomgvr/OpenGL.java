package com.drbeef.doomgvr;


import android.graphics.Bitmap;
import android.opengl.GLES11Ext;
import android.opengl.GLES20;
import android.opengl.GLUtils;
import android.opengl.Matrix;
import android.util.Log;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;
import java.nio.ShortBuffer;

public class OpenGL {

	private static final int GL_RGBA8 = 0x8058;
	Bitmap mDoomBitmap;

	public void SetBitmap(Bitmap bmp)
	{
		mDoomBitmap = bmp;
	}

	public void onCreate() {
		view = new float[16];
		modelViewProjection = new float[16];
		modelView = new float[16];
	}

	public void onSurfaceCreated() {
		Log.i("DVR", "onSurfaceCreated");

		ByteBuffer bbVertices = ByteBuffer.allocateDirect(SCREEN_COORDS.length * 4);
		bbVertices.order(ByteOrder.nativeOrder());
		screenVertices = bbVertices.asFloatBuffer();
		screenVertices.put(SCREEN_COORDS);
		screenVertices.position(0);


		ByteBuffer bbVertices2 = ByteBuffer.allocateDirect(GAME_SCREEN_COORDS.length * 4);
		bbVertices2.order(ByteOrder.nativeOrder());
		gameScreenVertices = bbVertices2.asFloatBuffer();
		gameScreenVertices.put(GAME_SCREEN_COORDS);
		gameScreenVertices.position(0);

		// initialize byte buffer for the draw list
		ByteBuffer dlb = ByteBuffer.allocateDirect(indices.length * 2);
		dlb.order(ByteOrder.nativeOrder());
		listBuffer = dlb.asShortBuffer();
		listBuffer.put(indices);
		listBuffer.position(0);

		// Create the shaders, images
		int vertexShader = loadShader(GLES20.GL_VERTEX_SHADER, vs_Image);
		int fragmentShader = loadShader(GLES20.GL_FRAGMENT_SHADER, fs_Image);

		sp_Image = GLES20.glCreateProgram();             // create empty OpenGL ES Program
		GLES20.glAttachShader(sp_Image, vertexShader);   // add the vertex shader to program
		GLES20.glAttachShader(sp_Image, fragmentShader); // add the fragment shader to program
		GLES20.glLinkProgram(sp_Image);                  // creates OpenGL ES program executable

		positionParam = GLES20.glGetAttribLocation(sp_Image, "a_Position");
		texCoordParam = GLES20.glGetAttribLocation(sp_Image, "a_texCoord");
		modelViewProjectionParam = GLES20.glGetUniformLocation(sp_Image, "u_MVPMatrix");
		samplerParam = GLES20.glGetUniformLocation(sp_Image, "s_texture");


		GLES20.glEnableVertexAttribArray(positionParam);
		GLES20.glEnableVertexAttribArray(texCoordParam);
	}

	void CopyBitmapToTexture(Bitmap bmp, int textureUnit)
	{
		// Bind texture to texturename
		GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
		GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, textureUnit);

		// Set filtering
		GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MIN_FILTER, GLES20.GL_LINEAR);
		GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MAG_FILTER, GLES20.GL_LINEAR);

		// Set wrapping mode
		GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_WRAP_S, GLES20.GL_CLAMP_TO_EDGE);
		GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_WRAP_T, GLES20.GL_CLAMP_TO_EDGE);

		// Load the bitmap into the bound texture.
		GLUtils.texImage2D(GLES20.GL_TEXTURE_2D, 0, bmp, 0);

		//unbind
		GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, 0);
	}

	public void SetupUVCoords()
	{
		// The texture buffer
		ByteBuffer bb = ByteBuffer.allocateDirect(uvs.length * 4);
		bb.order(ByteOrder.nativeOrder());
		uvBuffer = bb.asFloatBuffer();
		uvBuffer.put(uvs);
		uvBuffer.position(0);
	}

	public void SetupTriangle(int x, int y, int width, int height)
	{
		// We have to create the vertices of our triangle.
		vertices = new float[]
				{
						x, y, 0.0f,
						x, y + height, 0.0f,
						x + width, y + height, 0.0f,
						x + width, y, 0.0f,
				};

		// The vertex buffer.
		ByteBuffer bb = ByteBuffer.allocateDirect(vertices.length * 4);
		bb.order(ByteOrder.nativeOrder());
		vertexBuffer = bb.asFloatBuffer();
		vertexBuffer.put(vertices);
		vertexBuffer.position(0);
	}

	public FloatBuffer screenVertices;
	public FloatBuffer gameScreenVertices;

	public int positionParam;
	public int texCoordParam;
	public int samplerParam;
	public int modelViewProjectionParam;

	public float[] view;
	public float[] modelViewProjection;
	public float[] modelView;

	public float screenDistance = -8f;
	public float splashScreenDistance = -12f;
	public float gameScreenDistance = -3.5f;
	public float screenScale = 3f;

	public static final String vs_Image =
			"uniform mat4 u_MVPMatrix;" +
					"attribute vec4 a_Position;" +
					"attribute vec2 a_texCoord;" +
					"varying vec2 v_texCoord;" +
					"void main() {" +
					"  gl_Position = u_MVPMatrix * a_Position;" +
					"  v_texCoord = a_texCoord;" +
					"}";


	public static final String fs_Image =
			"precision mediump float;" +
					"varying vec2 v_texCoord;" +
					"uniform sampler2D s_texture;" +
					"void main() {" +
					"  gl_FragColor = texture2D( s_texture, v_texCoord );" +
					"}";

	public static int loadShader(int type, String shaderCode){
		int shader = GLES20.glCreateShader(type);
		GLES20.glShaderSource(shader, shaderCode);
		GLES20.glCompileShader(shader);
		return shader;
	}
	//FBO render eye buffer
	public DVRFBO fbo = new DVRFBO();


	boolean CreateFBO(DVRFBO fbo, int width, int height)
	{
		Log.d("DVR", "CreateFBO");
		// Create the color buffer texture.
		GLES20.glGenTextures(1, fbo.ColorTexture, 0);
		GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, fbo.ColorTexture[0]);
		GLES20.glTexImage2D(GLES20.GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GLES20.GL_RGBA, GLES20.GL_UNSIGNED_BYTE, null);
		GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_WRAP_S, GLES20.GL_CLAMP_TO_EDGE);
		GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_WRAP_T, GLES20.GL_CLAMP_TO_EDGE);
		GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MIN_FILTER, GLES20.GL_LINEAR);
		GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MAG_FILTER, GLES20.GL_LINEAR);
		GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, 0);

		// Create depth buffer.
		GLES20.glGenRenderbuffers(1, fbo.DepthBuffer, 0);
		GLES20.glBindRenderbuffer(GLES20.GL_RENDERBUFFER, fbo.DepthBuffer[0]);
		GLES20.glRenderbufferStorage(GLES20.GL_RENDERBUFFER, GLES11Ext.GL_DEPTH_COMPONENT24_OES, width, height);
		GLES20.glBindRenderbuffer(GLES20.GL_RENDERBUFFER, 0);

		// Create the frame buffer.
		GLES20.glGenFramebuffers(1, fbo.FrameBuffer, 0);
		GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, fbo.FrameBuffer[0]);
		GLES20.glFramebufferRenderbuffer(GLES20.GL_FRAMEBUFFER, GLES20.GL_DEPTH_ATTACHMENT, GLES20.GL_RENDERBUFFER, fbo.DepthBuffer[0]);
		GLES20.glFramebufferTexture2D(GLES20.GL_FRAMEBUFFER, GLES20.GL_COLOR_ATTACHMENT0, GLES20.GL_TEXTURE_2D, fbo.ColorTexture[0], 0);
		int renderFramebufferStatus = GLES20.glCheckFramebufferStatus(GLES20.GL_FRAMEBUFFER);
		GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, 0);
		if ( renderFramebufferStatus != GLES20.GL_FRAMEBUFFER_COMPLETE )
		{
			Log.d("DVR", "Incomplete frame buffer object!!");
			return false;
		}

		fbo.width = width;
		fbo.height = height;

		return true;
	}

	void DestroyFBO(DVRFBO fbo)
	{
		GLES20.glDeleteFramebuffers( 1, fbo.FrameBuffer, 0 );
		fbo.FrameBuffer[0] = 0;
		GLES20.glDeleteRenderbuffers( 1, fbo.DepthBuffer, 0 );
		fbo.DepthBuffer[0] = 0;
		GLES20.glDeleteTextures( 1, fbo.ColorTexture, 0 );
		fbo.ColorTexture[0] = 0;
		fbo.width = 0;
		fbo.height = 0;
	}

	// Geometric variables
	public static float vertices[];
	public static final short[] indices = new short[] {0, 1, 2, 0, 2, 3};
	public static final float uvs[] =  new float[] {
			0.0f, 1.0f,
			0.0f, 0.0f,
			1.0f, 0.0f,
			1.0f, 1.0f
	};

	public static final float[] SCREEN_COORDS = new float[] {
			-1.3f, -1.0f, 0.0f,
			-1.3f, 1.0f, 0.0f,
			1.3f, 1.0f, 0.0f,
			1.3f, -1.0f, 0.0f
	};

	public static final float[] GAME_SCREEN_COORDS = new float[] {
			-1.0f, -1.0f, 0.0f,
			-1.0f, 1.0f, 0.0f,
			1.0f, 1.0f, 0.0f,
			1.0f, -1.0f, 0.0f
	};

	public FloatBuffer vertexBuffer;
	public ShortBuffer listBuffer;
	public FloatBuffer uvBuffer;

	//Shader Program
	public static int sp_Image;
}
