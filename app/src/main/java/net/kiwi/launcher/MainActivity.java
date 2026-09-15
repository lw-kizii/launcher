package net.kiwi.launcher;

import android.annotation.SuppressLint;
import android.content.Intent;
import android.net.Uri;
import android.app.Activity;
import android.content.pm.ActivityInfo;
import android.content.pm.ApplicationInfo;
import android.content.res.AssetManager;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.Toast;

import androidx.annotation.Keep;
import androidx.annotation.NonNull;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentActivity;
import androidx.viewpager2.adapter.FragmentStateAdapter;
import androidx.viewpager2.widget.ViewPager2;

import com.L.SwordigoRuntime.GameRenderer;
import com.L.SwordigoRuntime.GameView;
import com.touchfoo.swordigo.Native;

import net.kiwi.launcher.databinding.ActivityMainBinding;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.PrintWriter;
import java.io.StringWriter;
import java.lang.reflect.Method;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

import org.json.JSONObject;

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.charset.StandardCharsets;

@Keep
public class MainActivity extends FragmentActivity {

	private static final String TAG = "Launcher";
	private static final int Version = 80;

	private static MainActivity instance;
	private GameView glSurfaceView;
	private String targetApkPath;
	private String[] targetApkPaths;
	private boolean hooksLoaded;
	private boolean swordigoReady;
	private ActivityMainBinding binding;
	private static AssetManager gameAssetManager;

	public static int getVersion() { return Version; }
	public static Activity getCurrentActivity() { return instance; }
	public static String currentMod() { return Launcher.currentMod(); }
	public static String getTargetApkPath() { return instance != null ? instance.targetApkPath : null; }

	public static native void init();
	public static native void initPaths(String internalFiles, String externalFiles);
	public static native void loadHooks();
	public static native void onModExit();
	public static native void setCrashLogPath(String path);

	public static String getAbi() {
		if (android.os.Build.SUPPORTED_ABIS.length > 0) {
			String primary = android.os.Build.SUPPORTED_ABIS[0];
			if ("arm64-v8a".equals(primary) || primary.contains("arm64")) return "arm64-v8a";
		}
		return "armeabi-v7a";
	}

	public static File getExtractedPath() {
		MainActivity act = instance;
		if (act == null) return null;
		File dir = new File(act.getFilesDir(), "Extracted/lib/" + getAbi());
		if (!dir.exists() && !dir.mkdirs() && !dir.isDirectory()) Log.w(TAG, "extract dir failed");
		return dir;
	}

	@Override
	@SuppressLint("UnsafeDynamicallyLoadedCode")
	protected void onCreate(Bundle bundle) {
		super.onCreate(bundle);
		getWindow().setSoftInputMode(android.view.WindowManager.LayoutParams.SOFT_INPUT_ADJUST_NOTHING);
		instance = this;

		File extDir = getExternalFilesDir(null);
		if (extDir != null) {
			File resourcesDir = new File(extDir, "resources");
			if (!resourcesDir.exists() && !resourcesDir.mkdirs() && !resourcesDir.isDirectory()) {
				Log.w(TAG, "resources dir failed");
			}
		}

		System.loadLibrary("launcher");
		try {
			setCrashLogPath(new File(getFilesDir(), "last_crash.log").getAbsolutePath());
		} catch (Throwable ignored) {}

		showLauncherUi();
		checkPreviousCrash();
		prepareSwordigo();
		checkForUpdate();
	}

	@SuppressLint("UnsafeDynamicallyLoadedCode")
	private void prepareSwordigo() {
		String abi = getAbi();
		File libDir = getExtractedPath();
		File openAlFile = new File(libDir, "libopenal-soft.so");
		File swordigoFile = new File(libDir, "libswordigo.so");
		try {
			ApplicationInfo info = getPackageManager().getApplicationInfo("com.touchfoo.swordigo", 0);
			targetApkPath = info.sourceDir;
			targetApkPaths = collectApkPaths(info);
			extractLibFromZip(targetApkPaths, "lib/" + abi + "/libopenal-soft.so", openAlFile);
			extractLibFromZip(targetApkPaths, "lib/" + abi + "/libswordigo.so", swordigoFile);
			System.load(openAlFile.getAbsolutePath());
			System.load(swordigoFile.getAbsolutePath());
			swordigoReady = true;
			File ext = getExternalFilesDir(null);
			initPaths(getFilesDir().getAbsolutePath(), ext != null ? ext.getAbsolutePath() : "");
			init();
		} catch (Exception e) {
			Log.e(TAG, "prepareSwordigo failed", e);
			swordigoReady = false;
			targetApkPath = null;
			runOnUiThread(() -> showErrorDialog(e));
		}
	}

