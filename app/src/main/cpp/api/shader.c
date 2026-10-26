// =============================================================================
// shader.c — ANGLE post-process shader INFRASTRUCTURE
//
// Contributed by Raijin
//
// Responsibilities (launcher side only):
//   • ANGLE renderer detection
//   • EGL/GLES2 secondary context management
//   • GLSL program compilation, linking, uniform/attrib location caching
//   • Two-pass rendering: GODRAY_PASS (half-res FBO, cached on scene change)
//                         FRAGMENT   (full-res, every frame)
//   • Scene_Draw hook → triggers post-process each frame; tracks scene pointer
//     for godray dirty detection
//   • LightComponent hooks → feeds directional + point light data as uniforms
//   • Lua API: Shader.isAngle() / .enable() / .load() / .reload()
//
// NO visual effect GLSL lives here.
// All shader code is provided by mods via resources/shader.glsl.
// If the mod shader has an #ifdef GODRAY_PASS section the launcher compiles it
// into a separate half-resolution program and caches the result in a FBO,
// passing it back as u_godray (sampler2D) in the FRAGMENT pass.
// Without GODRAY_PASS the system falls back to a standard single-pass blit.
// =============================================================================

#include "shader.h"
#include "hook.h"
#include "java.h"
#include "lauxlib.h"
#include "CameraController.h"
#include "Camera.h"
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <android/log.h>

// ── EGL / context state ───────────────────────────────────────────────────────
static EGLDisplay g_dpy  = EGL_NO_DISPLAY;
static EGLContext g_es1  = EGL_NO_CONTEXT;   // game's context
static EGLContext g_es2  = EGL_NO_CONTEXT;   // our post-process context
static EGLSurface g_draw = EGL_NO_SURFACE;
static EGLSurface g_read = EGL_NO_SURFACE;

// ── Scene capture texture (full-res, updated every frame) ─────────────────────
static GLuint g_tex   = 0;
static int    g_tex_w = 0;
static int    g_tex_h = 0;

// ── Uniform / attrib locations (one struct per program) ───────────────────────
typedef struct {
	GLint pos, tex;          // attribs: a_pos, a_tex
	GLint utex;              // u_tex (sampler2D, unit 0)
	GLint ures;              // u_resolution
	GLint utexel;            // u_texel
	GLint utime;             // u_time
	GLint udir_active;       // u_dirLightActive
	GLint udir_dir;          // u_dirLightDir
	GLint udir_color;        // u_dirLightColor
	GLint udir_intensity;    // u_dirLightIntensity
	GLint upt_count;         // u_pointLightCount
	GLint upt_pos;           // u_pointLightPos[8]
	GLint upt_color;         // u_pointLightColor[8]
	// World-space uniforms
	GLint uvp;               // u_viewProjection  (mat4, column-major)
	GLint uivp;              // u_invViewProjection (mat4, column-major)
	GLint ucam;              // u_cameraPos (vec2, world-space centre of screen)
} ShaderLocs;

#define LOCS_INIT {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}

// ── Composite program (FRAGMENT pass, full-res, every frame) ──────────────────
static GLuint     g_prog      = 0;
static ShaderLocs g_comp_locs = LOCS_INIT;
static GLint      g_ugodray   = -1;   // u_godray sampler (unit 1) in composite

// ── Godray program (GODRAY_PASS, half-res FBO, cached) ────────────────────────
static GLuint     g_gr_prog   = 0;
static ShaderLocs g_gr_locs   = LOCS_INIT;
static GLuint     g_gr_fbo    = 0;
static GLuint     g_gr_tex    = 0;
static int        g_gr_w      = 0;
static int        g_gr_h      = 0;
static int        g_has_gr    = 0;   // 1 when shader has GODRAY_PASS section

// ── Godray dirty tracking ─────────────────────────────────────────────────────
static void *g_last_scene   = NULL;
static int   g_gr_dirty     = 1;
static int   g_frame_count  = 0;
// Recompute godray at most every 2 s at 60 fps even without a scene change.
// Ensures it tracks slow camera pans without per-frame cost.
#define GODRAY_REFRESH_FRAMES 120

// ── Shader file tracking ──────────────────────────────────────────────────────
static int    g_shader_enabled = 1;
static char   g_loaded_path[512] = {0};
static time_t g_loaded_mtime     = 0;
static char   g_custom_path[512] = {0};

// ── Directional light state (updated by LightComponent_Enable hook) ───────────
static int   g_dir_active    = 0;
static float g_dir_x         = -0.3f;
static float g_dir_y         =  0.7f;
static float g_dir_r         =  1.0f;
static float g_dir_g         =  0.9f;
static float g_dir_b         =  0.75f;
static float g_dir_intensity =  1.0f;

