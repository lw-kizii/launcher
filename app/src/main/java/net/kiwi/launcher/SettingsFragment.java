package net.kiwi.launcher;

import android.content.Intent;
import android.content.SharedPreferences;
import android.net.Uri;
import android.os.Bundle;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;

import com.touchfoo.swordigo.Native;

import net.kiwi.launcher.databinding.FragmentModsBinding;
import net.kiwi.launcher.databinding.FragmentSettingsBinding;

public class SettingsFragment extends Fragment {

	static class FPSHandle {
		Button btn;
		int fps;

		public FPSHandle(Button btn, int fps) {
			this.btn = btn;
			this.fps = fps;
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

	@Nullable
	@Override
	public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState) {
		FragmentSettingsBinding binding = FragmentSettingsBinding.inflate(inflater, container, false);

		handleButtons(binding.fps30, binding.fps60, binding.fps90, binding.fps120, binding.fps180);;

		binding.discordbtn.setOnClickListener(l -> {
			// I don't trust Native.openUrl...
			Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("https://discord.gg/cF6Krcb28V"));
			startActivity(intent);
		});

		return binding.getRoot();
	}

	/*
	TODO:
		- Add launcher data exporting/importing
		- Add frame rate switcher
		- Add global permissions such as allow_networking...
		- Add clear cache
	 */
}