	private void showLauncherUi() {
		binding = ActivityMainBinding.inflate(getLayoutInflater());
		setContentView(binding.getRoot());
		Util.attachSystemBarsPadding(binding.getRoot());

		binding.pager.setAdapter(new FragmentStateAdapter(this) {
			@NonNull
			@Override
			public Fragment createFragment(int position) {
				if (position == 0) return new CurrentModFragment();
				if (position == 1) return new ModsFragment();
				if (position == 2) return new StoreFragment();
				return new SettingsFragment();
			}

			@Override
			public int getItemCount() { return 4; }
		});

		binding.bottomNav.setOnItemSelectedListener(item -> {
			int id = item.getItemId();
			if (id == R.id.nav_current) { binding.pager.setCurrentItem(0, true); return true; }
			if (id == R.id.nav_mods) { binding.pager.setCurrentItem(1, true); return true; }
			if (id == R.id.nav_store) { binding.pager.setCurrentItem(2, true); return true; }
			if (id == R.id.nav_settings) { binding.pager.setCurrentItem(3, true); return true; }
			return false;
		});

		binding.pager.registerOnPageChangeCallback(new ViewPager2.OnPageChangeCallback() {
			@Override
			public void onPageSelected(int position) {
				binding.bottomNav.getMenu().getItem(position).setChecked(true);
			}
		});
	}

	public void showCurrentModTab() {
		if (binding != null) {
			binding.pager.setCurrentItem(0, true);
			binding.bottomNav.getMenu().getItem(0).setChecked(true);
		}
	}

	private static String[] collectApkPaths(ApplicationInfo info) {
		if (info.splitSourceDirs == null || info.splitSourceDirs.length == 0)
			return new String[]{info.sourceDir};
		String[] paths = new String[info.splitSourceDirs.length + 1];
		paths[0] = info.sourceDir;
		System.arraycopy(info.splitSourceDirs, 0, paths, 1, info.splitSourceDirs.length);
		return paths;
	}

	private void checkPreviousCrash() {
		File crashLog = new File(getFilesDir(), "last_crash.log");
		if (!crashLog.exists() || crashLog.length() == 0) return;
		Util.alert(this, "Oops, the launcher crashed.",
			"A previous session ended with a native crash.",
			"Dismiss", () -> {
				//noinspection ResultOfMethodCallIgnored
				crashLog.delete();
			});
	}

	private void showErrorDialog(Exception e) {
		StringWriter sw = new StringWriter();
		e.printStackTrace(new PrintWriter(sw));
		String stack = sw.toString();
		String message;
		if (e instanceof android.content.pm.PackageManager.NameNotFoundException) {
			message = "Vanilla Swordigo (com.touchfoo.swordigo) is not installed.\n\n"
				+ "Install Swordigo from the Play Store, then reopen the launcher.";
		} else {
			message = "Failed to extract/load Swordigo resources.\n\n"
				+ e.getClass().getSimpleName() + ": " + e.getMessage() + "\n\n"
				+ (stack.length() > 300 ? stack.substring(0, 300) + "..." : stack);
		}
		Util.alert(this, "Swordigo Initialization Failed", message, "Got it", null);
	}


	public static void launch() {
		if (instance == null) return;
		if (!instance.swordigoReady || instance.targetApkPath == null) {
			instance.runOnUiThread(() ->
				Toast.makeText(instance, "Swordigo is not installed — can't launch.", Toast.LENGTH_LONG).show());
			return;
		}
		instance.startGame();
	}

	private void startGame() {
		if (targetApkPath == null) return;
		loadHooks();
		setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE);

		FrameLayout gameRoot = new FrameLayout(this);
		gameRoot.setLayoutParams(new ViewGroup.LayoutParams(
			ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));

		glSurfaceView = new GameView(this);
		glSurfaceView.setEGLConfigChooser(5, 6, 5, 0, 16, 0);
		glSurfaceView.setPreserveEGLContextOnPause(true);
		glSurfaceView.setRenderer(new GameRenderer());
		gameRoot.addView(glSurfaceView, new FrameLayout.LayoutParams(
			ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));

