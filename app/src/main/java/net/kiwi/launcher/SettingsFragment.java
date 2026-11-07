package net.kiwi.launcher;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageInfo;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.provider.Settings;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.Toast;

import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.content.pm.PackageInfoCompat;
import androidx.fragment.app.Fragment;

import net.kiwi.launcher.databinding.FragmentSettingsBinding;

import java.io.BufferedInputStream;
import java.io.File;
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

	private static void ensureDir(File dir) {
		if (dir != null && !dir.exists()) {
			dir.mkdirs();
		}
	}

	private static void extractZip(Context context, Uri uri, File destDir) throws Exception {
		try (InputStream is = context.getContentResolver().openInputStream(uri)) {
			if (is == null) throw new IOException("could not open file");
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

	public static int getFps() {
		Activity act = MainActivity.getCurrentActivity();
		if (act != null) {
			return act.getSharedPreferences("fps", 0).getInt("fps", 60);
		}
		return 60;
	}

	public static void applyAngle(Context ctx, boolean angle) {
		try {
			Settings.Global.putString(ctx.getContentResolver(), "angle_gl_driver_selection_pkgs", ctx.getPackageName());
			Settings.Global.putString(ctx.getContentResolver(), "angle_gl_driver_selection_values", angle ? "angle" : "default");
		} catch (Throwable ignored) {}
	}

	private void handleButtons(Button f30, Button f60, Button f90, Button f120, Button f180) {
		SharedPreferences prefs = requireContext().getSharedPreferences("fps", 0);
		FPSHandle[] handles = {
			new FPSHandle(f30, 30),
			new FPSHandle(f60, 60),
			new FPSHandle(f90, 90),
			new FPSHandle(f120, 120),
			new FPSHandle(f180, 180)
		};

		Runnable updateBtns = () -> {
			int currentFps = prefs.getInt("fps", 60);
			for (FPSHandle handle : handles) {
				if (currentFps == handle.fps) {
					handle.btn.setTextColor(0xff00ff00);
					continue;
				}
				handle.btn.setTextColor(0xffffffff);
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

	private void handleRenderer(FragmentSettingsBinding b) {
		SharedPreferences prefs = requireContext().getSharedPreferences("settings", Context.MODE_PRIVATE);
		Runnable update = () -> {
			boolean isAngle = "angle".equalsIgnoreCase(prefs.getString("renderer", "default"));
			b.rendererDefault.setTextColor(isAngle ? 0xffffffff : 0xff00ff00);
			b.rendererAngle.setTextColor(isAngle ? 0xff00ff00 : 0xffffffff);
		};
		update.run();
		b.rendererDefault.setOnClickListener(v -> {
			prefs.edit().putString("renderer", "default").apply();
			applyAngle(requireContext(), false);
			update.run();
		});
		b.rendererAngle.setOnClickListener(v -> {
			prefs.edit().putString("renderer", "angle").apply();
			applyAngle(requireContext(), true);
			update.run();
		});
	}

	@SuppressLint("SetTextI18n")
	private void updateDetails(FragmentSettingsBinding b) {
		Context ctx = getContext();
		if (ctx == null) return;

		String ver = "??";
		try {
			PackageInfo myInfo = ctx.getPackageManager().getPackageInfo(ctx.getPackageName(), 0);
			long vc = PackageInfoCompat.getLongVersionCode(myInfo);
			ver = "v" + myInfo.versionName + " (" + vc + ")";
		} catch (Exception ignored) {}

		b.detailLauncherVersion.setText("Kiwi Launcher Version: " + ver);
		b.detailDevice.setText("Device: " + Build.MANUFACTURER + " " + Build.MODEL + " (Android " + Build.VERSION.RELEASE + ", API " + Build.VERSION.SDK_INT + ")");
		b.detailAbi.setText("ABI: " + (Build.SUPPORTED_ABIS.length > 0 ? Build.SUPPORTED_ABIS[0] : "unknown"));

		String swordigoStatus = "Swordigo: Not Installed";
		try {
			PackageInfo pi = ctx.getPackageManager().getPackageInfo("com.touchfoo.swordigo", 0);
			swordigoStatus = "Swordigo: Installed (v" + pi.versionName + ")";
		} catch (Exception ignored) {}
		b.detailSwordigo.setText(swordigoStatus);
	}

	private static void deleteRecursive(File fileOrDirectory) {
		if (fileOrDirectory != null && fileOrDirectory.exists()) {
			if (fileOrDirectory.isDirectory()) {
				File[] children = fileOrDirectory.listFiles();
				if (children != null) {
					for (File child : children) {
						deleteRecursive(child);
					}
				}
			}
			fileOrDirectory.delete();
		}
	}

	private void handleGlobalAssets(Button importAssets, Button clearAssets) {
		Activity act = MainActivity.getCurrentActivity();
		File extFiles = act.getExternalFilesDir(null);

		importAssets.setOnClickListener(l -> importPicker.launch(new String[]{"application/zip"}));

		clearAssets.setOnClickListener(l -> {
			File resourcesDir = new File(extFiles, "resources");
			deleteRecursive(resourcesDir);
			if (resourcesDir.mkdirs()) {
				Toast.makeText(act, "Cleared global assets.", Toast.LENGTH_SHORT).show();
				Log.d("SettingsFragment", "Refreshed global resources");
			}
		});
	}

	private final ActivityResultLauncher<String[]> importPicker = registerForActivityResult(
		new ActivityResultContracts.OpenDocument(),
		this::onImportPicked
	);

	private void onImportPicked(@Nullable Uri uri) {
		if (uri == null) return;
		String name = uri.getLastPathSegment();
		if (name == null) name = "import";

		name = name.toLowerCase(Locale.ROOT);
		if (!name.endsWith(".zip")) return;

		Activity act = MainActivity.getCurrentActivity();
		try {
			extractZip(act, uri, new File(act.getExternalFilesDir(null), "resources"));
		} catch (Exception e) {
			Log.e("SettingsFragment", "Error while importing assets: ", e);
		}
	}

	@Nullable
	@Override
	public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState) {
		FragmentSettingsBinding binding = FragmentSettingsBinding.inflate(inflater, container, false);

		handleButtons(binding.fps30, binding.fps60, binding.fps90, binding.fps120, binding.fps180);
		handleRenderer(binding);
		handleGlobalAssets(binding.gaImport, binding.gaClear);
		updateDetails(binding);

		binding.discordbtn.setOnClickListener(l -> {
			Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("https://discord.gg/cF6Krcb28V"));
			startActivity(intent);
		});

		return binding.getRoot();
	}
}