// ── Point lights ──────────────────────────────────────────────────────────────
typedef struct {
	int     active;
	Vector3 world_pos;
	float   intensity, r, g, b, radius;
} PointLightInfo;

#define MAX_POINT_LIGHTS 8
static PointLightInfo g_point_lights[MAX_POINT_LIGHTS];

// ── Quad geometry ─────────────────────────────────────────────────────────────
static const float quad_verts[] = {
	-1.0f, -1.0f, 0.0f, 0.0f,
	 1.0f, -1.0f, 1.0f, 0.0f,
	-1.0f,  1.0f, 0.0f, 1.0f,
	 1.0f,  1.0f, 1.0f, 1.0f,
};

// ── Default passthrough vertex shader ─────────────────────────────────────────
static const char *default_vsrc =
	"attribute vec2 a_pos;\n"
	"attribute vec2 a_tex;\n"
	"varying vec2 v_tex;\n"
	"void main() {\n"
	"    gl_Position = vec4(a_pos, 0.0, 1.0);\n"
	"    v_tex = a_tex;\n"
	"}\n";

// =============================================================================
// LightComponent hooks
// =============================================================================

HOOK_SYMBOL(
	LightComponent_Enable,
	"_ZN5Caver14LightComponent6EnableEPNS_16RenderingContextEi",
	void, (void *this_ptr, void *ctx, int index)
) {
	if (this_ptr && index >= 0 && index < MAX_POINT_LIGHTS) {
		int type       = *(int   *)((char *)this_ptr + archSplit(0x40, 0x68));
		float intensity= *(float *)((char *)this_ptr + archSplit(0x44, 0x6c));
		float *col     =  (float *)((char *)this_ptr + archSplit(0x48, 0x70));
		void  *owner   = *(void **)((char *)this_ptr + archSplit(0x18, 0x28));

		if (type == 2) {
			g_dir_active    = 1;
			g_dir_r         = col ? col[0] : 1.0f;
			g_dir_g         = col ? col[1] : 0.9f;
			g_dir_b         = col ? col[2] : 0.75f;
			g_dir_intensity = intensity;
			if (owner) {
				Vector3 *opos = (Vector3 *)((char *)owner + archSplit(0x4c, 0x80));
				float dx = opos->x, dy = opos->y, dz = opos->z;
				float len = sqrtf(dx*dx + dy*dy + dz*dz);
				if (len > 0.001f) { g_dir_x = dx/len; g_dir_y = dy/len; }
			}
			__android_log_print(ANDROID_LOG_INFO, "LightHook",
				"DirLight col=(%.2f,%.2f,%.2f) int=%.2f dir=(%.2f,%.2f)",
				g_dir_r, g_dir_g, g_dir_b, g_dir_intensity, g_dir_x, g_dir_y);
		} else if (type == 3 && owner) {
			Vector3 *opos = (Vector3 *)((char *)owner + archSplit(0x4c, 0x80));
			float off_x   = *(float *)((char *)this_ptr + archSplit(0x6c, 0x98));
			float off_y   = *(float *)((char *)this_ptr + archSplit(0x70, 0x9c));
			float off_z   = *(float *)((char *)this_ptr + archSplit(0x74, 0xa0));
			g_point_lights[index].active      = 1;
			g_point_lights[index].world_pos.x = opos->x + off_x;
			g_point_lights[index].world_pos.y = opos->y + off_y;
			g_point_lights[index].world_pos.z = opos->z + off_z;
			g_point_lights[index].intensity   = intensity;
			g_point_lights[index].r           = col ? col[0] : 1.0f;
			g_point_lights[index].g           = col ? col[1] : 0.8f;
			g_point_lights[index].b           = col ? col[2] : 0.3f;
			g_point_lights[index].radius      = 280.0f;
			__android_log_print(ANDROID_LOG_INFO, "LightHook",
				"PointLight[%d] (%.1f,%.1f,%.1f) int=%.2f col=(%.2f,%.2f,%.2f)",
				index,
				g_point_lights[index].world_pos.x,
				g_point_lights[index].world_pos.y,
				g_point_lights[index].world_pos.z,
				intensity, g_point_lights[index].r,
				g_point_lights[index].g, g_point_lights[index].b);
		}
	}
	orig_LightComponent_Enable(this_ptr, ctx, index);
}

HOOK_SYMBOL(
	LightComponent_Disable,
	"_ZN5Caver14LightComponent7DisableEPNS_16RenderingContextE",
	void, (void *this_ptr, void *ctx)
) {
	if (this_ptr) {
		int index = *(int *)((char *)this_ptr + archSplit(0x78, 0xa4));
		if (index >= 0 && index < MAX_POINT_LIGHTS)
			g_point_lights[index].active = 0;
	}
	orig_LightComponent_Disable(this_ptr, ctx);
}