		setContentView(gameRoot);
		enableImmersiveMode();
		ButtonController.init(this, gameRoot);
		setupNativeEnvironment(targetApkPath);
		Launcher.initGameButtons(this);
		hooksLoaded = true;
	}

	private void unloadGameHooks() {
		if (!hooksLoaded) return;
		try { onModExit(); } catch (Throwable t) {
			Log.e(TAG, "onModExit failed", t);
		} finally {
			hooksLoaded = false;
		}
	}

	public static void returnToLauncher() {
		if (instance != null) instance.stopGame();
	}

	@SuppressLint("SourceLockedOrientationActivity")
	private void stopGame() {
		unloadGameHooks();
		Port.onGameStop();
		if (glSurfaceView != null) glSurfaceView.onPause();
		Launcher.destroyGameButtons();
		glSurfaceView = null;
		ButtonController.removeAll();
		setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
		showLauncherUi();
	}

	@Keep
	@SuppressWarnings("unused")
	public static AssetManager getGameAssetManager() {
		if (gameAssetManager != null) return gameAssetManager;
		String apk = getTargetApkPath();
		if (apk == null || instance == null) return null;
		AssetManager am = buildAssetManager(apk);
		if (am != null) gameAssetManager = am;
		return am;
	}

	private void setupNativeEnvironment(String apkPath) {
		try {
			Native.mainActivity = this;
			Native.setFilesDir(getApplicationContext().getFilesDir().toString());
			Native.setCacheDir(getApplicationContext().getCacheDir().toString());
			AssetManager am = buildAssetManager(apkPath);
			if (am == null) {
				finish();
				return;
			}
			Native.setAssetManager(am);
			Native.handleApplicationLaunch();
		} catch (Throwable t) {
			Log.e(TAG, "setupNativeEnvironment failed", t);
		}
	}

	@SuppressWarnings({"deprecation", "JavaReflectionMemberAccess"})
	private static AssetManager buildAssetManager(String apkPath) {
		try {
			AssetManager am = AssetManager.class.getDeclaredConstructor().newInstance();
			Method addAssetPath = AssetManager.class.getMethod("addAssetPath", String.class);
			Object result = addAssetPath.invoke(am, apkPath);
			boolean ok = (result instanceof Integer) && ((Integer) result) != 0;
			return ok ? am : null;
		} catch (Exception e) {
			return null;
		}
	}

	@SuppressWarnings("deprecation")
	private void enableImmersiveMode() {
		if (glSurfaceView == null) return;
		//noinspection deprecation
		int flags = View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
			| View.SYSTEM_UI_FLAG_FULLSCREEN
			| View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
			| View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
			| View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
			| View.SYSTEM_UI_FLAG_LAYOUT_STABLE;
		//noinspection deprecation
		glSurfaceView.setSystemUiVisibility(flags);
	}

	@Override
	protected void onPause() {
		super.onPause();
		if (glSurfaceView != null) glSurfaceView.onPause();
		Port.onGamePause();
	}

	@Override
	protected void onResume() {
		super.onResume();
		if (glSurfaceView != null) {
			glSurfaceView.onResume();
			enableImmersiveMode();
			Port.onGameResume();
		}
	}

	@Override
	protected void onDestroy() {
		super.onDestroy();
		unloadGameHooks();
		if (instance == this) instance = null;
	}

	@SuppressLint("SetWorldReadable")
	@SuppressWarnings("ResultOfMethodCallIgnored")
	private void extractLibFromZip(String[] apkPaths, String zipEntryPath, File destFile) throws IOException {
		// I should probably try to find another way to load the libs other than extraction
		if (destFile.exists() && destFile.length() > 0) {
			destFile.setReadable(true, false);
			destFile.setExecutable(true, false);
			return;
		}
		for (String apkPath : apkPaths) {
			try (ZipFile zipFile = new ZipFile(apkPath)) {
				ZipEntry entry = zipFile.getEntry(zipEntryPath);
				if (entry == null) continue;
				try (InputStream in = zipFile.getInputStream(entry);
				     FileOutputStream out = new FileOutputStream(destFile)) {
					byte[] buf = new byte[8192];
					int n;
					while ((n = in.read(buf)) != -1) out.write(buf, 0, n);
				}
				destFile.setReadable(true, false);
				destFile.setExecutable(true, false);
				return;
			}
		}
		throw new java.io.FileNotFoundException("missing " + zipEntryPath + " in any split apk");
	}

	private void checkForUpdate() {
		new Thread(() -> {
			try {
				HttpURLConnection c = (HttpURLConnection) new URL(
					"https://raw.githubusercontent.com/lw-kizii/_/main/launcher-data/update.json"
				).openConnection();
				c.setConnectTimeout(8000);
				c.setReadTimeout(8000);
				c.setInstanceFollowRedirects(true);
				c.setRequestProperty("User-Agent", "Mozilla/5.0");
				if (c.getResponseCode() != 200) return;
				StringBuilder sb = new StringBuilder();
				try (BufferedReader r = new BufferedReader(new InputStreamReader(c.getInputStream(), StandardCharsets.UTF_8))) {
					char[] buf = new char[1024];
					int n;
					while ((n = r.read(buf)) != -1) sb.append(buf, 0, n);
				}
				JSONObject o = new JSONObject(sb.toString().trim());
				int remote = o.optInt("version", 0);
				if (remote <= Version) return;
				String notes = o.optString("notes", "A new version is available.");
				String url = o.optString("directUrl", o.optString("url", ""));
				runOnUiThread(() -> Util.alert(this, "Update available", notes, "OK", () -> {
					if (url == null || url.isEmpty()) return;
					try {
						startActivity(new Intent(Intent.ACTION_VIEW, Uri.parse(url)));
					} catch (Exception ignored) {}
				}));
			} catch (Exception ignored) {}
		}).start();
	}

}