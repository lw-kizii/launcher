package net.kiwi.launcher;

import android.content.Context;
import android.graphics.Typeface;
import android.text.SpannableStringBuilder;
import android.text.Spanned;
import android.text.method.LinkMovementMethod;
import android.text.style.RelativeSizeSpan;
import android.text.style.StyleSpan;
import android.text.style.TypefaceSpan;
import android.text.style.URLSpan;
import android.util.TypedValue;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import java.util.ArrayList;
import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public final class Markdown {
	private Markdown() {}

	// Regex patterns.
	private static final Pattern HEADING = Pattern.compile("^(#{1,3})\\s+(.*)$");
	private static final Pattern UL = Pattern.compile("^[-*+]\\s+(.*)$");
	private static final Pattern OL = Pattern.compile("^\\d+\\.\\s+(.*)$");
	private static final Pattern IMG = Pattern.compile("^\\[img:(.+)]$");
	private static final Pattern INLINE = Pattern.compile(
		"(\\*\\*(.+?)\\*\\*)|(\\*(.+?)\\*)|(`([^`]+)`)|(\\[([^\\]]+)\\]\\(([^)]+)\\))"
	);

	public interface ImageLoader {
		void load(String url, ImageView target);
	}

	public static void set(TextView view, String md) {
		if (view == null) return;
		view.setText(parse(md));
		view.setMovementMethod(LinkMovementMethod.getInstance());
	}

	public static void set(LinearLayout container, String md, ImageLoader loader) {
		if (container == null) return;
		container.removeAllViews();
		if (md == null || md.isEmpty()) return;
		Context ctx = container.getContext();
		String normalized = md.replace("\r\n", "\n").replace('\r', '\n');
		String[] lines = normalized.split("\n", -1);
		SpannableStringBuilder buf = new SpannableStringBuilder();
		Runnable flush = () -> {
			if (buf.length() == 0) return;
			TextView tv = textView(ctx);
			tv.setText(new SpannableStringBuilder(buf));
			tv.setMovementMethod(LinkMovementMethod.getInstance());
			container.addView(tv, matchWrap());
			buf.clear();
		};
		for (int i = 0; i < lines.length; i++) {
			String line = lines[i];
			if (line == null) continue;
			Matcher img = IMG.matcher(line.trim());
			if (img.matches()) {
				flush.run();
				String payload = img.group(1);
				if (payload != null) addImages(container, payload, loader);
				continue;
			}
			if (i > 0 && buf.length() > 0) buf.append('\n');
			Matcher h = HEADING.matcher(line);
			if (h.matches()) {
				String marks = h.group(1);
				String body = h.group(2);
				if (marks == null || body == null) continue;
				int level = marks.length();
				int start = buf.length();
				appendInline(buf, body);
				int end = buf.length();
				if (end > start) {
					buf.setSpan(new StyleSpan(Typeface.BOLD), start, end, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
					float scale = level == 1 ? 1.3f : level == 2 ? 1.15f : 1.05f;
					buf.setSpan(new RelativeSizeSpan(scale), start, end, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
				}
				continue;
			}
			Matcher ul = UL.matcher(line);
			if (ul.matches()) {
				String body = ul.group(1);
				if (body != null) appendInline(buf, "- " + body);
				continue;
			}
			Matcher ol = OL.matcher(line);
			if (ol.matches()) {
				String body = ol.group(1);
				if (body != null) appendInline(buf, "- " + body);
				continue;
			}
			if (line.trim().isEmpty()) {
				if (buf.length() > 0) buf.append('\n');
				continue;
			}
			appendInline(buf, line);
		}
		flush.run();
	}

	private static void addImages(LinearLayout container, String payload, ImageLoader loader) {
		Context ctx = container.getContext();
		String[] parts = payload.split(",");
		List<String> urls = new ArrayList<>();
		for (String p : parts) {
			String u = p.trim();
			if (!u.isEmpty()) urls.add(u);
		}
		if (urls.isEmpty()) return;
		if (urls.size() == 1) {
			ImageView iv = imageView(ctx);
			LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
				ViewGroup.LayoutParams.MATCH_PARENT, dp(ctx, 160));
			lp.topMargin = dp(ctx, 8);
			lp.bottomMargin = dp(ctx, 8);
			container.addView(iv, lp);
			if (loader != null) loader.load(urls.get(0), iv);
			return;
		}
		LinearLayout row = new LinearLayout(ctx);
		row.setOrientation(LinearLayout.HORIZONTAL);
		LinearLayout.LayoutParams rowLp = new LinearLayout.LayoutParams(
			ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
		rowLp.topMargin = dp(ctx, 8);
		rowLp.bottomMargin = dp(ctx, 8);
		container.addView(row, rowLp);
		int gap = dp(ctx, 6);
		for (int i = 0; i < urls.size(); i++) {
			ImageView iv = imageView(ctx);
			LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(0, dp(ctx, 140), 1f);
			if (i > 0) lp.leftMargin = gap;
			row.addView(iv, lp);
			if (loader != null) loader.load(urls.get(i), iv);
		}
	}

	private static TextView textView(Context ctx) {
		TextView tv = new TextView(ctx);
		tv.setTextColor(0xFFE8E6F2);
		tv.setTextSize(TypedValue.COMPLEX_UNIT_SP, 14);
		tv.setLineSpacing(dp(ctx, 2), 1f);
		return tv;
	}

	private static ImageView imageView(Context ctx) {
		ImageView iv = new ImageView(ctx);
		iv.setScaleType(ImageView.ScaleType.CENTER_CROP);
		iv.setBackgroundColor(0xFF1A1826);
		iv.setAdjustViewBounds(true);
		return iv;
	}

	private static LinearLayout.LayoutParams matchWrap() {
		return new LinearLayout.LayoutParams(
			ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
	}

	private static int dp(Context ctx, int v) {
		return (int) (v * ctx.getResources().getDisplayMetrics().density);
	}

	// Simple Parser
	public static CharSequence parse(String md) {
		if (md == null || md.isEmpty()) return "";
		SpannableStringBuilder out = new SpannableStringBuilder();
		String normalized = md.replace("\r\n", "\n").replace('\r', '\n');
		String[] lines = normalized.split("\n", -1);
		for (int i = 0; i < lines.length; i++) {
			if (i > 0) out.append('\n');
			String line = lines[i];
			if (line == null) continue;
			Matcher img = IMG.matcher(line.trim());
			if (img.matches()) continue;
			Matcher h = HEADING.matcher(line);
			if (h.matches()) {
				String marks = h.group(1);
				String body = h.group(2);
				if (marks == null || body == null) continue;
				int level = marks.length();
				int start = out.length();
				appendInline(out, body);
				int end = out.length();
				if (end > start) {
					out.setSpan(new StyleSpan(Typeface.BOLD), start, end, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
					float scale = level == 1 ? 1.3f : level == 2 ? 1.15f : 1.05f;
					out.setSpan(new RelativeSizeSpan(scale), start, end, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
				}
				continue;
			}
			Matcher ul = UL.matcher(line);
			if (ul.matches()) {
				String body = ul.group(1);
				if (body != null) appendInline(out, "- " + body);
				continue;
			}
			Matcher ol = OL.matcher(line);
			if (ol.matches()) {
				String body = ol.group(1);
				if (body != null) appendInline(out, "- " + body);
				continue;
			}
			if (line.trim().isEmpty()) continue;
			appendInline(out, line);
		}
		return out;
	}

	private static void appendInline(SpannableStringBuilder out, String text) {
		if (text == null || text.isEmpty()) return;
		Matcher m = INLINE.matcher(text);
		int last = 0;
		while (m.find()) {
			if (m.start() > last) out.append(text.substring(last, m.start()));
			int start = out.length();
			if (m.group(1) != null && m.group(2) != null) {
				out.append(m.group(2));
				out.setSpan(new StyleSpan(Typeface.BOLD), start, out.length(), Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
			} else if (m.group(3) != null && m.group(4) != null) {
				out.append(m.group(4));
				out.setSpan(new StyleSpan(Typeface.ITALIC), start, out.length(), Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
			} else if (m.group(5) != null && m.group(6) != null) {
				out.append(m.group(6));
				out.setSpan(new TypefaceSpan("monospace"), start, out.length(), Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
			} else if (m.group(7) != null && m.group(8) != null && m.group(9) != null) {
				out.append(m.group(8));
				out.setSpan(new URLSpan(m.group(9)), start, out.length(), Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
			}
			last = m.end();
		}
		if (last < text.length()) out.append(text.substring(last));
	}
}
