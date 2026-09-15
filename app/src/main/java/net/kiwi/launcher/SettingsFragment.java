package net.kiwi.launcher;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;

import net.kiwi.launcher.databinding.FragmentSettingsBinding;

public class SettingsFragment extends Fragment {

	@Nullable
	@Override
	public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState) {
		return FragmentSettingsBinding.inflate(inflater, container, false).getRoot();
	}

	/*
	TODO:
		- Add launcher data exporting/importing
		- Add frame rate switcher
		- Add global permissions such as allow_networking...
		- Add clear cache
	 */
}