// =============================================================================
// Helpers
// =============================================================================

static char *read_file_text(const char *path) {
	FILE *f = fopen(path, "rb");
	if (!f) return NULL;
	fseek(f, 0, SEEK_END);
	long sz = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (sz <= 0) { fclose(f); return NULL; }
	char *buf = (char *)malloc((size_t)sz + 1);
	if (!buf) { fclose(f); return NULL; }
	size_t rd = fread(buf, 1, (size_t)sz, f);
	buf[rd] = '\0';
	fclose(f);
	return buf;
}

static GLuint compile_shader_stage(GLenum type, const char *src) {
	GLuint s = glCreateShader(type);
	if (!s) return 0;
	glShaderSource(s, 1, &src, NULL);
	glCompileShader(s);
	GLint ok = 0;
	glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		char log[512];
		glGetShaderInfoLog(s, sizeof(log), NULL, log);
		__android_log_print(ANDROID_LOG_ERROR, "ShaderLoader",
			"Compile error (type=%d): %s", type, log);
		glDeleteShader(s);
		return 0;
	}
	return s;
}

// Fill a ShaderLocs struct from a linked program.
static void lookup_locs(GLuint prog, ShaderLocs *L) {
	L->pos = glGetAttribLocation(prog, "a_pos");
	L->tex = glGetAttribLocation(prog, "a_tex");

	L->utex = glGetUniformLocation(prog, "u_tex");
	if (L->utex < 0) L->utex = glGetUniformLocation(prog, "u_texture");
	if (L->utex < 0) L->utex = glGetUniformLocation(prog, "s_texture");

	L->ures = glGetUniformLocation(prog, "u_resolution");
	if (L->ures < 0) L->ures = glGetUniformLocation(prog, "u_screenSize");

	L->utexel = glGetUniformLocation(prog, "u_texel");
	if (L->utexel < 0) L->utexel = glGetUniformLocation(prog, "u_pixelSize");

	L->utime = glGetUniformLocation(prog, "u_time");
	if (L->utime < 0) L->utime = glGetUniformLocation(prog, "u_globalTime");
	if (L->utime < 0) L->utime = glGetUniformLocation(prog, "time");

	L->udir_active    = glGetUniformLocation(prog, "u_dirLightActive");
	L->udir_dir       = glGetUniformLocation(prog, "u_dirLightDir");
	L->udir_color     = glGetUniformLocation(prog, "u_dirLightColor");
	L->udir_intensity = glGetUniformLocation(prog, "u_dirLightIntensity");

	L->upt_count = glGetUniformLocation(prog, "u_pointLightCount");
	L->upt_pos   = glGetUniformLocation(prog, "u_pointLightPos");
	L->upt_color = glGetUniformLocation(prog, "u_pointLightColor");

	L->uvp  = glGetUniformLocation(prog, "u_viewProjection");
	L->uivp = glGetUniformLocation(prog, "u_invViewProjection");
	L->ucam = glGetUniformLocation(prog, "u_cameraPos");
}

static float get_time(void) {
	static struct timespec t0 = {0, 0};
	if (t0.tv_sec == 0) clock_gettime(CLOCK_MONOTONIC, &t0);
	struct timespec tn;
	clock_gettime(CLOCK_MONOTONIC, &tn);
	return (float)(tn.tv_sec - t0.tv_sec) + (float)(tn.tv_nsec - t0.tv_nsec) * 1e-9f;
}

