package com.drbeef.doomgvr;

import android.app.ProgressDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.os.AsyncTask;
import android.os.SystemClock;
import android.util.Log;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.net.URL;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

import doom.util.DoomTools;


class DownloadTask extends AsyncTask<Void, String, Void> {
	
	private Context context;
	public boolean downloading = false;

	private static String status;

	public synchronized String getStatusString()
	{
		return status;
	}
	
	public boolean please_abort = false;

	private String url = "https://github.com/freedoom/freedoom/releases/download/v0.10.1/freedoom-0.10.1.zip";
	private String freedoomZip = DoomTools.GetDVRFolder() + "/freedoom-0.10.1.zip";

	//OLD Freedom
	//private String url = "https://www.dropbox.com/s/whngowjwpkwjht2/freedoom.zip?dl=1";
	//private String freedoomZip = DoomTools.GetDVRFolder() + "/freedoom.zip";
	//private String wadfile = DoomTools.GetDVRFolder() + "/freedoom.wad";

	public DownloadTask set_context(Context context){
		this.context = context;
		return this;
	}
	
	@Override
	protected void  onPreExecute  (){

		status = "Downloading";
		downloading = true;
	}
	
	
	public static String printSize( int size ){
		
		if ( size >= (1<<20) )
			return String.format("%.1f MB", size * (1.0/(1<<20)));
		
		if ( size >= (1<<10) )
			return String.format("%.1f KB", size * (1.0/(1<<10)));
		
		return String.format("%d bytes", size);
		
	}

	
	private void download_demo() throws Exception{
		
		Log.i( "DownloadTask.java", "starting to download "+ url);
		
	    if (new File(freedoomZip).exists()){
	    	Log.i( "DownloadTask.java", freedoomZip + " already there. skipping.");
	    	return;
	    }

		/// setup output directory		
		new File(DoomTools.GetDVRFolder()).mkdirs();

       	InputStream     is = null;
    	FileOutputStream        fos = null;
    		    		
		is = new URL(url).openStream();
    	fos = new FileOutputStream ( freedoomZip +".part");

    	byte[]  buffer = new byte [4096];
    	
    	int totalcount =0;
    	
    	long tprint = SystemClock.uptimeMillis();
    	int partialcount = 0;
    	
    	while(true){
    		
    		 if (please_abort)
    			 throw new Exception("aborting") ;	    	    	
	    	 
    		
    		int count = is.read (buffer);

    	    if ( count<=0 ) break;
    	    fos.write (buffer, 0, count);
    	    
    	    totalcount += count;
    	    partialcount += count;
    	    
    	    long tnow =  SystemClock.uptimeMillis();
    	    if ( (tnow-tprint)> 1000) {
    	    	
    	    	float size_MB = totalcount * (1.0f/(1<<20));

    	    	publishProgress( String.format("downloaded: %.2f MB",
    	    			size_MB));

    	    	tprint = tnow;
    	    	partialcount = 0;
    	    	
    	    }
    	       	    
    	   
    	}
    	
    	is.close();
    	fos.close();
    	

    	new File(freedoomZip +".part" )
    		.renameTo(new File(freedoomZip));
    	
    	// done
    	publishProgress("download done");
    	
		SystemClock.sleep(2000);
    	
	}
	
	private void extract_data() throws Exception{
		Log.i("DownloadTask.java", "extracting WAD data");

		ZipFile file = new ZipFile  (freedoomZip);
		//extract_file( file, "freedoom-0.10.1\\COPYING", DoomTools.GetDVRFolder() + File.separator + "COPYING.txt");
		//extract_file( file, "freedoom-0.10.1\\CREDITS", DoomTools.GetDVRFolder() + File.separator + "CREDITS.txt");
		publishProgress("extracting: freedoom1.wad");
		extract_file(file, "freedoom-0.10.1/freedoom1.wad", DoomTools.GetDVRFolder() + File.separator + "freedoom1.wad");
		publishProgress("extracting: freedoom2.wad");
		extract_file(file, "freedoom-0.10.1/freedoom2.wad", DoomTools.GetDVRFolder() + File.separator + "freedoom2.wad");
		publishProgress("extracting: README.html");
		extract_file(file, "freedoom-0.10.1/README.html", DoomTools.GetDVRFolder() + File.separator + "README.html");

    	file.close();
    	
    	// done
    	publishProgress("extract done");
		SystemClock.sleep(2000);
	}

	private void extract_file( ZipFile file, String entry_name, String output_name ) throws Exception{

		Log.i( "DownloadTask.java", "extracting " + entry_name + " to " + output_name);

		String short_name = new File(output_name).getName();

	    // create output directory
		publishProgress("Extracting Freedoom Zip" );
		new File(output_name).getParentFile().mkdirs();

		ZipEntry entry = file.getEntry(entry_name);

		if ( entry.isDirectory() ){
			Log.i( "DownloadTask.java", entry_name + " is a directory");
			new File(output_name).mkdir();
			return;
		}


       	InputStream is = null;
    	FileOutputStream  fos = null;

		is = file.getInputStream(entry);

    	fos = new FileOutputStream ( output_name+".part" );

    	byte[]  buffer = new byte [4096];

    	int totalcount =0;

    	long tprint = SystemClock.uptimeMillis();

    	while(true){

    		 if (please_abort)
    			 throw new Exception("aborting") ;


    		int count = is.read (buffer);
    		//Log.i( "DownloadTask.java", "extracted " + count + " bytes");

    	    if ( count<=0 ) break;
    	    fos.write (buffer, 0, count);

    	    totalcount += count;

    	    long tnow =  SystemClock.uptimeMillis();
    	    if ( (tnow-tprint)> 1000) {

    	    	float size_MB = totalcount * (1.0f/(1<<20));

    	    	publishProgress( String.format("%s : extracted %.1f MB",
    	    			short_name, size_MB));

    	    	tprint = tnow;
    	    }
    	}

    	is.close();
    	fos.close();

    	/// rename part file

    	new File(output_name+".part" )
    		.renameTo(new File(output_name));

    	// done
    	publishProgress( String.format("%s : done.",
    			short_name));
	}

	@Override
	protected Void doInBackground(Void... unused) {
		
    	try {
    		long t = SystemClock.uptimeMillis();

    		download_demo();
    		
    		extract_data();   		
    		
    		t = SystemClock.uptimeMillis() - t;

    		Log.i( "DownloadTask.java", "done in " + t + " ms");
	    	
    	} catch (Exception e) {

			e.printStackTrace();

			publishProgress("Error: " + e );
		}
    	
		return(null);
	}
	
	@Override
	protected void onProgressUpdate(String... progress) {
		Log.i( "DownloadTask.java", progress[0]);
		status = progress[0];
	}
	
	@Override
	protected void onPostExecute(Void unused) {

	}
}
