package doom.audio;

import android.content.Context;
import android.media.MediaPlayer;
import android.net.Uri;

/**
 * AudioClip
 * @author Owner
 *
 */
public class AudioClip
{
	static final String TAG = "AudioClip";
	
	private MediaPlayer mPlayer;
	private String name;
	
	private boolean mPlaying = false;
	private boolean mLoop = false;
	
	public AudioClip(Context ctx, int resID) {
		name = ctx.getResources().getResourceName(resID);
		
		mPlayer = MediaPlayer.create(ctx, resID);
		mPlayer.setOnCompletionListener(new MediaPlayer.OnCompletionListener(){

			@Override
			public void onCompletion(MediaPlayer mp) {
				mPlaying = false;
				if ( mLoop) {
					mp.start();
				}
			}
			
		});
	}

	public AudioClip(Context ctx, Uri uri) {
		name = uri.toString();
		
		mPlayer = MediaPlayer.create(ctx, uri);
		mPlayer.setOnCompletionListener(new MediaPlayer.OnCompletionListener(){

			@Override
			public void onCompletion(MediaPlayer mp) {
				mPlaying = false;
				if ( mLoop) {
					mp.start();
				}
			}
			
		});
	}

	public synchronized boolean isPlaying() {
		return mPlaying;
	}

	//For music
	public synchronized void play () {
		if (mPlaying) {
			mPlayer.seekTo(0);
			return;
		}

		if (mPlayer != null ) {
			mPlaying = true;
			mPlayer.start();
		}
	}

	//For SFX
	public synchronized void play (int vol) {
		mPlayer.seekTo(0);

		if (mPlayer != null ) {
			mPlaying = true;
			
			//Log.d(TAG, "Play " + name + " vol=" + vol);
			setVolume(vol);
			mPlayer.start();

		}
	}

	
	public synchronized void stop() {
		try {
			mLoop = false;
			if ( mPlaying ) { 
				mPlaying = false;
				mPlayer.pause();
			}
			
		} catch (Exception e) {
			System.err.println("AduioClip::stop " + name + " " + e.toString());
		}
	}
	
	public synchronized void loop () {
		mLoop = true;
		//mPlaying = true;
		//mPlayer.start();
		mPlayer.setLooping(true);
	}
	
	public void release () {
		if (mPlayer != null) { 
			mPlayer.release();
			mPlayer = null;
		}
	}
	
	public String getName() {
		return name;
	}
	
	/**
	 * Set volume
	 * @param vol (1-100)
	 */
	public void setVolume (int vol) {
		if ( mPlayer != null) {
			if (vol > 100)
				vol = 100;
			float log1 = AudioManager.getLogVolume(vol);
			mPlayer.setVolume(log1, log1);
		}
	}
}
