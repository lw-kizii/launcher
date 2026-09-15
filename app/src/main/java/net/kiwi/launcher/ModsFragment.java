package net.kiwi.launcher;

import android.content.Intent;
import android.graphics.BitmapFactory;
import android.net.Uri;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import net.kiwi.launcher.databinding.FragmentModsBinding;
import net.kiwi.launcher.databinding.ItemModBinding;

import java.io.File;
import java.util.ArrayList;
import java.util.List;

public class ModsFragment extends Fragment {

	private FragmentModsBinding binding;
	private final List<ModManager.ModInfo> mods = new ArrayList<>();
	private Adapter adapter;

	@Nullable
	@Override
	public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState) {
		binding = FragmentModsBinding.inflate(inflater, container, false);
		adapter = new Adapter();
		binding.modList.setLayoutManager(new LinearLayoutManager(requireContext()));
		binding.modList.setAdapter(adapter);
		binding.btnInstall.setOnClickListener(v -> openFilePicker());
		return binding.getRoot();
	}

	@Override
	public void onResume() {
		super.onResume();
		refresh();
	}

	public void refresh() {
		if (binding == null || getContext() == null) return;
		mods.clear();
		mods.addAll(ModManager.listInstalledMods(requireContext()));
		adapter.notifyDataSetChanged();
		binding.modCount.setText(mods.isEmpty() ? "" : String.valueOf(mods.size()));
		String lastId = ModManager.getLastPlayedId(requireContext());
		ModManager.ModInfo last = ModManager.findById(requireContext(), lastId);
		if (last != null) {
			binding.lastPlayed.setText("Last played: " + last.name);
		} else {
			binding.lastPlayed.setText(mods.isEmpty() ? "No mods installed" : "Tap a mod to open it");
		}
	}

	private void openMod(ModManager.ModInfo mod) {
		Launcher.currentMod = mod;
		if (getActivity() instanceof MainActivity) {
			((MainActivity) getActivity()).showCurrentModTab();
		}
	}

	private void confirmDelete(ModManager.ModInfo mod) {
		Util.alert(requireContext(),
			"Uninstall " + mod.name + "?",
			"This removes the mod folder and everything inside it.",
			"Uninstall",
			() -> {
				boolean ok = ModManager.deleteMod(mod);
				Toast.makeText(requireContext(), ok ? "Uninstalled " + mod.name : "Couldn't remove", Toast.LENGTH_SHORT).show();
				if (Launcher.currentMod != null && mod.id != null && mod.id.equals(Launcher.currentMod.id)) {
					Launcher.currentMod = null;
				}
				refresh();
			},
			"Cancel",
			null);
	}

	private void openFilePicker() {
		Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
		intent.addCategory(Intent.CATEGORY_OPENABLE);
		intent.setType("application/zip");
		intent.putExtra(Intent.EXTRA_MIME_TYPES, new String[]{"application/zip", "application/octet-stream", "*/*"});
		startActivityForResult(intent, 9999);
	}

	@Override
	public void onActivityResult(int requestCode, int resultCode, @Nullable Intent data) {
		super.onActivityResult(requestCode, resultCode, data);
		// 9999 should be the file picker request...
		if (requestCode != 9999 || resultCode != android.app.Activity.RESULT_OK
			|| data == null || data.getData() == null) return;
		Uri uri = data.getData();
		Toast.makeText(requireContext(), "Installing…", Toast.LENGTH_SHORT).show();
		ModManager.installMod(requireContext(), uri, new ModManager.InstallCallback() {
			@Override
			public void onSuccess(ModManager.ModInfo mod) {
				if (getActivity() == null) return;
				getActivity().runOnUiThread(() -> {
					Toast.makeText(requireContext(), "Installed " + mod.name, Toast.LENGTH_SHORT).show();
					Launcher.currentMod = mod;
					refresh();
					if (getActivity() instanceof MainActivity) {
						((MainActivity) getActivity()).showCurrentModTab();
					}
				});
			}

			@Override
			public void onFailure(String reason) {
				if (getActivity() == null) return;
				getActivity().runOnUiThread(() ->
					Util.alert(requireContext(), "Install failed", reason, "OK", null));
			}
		});
	}

	private class Adapter extends RecyclerView.Adapter<Adapter.VH> {
		@NonNull
		@Override
		public VH onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
			return new VH(ItemModBinding.inflate(LayoutInflater.from(parent.getContext()), parent, false));
		}

		@Override
		public void onBindViewHolder(@NonNull VH h, int position) {
			ModManager.ModInfo mod = mods.get(position);
			h.b.name.setText(mod.name);
			h.b.version.setText(mod.version != null ? "v" + mod.version : "");
			File icon = mod.iconFile();
			if (icon != null) {
				h.b.icon.setImageBitmap(BitmapFactory.decodeFile(icon.getAbsolutePath()));
			} else {
				h.b.icon.setImageResource(R.drawable.ic_mods);
			}
			h.itemView.setOnClickListener(v -> openMod(mod));
			h.b.btnDelete.setOnClickListener(v -> confirmDelete(mod));
		}

		@Override
		public int getItemCount() {
			return mods.size();
		}

		class VH extends RecyclerView.ViewHolder {
			final ItemModBinding b;
			VH(ItemModBinding b) {
				super(b.getRoot());
				this.b = b;
			}
		}
	}
}
