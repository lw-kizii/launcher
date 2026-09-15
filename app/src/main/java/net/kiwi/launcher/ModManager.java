package net.kiwi.launcher;

import android.content.Context;
import android.net.Uri;

import java.io.BufferedInputStream;
import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.IOException;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;
import java.util.zip.ZipOutputStream;

public class ModManager {

	public interface InstallCallback {
		void onSuccess(ModInfo mod);
		void onFailure(String reason);
	}

	public static class ModInfo {
		public String id;
		public String name;
		public String version;
		public String description;
		public File dir;
		public List<String> screenshots = new ArrayList<>();

		public File iconFile() {
			File f = new File(dir, "icon.png");
			return f.exists() ? f : null;
		}

		public File savesDir() {
			File s = new File(dir, "saves");
			ensureDir(s);
			return s;
		}
	}

	public static List<ModInfo> listInstalledMods(Context context) {
		List<ModInfo> mods = new ArrayList<>();
		File[] modDirs = modsDir(context).listFiles(File::isDirectory);
		if (modDirs == null) return mods;
		for (File modDir : modDirs) {
			File propsFile = new File(modDir, "properties.toml");
			if (!propsFile.exists()) continue;
			ModInfo info = parseProperties(propsFile);
			if (info == null) continue;
			info.dir = modDir;
			mods.add(info);
		}
		mods.sort(Comparator.comparing(a -> a.name.toLowerCase(Locale.ROOT)));
		return mods;
	}

	public static ModInfo findById(Context context, String id) {
		if (id == null || id.isEmpty()) return null;
		for (ModInfo m : listInstalledMods(context)) {
			if (id.equals(m.id)) return m;
		}
		return null;
	}

	public static boolean deleteMod(ModInfo mod) {
		return mod != null && deleteRecursive(mod.dir);
	}

	public static void setLastPlayed(Context context, String modId) {
		if (modId == null) return;
		try (FileOutputStream out = new FileOutputStream(new File(context.getFilesDir(), "last_mod.txt"))) {
			out.write(modId.getBytes());
		} catch (IOException ignored) {}
	}

	public static String getLastPlayedId(Context context) {
		File f = new File(context.getFilesDir(), "last_mod.txt");
		if (!f.exists()) return "";
		try (BufferedReader r = new BufferedReader(new FileReader(f))) {
			String line = r.readLine();
			return line != null ? line.trim() : "";
		} catch (IOException e) {
			return "";
		}
	}

	public static void installMod(Context context, Uri zipUri, InstallCallback callback) {
		installMod(context, zipUri, null, callback);
	}

	public static void installMod(Context context, Uri zipUri, String iconUrl, InstallCallback callback) {
		new Thread(() -> {
			try {
				callback.onSuccess(doInstall(context, zipUri, null, iconUrl));
			} catch (Exception e) {
				callback.onFailure(e.getMessage() != null ? e.getMessage() : "unexpected error");
			}
		}).start();
	}

	public static void installMod(Context context, File zipFile, String iconUrl, InstallCallback callback) {
		new Thread(() -> {
			try {
				callback.onSuccess(doInstall(context, null, zipFile, iconUrl));
			} catch (Exception e) {
				callback.onFailure(e.getMessage() != null ? e.getMessage() : "unexpected error");
			}
		}).start();
	}

