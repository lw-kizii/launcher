package net.kiwi.launcher;

import android.annotation.SuppressLint;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;

public class Launcher {

	public static ModManager.ModInfo currentMod;
	@SuppressLint("StaticFieldLeak") // Ehh, I'll deal with this later
    private static View sGearButton;

	public static String currentMod() {
		return currentMod != null && currentMod.id != null ? currentMod.id : "";
	}

	public static void initGameButtons(MainActivity activity) {
		activity.runOnUiThread(() -> {
			try {
				ImageView gear = new ImageView(activity);
				gear.setImageResource(R.drawable.ic_manage);
				gear.setColorFilter(0xFFFFFFFF);
				gear.setAlpha(0.5f);
				gear.setPadding(4, 4, 4, 4);
				gear.setOnClickListener(v -> MainActivity.returnToLauncher());
				int size = Util.dp(activity, 28);
				FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(size, size);
				params.gravity = Gravity.TOP | Gravity.CENTER_HORIZONTAL;
				activity.addContentView(gear, params);
				sGearButton = gear;
			} catch (Exception ignored) {}
		});
	}

	public static void destroyGameButtons() {
		if (sGearButton != null) {
			try {
				ViewGroup parent = (ViewGroup) sGearButton.getParent();
				if (parent != null) parent.removeView(sGearButton);
			} catch (Exception ignored) {}
			sGearButton = null;
		}
	}
}
