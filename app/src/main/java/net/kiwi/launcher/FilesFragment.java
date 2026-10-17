package net.kiwi.launcher;

import android.app.Dialog;
import android.content.ClipData;
import android.content.Intent;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.GradientDrawable;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.text.InputType;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.animation.DecelerateInterpolator;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.Toast;

import androidx.activity.OnBackPressedCallback;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.content.FileProvider;
import androidx.fragment.app.Fragment;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import net.kiwi.launcher.databinding.FragmentFilesBinding;
import net.kiwi.launcher.databinding.ItemFileBinding;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Date;
import java.util.List;
import java.util.Locale;

public class FilesFragment extends Fragment {

	private static final int COL_FOLDER = 0xFF57F287;
	private static final int COL_FILE = 0xFF9CA3AF;
	private static final int COL_NAME = 0xFFE8E6F2;
	private static final long LONG_PRESS_MS = 280;

	private FragmentFilesBinding binding;
	private final List<File> entries = new ArrayList<>();
	private Adapter adapter;
	private File rootInternal;
	private File rootExternal;
	private File current;
	private boolean externalMode;
	private OnBackPressedCallback backCallback;
	private final Handler main = new Handler(Looper.getMainLooper());
	private final SimpleDateFormat dateFmt = new SimpleDateFormat("yyyy-MM-dd HH:mm", Locale.US);

