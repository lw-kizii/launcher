package net.kiwi.launcher;

import static android.view.View.VISIBLE;

import android.annotation.SuppressLint;
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
import android.view.animation.DecelerateInterpolator;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.ProgressBar;
import android.widget.TextView;
import android.widget.Toast;

import androidx.activity.OnBackPressedCallback;
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

import java.io.BufferedInputStream;
import java.io.BufferedReader;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Objects;
import java.util.concurrent.ConcurrentHashMap;

public class StoreFragment extends Fragment {

	private static StoreFragment active;

	private FragmentStoreBinding binding;
	private final List<StoreEntry> all = new ArrayList<>();
	private final List<StoreEntry> filtered = new ArrayList<>();
	private final Map<String, Integer> downloadCounts = new HashMap<>();
	private Adapter adapter;
	private final Handler main = new Handler(Looper.getMainLooper());
	private final ConcurrentHashMap<String, Bitmap> iconCache = new ConcurrentHashMap<>();
	private StoreEntry openEntry;
	private OnBackPressedCallback backCallback;

	static class StoreEntry {
		String id, name, author, description, longDescription, version, downloadUrl, iconUrl;
		int downloads = -1;
		boolean installing;
		ItemStoreModBinding binding;
	}

	public static void closeDetailIfOpen() {
		if (active != null) active.hideDetail(false);
	}

	public static boolean isDetailOpen() {
		return active != null && active.openEntry != null
			&& active.binding != null
			&& active.binding.detailPanel.getVisibility() == View.VISIBLE;
	}