// 4×4 matrix inverse, column-major (same layout as OpenGL / GLES uniforms).
// Uses cofactor / adjugate method. Returns 1 on success, 0 if singular.
static int mat4_inverse(float *d, const float *m) {
	d[ 0] =  m[5]*(m[10]*m[15]-m[14]*m[11]) - m[9]*(m[6]*m[15]-m[14]*m[7]) + m[13]*(m[6]*m[11]-m[10]*m[7]);
	d[ 1] = -m[1]*(m[10]*m[15]-m[14]*m[11]) + m[9]*(m[2]*m[15]-m[14]*m[3]) - m[13]*(m[2]*m[11]-m[10]*m[3]);
	d[ 2] =  m[1]*(m[6]*m[15] -m[14]*m[7])  - m[5]*(m[2]*m[15]-m[14]*m[3]) + m[13]*(m[2]*m[7] -m[6]*m[3]);
	d[ 3] = -m[1]*(m[6]*m[11] -m[10]*m[7])  + m[5]*(m[2]*m[11]-m[10]*m[3]) - m[9]*(m[2]*m[7]  -m[6]*m[3]);
	d[ 4] = -m[4]*(m[10]*m[15]-m[14]*m[11]) + m[8]*(m[6]*m[15]-m[14]*m[7]) - m[12]*(m[6]*m[11]-m[10]*m[7]);
	d[ 5] =  m[0]*(m[10]*m[15]-m[14]*m[11]) - m[8]*(m[2]*m[15]-m[14]*m[3]) + m[12]*(m[2]*m[11]-m[10]*m[3]);
	d[ 6] = -m[0]*(m[6]*m[15] -m[14]*m[7])  + m[4]*(m[2]*m[15]-m[14]*m[3]) - m[12]*(m[2]*m[7] -m[6]*m[3]);
	d[ 7] =  m[0]*(m[6]*m[11] -m[10]*m[7])  - m[4]*(m[2]*m[11]-m[10]*m[3]) + m[8]*(m[2]*m[7]  -m[6]*m[3]);
	d[ 8] =  m[4]*(m[9]*m[15] -m[13]*m[11]) - m[8]*(m[5]*m[15]-m[13]*m[7]) + m[12]*(m[5]*m[11]-m[9]*m[7]);
	d[ 9] = -m[0]*(m[9]*m[15] -m[13]*m[11]) + m[8]*(m[1]*m[15]-m[13]*m[3]) - m[12]*(m[1]*m[11]-m[9]*m[3]);
	d[10] =  m[0]*(m[5]*m[15] -m[13]*m[7])  - m[4]*(m[1]*m[15]-m[13]*m[3]) + m[12]*(m[1]*m[7] -m[5]*m[3]);
	d[11] = -m[0]*(m[5]*m[11] -m[9]*m[7])   + m[4]*(m[1]*m[11]-m[9]*m[3])  - m[8]*(m[1]*m[7]  -m[5]*m[3]);
	d[12] = -m[4]*(m[9]*m[14] -m[13]*m[10]) + m[8]*(m[5]*m[14]-m[13]*m[6]) - m[12]*(m[5]*m[10]-m[9]*m[6]);
	d[13] =  m[0]*(m[9]*m[14] -m[13]*m[10]) - m[8]*(m[1]*m[14]-m[13]*m[2]) + m[12]*(m[1]*m[10]-m[9]*m[2]);
	d[14] = -m[0]*(m[5]*m[14] -m[13]*m[6])  + m[4]*(m[1]*m[14]-m[13]*m[2]) - m[12]*(m[1]*m[6] -m[5]*m[2]);
	d[15] =  m[0]*(m[5]*m[10] -m[9]*m[6])   - m[4]*(m[1]*m[10]-m[9]*m[2])  + m[8]*(m[1]*m[6]  -m[5]*m[2]);

	float det = m[0]*d[0] + m[1]*d[4] + m[2]*d[8] + m[3]*d[12];
	if (fabsf(det) < 1e-8f) return 0;
	float id = 1.0f / det;
	for (int i = 0; i < 16; i++) d[i] *= id;
	return 1;
}

// Bind the camera's view-projection matrix and its inverse as mat4 uniforms.
// u_cameraPos is world-space position of camera (or screen centre).
static void bind_world_uniforms(ShaderLocs *L, const float *vp, const float *ivp, float cam_x, float cam_y) {
	if (L->uvp  >= 0) glUniformMatrix4fv(L->uvp,  1, GL_FALSE, vp);
	if (L->uivp >= 0) glUniformMatrix4fv(L->uivp, 1, GL_FALSE, ivp);
	if (L->ucam >= 0) glUniform2f(L->ucam, cam_x, cam_y);
}

// Bind all standard uniforms. Caller must have already bound unit 0 texture.
static void bind_uniforms(ShaderLocs *L, float w, float h,
                           int pt_count, float *pt_pos, float *pt_col,
                           const float *vp, const float *ivp, float cam_x, float cam_y) {
	if (L->utex >= 0)          glUniform1i(L->utex, 0);
	if (L->ures >= 0)          glUniform2f(L->ures, w, h);
	if (L->utexel >= 0)        glUniform2f(L->utexel, 1.0f/w, 1.0f/h);
	if (L->utime >= 0)         glUniform1f(L->utime, get_time());
	if (L->udir_active >= 0)   glUniform1f(L->udir_active, g_dir_active ? 1.0f : 0.0f);
	if (L->udir_dir >= 0)      glUniform2f(L->udir_dir, g_dir_x, g_dir_y);
	if (L->udir_color >= 0)    glUniform3f(L->udir_color, g_dir_r, g_dir_g, g_dir_b);
	if (L->udir_intensity >= 0)glUniform1f(L->udir_intensity, g_dir_intensity);
	if (L->upt_count >= 0)     glUniform1i(L->upt_count, pt_count);
	if (L->upt_pos >= 0 && pt_count > 0)
		glUniform4fv(L->upt_pos, pt_count, pt_pos);
	if (L->upt_color >= 0 && pt_count > 0)
		glUniform4fv(L->upt_color, pt_count, pt_col);
	bind_world_uniforms(L, vp, ivp, cam_x, cam_y);
}

