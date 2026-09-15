package net.kiwi.launcher;

import android.graphics.Typeface;
import android.text.SpannableStringBuilder;
import android.text.Spanned;
import android.text.style.RelativeSizeSpan;
import android.text.style.StyleSpan;
import android.text.style.TypefaceSpan;
import android.text.style.URLSpan;
import android.widget.TextView;

import java.util.regex.Matcher;
import java.util.regex.Pattern;

public final class Markdown {
	private Markdown() {}

	// Regex patterns.
	private static final Pattern HEADING = Pattern.compile("^(#{1,3})\\s+(.*)$");
	private static final Pattern UL = Pattern.compile("^[-*+]\\s+(.*)$");
	private static final Pattern OL = Pattern.compile("^\\d+\\.\\s+(.*)$");
	private static final Pattern INLINE = Pattern.compile(
		"(\\*\\*(.+?)\\*\\*)|(\\*(.+?)\\*)|(`([^`]+)`)|(\\[([^\\]]+)\\]\\(([^)]+)\\))"
	);

	public static void set(TextView view, String md) {
		if (view == null) return;
		view.setText(parse(md));
		view.setMovementMethod(android.text.method.LinkMovementMethod.getInstance());
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