	@Nullable
	@Override
	public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState) {
		binding = FragmentStoreBinding.inflate(inflater, container, false);
		adapter = new Adapter();
		binding.storeList.setLayoutManager(new LinearLayoutManager(requireContext()));
		binding.storeList.setHasFixedSize(true);
		binding.storeList.setItemViewCacheSize(12);
		binding.storeList.setAdapter(adapter);
		binding.search.addTextChangedListener(new TextWatcher() {
			@Override public void beforeTextChanged(CharSequence s, int start, int count, int after) {}
			@Override public void onTextChanged(CharSequence s, int start, int before, int count) {
				applyFilter(s != null ? s.toString() : "");
			}
			@Override public void afterTextChanged(Editable s) {}
		});
		binding.btnClose.setOnClickListener(v -> hideDetail(true));
		binding.detailInstall.setOnClickListener(v -> {
			if (openEntry != null) install(openEntry);
		});
		backCallback = new OnBackPressedCallback(false) {
			@Override public void handleOnBackPressed() { hideDetail(true); }
		};
		requireActivity().getOnBackPressedDispatcher().addCallback(getViewLifecycleOwner(), backCallback);
		return binding.getRoot();
	}

	@Override
	public void onResume() {
		super.onResume();
		active = this;
		refreshStore();
		syncSwipe();
	}

	@Override
	public void onPause() {
		if (active == this) active = null;
		super.onPause();
	}

	private void syncSwipe() {
		boolean lock = isDetailOpen();
		MainActivity.setTabSwipeEnabled(!lock);
	}

	private void refreshStore() {
		if (binding == null || getContext() == null) return;
		if (all.isEmpty()) {
			binding.progress.setVisibility(VISIBLE);
		}

		new Thread(() -> {
			try {
				Map<String, Integer> counts = fetchDownloadCounts();
				final List<StoreEntry> list = fetchStoreEntriesFromFolder();
				for (StoreEntry e : list) {
					Integer c = counts.get(assetKey(e.downloadUrl));
					if (c == null && e.id != null) c = counts.get(e.id.toLowerCase(Locale.ROOT));
					if (c == null && e.name != null) c = counts.get(e.name.toLowerCase(Locale.ROOT));
					if (c != null) e.downloads = c;
				}
				list.sort((a, b) -> Integer.compare(b.downloads, a.downloads));
				main.post(() -> {
					if (binding == null) return;
					binding.progress.setVisibility(View.GONE);
					downloadCounts.clear();
					downloadCounts.putAll(counts);
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

	private static String assetKey(String url) {
		if (url == null) return "";
		int slash = url.lastIndexOf('/');
		return slash >= 0 ? url.substring(slash + 1).toLowerCase(Locale.ROOT) : url.toLowerCase(Locale.ROOT);
	}

	private Map<String, Integer> fetchDownloadCounts() {
		Map<String, Integer> map = new HashMap<>();
		try {
			HttpURLConnection conn = openGet("https://raw.githubusercontent.com/lw-kizii/_/main/launcher-data/downloads.txt", 8000, 10000);
			if (conn.getResponseCode() != 200) return map;
			try (BufferedReader reader = new BufferedReader(new InputStreamReader(conn.getInputStream(), StandardCharsets.UTF_8))) {
				String line;
				while ((line = reader.readLine()) != null) {
					line = line.trim();
					if (line.isEmpty() || line.startsWith("#") || line.startsWith("//")) continue;
					String[] parts = line.split("[=: \t]+", 2);
					if (parts.length == 2) {
						String key = parts[0].trim().toLowerCase(Locale.ROOT);
						String val = parts[1].trim();
						try {
							int count = Integer.parseInt(val);
							map.put(key, count);
							if (key.endsWith(".zip")) {
								map.put(key.substring(0, key.length() - 4), count);
							} else {
								map.put(key + ".zip", count);
							}
						} catch (Exception ignored) {}
					}
				}
			}
		} catch (Exception ignored) {}
		return map;
	}

	private void applyFilter(String q) {
		final List<StoreEntry> next = new ArrayList<>();
		String needle = q == null ? "" : q.trim().toLowerCase(Locale.ROOT);
		for (StoreEntry e : all) {
			if (needle.isEmpty()
				|| (e.name != null && e.name.toLowerCase(Locale.ROOT).contains(needle))
				|| (e.author != null && e.author.toLowerCase(Locale.ROOT).contains(needle))
				|| (e.description != null && e.description.toLowerCase(Locale.ROOT).contains(needle))
				|| (e.id != null && e.id.toLowerCase(Locale.ROOT).contains(needle))) {
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
					&& Objects.equals(a.author, b.author)
					&& Objects.equals(a.description, b.description)
					&& Objects.equals(a.iconUrl, b.iconUrl)
					&& Objects.equals(a.downloadUrl, b.downloadUrl)
					&& Objects.equals(a.version, b.version)
					&& a.downloads == b.downloads
					&& a.installing == b.installing;
			}
		});
		filtered.clear();
		filtered.addAll(next);
		diff.dispatchUpdatesTo(adapter);
	}

	private void showDetail(StoreEntry entry) {
		if (binding == null || getContext() == null) return;
		openEntry = entry;
		binding.detailName.setText(entry.name != null ? entry.name : entry.id);
		if (entry.author != null && !entry.author.trim().isEmpty()) {
			binding.detailAuthor.setText("by " + entry.author.trim());
			binding.detailAuthor.setVisibility(View.VISIBLE);
		} else {
			binding.detailAuthor.setVisibility(View.GONE);
		}
		binding.detailVersion.setText(entry.version != null && !entry.version.isEmpty() ? "v" + entry.version : "");
		binding.detailIcon.setImageResource(R.drawable.ic_store);
		loadIcon(entry.iconUrl, bmp -> { if (bmp != null) binding.detailIcon.setImageBitmap(bmp); });
		String body = entry.longDescription != null && !entry.longDescription.isEmpty()
			? entry.longDescription
			: (entry.description != null ? entry.description : "No description");
		Markdown.set(binding.detailBody, body, this::loadIconInto);
		View panel = binding.detailPanel;
		panel.setVisibility(View.VISIBLE);
		float w = panel.getWidth() > 0 ? panel.getWidth() : binding.getRoot().getWidth();
		panel.setTranslationX(w > 0 ? w : 800);
		panel.animate().translationX(0).setDuration(200).setInterpolator(new DecelerateInterpolator()).start();
		if (backCallback != null) backCallback.setEnabled(true);
		MainActivity.setTabSwipeEnabled(false);
	}

	private void hideDetail(boolean animate) {
		if (binding == null) return;
		View panel = binding.detailPanel;
		if (panel.getVisibility() != View.VISIBLE) return;
		Runnable end = () -> {
			panel.setVisibility(View.GONE);
			panel.setTranslationX(0);
			openEntry = null;
			MainActivity.setTabSwipeEnabled(true);
		};
		if (!animate) {
			panel.animate().cancel();
			end.run();
		} else {
			float w = panel.getWidth() > 0 ? panel.getWidth() : binding.getRoot().getWidth();
			panel.animate().translationX(w).setDuration(180).setInterpolator(new DecelerateInterpolator())
				.withEndAction(end).start();
		}
		if (backCallback != null) backCallback.setEnabled(false);
	}

	@SuppressLint("SetTextI18n")
	@SuppressWarnings("ResultOfMethodCallIgnored")
	private void install(StoreEntry entry) {
		if (entry.installing) return;
		entry.installing = true;
		ItemStoreModBinding b = entry.binding;
		if (b != null) setInstallingUi(b, true, 0);

		Toast.makeText(requireContext(), "Downloading " + entry.name + "...", Toast.LENGTH_SHORT).show();
		new Thread(() -> {
			File zip = new File(requireContext().getCacheDir(), "store_" + System.currentTimeMillis() + ".zip");
			try {
				downloadTo(entry.downloadUrl, zip, pct -> main.post(() -> {
					if (entry.binding != null) setInstallingUi(entry.binding, true, pct);
				}));
				ModManager.installMod(requireContext(), zip, entry.iconUrl, new ModManager.InstallCallback() {
					@Override
					public void onSuccess(ModManager.ModInfo mod) {
						main.post(() -> {
							entry.installing = false;
							Toast.makeText(requireContext(), "Installed " + mod.name, Toast.LENGTH_SHORT).show();
							if (entry.binding != null) setInstallingUi(entry.binding, false, 100);
						});
						zip.delete();
					}
					@Override
					public void onFailure(String reason) {
						main.post(() -> {
							entry.installing = false;
							Util.alert(requireContext(), "Install failed", reason, "OK", null);
							if (entry.binding != null) setInstallingUi(entry.binding, false, 0);
						});
						zip.delete();
					}
				});
			} catch (Exception e) {
				zip.delete();
				main.post(() -> {
					entry.installing = false;
					Util.alert(requireContext(), "Download failed",
						e.getMessage() != null ? e.getMessage() : "unknown error", "OK", null);
					if (entry.binding != null) setInstallingUi(entry.binding, false, 0);
				});
			}
		}).start();
	}

	@SuppressLint("SetTextI18n")
	private void setInstallingUi(ItemStoreModBinding b, boolean on, int pct) {
		if (b == null) return;
		b.progressRow.setVisibility(on ? View.VISIBLE : View.GONE);
		b.progressBar.setProgress(Math.max(0, Math.min(100, pct)));
		b.progressText.setText(on ? (pct + "%") : "");
		b.btnInstall.setEnabled(!on);
		b.btnInstall.setClickable(!on);
		int tint = on ? 0xFF6B7280 : 0xFF57F287;
		b.btnInstall.setColorFilter(tint);
		b.btnInstall.setAlpha(on ? 0.55f : 1f);
		b.downloads.setTextColor(on ? 0xFF6B7280 : 0xFF9CA3AF);
	}

	private void loadIconInto(String url, ImageView target) {
		loadIcon(url, bmp -> { if (bmp != null && target != null) target.setImageBitmap(bmp); });
	}

	private void loadIcon(String urlStr, ImageCallback cb) {
		if (urlStr == null || urlStr.trim().isEmpty()) { cb.onBitmap(null); return; }
		String url = urlStr.trim();
		if (!url.startsWith("http://") && !url.startsWith("https://")) {
			if (url.startsWith("/")) url = url.substring(1);
			url = "https://raw.githubusercontent.com/lw-kizii/_/main/launcher-data/" + url;
		}
		Bitmap cached = iconCache.get(url);
		if (cached != null && !cached.isRecycled()) { cb.onBitmap(cached); return; }
		final String finalUrl = url;
		new Thread(() -> {
			try {
				HttpURLConnection conn = openGet(finalUrl, 8000, 8000);
				if (conn.getResponseCode() != 200) { main.post(() -> cb.onBitmap(null)); return; }
				Bitmap bmp = BitmapFactory.decodeStream(new BufferedInputStream(conn.getInputStream()));
				if (bmp != null) iconCache.put(finalUrl, bmp);
				main.post(() -> cb.onBitmap(bmp));
			} catch (Exception e) {
				main.post(() -> cb.onBitmap(null));
			}
		}).start();
	}

	interface ImageCallback { void onBitmap(Bitmap bmp); }
	interface ProgressCallback { void onProgress(int pct); }

	private static HttpURLConnection openGet(String urlStr, int connectMs, int readMs) throws Exception {
		HttpURLConnection conn = (HttpURLConnection) new URL(urlStr).openConnection();
		conn.setRequestMethod("GET");
		conn.setConnectTimeout(connectMs);
		conn.setReadTimeout(readMs);
		conn.setInstanceFollowRedirects(true);
		conn.setRequestProperty("User-Agent", "Mozilla/5.0 (Linux; Android 13) AppleWebKit/537.36");
		conn.setRequestProperty("Accept", "*/*");
		return conn;
	}

	private static List<StoreEntry> fetchStoreEntriesFromFolder() {
		List<StoreEntry> list = new ArrayList<>();
		List<String> tomlUrls = new ArrayList<>();
		try {
			HttpURLConnection conn = openGet("https://api.github.com/repos/lw-kizii/_/contents/launcher-data/mods", 10000, 10000);
			conn.setRequestProperty("Accept", "application/vnd.github+json");
			if (conn.getResponseCode() == 200) {
				String body;
				try (BufferedReader r = new BufferedReader(new InputStreamReader(conn.getInputStream(), StandardCharsets.UTF_8))) {
					StringBuilder sb = new StringBuilder();
					char[] buf = new char[8192];
					int n;
					while ((n = r.read(buf)) != -1) sb.append(buf, 0, n);
					body = sb.toString();
				}
				JSONArray files = new JSONArray(body);
				for (int i = 0; i < files.length(); i++) {
					JSONObject f = files.optJSONObject(i);
					if (f == null) continue;
					String name = f.optString("name", "");
					String downloadUrl = f.optString("download_url", "");
					if (name.endsWith(".toml") && !downloadUrl.isEmpty()) {
						tomlUrls.add(downloadUrl);
					}
				}
			}
		} catch (Exception ignored) {}

		if (tomlUrls.isEmpty()) {
			tomlUrls.add("https://raw.githubusercontent.com/lw-kizii/_/main/launcher-data/mods/net.kiwi.cli.toml");
		}

		for (String url : tomlUrls) {
			try {
				HttpURLConnection tomlConn = openGet(url, 8000, 8000);
				if (tomlConn.getResponseCode() != 200) continue;
				String tomlStr;
				try (BufferedReader r = new BufferedReader(new InputStreamReader(tomlConn.getInputStream(), StandardCharsets.UTF_8))) {
					StringBuilder sb = new StringBuilder();
					char[] buf = new char[8192];
					int n;
					while ((n = r.read(buf)) != -1) sb.append(buf, 0, n);
					tomlStr = sb.toString();
				}
				StoreEntry entry = parseToml(tomlStr);
				if (entry != null && entry.downloadUrl != null && !entry.downloadUrl.isEmpty()) {
					if (entry.id == null || entry.id.isEmpty()) {
						int slash = url.lastIndexOf('/');
						String fn = slash >= 0 ? url.substring(slash + 1) : url;
						entry.id = fn.replace(".toml", "");
					}
					if (entry.name == null || entry.name.isEmpty()) entry.name = entry.id;
					list.add(entry);
				}
			} catch (Exception ignored) {}
		}
		return list;
	}

	private static StoreEntry parseToml(String content) {
		StoreEntry e = new StoreEntry();
		if (content == null || content.isEmpty()) return e;
		Map<String, String> map = Util.parseTomlSection(content, "");
		if (map.isEmpty()) map = Util.parseTomlSection(content, "mod");
		e.id = map.get("id");
		e.name = map.get("name");
		e.author = map.containsKey("author") ? map.get("author") : map.get("authors");
		e.description = map.get("description");
		e.longDescription = map.containsKey("longDescription") ? map.get("longDescription") : map.get("long_description");
		e.version = map.get("version");
		e.downloadUrl = map.containsKey("downloadUrl") ? map.get("downloadUrl") : (map.containsKey("download") ? map.get("download") : map.get("url"));
		e.iconUrl = map.containsKey("iconUrl") ? map.get("iconUrl") : (map.containsKey("icon") ? map.get("icon") : map.get("icon_url"));
		return e;
	}

	private void downloadTo(String urlStr, File dest, ProgressCallback cb) throws Exception {
		HttpURLConnection conn = openGet(urlStr, 15000, 120000);
		if (conn.getResponseCode() != HttpURLConnection.HTTP_OK) throw new Exception("HTTP " + conn.getResponseCode());
		long contentLength = conn.getContentLengthLong();
		try (InputStream in = new BufferedInputStream(conn.getInputStream(), 65536);
		     FileOutputStream out = new FileOutputStream(dest)) {
			byte[] buf = new byte[65536];
			int n;
			long totalRead = 0;
			long lastUi = 0;
			while ((n = in.read(buf)) != -1) {
				out.write(buf, 0, n);
				totalRead += n;
				long now = System.currentTimeMillis();
				if (cb != null && now - lastUi > 120) {
					lastUi = now;
					int pct = contentLength > 0 ? (int) ((totalRead * 100) / contentLength) : 0;
					cb.onProgress(pct);
				}
			}
		}
		if (cb != null) cb.onProgress(100);
	}

	private static String formatCount(int n) {
		if (n >= 1000000) return String.format(Locale.US, "%.1fM", n / 1000000.0); // Never going to be possible
		if (n >= 1000) return String.format(Locale.US, "%.2fK", n / 1000.0); // 1.xxK
		return String.valueOf(n);
	}

	private class Adapter extends RecyclerView.Adapter<Adapter.VH> {
		@NonNull
		@Override
		public VH onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
			return new VH(ItemStoreModBinding.inflate(LayoutInflater.from(parent.getContext()), parent, false));
		}

		@SuppressLint("SetTextI18n")
		@Override
		public void onBindViewHolder(@NonNull VH h, int position) {
			StoreEntry e = filtered.get(position);
			e.binding = h.b;
			h.b.name.setText(e.name);
			if (e.author != null && !e.author.trim().isEmpty()) {
				h.b.author.setText("by " + e.author.trim());
				h.b.author.setVisibility(View.VISIBLE);
			} else {
				h.b.author.setVisibility(View.GONE);
			}
			h.b.desc.setText(e.description != null ? e.description : "");
			if (e.downloads >= 0) h.b.downloads.setText(StoreFragment.formatCount(e.downloads));
			else h.b.downloads.setText("0");
			setInstallingUi(h.b, e.installing, e.installing ? h.b.progressBar.getProgress() : 0);
			h.b.icon.setImageResource(R.drawable.ic_store);
			loadIcon(e.iconUrl, bmp -> {
				if (bmp != null && e.binding == h.b) h.b.icon.setImageBitmap(bmp);
			});
			h.b.btnInstall.setOnClickListener(v -> install(e));
			h.itemView.setOnClickListener(v -> showDetail(e));
		}

		@Override
		public int getItemCount() { return filtered.size(); }

		class VH extends RecyclerView.ViewHolder {
			final ItemStoreModBinding b;
			VH(ItemStoreModBinding b) { super(b.getRoot()); this.b = b; }
		}
	}
}