static void draw_quad(ShaderLocs *L) {
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glVertexAttribPointer(L->pos, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), quad_verts);
	glEnableVertexAttribArray(L->pos);
	glVertexAttribPointer(L->tex, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), quad_verts+2);
	glEnableVertexAttribArray(L->tex);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glDisableVertexAttribArray(L->pos);
	glDisableVertexAttribArray(L->tex);
}

// Compile + link a single program with a given #define prepended to the fragment.
static GLuint link_program(const char *vsrc, const char *glsl, const char *define) {
	GLuint vs = compile_shader_stage(GL_VERTEX_SHADER, vsrc);
	if (!vs) return 0;

	size_t flen = strlen(glsl) + 200;
	char *fsrc = (char *)malloc(flen);
	if (!fsrc) { glDeleteShader(vs); return 0; }
	snprintf(fsrc, flen,
		"#ifdef GL_ES\n"
		"#ifdef GL_FRAGMENT_PRECISION_HIGH\n"
		"precision highp float;\n"
		"#else\n"
		"precision mediump float;\n"
		"#endif\n"
		"#endif\n"
		"#define %s 1\n"
		"%s", define, glsl);

	GLuint fs = compile_shader_stage(GL_FRAGMENT_SHADER, fsrc);
	free(fsrc);
	if (!fs) { glDeleteShader(vs); return 0; }

	GLuint prog = glCreateProgram();
	glAttachShader(prog, vs);
	glAttachShader(prog, fs);
	glLinkProgram(prog);
	glDeleteShader(vs);
	glDeleteShader(fs);

	GLint ok = 0;
	glGetProgramiv(prog, GL_LINK_STATUS, &ok);
	if (!ok) {
		char log[512];
		glGetProgramInfoLog(prog, sizeof(log), NULL, log);
		__android_log_print(ANDROID_LOG_ERROR, "ShaderLoader",
			"Link(%s) error: %s", define, log);
		glDeleteProgram(prog);
		return 0;
	}
	return prog;
}

// Determine the vertex source: use custom #ifdef VERTEX block if present,
// else the default passthrough.
static const char *get_vsrc(const char *glsl, char **out_custom) {
	*out_custom = NULL;
	if (!strstr(glsl, "#ifdef VERTEX")) return default_vsrc;
	size_t vlen = strlen(glsl) + 32;
	char *cv = (char *)malloc(vlen);
	if (!cv) return default_vsrc;
	snprintf(cv, vlen, "#define VERTEX 1\n%s", glsl);
	*out_custom = cv;
	return cv;
}

// Build both the composite (FRAGMENT) program and, if present, the godray
// (GODRAY_PASS) program. Returns 1 on success (composite is mandatory).
static int build_programs(const char *glsl) {
	// Tear down old programs
	if (g_prog)    { glDeleteProgram(g_prog);    g_prog    = 0; }
	if (g_gr_prog) { glDeleteProgram(g_gr_prog); g_gr_prog = 0; }
	g_has_gr = 0;

	char *custom_vs = NULL;
	const char *vsrc = get_vsrc(glsl, &custom_vs);

	// ── Composite program ────────────────────────────────────────────────────
	g_prog = link_program(vsrc, glsl, "FRAGMENT");
	if (custom_vs) { free(custom_vs); custom_vs = NULL; }
	if (!g_prog) return 0;

	lookup_locs(g_prog, &g_comp_locs);
	g_ugodray = glGetUniformLocation(g_prog, "u_godray");

	// ── Godray program (optional) ────────────────────────────────────────────
	if (strstr(glsl, "#ifdef GODRAY_PASS")) {
		vsrc = get_vsrc(glsl, &custom_vs);
		g_gr_prog = link_program(vsrc, glsl, "GODRAY_PASS");
		if (custom_vs) free(custom_vs);
		if (g_gr_prog) {
			lookup_locs(g_gr_prog, &g_gr_locs);
			g_has_gr   = 1;
			g_gr_dirty = 1;
			__android_log_print(ANDROID_LOG_INFO, "ShaderLoader",
				"Two-pass mode: GODRAY_PASS compiled.");
		}
	}
	return 1;
}

