package net.kiwi.launcher;

import android.app.Dialog;
import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.RippleDrawable;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.IOException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public final class Util {
	private Util() {}

	public static final int BG = 0xFF0F0E17;
	public static final int CARD = 0xFF1A1826;
	public static final int BORDER = 0xFF2A2740;
	public static final int TEXT_MAIN = 0xFFE8E6F2;
	public static final int TEXT_DIM = 0xFFA39EC0;
	public static final int ACCENT_GREEN = 0xFF57F287;
	public static final int ACCENT_BLUE = 0xFF5865F2;
	public static final int ACCENT_RED = 0xFFED4245;
	public static final int ACCENT_DARK = 0xFF0F0E17;

	public static int dp(Context ctx, int dp) {
		return (int) (dp * ctx.getResources().getDisplayMetrics().density);
	}

	public static void attachSystemBarsPadding(View root) {
		ViewCompat.setOnApplyWindowInsetsListener(root, (v, insets) -> {
			Insets bars = insets.getInsets(WindowInsetsCompat.Type.systemBars());
			v.setPadding(bars.left, bars.top, bars.right, bars.bottom);
			return insets;
		});
		if (ViewCompat.isAttachedToWindow(root)) {
			ViewCompat.requestApplyInsets(root);
		} else {
			root.addOnAttachStateChangeListener(new View.OnAttachStateChangeListener() {
				@Override public void onViewAttachedToWindow(View v) {
					ViewCompat.requestApplyInsets(v);
					v.removeOnAttachStateChangeListener(this);
				}
				@Override public void onViewDetachedFromWindow(View v) {}
			});
		}
	}

	public static Dialog alert(Context ctx, String title, String message, String positive, Runnable onPositive) {
		return alert(ctx, title, message, positive, onPositive, null, null);
	}

	public static Dialog alert(Context ctx, String title, String message,
			String positive, Runnable onPositive,
			String negative, Runnable onNegative) {
		int pad = dp(ctx, 20);
		int radius = dp(ctx, 16);
		int btnRadius = dp(ctx, 12);

		LinearLayout root = new LinearLayout(ctx);
		root.setOrientation(LinearLayout.VERTICAL);
		root.setPadding(pad, pad, pad, pad);
		GradientDrawable cardBg = new GradientDrawable();
		cardBg.setColor(CARD);
		cardBg.setCornerRadius(radius);
		cardBg.setStroke(dp(ctx, 1), BORDER);
		root.setBackground(cardBg);

		if (title != null && !title.isEmpty()) {
			TextView t = new TextView(ctx);
			t.setText(title);
			t.setTextColor(TEXT_MAIN);
			t.setTextSize(TypedValue.COMPLEX_UNIT_SP, 18);
			t.setTypeface(null, Typeface.BOLD);
			root.addView(t, matchWrap());
		}

		if (message != null && !message.isEmpty()) {
			TextView m = new TextView(ctx);
			m.setText(message);
			m.setTextColor(TEXT_DIM);
			m.setTextSize(TypedValue.COMPLEX_UNIT_SP, 14);
			m.setPadding(0, dp(ctx, title != null ? 12 : 0), 0, 0);
			root.addView(m, matchWrap());
		}

		LinearLayout actions = new LinearLayout(ctx);
		actions.setOrientation(LinearLayout.HORIZONTAL);
		actions.setGravity(Gravity.END);
		actions.setPadding(0, dp(ctx, 20), 0, 0);

		Dialog dialog = new Dialog(ctx);
		dialog.requestWindowFeature(Window.FEATURE_NO_TITLE);
		dialog.setContentView(root);
		Window w = dialog.getWindow();
		if (w != null) {
			w.setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
			w.setLayout(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
		}

		if (negative != null) {
			TextView neg = dialogButton(ctx, negative, CARD, TEXT_MAIN, BORDER, btnRadius);
			neg.setOnClickListener(v -> {
				dialog.dismiss();
				if (onNegative != null) onNegative.run();
			});
			LinearLayout.LayoutParams np = new LinearLayout.LayoutParams(
				ViewGroup.LayoutParams.WRAP_CONTENT, dp(ctx, 44));
			np.rightMargin = dp(ctx, 8);
			actions.addView(neg, np);
		}

		if (positive != null) {
			TextView pos = dialogButton(ctx, positive, ACCENT_GREEN, ACCENT_DARK, ACCENT_GREEN, btnRadius);
			pos.setOnClickListener(v -> {
				dialog.dismiss();
				if (onPositive != null) onPositive.run();
			});
			actions.addView(pos, new LinearLayout.LayoutParams(
				ViewGroup.LayoutParams.WRAP_CONTENT, dp(ctx, 44)));
		}

		root.addView(actions, matchWrap());
		dialog.show();
		return dialog;
	}

	private static TextView dialogButton(Context ctx, String label, int bg, int fg, int ripple, int radius) {
		TextView btn = new TextView(ctx);
		btn.setText(label);
		btn.setTextColor(fg);
		btn.setTextSize(TypedValue.COMPLEX_UNIT_SP, 14);
		btn.setTypeface(null, Typeface.BOLD);
		btn.setGravity(Gravity.CENTER);
		btn.setPadding(dp(ctx, 18), 0, dp(ctx, 18), 0);
		GradientDrawable content = new GradientDrawable();
		content.setCornerRadius(radius);
		content.setColor(bg);
		GradientDrawable mask = new GradientDrawable();
		mask.setCornerRadius(radius);
		mask.setColor(Color.WHITE);
		btn.setBackground(new RippleDrawable(ColorStateList.valueOf(ripple), content, mask));
		btn.setClickable(true);
		btn.setFocusable(true);
		return btn;
	}

	private static LinearLayout.LayoutParams matchWrap() {
		return new LinearLayout.LayoutParams(
			ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
	}

	private static final Pattern KV_BASIC = Pattern.compile("^\\s*(\\w+)\\s*=\\s*\"([^\"]*)\"\\s*$");
	private static final Pattern KV_SINGLE = Pattern.compile("^\\s*(\\w+)\\s*=\\s*'([^']*)'\\s*$");
	private static final Pattern KV_TRIPLE_OPEN = Pattern.compile("^\\s*(\\w+)\\s*=\\s*\"\"\"(.*)$");

	public static Map<String, String> parseTomlSection(File file, String section) {
		Map<String, String> out = new HashMap<>();
		boolean inSection = false;
		String want = "[" + section + "]";
		try (BufferedReader reader = new BufferedReader(new FileReader(file))) {
			String line;
			while ((line = reader.readLine()) != null) {
				String trimmed = line.trim();
				if (trimmed.isEmpty() || trimmed.startsWith("#")) continue;
				if (trimmed.startsWith("[")) {
					inSection = trimmed.equalsIgnoreCase(want);
					continue;
				}
				if (!inSection) continue;
				Matcher triple = KV_TRIPLE_OPEN.matcher(line);
				if (triple.matches()) {
					String key = triple.group(1);
					String rest = triple.group(2);
					StringBuilder body = new StringBuilder();
					int close = rest.indexOf("\"\"\"");
					if (close >= 0) {
						body.append(rest.substring(0, close));
					} else {
						if (!rest.isEmpty()) body.append(rest).append('\n');
						while ((line = reader.readLine()) != null) {
							int c = line.indexOf("\"\"\"");
							if (c >= 0) {
								body.append(line.substring(0, c));
								break;
							}
							body.append(line).append('\n');
						}
					}
					out.put(key, body.toString().replace("\r", "").trim());
					continue;
				}
				Matcher m = KV_BASIC.matcher(trimmed);
				if (!m.matches()) m = KV_SINGLE.matcher(trimmed);
				if (!m.matches()) continue;
				out.put(m.group(1), m.group(2));
			}
		} catch (IOException ignored) {}
		return out;
	}

	public static List<String> splitCsv(String value) {
		List<String> list = new ArrayList<>();
		if (value == null) return list;
		for (String part : value.split(",")) {
			String s = part.trim();
			if (!s.isEmpty()) list.add(s);
		}
		return list;
	}
}
