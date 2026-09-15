package net.kiwi.launcher;

import android.app.Activity;
import android.content.Context;
import android.content.res.AssetFileDescriptor;
import android.media.AudioAttributes;
import android.media.MediaPlayer;

import java.io.File;
import java.io.FileInputStream;


// Probably a bad idea but I'll start moving methods from Native to here!
public class Port {

	private static MediaPlayer player;
	private static String currentTrack = "";
	private static String pendingTrack = "";
	private static boolean preparing;
	private static boolean playWhenReady;
	private static boolean looping;
	private static float volume = 1f;

	private static void ensure() {
		if (player != null) return;
		player = new MediaPlayer();
		player.setAudioAttributes(new AudioAttributes.Builder()
			.setContentType(AudioAttributes.CONTENT_TYPE_MUSIC)
			.setUsage(AudioAttributes.USAGE_GAME)
			.build());
		player.setOnPreparedListener(mp -> {
			preparing = false;
			if (!currentTrack.equals(pendingTrack)) {
				prepare();
				return;
			}
			if (!preparing) {
				player.setLooping(looping);
				player.setVolume(volume, volume);
			}
			if (playWhenReady) {
				playWhenReady = false;
				try { player.start(); } catch (IllegalStateException ignored) {}
			}
		});
		player.setOnErrorListener((mp, what, extra) -> {
			preparing = false;
			return true;
		});
	}

	public static boolean loadFile(String track) {
		if (track == null) return false;
		ensure();
		pendingTrack = track;
		playWhenReady = false;
		return preparing || prepare();
	}

	public static void play() {
		ensure();
		if (preparing) {
			playWhenReady = true;
			return;
		}
		try { player.start(); } catch (IllegalStateException ignored) {}
	}

	public static void pause() {
		playWhenReady = false;
		if (player == null || preparing || currentTrack.isEmpty()) return;
		try { player.pause(); } catch (IllegalStateException ignored) {}
	}

	public static void stop() {
		playWhenReady = false;
		if (player == null || preparing || currentTrack.isEmpty()) return;
		try {
			player.stop();
			player.reset();
		} catch (IllegalStateException ignored) {}
		currentTrack = "";
	}

	public static void setLooping(boolean loop) {
		looping = loop;
		if (player != null && !preparing) player.setLooping(loop);
	}

	public static void setVolume(float vol) {
		volume = vol;
		if (player != null && !preparing) player.setVolume(vol, vol);
	}

	public static void onGamePause() {
		pause();
	}

	public static void onGameResume() {
		if (player == null || preparing || currentTrack.isEmpty()) return;
		try { player.start(); } catch (IllegalStateException ignored) {}
	}

	public static void onGameStop() {
		playWhenReady = false;
		preparing = false;
		if (player != null) {
			try {
				player.stop();
				player.reset();
			} catch (IllegalStateException ignored) {}
		}
		currentTrack = "";
		pendingTrack = "";
	}

	private static boolean prepare() {
		if (currentTrack.equals(pendingTrack) && !currentTrack.isEmpty()) return true;
		ensure();
		try {
			if (!currentTrack.isEmpty()) {
				player.stop();
				player.reset();
			}
			File modFile = resolveModFile(pendingTrack);
			if (modFile != null) {
				try (FileInputStream fis = new FileInputStream(modFile)) {
					player.setDataSource(fis.getFD());
				}
			} else if (!openFromApk(pendingTrack)) {
				currentTrack = "";
				return false;
			}
			preparing = true;
			currentTrack = pendingTrack;
			player.prepareAsync();
			return true;
		} catch (Exception e) {
			preparing = false;
			currentTrack = "";
			return false;
		}
	}

	private static File resolveModFile(String track) {
		Activity act = MainActivity.getCurrentActivity();
		if (act == null) return null;
		String modId = MainActivity.currentMod();
		if (modId == null || modId.isEmpty()) return null;
		File ext = act.getExternalFilesDir(null);
		if (ext == null) return null;
		File dir = new File(ext, "mods/" + modId + "/music");
		String norm = track.replace('-', '_');
		File[] candidates = {
			new File(dir, norm + ".mp3"),
			new File(dir, norm),
			new File(dir, track + ".mp3"),
			new File(dir, track),
			new File(dir, "music_" + norm + ".mp3"),
			new File(dir, "music_" + track + ".mp3")
		};
		for (File f : candidates) {
			if (f.isFile()) return f;
		}
		return null;
	}

	// This is way better than extraction !!!!
	// I'll probably follow the same pattern for loading libs
	private static boolean openFromApk(String track) {
		Activity act = MainActivity.getCurrentActivity();
		if (act == null) return false;
		String base = track.replace('-', '_');
		if (base.startsWith("music_")) base = base.substring(6);
		String[] names = {"music_" + base, base};
		try {
			Context game = act.createPackageContext("com.touchfoo.swordigo", Context.CONTEXT_IGNORE_SECURITY);
			for (String name : names) {
				int id = game.getResources().getIdentifier(name, "raw", "com.touchfoo.swordigo");
				if (id == 0) continue;
				AssetFileDescriptor afd = game.getResources().openRawResourceFd(id);
				if (afd == null) continue;
				player.setDataSource(afd.getFileDescriptor(), afd.getStartOffset(), afd.getLength());
				afd.close();
				return true;
			}
		} catch (Exception ignored) {}
		return false;
	}
}