// Create or resize the half-resolution godray FBO.
static int ensure_gr_fbo(int w, int h) {
	if (!g_gr_fbo) glGenFramebuffers(1, &g_gr_fbo);

	if (!g_gr_tex || g_gr_w != w || g_gr_h != h) {
		if (!g_gr_tex) glGenTextures(1, &g_gr_tex);
		glBindTexture(GL_TEXTURE_2D, g_gr_tex);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glBindTexture(GL_TEXTURE_2D, 0);

		glBindFramebuffer(GL_FRAMEBUFFER, g_gr_fbo);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
		                       GL_TEXTURE_2D, g_gr_tex, 0);
		GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		if (status != GL_FRAMEBUFFER_COMPLETE) {
			__android_log_print(ANDROID_LOG_ERROR, "ShaderLoader",
				"Godray FBO incomplete: 0x%x", (unsigned)status);
			glDeleteFramebuffers(1, &g_gr_fbo);
			glDeleteTextures(1, &g_gr_tex);
			g_gr_fbo = g_gr_tex = 0;
			g_gr_w   = g_gr_h   = 0;
			return 0;
		}
		g_gr_w = w;
		g_gr_h = h;
		g_gr_dirty = 1; // size changed, must recompute
	}
	return 1;
}

static int locate_shader_file(char *out_path, size_t out_sz) {
	if (g_custom_path[0] && access(g_custom_path, R_OK) == 0) {
		snprintf(out_path, out_sz, "%s", g_custom_path);
		return 1;
	}
	const char *mod = java_current_mod_id();
	if (!mod || !mod[0]) return 0;
	const char *ext = java_external_files();
	if (!ext || !ext[0]) return 0;

	snprintf(out_path, out_sz, "%s/mods/%s/resources/shader.glsl", ext, mod);
	if (access(out_path, R_OK) == 0) return 1;
	snprintf(out_path, out_sz, "%s/mods/%s/resources/shaders/shader.glsl", ext, mod);
	if (access(out_path, R_OK) == 0) return 1;
	snprintf(out_path, out_sz, "%s/mods/%s/shaders/shader.glsl", ext, mod);
	if (access(out_path, R_OK) == 0) return 1;
	snprintf(out_path, out_sz, "%s/mods/%s/shader.glsl", ext, mod);
	if (access(out_path, R_OK) == 0) return 1;
	return 0;
}