	@Nullable
	@Override
	public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState) {
		binding = FragmentFilesBinding.inflate(inflater, container, false);
		adapter = new Adapter();
		binding.fileList.setLayoutManager(new LinearLayoutManager(requireContext()));
		binding.fileList.setHasFixedSize(true);
		binding.fileList.setItemViewCacheSize(16);
		binding.fileList.setAdapter(adapter);

		rootInternal = requireContext().getFilesDir();
		File ext = requireContext().getExternalFilesDir(null);
		rootExternal = ext != null ? ext : rootInternal;
		externalMode = false;
		current = rootInternal;

		binding.tabInternal.setOnClickListener(v -> switchRoot(false));
		binding.tabExternal.setOnClickListener(v -> switchRoot(true));
		binding.btnUp.setOnClickListener(v -> goUp());
		binding.btnCloseViewer.setOnClickListener(v -> hideViewer());

		backCallback = new OnBackPressedCallback(false) {
			@Override public void handleOnBackPressed() {
				if (binding.viewerPanel.getVisibility() == View.VISIBLE) hideViewer();
				else if (canGoUp()) goUp();
				else setEnabled(false);
			}
		};
		requireActivity().getOnBackPressedDispatcher().addCallback(getViewLifecycleOwner(), backCallback);

		updateTabs();
		refreshList();
		return binding.getRoot();
	}

	@Override
	public void onResume() {
		super.onResume();
		MainActivity.setTabSwipeEnabled(false);
		if (binding != null && binding.viewerPanel.getVisibility() != View.VISIBLE) refreshList();
	}

	@Override
	public void onPause() {
		MainActivity.setTabSwipeEnabled(true);
		super.onPause();
	}

	private void switchRoot(boolean external) {
		externalMode = external;
		current = externalMode ? rootExternal : rootInternal;
		updateTabs();
		refreshList();
	}

	private void updateTabs() {
		if (binding == null) return;
		binding.tabInternal.setTextColor(externalMode ? 0xFFBAC2DE : 0xFFCDD6F4);
		binding.tabInternal.setBackgroundColor(externalMode ? 0xFF1E1E2E : 0xFF2A2740);
		binding.tabExternal.setTextColor(externalMode ? 0xFFCDD6F4 : 0xFFBAC2DE);
		binding.tabExternal.setBackgroundColor(externalMode ? 0xFF2A2740 : 0xFF1E1E2E);
	}

	private boolean canGoUp() {
		if (current == null) return false;
		File root = externalMode ? rootExternal : rootInternal;
		return current.getAbsolutePath().startsWith(root.getAbsolutePath())
			&& !current.getAbsolutePath().equals(root.getAbsolutePath());
	}

	private void goUp() {
		if (!canGoUp()) return;
		File parent = current.getParentFile();
		if (parent != null) {
			current = parent;
			refreshList();
		}
	}

	private void refreshList() {
		if (binding == null || current == null) return;
		File root = externalMode ? rootExternal : rootInternal;
		String abs = current.getAbsolutePath();
		String rootAbs = root.getAbsolutePath();
		String rel = abs.equals(rootAbs) ? "/" : abs.substring(rootAbs.length());
		if (!rel.startsWith("/")) rel = "/" + rel;
		binding.path.setText((externalMode ? "ext" : "int") + rel);

		entries.clear();
		File[] kids = current.listFiles();
		if (kids != null) {
			Arrays.sort(kids, (a, b) -> {
				if (a.isDirectory() != b.isDirectory()) return a.isDirectory() ? -1 : 1;
				return a.getName().compareToIgnoreCase(b.getName());
			});
			entries.addAll(Arrays.asList(kids));
		}
		adapter.notifyDataSetChanged();
		backCallback.setEnabled(canGoUp() || binding.viewerPanel.getVisibility() == View.VISIBLE);
	}

	private void openEntry(File f) {
		if (f.isDirectory()) {
			current = f;
			refreshList();
			return;
		}
		String name = f.getName().toLowerCase(Locale.ROOT);
		if (isTextName(name)) showViewer(f);
		else Toast.makeText(requireContext(), formatSize(f.length()) + " · long-press to open", Toast.LENGTH_SHORT).show();
	}

	private static boolean isTextName(String name) {
		return name.endsWith(".txt") || name.endsWith(".log") || name.endsWith(".json")
			|| name.endsWith(".toml") || name.endsWith(".md") || name.endsWith(".xml")
			|| name.endsWith(".cfg") || name.endsWith(".ini") || name.endsWith(".properties")
			|| name.endsWith(".lua") || name.endsWith(".csv");
	}

	private void showContextMenu(File f) {
		if (getContext() == null) return;
		int pad = Util.dp(requireContext(), 16);
		int radius = Util.dp(requireContext(), 14);

		LinearLayout root = new LinearLayout(requireContext());
		root.setOrientation(LinearLayout.VERTICAL);
		root.setPadding(pad, pad, pad, pad);
		GradientDrawable bg = new GradientDrawable();
		bg.setColor(Util.CARD);
		bg.setCornerRadius(radius);
		bg.setStroke(Util.dp(requireContext(), 1), Util.BORDER);
		root.setBackground(bg);

		TextView title = new TextView(requireContext());
		title.setText(f.getName());
		title.setTextColor(Util.TEXT_MAIN);
		title.setTextSize(TypedValue.COMPLEX_UNIT_SP, 15);
		title.setTypeface(null, Typeface.BOLD);
		title.setPadding(0, 0, 0, Util.dp(requireContext(), 10));
		root.addView(title);

		Dialog dialog = new Dialog(requireContext());
		dialog.requestWindowFeature(Window.FEATURE_NO_TITLE);
		dialog.setContentView(root);
		if (dialog.getWindow() != null) {
			dialog.getWindow().setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
			dialog.getWindow().setLayout(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
		}

		String[] labels;
		Runnable[] actions;
		if (f.isFile()) {
			labels = new String[]{"Open as text", "Share", "Export", "Rename", "Delete"};
			actions = new Runnable[]{
				() -> showViewer(f),
				() -> shareFile(f),
				() -> exportFile(f),
				() -> renameFile(f),
				() -> deleteFile(f)
			};
		} else {
			labels = new String[]{"Rename", "Delete"};
			actions = new Runnable[]{
				() -> renameFile(f),
				() -> deleteFile(f)
			};
		}
		for (int i = 0; i < labels.length; i++) {
			final int idx = i;
			TextView row = new TextView(requireContext());
			row.setText(labels[i]);
			row.setTextColor(labels[i].equals("Delete") ? Util.ACCENT_RED : Util.TEXT_MAIN);
			row.setTextSize(TypedValue.COMPLEX_UNIT_SP, 15);
			row.setPadding(Util.dp(requireContext(), 8), Util.dp(requireContext(), 12), Util.dp(requireContext(), 8), Util.dp(requireContext(), 12));
			row.setBackground(selectableBg());
			row.setOnClickListener(v -> {
				dialog.dismiss();
				actions[idx].run();
			});
			root.addView(row);
		}
		dialog.show();
	}

	private android.graphics.drawable.Drawable selectableBg() {
		android.util.TypedValue out = new android.util.TypedValue();
		requireContext().getTheme().resolveAttribute(android.R.attr.selectableItemBackground, out, true);
		return requireContext().getDrawable(out.resourceId);
	}

	private void shareFile(File f) {
		try {
			Uri uri = FileProvider.getUriForFile(requireContext(),
				requireContext().getPackageName() + ".fileprovider", f);
			Intent share = new Intent(Intent.ACTION_SEND);
			share.setType("*/*");
			share.putExtra(Intent.EXTRA_STREAM, uri);
			share.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
			share.setClipData(ClipData.newRawUri(f.getName(), uri));
			startActivity(Intent.createChooser(share, "Share " + f.getName()));
		} catch (Exception e) {
			Toast.makeText(requireContext(), "Share failed", Toast.LENGTH_SHORT).show();
		}
	}

	private void exportFile(File f) {
		if (!f.isFile()) return;
		File destDir = new File(requireContext().getExternalFilesDir(null), "exports");
		if (!destDir.exists() && !destDir.mkdirs()) {
			Toast.makeText(requireContext(), "Export folder failed", Toast.LENGTH_SHORT).show();
			return;
		}
		File dest = new File(destDir, f.getName());
		try (FileInputStream in = new FileInputStream(f); FileOutputStream out = new FileOutputStream(dest)) {
			byte[] buf = new byte[8192];
			int n;
			while ((n = in.read(buf)) != -1) out.write(buf, 0, n);
			Toast.makeText(requireContext(), "Exported to exports/" + dest.getName(), Toast.LENGTH_SHORT).show();
		} catch (Exception e) {
			Toast.makeText(requireContext(), "Export failed", Toast.LENGTH_SHORT).show();
		}
	}

	private void renameFile(File f) {
		EditText input = new EditText(requireContext());
		input.setInputType(InputType.TYPE_CLASS_TEXT);
		input.setText(f.getName());
		input.setTextColor(Util.TEXT_MAIN);
		input.setHintTextColor(Util.TEXT_DIM);
		input.setSelectAllOnFocus(true);
		int pad = Util.dp(requireContext(), 16);
		input.setPadding(pad, pad, pad, pad);
		new android.app.AlertDialog.Builder(requireContext())
			.setTitle("Rename")
			.setView(input)
			.setPositiveButton("OK", (d, w) -> {
				String name = input.getText() != null ? input.getText().toString().trim() : "";
				if (name.isEmpty() || name.contains("/") || name.contains("\\")) {
					Toast.makeText(requireContext(), "Invalid name", Toast.LENGTH_SHORT).show();
					return;
				}
				File dest = new File(f.getParentFile(), name);
				if (dest.exists()) {
					Toast.makeText(requireContext(), "Already exists", Toast.LENGTH_SHORT).show();
					return;
				}
				if (f.renameTo(dest)) refreshList();
				else Toast.makeText(requireContext(), "Rename failed", Toast.LENGTH_SHORT).show();
			})
			.setNegativeButton("Cancel", null)
			.show();
	}

	private void deleteFile(File f) {
		Util.alert(requireContext(), "Delete " + f.getName() + "?",
			f.isDirectory() ? "Delete this folder and everything inside?" : "Delete this file?",
			"Delete", () -> {
				boolean ok = deleteRecursive(f);
				Toast.makeText(requireContext(), ok ? "Deleted" : "Delete failed", Toast.LENGTH_SHORT).show();
				refreshList();
			}, "Cancel", null);
	}

	private static boolean deleteRecursive(File file) {
		if (file == null || !file.exists()) return true;
		if (file.isDirectory()) {
			File[] kids = file.listFiles();
			if (kids != null) for (File c : kids) deleteRecursive(c);
		}
		return file.delete();
	}

	private void showViewer(File f) {
		if (binding == null) return;
		binding.viewerTitle.setText(f.getName());
		binding.viewerBody.setText("Loading…");
		View panel = binding.viewerPanel;
		panel.setVisibility(View.VISIBLE);
		float w = panel.getWidth() > 0 ? panel.getWidth() : binding.getRoot().getWidth();
		panel.setTranslationX(w > 0 ? w : 800);
		panel.animate().translationX(0).setDuration(180).setInterpolator(new DecelerateInterpolator()).start();
		backCallback.setEnabled(true);

		new Thread(() -> {
			String text = readText(f, 512 * 1024);
			main.post(() -> {
				if (binding == null) return;
				binding.viewerBody.setText(text);
			});
		}).start();
	}

	private void hideViewer() {
		if (binding == null) return;
		View panel = binding.viewerPanel;
		if (panel.getVisibility() != View.VISIBLE) return;
		float w = panel.getWidth() > 0 ? panel.getWidth() : binding.getRoot().getWidth();
		panel.animate().translationX(w).setDuration(160).setInterpolator(new DecelerateInterpolator())
			.withEndAction(() -> {
				panel.setVisibility(View.GONE);
				panel.setTranslationX(0);
				binding.viewerBody.setText("");
				backCallback.setEnabled(canGoUp());
			}).start();
	}

	private static String readText(File f, int maxBytes) {
		StringBuilder sb = new StringBuilder();
		try (BufferedReader r = new BufferedReader(new FileReader(f))) {
			char[] buf = new char[8192];
			int n;
			int total = 0;
			while ((n = r.read(buf)) != -1) {
				sb.append(buf, 0, n);
				total += n;
				if (total >= maxBytes) {
					sb.append("\n\n… truncated (").append(formatSize(f.length())).append(" total)");
					break;
				}
			}
		} catch (Exception e) {
			return "Failed to read: " + (e.getMessage() != null ? e.getMessage() : "error");
		}
		if (sb.length() == 0) return "(empty)";
		return sb.toString();
	}

	private static String formatSize(long bytes) {
		if (bytes < 1024) return bytes + " B";
		if (bytes < 1024 * 1024) return String.format(Locale.US, "%.1f KB", bytes / 1024.0);
		return String.format(Locale.US, "%.1f MB", bytes / (1024.0 * 1024.0));
	}

	private class Adapter extends RecyclerView.Adapter<Adapter.VH> {
		@NonNull
		@Override
		public VH onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
			return new VH(ItemFileBinding.inflate(LayoutInflater.from(parent.getContext()), parent, false));
		}

		@Override
		public void onBindViewHolder(@NonNull VH h, int position) {
			File f = entries.get(position);
			h.b.name.setText(f.getName());
			h.b.name.setTextColor(COL_NAME);
			if (f.isDirectory()) {
				h.b.icon.setImageResource(R.drawable.ic_folder);
				h.b.icon.setColorFilter(COL_FOLDER);
				h.b.meta.setText("Folder");
			} else {
				h.b.icon.setImageResource(R.drawable.ic_file);
				h.b.icon.setColorFilter(COL_FILE);
				String meta = formatSize(f.length());
				long mod = f.lastModified();
				if (mod > 0) meta += " · " + dateFmt.format(new Date(mod));
				h.b.meta.setText(meta);
			}
			h.itemView.setOnClickListener(v -> openEntry(f));
			h.itemView.setOnTouchListener(new View.OnTouchListener() {
				private final Runnable longPress = () -> showContextMenu(f);
				private long downAt;
				@Override
				public boolean onTouch(View v, MotionEvent e) {
					if (e.getAction() == MotionEvent.ACTION_DOWN) {
						downAt = SystemClock.uptimeMillis();
						main.removeCallbacks(longPress);
						main.postDelayed(longPress, LONG_PRESS_MS);
					} else if (e.getAction() == MotionEvent.ACTION_UP || e.getAction() == MotionEvent.ACTION_CANCEL) {
						main.removeCallbacks(longPress);
						if (e.getAction() == MotionEvent.ACTION_UP
							&& SystemClock.uptimeMillis() - downAt < LONG_PRESS_MS) {
							v.performClick();
						}
					}
					return true;
				}
			});
		}

		@Override
		public int getItemCount() { return entries.size(); }

		class VH extends RecyclerView.ViewHolder {
			final ItemFileBinding b;
			VH(ItemFileBinding b) { super(b.getRoot()); this.b = b; }
		}
	}

	/*
	TODO:
		- Add global permissions such as allow_networking...
		- Add clear cache
	 */
}
