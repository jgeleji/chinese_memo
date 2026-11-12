package com.geleji.chinesememo;

import android.content.Context;
import android.content.res.AssetManager;
import android.graphics.Rect;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.ViewTreeObserver;
import android.view.WindowManager;
import android.view.inputmethod.InputMethodManager;

import org.libsdl.app.SDLActivity;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

/**
 * SDL2 host activity for the ported chinese_memo console game.
 *
 * libSDL2.so provides the engine, libquestions_sdl.so contains the ported C++
 * game (its SDL_main is picked up as the entry point because it is the last
 * library in getLibraries()).
 */
public class MainActivity extends SDLActivity
{
	private static final String TAG = "chinese_memo";

	@Override
	protected String[] getLibraries()
	{
		return new String[] { "SDL2", "questions_sdl" };
	}

	@Override
	protected void onCreate(Bundle savedInstanceState)
	{
		// the native code chdir()s into getFilesDir() and then opens
		// "union_q.txt", "NotoSansSC.ttf", "status1.txt" and "data/..." with
		// relative paths, so the assets must be unpacked there first
		try { copyAssetsIfNeeded(); }
		catch(Exception e) { Log.e(TAG, "could not unpack assets", e); }

		getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

		super.onCreate(savedInstanceState);
	}

	@Override
	protected void onDestroy()
	{
		super.onDestroy();
		// the game writes its status file after every block; SDL_QUIT / back
		// already ends the process, nothing else to clean up here
	}

	// -----------------------------------------------------------------------
	// measuring how tall the keyboard currently is
	//
	// MIUI answers the password-style input field (which keeps it from typing
	// Chinese characters) with its "Mi Secure Keyboard", and that keyboard is
	// considerably taller than the ordinary one; its exact height also changes
	// with the display size and the keyboard's toolbar. So instead of trusting
	// a fixed fraction of the screen, the real keyboard height is measured here
	// (root view height minus the visible display frame) and written to the
	// file cm_ime_height; the native layout reads that file every frame and
	// keeps all buttons and text above the keyboard.
	// -----------------------------------------------------------------------

	private ViewTreeObserver.OnGlobalLayoutListener ime_height_listener;
	private int last_reported_ime_height = -1;

	private void start_ime_height_watcher()
	{
		if(ime_height_listener != null) return;
		ime_height_listener = new ViewTreeObserver.OnGlobalLayoutListener()
		{
			@Override
			public void onGlobalLayout()
			{
				try
				{
					Rect r = new Rect();
					getWindow().getDecorView().getWindowVisibleDisplayFrame(r);
					int total = getWindow().getDecorView().getRootView().getHeight();
					int ime = total - r.bottom;
					// ignore the status/navigation-bar-only case and small
					// jitter; rewrite the file only when it changed for real
					if(ime < 100) ime = 0;
					if(Math.abs(ime - last_reported_ime_height) < 8) return;
					last_reported_ime_height = ime;
					File f = new File(getFilesDir(), "cm_ime_height");
					try(OutputStream out = new FileOutputStream(f))
					{
						out.write((Integer.toString(ime) + "\n").getBytes());
					}
					Log.i(TAG, "keyboard height: " + ime + "px");
				}
				catch(Exception e) { /* not laid out yet */ }
			}
		};
		getWindow().getDecorView().getViewTreeObserver()
			.addOnGlobalLayoutListener(ime_height_listener);
	}

	private void stop_ime_height_watcher()
	{
		if(ime_height_listener == null) return;
		try
		{
			getWindow().getDecorView().getViewTreeObserver()
				.removeOnGlobalLayoutListener(ime_height_listener);
		}
		catch(Exception e) { /* the observer may already be dead */ }
		ime_height_listener = null;
	}

	// -----------------------------------------------------------------------
	// the soft keyboard must never disappear
	//
	// The game is played by tapping, and MIUI's keyboard hides itself as soon as
	// a touch lands outside its own input view (which for SDL is a hidden 1x1
	// EditText). That costs a second or two after every question, and the
	// keyboard then jumps over the screen again when it comes back. So the
	// keyboard is kept shown for as long as the activity is in the foreground:
	// the manifest asks for stateAlwaysVisible, and this re-asserts it on focus
	// changes and once in a while on top of that. The native side lays out its
	// buttons and text above the keyboard band, so it never covers anything.
	// -----------------------------------------------------------------------

	private static final long IME_KEEPALIVE_MS = 1500;

	private final Handler ime_handler = new Handler(Looper.getMainLooper());

	private final Runnable ime_keepalive = new Runnable()
	{
		@Override
		public void run()
		{
			show_keyboard();
			ime_handler.postDelayed(this, IME_KEEPALIVE_MS);
		}
	};

	@Override
	protected void onResume()
	{
		super.onResume();
		start_ime_height_watcher();
		ime_handler.removeCallbacks(ime_keepalive);
		ime_handler.postDelayed(ime_keepalive, 400);
	}

	@Override
	protected void onPause()
	{
		stop_ime_height_watcher();
		ime_handler.removeCallbacks(ime_keepalive);
		super.onPause();
	}

	@Override
	public void onWindowFocusChanged(boolean hasFocus)
	{
		super.onWindowFocusChanged(hasFocus);
		if(hasFocus) show_keyboard();
	}

	private void show_keyboard()
	{
		// exactly the path SDL itself uses: put the hidden DummyEdit back in
		// focus and ask the IME to show itself for it
		try { showTextInput(0, 0, 0, 0); }
		catch(Exception e) { /* the activity/surface is not up yet, try later */ }

		try
		{
			if(mTextEdit != null)
			{
				InputMethodManager imm =
					(InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);
				if(imm != null) imm.showSoftInput(mTextEdit, 0);
			}
		}
		catch(Exception e) { /* ignore */ }
	}

	// -----------------------------------------------------------------------
	// assets
	// -----------------------------------------------------------------------

	private void copyAssetsIfNeeded() throws IOException
	{
		File root = getFilesDir();
		File marker = new File(root, ".assets_version");
		long apkTime = apkInstallTime();

		if(marker.exists() && readMarker(marker) == apkTime && assetSentinelIsThere(root))
		{
			Log.i(TAG, "assets already unpacked in " + root.getAbsolutePath());
			return;
		}

		copyAsset(root, "union_q.txt");
		copyAsset(root, "NotoSansSC.ttf");
		copyAssetIfMissing(root, "status1.txt");   // never clobber progress on the phone
		copyAssetTree(root, "data");

		writeMarker(marker, apkTime);
		Log.i(TAG, "assets unpacked into " + root.getAbsolutePath());
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

	private boolean assetSentinelIsThere(File root)
	{
		File f = new File(root, "data" + File.separator + "general-standard?page=1");
		return f.isFile() && f.length() > 0;
	}

	private void copyAssetTree(File dstDir, String assetPath) throws IOException
	{
		AssetManager am = getAssets();
		String[] children = am.list(assetPath);
		if(children == null || children.length == 0)
		{
			copyAsset(dstDir, assetPath);
			return;
		}
		File dir = new File(dstDir, assetPath.replace('/', File.separatorChar));
		if(!dir.isDirectory() && !dir.mkdirs()) throw new IOException("cannot create " + dir);
		for(String child : children) copyAssetTree(dstDir, assetPath + "/" + child);
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

	private void copyAssetIfMissing(File dstDir, String assetPath) throws IOException
	{
		File out = new File(dstDir, assetPath.replace('/', File.separatorChar));
		if(out.exists() && out.length() > 0) return;
		copyAsset(dstDir, assetPath);
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
