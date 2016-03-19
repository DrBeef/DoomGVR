package doom.audio;

import java.io.File;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;

import android.content.Context;
import android.net.Uri;
import android.util.Log;
import doom.util.DoomTools;

/**
 * Audio manager. Caches sounds for performance
 * @author vsilva
 *
 */
public class AudioManager 
{
	static final String TAG = "AudioMgr";
	public static final int MAX_CLIPS = 25;
	
	static private AudioManager am ;

	// Game sound (WAVs)
	private volatile HashMap<String, List<AudioClip>> mSounds = new HashMap<String, List<AudioClip>>();
	private Context mContext;
	private int mClipCount = 0;
	private boolean mPaused = false;
	private int mMusicVolume = 0;

	// BG music
	private AudioClip music;
	
	/**
	 * get Instance
	 * @param ctx
	 * @return
	 */
	static public AudioManager getInstance(Context ctx) {
		if ( am == null) return new AudioManager(ctx);
		return am;
	}

	private AudioManager(Context ctx) {
		mContext = ctx;
	}

	static public float getLogVolume(int volume) {
		return 1.0f - (float)(Math.log(101-volume)/Math.log(101));
	}

	/**
	 * Start a sound by name & volume
	 * @param name example "pistol" when firing the gun
	 * @param vol
	 */
	public synchronized void startSound( String name, int vol) 
	{ 
		if(mPaused)
			return;
		// The sound key as stored in the FS -> DS[NAME-UCASE].wav
		//String key = "DS" + name.toUpperCase() + ".wav";
		String key = name + ".wav";

		AudioClip clip = null;
		if ( mSounds.containsKey(key)) {
			//Log.d(TAG, "Playing " + key + " from cache  vol:" + vol);
			List<AudioClip> clipList = mSounds.get(key);

			for (AudioClip c: clipList) {
				if (!c.isPlaying())
				{
					clip = c;
					break;
				}
			}

			if (clip == null &&
					clipList.size() >= 2)
			{
				//Limit to 2 of the same sound
				return;
			}
		}
		else
		{
			List<AudioClip> clipList = new LinkedList<AudioClip>();
			mSounds.put(key, clipList);
		}

		if (clip == null)
		{
			// load clip from disk
			File folder = DoomTools.GetSoundFolder(); //DoomTools.DOOM_WADS[mWadIdx]);
			File sound = new File(folder.getAbsolutePath() + File.separator + key);
			
			if ( ! sound.exists()) {
				//key = "DS" + name.toUpperCase() + ".wav";
				key = name + ".wav";
				sound = new File(folder.getAbsolutePath() + File.separator + key);
				if(!sound.exists()) {
					Log.e(TAG, sound + " not found.");
					return;
				}
			}

			AudioClip newClip = new AudioClip(mContext, Uri.fromFile(sound));
			newClip.play(vol);

			List<AudioClip> clipList = mSounds.get(key);
			clipList.add(newClip);
			mClipCount++;
		}
		else
		{
			clip.play(vol);
		}

		// If the sound table is full remove a random entry
		boolean firstLoop = true;
		while ( mClipCount > MAX_CLIPS) {
			for (Map.Entry<String, List<AudioClip>> entry : mSounds.entrySet()) {
				if (entry.getValue().size() > 1 || !firstLoop) {
					for (AudioClip c : entry.getValue()) {
						if (!c.isPlaying()) {
							if (entry.getValue().remove(c)) {
								c.release();
								c = null;
								mClipCount--;
								//Done, move to next list
								break;
							}
						}
					}
				}
			}
			firstLoop = false;
		}
	}

	/**
	 * Start background music
	 * @param ctx
	 * @param key music key (e.g intro, e1m1)
	 */
	public void startMusic (Context ctx , String key, int loop) {
		if(mPaused)
			return;
		// Sound file
		File sound = new File(key);

		if ( !sound.exists()) {
			Log.e(TAG, "Unable to find music " + sound);
			return;
		}
		
		if ( music != null) {
			music.stop();
			music.release();
		}
		
		Log.d(TAG, "Starting music " + sound + " loop=" + loop);
		music = new AudioClip(ctx, Uri.fromFile( sound ));
		
		music.setVolume(mMusicVolume);
		if (loop != 0)
			music.loop();
		music.play();
	}

	/**
	 * Stop background music
	 * @param key
	 */
	public void stopMusic (String key) {
		music.stop();
		music.release();
		music = null;
	}

	public void setMusicVolume (int vol) {
		//Store
		mMusicVolume = vol;
		if ( music != null) {
			music.setVolume(mMusicVolume);
		}
		else
			Log.e(TAG, "setMusicVolume " + vol + " called with NULL music player");
	}
	
	public void pauseAudioMgr(boolean pause) {
		mPaused = pause;
	}
	
}
