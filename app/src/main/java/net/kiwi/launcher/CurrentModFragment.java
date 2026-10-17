package net.kiwi.launcher;

import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.content.FileProvider;
import androidx.fragment.app.Fragment;

import net.kiwi.launcher.databinding.FragmentCurrentModBinding;

import java.io.BufferedInputStream;
import java.io.File;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.concurrent.ConcurrentHashMap;

public class CurrentModFragment extends Fragment {

	private FragmentCurrentModBinding binding;
	private ModManager.ModInfo mod;
	private final Handler main = new Handler(Looper.getMainLooper());
	private final ConcurrentHashMap<String, Bitmap> imgCache = new ConcurrentHashMap<>();

	@Nullable
	@Override
	public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState) {
		binding = FragmentCurrentModBinding.inflate(inflater, container, false);
		binding.btnLaunch.setOnClickListener(v -> launch());
		binding.actionExport.setOnClickListener(v -> exportSaves());
		binding.actionImport.setOnClickListener(v -> importSaves());
		binding.actionDelete.setOnClickListener(v -> confirmDelete());
		return binding.getRoot();
	}

	@Override
	public void onResume() {
		super.onResume();
		refresh();
	}

	public void refresh() {
		if (binding == null || getContext() == null) return;
		mod = Launcher.currentMod;
		if (mod == null || mod.id == null) {
			String lastId = ModManager.getLastPlayedId(requireContext());
			mod = ModManager.findById(requireContext(), lastId);
			if (mod != null) Launcher.currentMod = mod;
		} else {
			ModManager.ModInfo still = ModManager.findById(requireContext(), mod.id);
			mod = still;
			Launcher.currentMod = still;
		}
		if (mod == null) {
			binding.empty.setVisibility(View.VISIBLE);
			binding.content.setVisibility(View.GONE);
			return;
		}
		binding.empty.setVisibility(View.GONE);
		binding.content.setVisibility(View.VISIBLE);
		binding.name.setText(mod.name != null ? mod.name : mod.id);
		binding.version.setText(mod.version != null ? "v" + mod.version : "");
		File icon = mod.iconFile();
		if (icon != null) {
			binding.icon.setImageBitmap(BitmapFactory.decodeFile(icon.getAbsolutePath()));
		} else {
			binding.icon.setImageResource(R.drawable.ic_mods);
		}
		String desc = mod.description != null && !mod.description.isEmpty() ? mod.description : "No description";
		Markdown.set(binding.description, desc, this::loadRemoteImage);
	}

	private void loadRemoteImage(String url, ImageView target) {
		if (url == null || url.isEmpty() || target == null) return;
		Bitmap cached = imgCache.get(url);
		if (cached != null && !cached.isRecycled()) {
			target.setImageBitmap(cached);
			return;
		}
		new Thread(() -> {
			try {
				HttpURLConnection conn = (HttpURLConnection) new URL(url).openConnection();
				conn.setConnectTimeout(8000);
				conn.setReadTimeout(8000);
				conn.setInstanceFollowRedirects(true);
				conn.setRequestProperty("User-Agent", "Mozilla/5.0");
				if (conn.getResponseCode() != 200) return;
				Bitmap bmp = BitmapFactory.decodeStream(new BufferedInputStream(conn.getInputStream()));
				if (bmp != null) {
					imgCache.put(url, bmp);
					main.post(() -> target.setImageBitmap(bmp));
				}
			} catch (Exception ignored) {}
		}).start();
	}

	private void launch() {
		if (mod == null) return;
		Launcher.currentMod = mod;
		ModManager.setLastPlayed(requireContext(), mod.id);
		MainActivity.launch();
	}

	private void exportSaves() {
		if (mod == null) return;
		File out = new File(requireContext().getCacheDir(), mod.id + "_saves.zip");
		if (!ModManager.exportSaves(mod, out)) {
			Util.alert(requireContext(), "Export", "No saves to export.", "OK", null);
			return;
		}
		try {
			Uri uri = FileProvider.getUriForFile(requireContext(),
				requireContext().getPackageName() + ".fileprovider", out);
			Intent share = new Intent(Intent.ACTION_SEND);
			share.setType("application/zip");
			share.putExtra(Intent.EXTRA_STREAM, uri);
			share.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
			startActivity(Intent.createChooser(share, "Export saves"));
		} catch (Exception e) {
			Util.alert(requireContext(), "Export failed",
				e.getMessage() != null ? e.getMessage() : "unknown error", "OK", null);
		}
	}

	private void importSaves() {
		if (mod == null) return;
		Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
		intent.addCategory(Intent.CATEGORY_OPENABLE);
		intent.setType("*/*");
		startActivityForResult(intent, 10001);
	}

	private void confirmDelete() {
		if (mod == null) return;
		ModManager.ModInfo target = mod;
		Util.alert(requireContext(),
			"Uninstall " + target.name + "?",
			"This removes the mod folder and everything inside it.",
			"Uninstall",
			() -> {
				boolean ok = ModManager.deleteMod(target);
				Toast.makeText(requireContext(), ok ? "Uninstalled " + target.name : "Couldn't remove", Toast.LENGTH_SHORT).show();
				Launcher.currentMod = null;
				refresh();
			},
			"Cancel",
			null);
	}

	@Override
	public void onActivityResult(int requestCode, int resultCode, @Nullable Intent data) {
		super.onActivityResult(requestCode, resultCode, data);
		if (requestCode != 10001 || resultCode != android.app.Activity.RESULT_OK
			|| data == null || data.getData() == null || mod == null) return;
		boolean ok = ModManager.importSaves(mod, data.getData(), requireContext());
		Toast.makeText(requireContext(), ok ? "Saves imported" : "Import failed", Toast.LENGTH_SHORT).show();
	}
}
