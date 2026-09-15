package net.kiwi.launcher;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;
import androidx.recyclerview.widget.DiffUtil;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import net.kiwi.launcher.databinding.FragmentStoreBinding;
import net.kiwi.launcher.databinding.ItemStoreModBinding;

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.BufferedReader;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.Objects;
import java.util.zip.GZIPInputStream;

public class StoreFragment extends Fragment {

	private FragmentStoreBinding binding;
	private final List<StoreEntry> all = new ArrayList<>();
	private final List<StoreEntry> filtered = new ArrayList<>();
	private Adapter adapter;
	private final Handler main = new Handler(Looper.getMainLooper());

	static class StoreEntry {
		String id, name, description, downloadUrl, iconUrl;
	}

	@Nullable
	@Override
	public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState) {
		binding = FragmentStoreBinding.inflate(inflater, container, false);
		adapter = new Adapter();
		binding.storeList.setLayoutManager(new LinearLayoutManager(requireContext()));
		binding.storeList.setAdapter(adapter);
		binding.search.addTextChangedListener(new TextWatcher() {
			@Override public void beforeTextChanged(CharSequence s, int start, int count, int after) {}
			@Override public void onTextChanged(CharSequence s, int start, int before, int count) {
				applyFilter(s != null ? s.toString() : "");
			}
			@Override public void afterTextChanged(Editable s) {}
		});
		return binding.getRoot();
	}

	@Override
	public void onResume() {
		super.onResume();
		refreshStore();
	}

	private void refreshStore() {
		if (binding == null || getContext() == null) return;
		final File cacheFile = new File(requireContext().getCacheDir(), "store_cache.json");

		if (all.isEmpty()) {
			JSONArray cached = readCache(cacheFile);
			if (cached != null) {
				all.clear();
				all.addAll(parse(cached));
				applyFilter(textOf(binding.search));
			}
			binding.progress.setVisibility(View.VISIBLE);
		}

		new Thread(() -> {
			try {
				String raw = httpGetStore();
				JSONArray arr = new JSONArray(sanitize(raw));
				writeCache(cacheFile, raw);
				final List<StoreEntry> list = parse(arr);
				main.post(() -> {
					if (binding == null) return;
					binding.progress.setVisibility(View.GONE);
					all.clear();
					all.addAll(list);
					applyFilter(textOf(binding.search));
				});
			} catch (Exception e) {
				main.post(() -> {
					if (binding == null) return;
					binding.progress.setVisibility(View.GONE);
					if (all.isEmpty()) {
						Toast.makeText(requireContext(),
							"Store load failed: " + (e.getMessage() != null ? e.getMessage() : "network error"),
							Toast.LENGTH_LONG).show();
					}
				});
			}
		}).start();
	}

	private static String textOf(android.widget.EditText e) {
		return e.getText() != null ? e.getText().toString() : "";
	}

	private List<StoreEntry> parse(JSONArray arr) {
		List<StoreEntry> out = new ArrayList<>();
		if (arr == null) return out;
		for (int i = 0; i < arr.length(); i++) {
			JSONObject o = arr.optJSONObject(i);
			if (o == null) continue;
			StoreEntry e = new StoreEntry();
			e.id = o.optString("id", "");
			e.name = o.optString("name", e.id);
			e.description = o.optString("description", "");
			e.downloadUrl = firstNonEmpty(o.optString("downloadUrl", null), o.optString("download", null), o.optString("url", null));
			e.iconUrl = o.optString("icon", "");
			if (e.downloadUrl != null && !e.downloadUrl.isEmpty()) out.add(e);
		}
		return out;
	}

	private static String firstNonEmpty(String... values) {
		if (values == null) return "";
		for (String v : values) {
			if (v != null && !v.isEmpty() && !"null".equals(v)) return v;
		}
		return "";
	}

	private void applyFilter(String q) {
		final List<StoreEntry> next = new ArrayList<>();
		String needle = q == null ? "" : q.trim().toLowerCase(Locale.ROOT);
		for (StoreEntry e : all) {
			if (needle.isEmpty()
				|| (e.name != null && e.name.toLowerCase(Locale.ROOT).contains(needle))
				|| (e.description != null && e.description.toLowerCase(Locale.ROOT).contains(needle))) {
				next.add(e);
			}
		}
		DiffUtil.DiffResult diff = DiffUtil.calculateDiff(new DiffUtil.Callback() {
			@Override public int getOldListSize() { return filtered.size(); }
			@Override public int getNewListSize() { return next.size(); }
			@Override
			public boolean areItemsTheSame(int o, int n) {
				StoreEntry a = filtered.get(o), b = next.get(n);
				if (a.id != null && !a.id.isEmpty() && b.id != null && !b.id.isEmpty()) return a.id.equals(b.id);
				return Objects.equals(a.name, b.name) && Objects.equals(a.downloadUrl, b.downloadUrl);
			}
			@Override
			public boolean areContentsTheSame(int o, int n) {
				StoreEntry a = filtered.get(o), b = next.get(n);
				return Objects.equals(a.name, b.name)
					&& Objects.equals(a.description, b.description)
					&& Objects.equals(a.iconUrl, b.iconUrl)
					&& Objects.equals(a.downloadUrl, b.downloadUrl);
			}
		});
		filtered.clear();
		filtered.addAll(next);
		diff.dispatchUpdatesTo(adapter);
	}

	@SuppressWarnings("ResultOfMethodCallIgnored")
	private void install(StoreEntry entry) {
		Toast.makeText(requireContext(), "Downloading " + entry.name + "…", Toast.LENGTH_SHORT).show();
		new Thread(() -> {
			File zip = new File(requireContext().getCacheDir(), "store_" + System.currentTimeMillis() + ".zip");
			try {
				downloadTo(entry.downloadUrl, zip);
				ModManager.installMod(requireContext(), zip, entry.iconUrl, new ModManager.InstallCallback() {
					@Override
					public void onSuccess(ModManager.ModInfo mod) {
						main.post(() -> Toast.makeText(requireContext(), "Installed " + mod.name, Toast.LENGTH_SHORT).show());
						zip.delete();
					}
					@Override
					public void onFailure(String reason) {
						main.post(() -> Util.alert(requireContext(), "Install failed", reason, "OK", null));
						zip.delete();
					}
				});
			} catch (Exception e) {
				zip.delete();
				main.post(() -> Util.alert(requireContext(), "Download failed",
					e.getMessage() != null ? e.getMessage() : "unknown error", "OK", null));
			}
		}).start();
	}

	private void loadIcon(String url, ImageCallback cb) {
		if (url == null || url.isEmpty()) { cb.onBitmap(null); return; }
		new Thread(() -> {
			try {
				HttpURLConnection conn = (HttpURLConnection) new URL(url).openConnection();
				conn.setConnectTimeout(5000);
				conn.setReadTimeout(5000);
				conn.setRequestProperty("User-Agent", "Mozilla/5.0");
				conn.setInstanceFollowRedirects(true);
				if (conn.getResponseCode() != 200) { main.post(() -> cb.onBitmap(null)); return; }
				Bitmap bmp = BitmapFactory.decodeStream(conn.getInputStream());
				main.post(() -> cb.onBitmap(bmp));
			} catch (Exception e) {
				main.post(() -> cb.onBitmap(null));
			}
		}).start();
	}

	interface ImageCallback { void onBitmap(Bitmap bmp); }

	private static String httpGetStore() throws Exception {
		HttpURLConnection conn = (HttpURLConnection) new URL(
			"https://raw.githubusercontent.com/lw-kizii/_/main/launcher-data/store.json"
		).openConnection();
		conn.setRequestMethod("GET");
		conn.setConnectTimeout(10000);
		conn.setReadTimeout(10000);
		conn.setInstanceFollowRedirects(true);
		conn.setRequestProperty("User-Agent", "Mozilla/5.0");
		conn.setRequestProperty("Accept", "application/json,text/plain,*/*");
		if (conn.getResponseCode() != 200) throw new Exception("HTTP " + conn.getResponseCode());
		InputStream in = conn.getInputStream();
		String encoding = conn.getContentEncoding();
		if (encoding != null && encoding.toLowerCase(Locale.ROOT).contains("gzip")) in = new GZIPInputStream(in);
		try (BufferedReader reader = new BufferedReader(new InputStreamReader(in, StandardCharsets.UTF_8))) {
			StringBuilder sb = new StringBuilder();
			char[] buf = new char[4096];
			int n;
			while ((n = reader.read(buf)) != -1) sb.append(buf, 0, n);
			return sb.toString();
		}
	}

	private static void downloadTo(String urlStr, File dest) throws Exception {
		HttpURLConnection conn = (HttpURLConnection) new URL(urlStr).openConnection();
		conn.setConnectTimeout(15000);
		conn.setReadTimeout(60000);
		conn.setInstanceFollowRedirects(true);
		conn.setRequestProperty("User-Agent", "Mozilla/5.0");
		if (conn.getResponseCode() != 200) throw new Exception("HTTP " + conn.getResponseCode());
		try (InputStream in = conn.getInputStream(); FileOutputStream out = new FileOutputStream(dest)) {
			byte[] buf = new byte[8192];
			int n;
			while ((n = in.read(buf)) != -1) out.write(buf, 0, n);
		}
	}

	private static String sanitize(String raw) {
		return raw == null ? "[]" : raw.trim();
	}

	private static void writeCache(File file, String raw) {
		try (FileOutputStream out = new FileOutputStream(file)) {
			out.write(raw.getBytes(StandardCharsets.UTF_8));
		} catch (Exception ignored) {}
	}

	private static JSONArray readCache(File file) {
		if (file == null || !file.exists()) return null;
		try (FileInputStream in = new FileInputStream(file); ByteArrayOutputStream bos = new ByteArrayOutputStream()) {
			byte[] buf = new byte[4096];
			int n;
			while ((n = in.read(buf)) != -1) bos.write(buf, 0, n);
			return new JSONArray(sanitize(new String(bos.toByteArray(), StandardCharsets.UTF_8)));
		} catch (Exception e) {
			return null;
		}
	}

	private class Adapter extends RecyclerView.Adapter<Adapter.VH> {
		@NonNull
		@Override
		public VH onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
			return new VH(ItemStoreModBinding.inflate(LayoutInflater.from(parent.getContext()), parent, false));
		}

		@Override
		public void onBindViewHolder(@NonNull VH h, int position) {
			StoreEntry e = filtered.get(position);
			h.b.name.setText(e.name);
			h.b.desc.setText(e.description != null ? e.description : "");
			h.b.icon.setImageResource(R.drawable.ic_store);
			loadIcon(e.iconUrl, bmp -> { if (bmp != null) h.b.icon.setImageBitmap(bmp); });
			h.b.btnInstall.setOnClickListener(v -> install(e));
		}

		@Override
		public int getItemCount() { return filtered.size(); }

		class VH extends RecyclerView.ViewHolder {
			final ItemStoreModBinding b;
			VH(ItemStoreModBinding b) { super(b.getRoot()); this.b = b; }
		}
	}
}