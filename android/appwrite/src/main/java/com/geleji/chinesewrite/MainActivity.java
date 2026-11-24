package com.geleji.chinesewrite;

import android.content.res.AssetManager;
import android.os.Bundle;
import android.util.Log;
import android.view.WindowManager;

import org.libsdl.app.SDLActivity;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

/**
 * SDL2 host activity for chinese write (character-writing practice).
 *
 * libSDL2.so provides the engine, libchinesewrite_sdl.so contains the app
 * (its SDL_main is picked up as the entry point because it is the last
 * library in getLibraries()).
 *
 * There is deliberately no keyboard plumbing here (unlike chinese_memo):
 * the app is operated entirely by touching the screen, so no EditText, no
 * IME keep-alive, no windowSoftInputMode.
 */
public class MainActivity extends SDLActivity
{
	private static final String TAG = "chinese_write";

	@Override
	protected String[] getLibraries()
	{
		return new String[] { "SDL2", "chinesewrite_sdl" };
	}

	@Override
	protected void onCreate(Bundle savedInstanceState)
	{
		// the native code chdir()s into getFilesDir() and then opens
		// "write_data.txt", "write_pinyin.txt", "union_q.txt" and
		// "NotoSansSC.ttf" with relative paths, so the assets must be
		// unpacked there first
		copyAssetsIfNeeded();

		getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

		super.onCreate(savedInstanceState);
	}

	// -----------------------------------------------------------------------
	// assets
	// -----------------------------------------------------------------------

	private void copyAssetsIfNeeded()
	{
		try
		{
			File root = getFilesDir();
			File marker = new File(root, ".assets_version");
			long apkTime = apkInstallTime();

			if(marker.exists() && readMarker(marker) == apkTime && sentinelIsThere(root))
			{
				Log.i(TAG, "assets already unpacked in " + root.getAbsolutePath());
				return;
			}

			copyAsset(root, "union_q.txt");
			copyAsset(root, "write_data.txt");
			copyAsset(root, "write_pinyin.txt");
			copyAsset(root, "NotoSansSC.ttf");
			// wstatus.txt is the app's own progress file: never shipped and
			// never clobbered (progress_pull.sh pushes a pulled copy manually)

			writeMarker(marker, apkTime);
			Log.i(TAG, "assets unpacked into " + root.getAbsolutePath());
		}
		catch(IOException e)
		{
			Log.e(TAG, "could not unpack assets", e);
		}
	}

	// the assets only need unpacking once per installed build; the install time
	// works as a marker for that (ApplicationInfo.lastUpdateTime is API 30-,
	// so the PackageInfo field is used instead)
	@SuppressWarnings("deprecation")
	private long apkInstallTime()
	{
		try
		{
			return getPackageManager().getPackageInfo(getPackageName(), 0).lastUpdateTime;
		}
		catch(Exception e)
		{
			return -1;
		}
	}

	private boolean sentinelIsThere(File root)
	{
		File f = new File(root, "write_data.txt");
		return f.isFile() && f.length() > 0;
	}

	private void copyAsset(File dstDir, String assetPath) throws IOException
	{
		File out = new File(dstDir, assetPath.replace('/', File.separatorChar));
		File parent = out.getParentFile();
		if(parent != null && !parent.isDirectory() && !parent.mkdirs())
			throw new IOException("cannot create " + parent);

		InputStream in = getAssets().open(assetPath);
		try { writeAll(in, out); }
		finally { in.close(); }
	}

	private static void writeAll(InputStream in, File out) throws IOException
	{
		File tmp = new File(out.getAbsolutePath() + ".tmp");
		OutputStream os = new FileOutputStream(tmp);
		try
		{
			byte[] buf = new byte[64 * 1024];
			int n;
			while((n = in.read(buf)) > 0) os.write(buf, 0, n);
			os.flush();
		}
		finally { os.close(); }

		if(out.exists() && !out.delete()) throw new IOException("cannot replace " + out);
		if(!tmp.renameTo(out)) throw new IOException("cannot move " + tmp);
	}

	private static long readMarker(File f)
	{
		try
		{
			BufferedReader r = new BufferedReader(new FileReader(f));
			try { return Long.parseLong(r.readLine().trim()); }
			finally { r.close(); }
		}
		catch(Exception e) { return -1; }
	}

	private static void writeMarker(File f, long value)
	{
		try
		{
			OutputStream os = new FileOutputStream(f);
			try
			{
				os.write(Long.toString(value).getBytes("UTF-8"));
				os.flush();
			}
			finally { os.close(); }
		}
		catch(IOException e) { Log.w(TAG, "cannot write " + f, e); }
	}
}