	private static ModInfo doInstall(Context context, Uri uri, File zipFile, String iconUrl) throws Exception {
		File staging = new File(context.getCacheDir(), "mod_staging_" + System.currentTimeMillis());
		ensureDir(staging);
		try {
			if (zipFile != null) extractZipFile(zipFile, staging);
			else extractZip(context, uri, staging);
			File propsFile = new File(staging, "properties.toml");
			if (!propsFile.exists()) throw new IOException("missing properties.toml");
			ModInfo info = parseProperties(propsFile);
			if (info == null || info.id == null || info.id.isEmpty()) throw new IOException("properties.toml missing [mod] id");

			File targetDir = new File(modsDir(context), sanitizeName(info.id));
			File backup = new File(context.getCacheDir(), "preserve_" + System.currentTimeMillis());
			String[] preserve = { "saves", "data" };
			for (String name : preserve) {
				File src = new File(targetDir, name);
				if (src.isDirectory()) copyRecursive(src, new File(backup, name));
			}

			deleteRecursive(targetDir);
			ensureDir(targetDir);
			copyRecursive(staging, targetDir);

			// Hopefully this works...
			for (String name : preserve) {
				File src = new File(backup, name);
				if (src.isDirectory()) {
					deleteRecursive(new File(targetDir, name));
					copyRecursive(src, new File(targetDir, name));
				}
			}
			deleteRecursive(backup);

			// Fetch icon from Github lol
			if (iconUrl != null && !iconUrl.isEmpty()) downloadIcon(iconUrl, new File(targetDir, "icon.png"));
			info.dir = targetDir;
			return info;
		} finally {
			deleteRecursive(staging);
		}
	}

	public static boolean exportSaves(ModInfo mod, File destZip) {
		if (mod == null || mod.dir == null) return false;
		File saves = new File(mod.dir, "saves");
		if (!saves.isDirectory()) return false;
		File[] kids = saves.listFiles();
		if (kids == null || kids.length == 0) return false;
		try {
			zipDirectory(saves, destZip);
			return destZip.exists() && destZip.length() > 0;
		} catch (Exception e) {
			return false;
		}
	}

	public static boolean importSaves(ModInfo mod, Uri uri, Context context) {
		if (mod == null || mod.dir == null || uri == null || context == null) return false;
		File saves = mod.savesDir();
		try {
			String name = uri.getLastPathSegment();
			if (name == null) name = "import";
			name = name.toLowerCase(Locale.ROOT);
			if (name.endsWith(".zip") || name.contains(".zip")) {
				extractZip(context, uri, saves);
			} else {
				String fileName = "save_" + System.currentTimeMillis();
				try {
					android.database.Cursor c = context.getContentResolver().query(uri, null, null, null, null);
					if (c != null) {
						if (c.moveToFirst()) {
							int idx = c.getColumnIndex(android.provider.OpenableColumns.DISPLAY_NAME);
							if (idx >= 0) {
								String dn = c.getString(idx);
								if (dn != null && !dn.isEmpty()) fileName = dn;
							}
						}
						c.close();
					}
				} catch (Exception ignored) {}
				File dest = new File(saves, fileName);
				try (InputStream in = context.getContentResolver().openInputStream(uri);
					FileOutputStream out = new FileOutputStream(dest)) {
					if (in == null) return false;
					byte[] buf = new byte[8192];
					int n;
					while ((n = in.read(buf)) != -1) out.write(buf, 0, n);
				}
			}
			return true;
		} catch (Exception e) {
			return false;
		}
	}

	private static ModInfo parseProperties(File propsFile) {
		Map<String, String> map = Util.parseTomlSection(propsFile, "mod");
		if (map.isEmpty()) return null;
		ModInfo info = new ModInfo();
		info.id = map.get("id");
		info.name = map.get("name");
		info.version = map.get("version");
		info.description = map.get("description");
		if (map.containsKey("screenshots"))
			info.screenshots.addAll(Util.splitCsv(map.get("screenshots")));
		if (info.name == null) info.name = info.id;
		return info;
	}

	private static File modsDir(Context context) {
		File dir = new File(context.getExternalFilesDir(null), "mods");
		ensureDir(dir);
		return dir;
	}

	static void doNothing() {}

	private static void ensureDir(File dir) {
		if (dir != null && !dir.exists() && !dir.mkdirs() && !dir.isDirectory()) {
			doNothing();
		}
	}