// =============================================================================
// Main post-process entry point — called every frame after Scene::Draw
// =============================================================================
void shader_on_scene_drawn(void *scene) {
	const char *rend = (const char *)glGetString(GL_RENDERER);
	if (!rend || !strstr(rend, "ANGLE")) return;
	if (!g_shader_enabled) return;

	char path[512];
	if (!locate_shader_file(path, sizeof(path))) {
		if (g_prog && g_dpy && g_es2) {
			eglMakeCurrent(g_dpy, g_draw, g_read, g_es2);
			if (g_prog)   { glDeleteProgram(g_prog);    g_prog   = 0; }
			if (g_gr_prog){ glDeleteProgram(g_gr_prog); g_gr_prog= 0; }
			if (g_tex)    { glDeleteTextures(1, &g_tex); g_tex   = 0; }
			if (g_gr_tex) { glDeleteTextures(1, &g_gr_tex); g_gr_tex = 0; }
			if (g_gr_fbo) { glDeleteFramebuffers(1, &g_gr_fbo); g_gr_fbo = 0; }
			eglMakeCurrent(g_dpy, g_draw, g_read, g_es1);
			g_loaded_path[0] = '\0';
			g_loaded_mtime   = 0;
		}
		return;
	}

	struct stat st;
	if (stat(path, &st) != 0) return;

	g_dpy  = eglGetCurrentDisplay();
	g_es1  = eglGetCurrentContext();
	g_draw = eglGetCurrentSurface(EGL_DRAW);
	g_read = eglGetCurrentSurface(EGL_READ);
	if (!g_dpy || !g_es1 || !g_draw) return;

	EGLint w = 0, h = 0;
	eglQuerySurface(g_dpy, g_draw, EGL_WIDTH,  &w);
	eglQuerySurface(g_dpy, g_draw, EGL_HEIGHT, &h);
	if (w <= 0 || h <= 0) return;

	if (!g_es2) {
		EGLint cfgId = 0;
		eglQueryContext(g_dpy, g_es1, EGL_CONFIG_ID, &cfgId);
		EGLint attribs[]    = { EGL_CONFIG_ID, cfgId, EGL_NONE };
		EGLConfig cfg       = NULL;
		EGLint n            = 0;
		eglChooseConfig(g_dpy, attribs, &cfg, 1, &n);
		EGLint ctxAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
		g_es2 = eglCreateContext(g_dpy, cfg, EGL_NO_CONTEXT, ctxAttribs);
	}
	if (!g_es2) return;

	glFlush();
	if (!eglMakeCurrent(g_dpy, g_draw, g_read, g_es2)) return;

	// ── Reload shader when file changes ───────────────────────────────────────
	if (!g_prog || strcmp(g_loaded_path, path) != 0 || g_loaded_mtime != st.st_mtime) {
		char *src = read_file_text(path);
		if (src) {
			if (build_programs(src)) {
				snprintf(g_loaded_path, sizeof(g_loaded_path), "%s", path);
				g_loaded_mtime = st.st_mtime;
				g_gr_dirty     = 1;
				__android_log_print(ANDROID_LOG_INFO, "ShaderLoader",
					"Shader loaded: %s (two_pass=%d)", path, g_has_gr);
			}
			free(src);
		}
	}
	if (!g_prog) {
		eglMakeCurrent(g_dpy, g_draw, g_read, g_es1);
		return;
	}

	// ── Capture current frame → g_tex ─────────────────────────────────────────
	if (!g_tex || g_tex_w != w || g_tex_h != h) {
		if (!g_tex) glGenTextures(1, &g_tex);
		glBindTexture(GL_TEXTURE_2D, g_tex);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		g_tex_w = w;
		g_tex_h = h;
		g_gr_dirty = 1;
	} else {
		glBindTexture(GL_TEXTURE_2D, g_tex);
	}
	glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, w, h);
	glBindTexture(GL_TEXTURE_2D, 0);

	// ── Godray dirty tracking ─────────────────────────────────────────────────
	if (scene != g_last_scene) {
		g_last_scene  = scene;
		g_gr_dirty    = 1;
		g_frame_count = 0;
	}
	if (++g_frame_count >= GODRAY_REFRESH_FRAMES) {
		g_gr_dirty    = 1;
		g_frame_count = 0;
	}

	// ── Project point lights into screen UV space ─────────────────────────────
	CameraController *cc = cameraController_get();
	Camera *cam = cc ? cc->camera : NULL;
	float pt_pos[MAX_POINT_LIGHTS * 4];
	float pt_col[MAX_POINT_LIGHTS * 4];
	int   pt_count = 0;

	// World-space matrices and camera position (computed once, used in both passes)
	float g_vp[16]  = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
	float g_ivp[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
	float cam_x = 0.0f, cam_y = 0.0f;
	int   has_world = 0;

	if (cam) {
		float *m = cam->viewProjection.m;
		for (int i = 0; i < 16; i++) g_vp[i] = m[i];
		has_world = mat4_inverse(g_ivp, m);
		cam_x = cam->position.x;
		cam_y = cam->position.y;
		if (!has_world) {
			// Fallback: identity
			for (int i = 0; i < 16; i++) g_ivp[i] = (i % 5 == 0) ? 1.0f : 0.0f;
		}

		// Refresh god rays if camera moved significantly in world space
		static float s_last_cam_x = 0.0f;
		static float s_last_cam_y = 0.0f;
		float cdist = fabsf(cam_x - s_last_cam_x) + fabsf(cam_y - s_last_cam_y);
		if (cdist > 120.0f) {
			g_gr_dirty = 1;
			s_last_cam_x = cam_x;
			s_last_cam_y = cam_y;
		}

		for (int i = 0; i < MAX_POINT_LIGHTS; i++) {
			if (!g_point_lights[i].active) continue;
			float px = g_point_lights[i].world_pos.x;
			float py = g_point_lights[i].world_pos.y;
			float pz = g_point_lights[i].world_pos.z;
			float cx = m[0]*px + m[4]*py + m[8]*pz + m[12];
			float cy = m[1]*px + m[5]*py + m[9]*pz + m[13];
			float cw = m[3]*px + m[7]*py + m[11]*pz + m[15];
			if (cw > 0.01f) {
				float uv_x = cx/cw * 0.5f + 0.5f;
				float uv_y = cy/cw * 0.5f + 0.5f;
				float rw   = m[3]*(px + g_point_lights[i].radius) + m[7]*py + m[11]*pz + m[15];
				float rx   = m[0]*(px + g_point_lights[i].radius) + m[4]*py + m[8]*pz + m[12];
				float srad = 0.22f;
				if (rw > 0.01f) srad = fabsf((rx/rw)*0.5f+0.5f - uv_x);
				if (srad < 0.08f) srad = 0.08f;
				if (srad > 0.60f) srad = 0.60f;
				pt_pos[pt_count*4+0] = uv_x;
				pt_pos[pt_count*4+1] = uv_y;
				pt_pos[pt_count*4+2] = srad;
				pt_pos[pt_count*4+3] = g_point_lights[i].intensity;
				pt_col[pt_count*4+0] = g_point_lights[i].r;
				pt_col[pt_count*4+1] = g_point_lights[i].g;
				pt_col[pt_count*4+2] = g_point_lights[i].b;
				pt_col[pt_count*4+3] = 1.0f;
				pt_count++;
			}
		}
	}

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_BLEND);

	if (g_has_gr && g_gr_prog) {
		// ── PASS 1: Godray (half-res, cached) ────────────────────────────────
		int gw = (w / 2 > 0) ? w / 2 : 1;
		int gh = (h / 2 > 0) ? h / 2 : 1;

		if (g_gr_dirty && ensure_gr_fbo(gw, gh)) {
			glBindFramebuffer(GL_FRAMEBUFFER, g_gr_fbo);
			glViewport(0, 0, gw, gh);
			glUseProgram(g_gr_prog);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, g_tex);
			bind_uniforms(&g_gr_locs, (float)w, (float)h, pt_count, pt_pos, pt_col, g_vp, g_ivp, cam_x, cam_y);
			draw_quad(&g_gr_locs);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			g_gr_dirty = 0;
		}

		// ── PASS 2: Composite (full-res, every frame) ─────────────────────────
		glViewport(0, 0, w, h);
		glUseProgram(g_prog);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, g_tex);
		bind_uniforms(&g_comp_locs, (float)w, (float)h, pt_count, pt_pos, pt_col, g_vp, g_ivp, cam_x, cam_y);

		// Bind cached godray texture on unit 1
		if (g_ugodray >= 0 && g_gr_tex) {
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, g_gr_tex);
			glUniform1i(g_ugodray, 1);
			glActiveTexture(GL_TEXTURE0);
		}
		draw_quad(&g_comp_locs);
	} else {
		// ── Single-pass fallback ──────────────────────────────────────────────
		glViewport(0, 0, w, h);
		glUseProgram(g_prog);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, g_tex);
		bind_uniforms(&g_comp_locs, (float)w, (float)h, pt_count, pt_pos, pt_col, g_vp, g_ivp, cam_x, cam_y);
		draw_quad(&g_comp_locs);
	}

	glBindTexture(GL_TEXTURE_2D, 0);
	glUseProgram(0);
	glFlush();
	eglMakeCurrent(g_dpy, g_draw, g_read, g_es1);
}

