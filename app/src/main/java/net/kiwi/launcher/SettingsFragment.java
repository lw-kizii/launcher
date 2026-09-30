package net.kiwi.launcher;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.net.Uri;
import android.os.Bundle;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;

import com.touchfoo.swordigo.Native;

import net.kiwi.launcher.databinding.FragmentModsBinding;
import net.kiwi.launcher.databinding.FragmentSettingsBinding;

import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.Locale;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

public class SettingsFragment extends Fragment {

	static class FPSHandle {
		Button btn;
		int fps;

		public FPSHandle(Button btn, int fps) {
			this.btn = btn;
			this.fps = fps;
		}
	}

	// Simple helpers, should be moved to Util honestly
	static void doNothing() {}
	private static void ensureDir(File dir) {
		if (dir != null && !dir.exists() && !dir.mkdirs() && !dir.isDirectory()) {
			doNothing();
		}
	}
	private static void extractZip(Context context, Uri uri, File destDir) throws Exception {
		try (InputStream is = context.getContentResolver().openInputStream(uri)) {
			if (is == null) throw new IOException("could not open file");
			inflateZip(is, destDir);
		}
	}
	private static void extractZipFile(File zipFile, File destDir) throws Exception {
		try (InputStream is = new FileInputStream(zipFile)) {
			inflateZip(is, destDir);
		}
	}
	private static void inflateZip(InputStream is, File destDir) throws Exception {
		try (ZipInputStream zis = new ZipInputStream(new BufferedInputStream(is))) {
			ZipEntry entry;
			while ((entry = zis.getNextEntry()) != null) {
				if (entry.isDirectory()) continue;
				String entryName = entry.getName();
				if (entryName.contains("..")) continue;
				File target = new File(destDir, entryName);
				File parent = target.getParentFile();
				if (parent != null) ensureDir(parent);
				try (FileOutputStream fos = new FileOutputStream(target)) {
					byte[] buffer = new byte[8192];
					int count;
					while ((count = zis.read(buffer)) != -1) fos.write(buffer, 0, count);
				}
				zis.closeEntry();
			}
		}
	}

	public static int g_fps = 60;

	public static int getFps() {
		return g_fps;
	}

	void handleButtons(Button f30, Button f60, Button f90, Button f120, Button f180) {
		SharedPreferences prefs = requireContext().getSharedPreferences("fps", 0);
		FPSHandle[] handles = {
			new FPSHandle(f30, 30),
			new FPSHandle(f60, 60),
			new FPSHandle(f90, 90),
			new FPSHandle(f120, 120),
			new FPSHandle(f180, 180),
		};

		Runnable updateBtns = () -> {
			int currentFps = prefs.getInt("fps", 60);
			for (FPSHandle handle : handles) {
				if (currentFps == handle.fps) {
					g_fps = handle.fps;
					handle.btn.setTextColor(0xff00ff00);
					continue;
				}
				handle.btn.setTextColor(0xffffffff); // white
			}
		};

		updateBtns.run();

		for (FPSHandle h : handles) {
			h.btn.setOnClickListener(l -> {
				int fps = h.fps;
				Log.d("SettingsFragment", "FPS Set to " + fps);
				prefs.edit().putInt("fps", fps).apply();
				updateBtns.run();
			});
		}
	}

	private static boolean deleteRecursive(File fileOrDirectory) {
		if (fileOrDirectory != null && fileOrDirectory.exists()) {
			if (fileOrDirectory.isDirectory()) {
				File[] children = fileOrDirectory.listFiles();
				if (children != null) {
					for (File child : children) {
						deleteRecursive(child);
					}
				}
			}
			return fileOrDirectory.delete();
		}
		return false;
	}

	void handleGlobalAssets(Button clear, Button imp) {
		Activity act = MainActivity.getCurrentActivity();
		File extFiles = act.getExternalFilesDir(null);
		clear.setOnClickListener(l -> {
			File resourcesDir = new File(extFiles, "resources");
			deleteRecursive(resourcesDir);
			if (resourcesDir.mkdirs()) {
				Toast.makeText(act, "Cleared global assets.", Toast.LENGTH_SHORT).show();
				Log.d("SettingsFragment", "Refreshed global resources");
			}
		});
		imp.setOnClickListener(l -> {
			Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
			intent.addCategory(Intent.CATEGORY_OPENABLE);
			intent.setType("*/*");
			startActivityForResult(intent, 10001);
		});
	}

	@Override
	public void onActivityResult(int requestCode, int resultCode, @Nullable Intent data) {
		super.onActivityResult(requestCode, resultCode, data);
		if (requestCode != 10001 || resultCode != android.app.Activity.RESULT_OK || data == null || data.getData() == null) return;
		Uri uri = data.getData();
		String name = uri.getLastPathSegment();
		if (name == null) name = "import";
		name = name.toLowerCase(Locale.ROOT);
		if (!name.endsWith(".zip")) return;
		Activity act = MainActivity.getCurrentActivity();
		try {
			throw new RuntimeException();
//			extractZip(MainActivity.getCurrentActivity(), uri,
//				new File(act.getExternalFilesDir(null), "resources")
//			);
		} catch (Exception e) {
			Util.alert(
				act,
				"Failed to import assets",
				String.valueOf(e),
				"Okay", () -> {
					Log.d("SettingsFragment", "Clicked on Okay");
				},
				"Report on Discord", () -> {
					Toast.makeText(act, "Opening Discord...", Toast.LENGTH_SHORT).show();
					Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("https://discord.gg/cF6Krcb28V"));
					startActivity(intent);
				}
			);
		}
	}

	@Nullable
	@Override
	public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState) {
		FragmentSettingsBinding binding = FragmentSettingsBinding.inflate(inflater, container, false);

		handleButtons(binding.fps30, binding.fps60, binding.fps90, binding.fps120, binding.fps180);
		handleGlobalAssets(binding.gaClear, binding.gaImport);

		binding.discordbtn.setOnClickListener(l -> {
			// I don't trust Native.openUrl...
			Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("https://discord.gg/cF6Krcb28V"));
			startActivity(intent);
		});

		return binding.getRoot();
	}

	/*
	TODO:
		- Add global permissions such as allow_networking...
		- Add clear cache
	 */
}