	// This is slow... and I don't know why.
	private static void downloadIcon(String urlStr, File dest) {
		try {
			HttpURLConnection conn = (HttpURLConnection) new URL(urlStr).openConnection();
			conn.setConnectTimeout(5000);
			conn.setReadTimeout(5000);
			conn.setRequestProperty("User-Agent", "Mozilla/5.0");
			if (conn.getResponseCode() != 200) return;
			try (InputStream in = conn.getInputStream(); FileOutputStream out = new FileOutputStream(dest)) {
				byte[] buffer = new byte[8192];
				int read;
				while ((read = in.read(buffer)) != -1) out.write(buffer, 0, read);
			}
		} catch (Exception ignored) {}
	}

	// Simple ZIP helpers, should be moved to Util honestly
	private static void extractZip(Context context, Uri uri, File destDir) throws Exception {
		try (InputStream is = context.getContentResolver().openInputStream(uri)) {
			if (is == null) throw new IOException("could not open file");
			inflateZip(is, destDir);
		}
	}
	private static void extractZipFile(File zipFile, File destDir) throws Exception {
		try (InputStream is = new FileInputStream(zipFile)) {
			inflateZip(is, destDir);
		}
	}
	private static void inflateZip(InputStream is, File destDir) throws Exception {
		try (ZipInputStream zis = new ZipInputStream(new BufferedInputStream(is))) {
			ZipEntry entry;
			while ((entry = zis.getNextEntry()) != null) {
				if (entry.isDirectory()) continue;
				String entryName = entry.getName();
				if (entryName.contains("..")) continue;
				File target = new File(destDir, entryName);
				File parent = target.getParentFile();
				if (parent != null) ensureDir(parent);
				try (FileOutputStream fos = new FileOutputStream(target)) {
					byte[] buffer = new byte[8192];
					int count;
					while ((count = zis.read(buffer)) != -1) fos.write(buffer, 0, count);
				}
				zis.closeEntry();
			}
		}
	}
	private static void zipDirectory(File sourceDir, File zipFile) throws IOException {
		try (ZipOutputStream zos = new ZipOutputStream(new FileOutputStream(zipFile))) {
			zipWalk(sourceDir, sourceDir, zos);
		}
	}
	private static void zipWalk(File root, File current, ZipOutputStream zos) throws IOException {
		File[] kids = current.listFiles();
		if (kids == null) return;
		for (File f : kids) {
			String rel = root.toURI().relativize(f.toURI()).getPath();
			if (f.isDirectory()) {
				if (!rel.endsWith("/")) rel += "/";
				zos.putNextEntry(new ZipEntry(rel));
				zos.closeEntry();
				zipWalk(root, f, zos);
			} else {
				zos.putNextEntry(new ZipEntry(rel));
				try (FileInputStream in = new FileInputStream(f)) {
					byte[] buf = new byte[8192];
					int n;
					while ((n = in.read(buf)) != -1) zos.write(buf, 0, n);
				}
				zos.closeEntry();
			}
		}
	}

	private static String sanitizeName(String raw) {
		if (raw == null) return "";
		return raw.trim().replaceAll("[^a-zA-Z0-9._-]", "_");
	}

	private static void copyFile(File src, File dst) throws IOException {
		try (InputStream in = new FileInputStream(src); FileOutputStream out = new FileOutputStream(dst)) {
			byte[] buffer = new byte[8192];
			int count;
			while ((count = in.read(buffer)) != -1) out.write(buffer, 0, count);
		}
	}

	private static void copyRecursive(File src, File dst) throws IOException {
		if (src.isDirectory()) {
			ensureDir(dst);
			File[] children = src.listFiles();
			if (children != null) {
				for (File child : children) copyRecursive(child, new File(dst, child.getName()));
			}
		} else {
			copyFile(src, dst);
		}
	}

	private static boolean deleteRecursive(File file) {
		if (file == null || !file.exists()) return true;
		if (file.isDirectory()) {
			File[] children = file.listFiles();
			if (children != null) {
				for (File child : children) deleteRecursive(child);
			}
		}
		return file.delete();
	}
}