void shader_cleanup(void) {
	if (g_dpy && g_es2) {
		eglMakeCurrent(g_dpy, g_draw, g_read, g_es2);
		if (g_prog)   glDeleteProgram(g_prog);
		if (g_gr_prog)glDeleteProgram(g_gr_prog);
		if (g_tex)    glDeleteTextures(1, &g_tex);
		if (g_gr_tex) glDeleteTextures(1, &g_gr_tex);
		if (g_gr_fbo) glDeleteFramebuffers(1, &g_gr_fbo);
		eglMakeCurrent(g_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
		eglDestroyContext(g_dpy, g_es2);
	}
	g_prog = g_gr_prog = 0;
	g_tex  = g_gr_tex  = g_gr_fbo = 0;
	g_es2  = EGL_NO_CONTEXT;
	g_tex_w = g_tex_h = 0;
	g_gr_w  = g_gr_h  = 0;
	g_has_gr = 0;
	g_gr_dirty   = 1;
	g_last_scene = NULL;
	g_frame_count= 0;
	g_loaded_path[0] = '\0';
	g_loaded_mtime   = 0;
}

// =============================================================================
// Scene_Draw hook — triggers post-process and passes scene ptr for dirty check
// =============================================================================
HOOK_SYMBOL(
	Scene_Draw,
	"_ZN5Caver5Scene4DrawEPNS_16RenderingContextERKNS_7Matrix4E",
	void, (void *scene, void *ctx, void *matrix)
) {
	orig_Scene_Draw(scene, ctx, matrix);
	shader_on_scene_drawn(scene);
}

// =============================================================================
// Lua API
// =============================================================================
static int l_shader_is_angle(lua_State *L) {
	const char *rend = (const char *)glGetString(GL_RENDERER);
	lua_pushboolean(L, rend && strstr(rend, "ANGLE") != NULL);
	return 1;
}

static int l_shader_enable(lua_State *L) {
	g_shader_enabled = lua_toboolean(L, 1);
	return 0;
}

static int l_shader_load(lua_State *L) {
	const char *path = lua_tostring(L, 1);
	if (path) {
		snprintf(g_custom_path, sizeof(g_custom_path), "%s", path);
		g_loaded_mtime = 0;
		g_gr_dirty     = 1;
	}
	return 0;
}

static int l_shader_reload(lua_State *L) {
	g_loaded_mtime = 0;
	g_gr_dirty     = 1;
	return 0;
}

static const luaL_Reg shader_funcs[] = {
	{"isAngle", l_shader_is_angle},
	{"enable",  l_shader_enable},
	{"load",    l_shader_load},
	{"reload",  l_shader_reload},
	{NULL, NULL}
};

void API_register_shader(lua_State *L) {
	luaL_register(L, "Shader", shader_funcs);
	lua_pop(L, 1);
}
