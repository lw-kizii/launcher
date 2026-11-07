package net.kiwi.launcher;

import android.content.Context;

import androidx.annotation.NonNull;

import java.util.Objects;

public class API {

	public static native void init();
	public static native void initPaths(String internalFiles, String internalCache, String externalFiles);
	public static native void loadHooks();
	public static native void onModExit();

	public static String currentMod() {
		return MainActivity.currentMod();
	}

    public static class ModInfo {
        String rootPath;
        String id;
    }

    @NonNull
    public static ModInfo currentModInfo() {
        ModInfo info = new ModInfo();
        // Mod's root path
        Context ctx = MainActivity.getCurrentActivity();
        info.id = MainActivity.currentMod();
        info.rootPath = Objects.requireNonNull(ctx.getExternalFilesDir(null)).getAbsolutePath() + "/mods/" + info.id;
        return info;
    }
}
