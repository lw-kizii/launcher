package net.kiwi.launcher;

import android.content.Context;
import android.os.Build;
import android.util.Log;

import java.io.File;
import java.io.FileOutputStream;
import java.io.PrintWriter;
import java.io.StringWriter;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;

public final class AppLog {
	private static final String TAG = "AppLog";
	private static File sessionFile;
	private static final Object lock = new Object();

	private AppLog() {}

	public static File logsDir(Context ctx) {
		File base = ctx.getExternalFilesDir(null);
		if (base == null) base = ctx.getFilesDir();
		File dir = new File(base, "logs");
		if (!dir.exists()) dir.mkdirs();
		return dir;
	}

	public static void init(Context ctx) {
		synchronized (lock) {
			File dir = logsDir(ctx);
			String stamp = new SimpleDateFormat("yyyyMMdd_HHmmss", Locale.US).format(new Date());
			sessionFile = new File(dir, "session_" + stamp + ".log");
			String abi = MainActivity.getAbi();
			boolean is64 = abi != null && abi.contains("64");
			String mod = Launcher.currentMod();
			if (mod == null || mod.isEmpty()) mod = "(none)";
			writeRaw("===== session start =====\n"
				+ "time: " + new SimpleDateFormat("yyyy-MM-dd HH:mm:ss", Locale.US).format(new Date()) + "\n"
				+ "device: " + Build.MANUFACTURER + " " + Build.MODEL + "\n"
				+ "android: " + Build.VERSION.RELEASE + " (API " + Build.VERSION.SDK_INT + ")\n"
				+ "abi: " + abi + " (" + (is64 ? "64-bit" : "32-bit") + ")\n"
				+ "mod: " + mod + "\n"
				+ "==========================\n");
		}
	}

	public static File crashLogFile(Context ctx) {
		return new File(logsDir(ctx), "last_crash.log");
	}

	public static void i(String tag, String msg) {
		Log.i(tag, msg);
		append("I", tag, msg);
	}

	public static void e(String tag, String msg) {
		Log.e(tag, msg);
		append("E", tag, msg);
	}

	public static void e(String tag, String msg, Throwable t) {
		Log.e(tag, msg, t);
		StringWriter sw = new StringWriter();
		if (t != null) t.printStackTrace(new PrintWriter(sw));
		append("E", tag, msg + "\n" + sw);
	}

	public static void noteMod(String modId) {
		String m = modId != null && !modId.isEmpty() ? modId : "(vanilla)";
		append("I", "Mod", "active mod: " + m);
	}

	private static void append(String level, String tag, String msg) {
		String line = new SimpleDateFormat("HH:mm:ss.SSS", Locale.US).format(new Date())
			+ " " + level + "/" + tag + ": " + msg + "\n";
		writeRaw(line);
	}

	private static void writeRaw(String s) {
		synchronized (lock) {
			if (sessionFile == null) return;
			try (FileOutputStream out = new FileOutputStream(sessionFile, true)) {
				out.write(s.getBytes("UTF-8"));
			} catch (Exception ignored) {}
		}
	}
}
