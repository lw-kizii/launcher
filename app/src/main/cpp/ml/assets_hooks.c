#include "hook.h"
#include "ml.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <zlib.h>

HOOK_SYMBOL(
	NewByteBufferFromAA,
	"_ZN5Caver29NewByteBufferFromAndroidAssetERKNSt6__ndk112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEEPj",
	void*, (String *file, uint *param_2)
) {
	/* scl, scene, pvr and pod */
	FILE *f = ML_fetch_asset(file);
	if (!f) goto bailout;

	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (size < 0) goto bailout;
	void *buf = malloc((size_t)size);
	if (!buf) goto bailout;
	size_t readbytes = fread(buf, 1, (size_t)size, f);
	fclose(f);
	*param_2 = (uint)readbytes;
	return buf;

	bailout:
	if (f) fclose(f);
	return orig_NewByteBufferFromAA(file, param_2);
}

HOOK_SYMBOL(
	BinaryFile_Open,
	"_ZN5Caver10BinaryFile4OpenERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEENS0_4ModeEb",
	uint, (void *this, String *filename, int mode, bool use_asset)
) {
	FILE *f = ML_fetch_asset(filename);
	if (!f) return orig_BinaryFile_Open(this, filename, mode, use_asset);
	int fd = dup(fileno(f)); // Don't kill the gzdopen!
	fclose(f);
	if (fd < 0) return 0;
	const char *m = (mode == 1) ? "wb" : "rb";
	void *gz = gzdopen(fd, m);
	if (!gz) { close(fd); return 0; }
	*(int *)this = 2;
	*(void **)(this + 8) = gz;
	*(int *)(this + 0x10) = 0;
	return 1;
}

HOOK_SYMBOL(
	GetAudioFileData,
	"_ZN5Caver16GetAudioFileDataERKNSt6__ndk112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEEPNS_11AudioBuffer12BufferFormatEPPvPiSE_",
	bool, (String *path, int *fmt, void **data, int *size, int *rate)
) {
	FILE *f = ML_fetch_asset(path);
	if (!f) return orig_GetAudioFileData(path, fmt, data, size, rate);
	unsigned char hdr[0x2c]; // WAV header... 0x2c
	if (fread(hdr, 1, 0x2c, f) != 0x2c) {
		fclose(f);
		return false;
	}

	if (*(uint *)(hdr + 0) != 0x46464952 || *(uint *)(hdr + 8) != 0x45564157 || *(uint *)(hdr + 12) != 0x20746d66 || *(uint *)(hdr + 36) != 0x61746164) {
		fclose(f);
		return false;
	}

	uint datasz = *(uint *)(hdr + 40); // wav + 0x40 == sizeof raw PCM!! (Subchunk2Size)
	void *buf = malloc(datasz);

	// Edge case.
	// No memory or the wav file was shorter
	if (!buf || fread(buf, 1, datasz, f) != datasz) {
		free(buf); // free our buffer!
		fclose(f); // ...and close the file too.
		return false; // no valid data.
	}
	fclose(f);

	short ch = *(short *)(hdr + 22); // 22: NumChannels | Mono = 1, Stereo = 2, etc.
	short bits = *(short *)(hdr + 34); // 34: BitsPerSample | 8 bits = 8, 16 bits = 16, etc.
	int sr = *(int *)(hdr + 24); // 24: SampleRate | 8000, 44100, etc.

	int bf = 0; // default to an invalid format!!
	if (bits == 16) bf = (ch == 1) ? 2 : 4;
	if (bits == 8) bf = (ch == 1) ? 1 : 3;

	*fmt = bf; // BufferFormat that the audio system expects
	*data = buf; // Pointer to Audio Data
	*size = (int)datasz; // Size of Audio Data
	*rate = sr; // Sample Rate for OpenAL

	return true; // success ^^
}

HOOK_SYMBOL(
	FileExistsAtPath,
	"_ZN5Caver16FileExistsAtPathERKNSt6__ndk112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE",
	bool, (String *path)
) {
	const char *p = String_get(path);
	if (!p || !p[0]) return orig_FileExistsAtPath(path);
	if (ML_is_save_path(p)) {
		String s;
		ML_redirect_path(&s, p);
		bool ret = orig_FileExistsAtPath(&s);
		String_destroy(&s);
		return ret;
	}
	if (p[0] != '/') {
		FILE *f = ML_fetch_asset(path);
		if (f) { fclose(f); return true; }
	}
	return orig_FileExistsAtPath(path);
}
