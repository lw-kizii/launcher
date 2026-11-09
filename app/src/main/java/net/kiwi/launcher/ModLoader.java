package net.kiwi.launcher;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.os.Build;
import android.util.Log;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

public class ModLoader {

	private static final String TAG = "ModLoader";

	private static String targetApkPath;
	private static String[] targetApkPaths;
	private static boolean swordigoReady;
	private static boolean hooksLoaded;

	// Get the ABI.
	public static String getAbi() {
		if (Build.SUPPORTED_ABIS.length > 0) {
			String primary = Build.SUPPORTED_ABIS[0];
			if ("arm64-v8a".equals(primary) || primary.contains("arm64")) return "arm64-v8a";
		}
		return "armeabi-v7a";
	}

	// Get the extracted path of the libraries
	public static File getExtractedPath(Context context) {
		if (context == null) return null;
		File dir = new File(context.getFilesDir(), "Extracted/lib/" + getAbi());
		if (!dir.exists() && !dir.mkdirs() && !dir.isDirectory()) Log.w(TAG, "extract dir failed");
		return dir;
	}

	public static String getTargetApkPath() {
		return targetApkPath;
	}

	public static String[] getTargetApkPaths() {
		return targetApkPaths;
	}

	public static boolean isSwordigoReady() {
		return swordigoReady;
	}

	public static boolean isHooksLoaded() {
		return hooksLoaded;
	}

	@SuppressLint("UnsafeDynamicallyLoadedCode")
	public static void prepareSwordigo(Context context) throws Exception {
		String abi = getAbi();
		File libDir = getExtractedPath(context);
		File openAlFile = new File(libDir, "libopenal-soft.so");
		File swordigoFile = new File(libDir, "libswordigo.so");
		try {
			ApplicationInfo info = context.getPackageManager().getApplicationInfo("com.touchfoo.swordigo", 0);
			targetApkPath = info.sourceDir;
			targetApkPaths = collectApkPaths(info);
			extractLibFromZip(targetApkPaths, "lib/" + abi + "/libopenal-soft.so", openAlFile);
			extractLibFromZip(targetApkPaths, "lib/" + abi + "/libswordigo.so", swordigoFile);
			swordigoReady = true;
			File ext = context.getExternalFilesDir(null);
			API.initPaths(context.getFilesDir().getAbsolutePath(), context.getCacheDir().getAbsolutePath(), ext != null ? ext.getAbsolutePath() : "");
			API.init();
		} catch (Exception e) {
			Log.e(TAG, "prepareSwordigo failed", e);
			swordigoReady = false;
			targetApkPath = null;
			targetApkPaths = null;
			throw e;
		}
	}

	@SuppressLint("UnsafeDynamicallyLoadedCode")
	public static void loadGameLibraries(Context context) {
		File libDir = getExtractedPath(context);
		System.load(new File(libDir, "libopenal-soft.so").getAbsolutePath());

		File customLib = getCustomSwordigoLib(context);
		if (customLib != null) {
			Log.i(TAG, "Loading custom libswordigo from cache: " + customLib.getAbsolutePath());
			System.load(customLib.getAbsolutePath());
			hooksLoaded = false;
		} else {
			File vanillaLib = new File(libDir, "libswordigo.so");
			Log.i(TAG, "Loading vanilla libswordigo from: " + vanillaLib.getAbsolutePath());
			System.load(vanillaLib.getAbsolutePath());
			API.loadHooks();
			hooksLoaded = true;
		}
	}

	private static File getCustomSwordigoLib(Context context) {
		if (Launcher.currentMod == null || Launcher.currentMod.dir == null) return null;
		File customFile = new File(Launcher.currentMod.dir, "libswordigo/" + getAbi() + "/libswordigo.so");
		if (customFile.exists() && customFile.isFile()) {
			return copyToCache(context, customFile);
		}
		return null;
	}

	private static File copyToCache(Context context, File srcFile) {
		File destFile = new File(context.getCacheDir(), "libswordigo_custom.so");
		try (InputStream in = new java.io.FileInputStream(srcFile)) {
			copyStream(in, destFile);
			return destFile;
		} catch (IOException e) {
			Log.e(TAG, "Failed to copy " + srcFile.getName() + " to cache", e);
			return srcFile;
		}
	}

	public static void unloadGameHooks() {
		if (!hooksLoaded) return;
		try {
			API.onModExit();
		} catch (Throwable t) {
			Log.e(TAG, "onModExit failed", t);
		} finally {
			hooksLoaded = false;
		}
	}

	public static String[] collectApkPaths(ApplicationInfo info) {
		if (info.splitSourceDirs == null || info.splitSourceDirs.length == 0)
			return new String[]{info.sourceDir};
		String[] paths = new String[info.splitSourceDirs.length + 1];
		paths[0] = info.sourceDir;
		System.arraycopy(info.splitSourceDirs, 0, paths, 1, info.splitSourceDirs.length);
		return paths;
	}

	@SuppressLint("SetWorldReadable")
	@SuppressWarnings("ResultOfMethodCallIgnored")
	private static void copyStream(InputStream in, File destFile) throws IOException {
		try (FileOutputStream out = new FileOutputStream(destFile)) {
			byte[] buf = new byte[8192];
			int n;
			while ((n = in.read(buf)) != -1) {
				out.write(buf, 0, n);
			}
		}
		destFile.setReadable(true, false);
		destFile.setExecutable(true, false);
	}

	@SuppressLint("SetWorldReadable")
	@SuppressWarnings("ResultOfMethodCallIgnored")
	public static void extractLibFromZip(String[] apkPaths, String zipEntryPath, File destFile) throws IOException {
		if (destFile.exists() && destFile.length() > 0) {
			destFile.setReadable(true, false);
			destFile.setExecutable(true, false);
			return;
		}
		for (String apkPath : apkPaths) {
			try (ZipFile zipFile = new ZipFile(apkPath)) {
				ZipEntry entry = zipFile.getEntry(zipEntryPath);
				if (entry == null) continue;
				try (InputStream in = zipFile.getInputStream(entry)) {
					copyStream(in, destFile);
				}
				return;
			}
		}
		throw new java.io.FileNotFoundException("missing " + zipEntryPath + " in any split apk");
	}

}
