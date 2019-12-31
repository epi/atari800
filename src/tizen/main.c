#include <time.h>
#include <pthread.h>

#include <app.h>
#include <dlog.h>
#include <device/power.h>
#include <system_settings.h>

#include <efl_extension.h>
#include <Ecore.h>
#include <Elementary.h>
#include <Elementary_GL_Helpers.h>
#include <Evas.h>

#include "platform.h"
#include "akey.h"
#include "input.h"
#include "screen.h"
#include "colours.h"
#include "util.h"

#ifdef  LOG_TAG
#undef  LOG_TAG
#endif
#define LOG_TAG "ATARI800"

#if !defined(PACKAGE)
#define PACKAGE "io.github.atari800"
#endif

ELEMENTARY_GLVIEW_GLOBAL_DEFINE();

enum State
{
	State_running,
	State_paused,
	State_exit,
};

typedef struct AppData_s {
	int argc;
	char** argv;
	Ecore_Animator* animator;
	Evas_Object *win;
	Evas_Object *img;

	Evas_GL *evasgl;
	Evas_GL_API *glapi;
	Evas_GL_Context *ctx;
	Evas_GL_Surface *sfc;
	Evas_GL_Config *cfg;
	Evas_Coord sfc_w;
	Evas_Coord sfc_h;

	pthread_t thread;
	volatile enum State state;
	int event_type;
	Ecore_Event_Handler* eh;
	Evas_Object *glview;
} AppData;


int PLATFORM_Keyboard(void)
{
	// TODO
	return AKEY_NONE;
}

void PLATFORM_DisplayScreen(void)
{
	// TODO
}

int PLATFORM_PORT(int num)
{
	// TODO
	return 0xff;
}

int PLATFORM_TRIG(int num)
{
	// TODO
	return 1;
}

volatile double total_slept = 0.0;

int PLATFORM_Exit(int run_monitor)
{
#ifdef BUFFERED_LOG
	Log_flushlog();
#endif

	Sound_Exit();

	return 0;
}

int PLATFORM_Initialise(int *argc, char *argv[])
{
	dlog_print(DLOG_INFO, LOG_TAG, "Platform initialise");
	return Sound_Initialise(argc, argv);
}

unsigned char FrameCallback(void *data)
{
	AppData* ad = (AppData*) data;
	if (ad->state == State_paused)
		return ECORE_CALLBACK_RENEW;

	unsigned *buf = (unsigned*) evas_object_image_data_get(ad->img, EINA_TRUE);
	unsigned char* scr = (unsigned char*)Screen_atari;
	for (int x = 24; x < 24 + 336; x++) {
		for (int y = 8; y < 8 + 224; y++) {
			unsigned char col = scr[y * Screen_WIDTH + x];
			unsigned r = Colours_GetR(col);
			unsigned b = Colours_GetB(col);
			unsigned g = Colours_GetG(col);
			unsigned rgb = (0x80 << 24) | (r << 16) | (g << 8) | b;
			buf[(x + 32) * 224 + 223 + 12 - y] = rgb;
		}
	}
	evas_object_image_data_update_add(ad->img, 0, 0, 224, 432);
	evas_object_show(ad->img);

	return ECORE_CALLBACK_RENEW;
}

unsigned char FrameCallback2(void *data)
{
	AppData *ad = data;
	elm_glview_changed_set(ad->glview);
	return ECORE_CALLBACK_RENEW;
}

static void
win_delete_request_cb(void *data, Evas_Object *obj, void *event_info)
{
	dlog_print(DLOG_INFO, LOG_TAG, "exit!");
	ui_app_exit();
}

static void
win_back_cb(void *data, Evas_Object *obj, void *event_info)
{
	AppData *ad = data;
	elm_win_lower(ad->win);
	dlog_print(DLOG_INFO, LOG_TAG, "lower");
}

enum {
    UNIFORM_VIDEOFRAME,
	UNIFORM_PALETTE,
    NUM_UNIFORMS
};
int uniforms[NUM_UNIFORMS];

enum {
    ATTRIB_VERTEX,
    ATTRIB_TEXTUREPOSITON,
    NUM_ATTRIBUTES
};

unsigned program;
unsigned videoFrameTexture;

static GLfloat colormap[256 * 4];

static void
init_glview(Evas_Object *glview)
{
	program = glCreateProgram();
	const char *vsh =
		"attribute vec4 position;\n"
		"attribute vec4 inputTextureCoordinate;\n"
		"varying vec2 textureCoordinate;\n"
		"void main()\n"
		"{\n"
			"gl_Position = position;\n"
			"textureCoordinate = inputTextureCoordinate.xy;\n"
		"}\n";
	const char *fsh =
		"varying highp vec2 textureCoordinate;\n"
		"uniform sampler2D videoFrame;\n"
		"uniform highp vec4 palette[256];\n"
		"void main()\n"
		"{\n"
			"highp vec4 index = texture2D(videoFrame, textureCoordinate);\n"
			"highp vec4 rgba = palette[int(index.r * 255.0 + 0.5)];\n"
			"gl_FragColor = rgba;\n"
		"}\n";

	unsigned vshader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vshader, 1, &vsh, NULL);
    glCompileShader(vshader);
    int status;
    glGetShaderiv(vshader, GL_COMPILE_STATUS, &status);
	dlog_print(DLOG_INFO, LOG_TAG, "vshader %d", status);

	unsigned fshader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fshader, 1, &fsh, NULL);
	glCompileShader(fshader);
    glGetShaderiv(fshader, GL_COMPILE_STATUS, &status);
	dlog_print(DLOG_INFO, LOG_TAG, "fshader %d", status);

    GLint logLength;
    glGetShaderiv(fshader, GL_INFO_LOG_LENGTH, &logLength);
    if (logLength > 0)
    {
        GLchar *log = (GLchar *)malloc(logLength);
        glGetShaderInfoLog(fshader, logLength, &logLength, log);
		dlog_print(DLOG_INFO, LOG_TAG, "Shader compile log:\n%s", log);
        free(log);
    }
    glAttachShader(program, vshader);
    glAttachShader(program, fshader);
    glBindAttribLocation(program, ATTRIB_VERTEX, "position");
    glBindAttribLocation(program, ATTRIB_TEXTUREPOSITON, "inputTextureCoordinate");
    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &status);
	dlog_print(DLOG_INFO, LOG_TAG, "link %d", status);

    uniforms[UNIFORM_VIDEOFRAME] = glGetUniformLocation(program, "videoFrame");
    uniforms[UNIFORM_PALETTE] = glGetUniformLocation(program, "palette");

	const float f = 1.0 / 255;
	for (int i = 0; i < 256; i++) {
		colormap[i * 4] = Colours_GetR(i) * f;
		colormap[i * 4 + 1] = Colours_GetG(i) * f;
		colormap[i * 4 + 2] = Colours_GetB(i) * f;
		colormap[i * 4 + 3] = 1.0;
    }
}

static void
resize_glview(Evas_Object *glview)
{
    int w;
    int h;
    elm_glview_size_get(glview, &w, &h);
    glViewport(0, 0, w, h);
}

static void
draw_glview(Evas_Object *glview)
{
    static const GLfloat screenVertices[] = {
		-1.0f, -1.0f,
		 1.0f, -1.0f,
		-1.0f, -0.8888888888f,
		 1.0f, -0.8888888888f,
		-1.0f,  0.8888888888f,
		 1.0f,  0.8888888888f,
		-1.0f,  1.0f,
		 1.0f,  1.0f,
    };

	static const GLfloat textureVertices[] = {
        1.0f, 1.0f,
        1.0f, 0.0f,
        0.9375f, 1.0f,
        0.9375f, 0.0f,
        0.0625f, 1.0f,
        0.0625f, 0.0f,
        0.0f, 1.0f,
        0.0f, 0.0f,
    };

	glGenTextures(1, &videoFrameTexture);
	glBindTexture(GL_TEXTURE_2D, videoFrameTexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);//LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);//LINEAR);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	unsigned char* buf = (unsigned char*) Screen_atari;
	buf += 12 * 384;
	glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, 384, 216, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, buf);

	glUseProgram(program);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, videoFrameTexture);

	glUniform1i(uniforms[UNIFORM_VIDEOFRAME], 0);
	glUniform4fv(uniforms[UNIFORM_PALETTE], 256, colormap);

	glVertexAttribPointer(ATTRIB_VERTEX, 2, GL_FLOAT, 0, 0, screenVertices);
	glEnableVertexAttribArray(ATTRIB_VERTEX);
	glVertexAttribPointer(ATTRIB_TEXTUREPOSITON, 2, GL_FLOAT, 0, 0, textureVertices);
	glEnableVertexAttribArray(ATTRIB_TEXTUREPOSITON);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 8);

	glDeleteTextures(1, &videoFrameTexture);
}

static void
del_glview(Evas_Object *glview)
{
	dlog_print(DLOG_INFO, LOG_TAG, "destroy");
}

static void
create_base_gui(AppData* ad)
{
	elm_config_accel_preference_set("opengl");

	ad->win = elm_win_util_standard_add(PACKAGE, PACKAGE);
	evas_object_show(ad->win);

	ad->glview = elm_glview_add(ad->win);
	elm_win_resize_object_add(ad->win, ad->glview);
	dlog_print(DLOG_INFO, LOG_TAG, "glview %p", ad->glview);
	elm_glview_mode_set(ad->glview, ELM_GLVIEW_ALPHA | ELM_GLVIEW_DEPTH |ELM_GLVIEW_STENCIL);
	elm_glview_resize_policy_set(ad->glview, ELM_GLVIEW_RESIZE_POLICY_RECREATE);
	elm_glview_render_policy_set(ad->glview, ELM_GLVIEW_RENDER_POLICY_ON_DEMAND);

	evas_object_resize(ad->win, 216, 432);
    elm_win_autodel_set(ad->win, true);

	elm_glview_init_func_set(ad->glview, init_glview);
	elm_glview_resize_func_set(ad->glview, resize_glview);
	elm_glview_render_func_set(ad->glview, draw_glview);
	elm_glview_del_func_set(ad->glview, del_glview);
	elm_glview_changed_set(ad->glview);

	evas_object_size_hint_min_set(ad->glview, 216, 432);
	evas_object_show(ad->glview);

	ELEMENTARY_GLVIEW_GLOBAL_USE(ad->glview);

	evas_object_show(ad->win);

    evas_object_smart_callback_add(ad->win, "delete,request", &win_delete_request_cb, NULL);
    eext_object_event_callback_add(ad->win, EEXT_CALLBACK_BACK, &win_back_cb, ad);
    if (elm_win_wm_rotation_supported_get(ad->win)) {
        const int rots[4] = { 0, 90, 180, 270 };
        elm_win_wm_rotation_available_rotations_set(ad->win, rots, 4);
    }

}

static void *loop(void *data)
{
	AppData *ad = (AppData*) data;

	for (;;) {
		int state = ad->state;
		if (state == State_paused) {
			Util_sleep(0.1);
		} else if (state == State_running) {
			Atari800_Frame();
			ecore_event_add(ad->event_type, NULL, NULL, NULL);
		} else if (state == State_exit) {
			break;
		}
	}
	return NULL;
}

static bool
app_create(void *data)
{
	AppData* ad = (AppData*) data;
	ad->state = State_paused;
	create_base_gui(ad);

	if (!Atari800_Initialise(&ad->argc, ad->argv)) {
		dlog_print(DLOG_ERROR, LOG_TAG, "Initialization failed");
		return false;
	}

	dlog_print(DLOG_INFO, LOG_TAG, "Initialized successfully");

	ad->state = State_running;
	(void) pthread_create(&ad->thread, NULL, &loop, ad);

	ad->animator = ecore_animator_add(&FrameCallback2, ad);
	ecore_animator_frametime_set(1.0 / 50);

	dlog_print(DLOG_INFO, LOG_TAG, "app created");
	return true;
}

static void
app_control(app_control_h app_control, void *data)
{
	dlog_print(DLOG_INFO, LOG_TAG, "control");
}

static void
app_pause(void *data)
{
	Sound_Pause();
	int res = device_power_release_lock(POWER_LOCK_DISPLAY);
	dlog_print(DLOG_INFO, LOG_TAG, "pause %d", res);
	((AppData*) data)->state = State_paused;
}

static void
app_resume(void *data)
{
	int res = device_power_request_lock(POWER_LOCK_DISPLAY, 0);
	dlog_print(DLOG_INFO, LOG_TAG, "resume %d", res);
	((AppData*) data)->state = State_running;
	Sound_Continue();
}

static void
app_terminate(void *data)
{
	AppData* ad = (AppData*) data;
	ad->state = State_exit;
	pthread_join(ad->thread, NULL);

	Atari800_Exit(0);

	dlog_print(DLOG_INFO, LOG_TAG, "terminate");
}

static void
ui_app_lang_changed(app_event_info_h event_info, void *user_data)
{
	char *locale = NULL;
	dlog_print(DLOG_INFO, LOG_TAG, "lang_changed");

	system_settings_get_value_string(SYSTEM_SETTINGS_KEY_LOCALE_LANGUAGE, &locale);
	elm_language_set(locale);
	free(locale);
	return;
}

static void
ui_app_orient_changed(app_event_info_h event_info, void *user_data)
{
	dlog_print(DLOG_INFO, LOG_TAG, "orient changed");
}

static void
ui_app_region_changed(app_event_info_h event_info, void *user_data)
{
	dlog_print(DLOG_INFO, LOG_TAG, "region changed");
}

static void
ui_app_low_battery(app_event_info_h event_info, void *user_data)
{
	dlog_print(DLOG_INFO, LOG_TAG, "low battery");
}

static void
ui_app_low_memory(app_event_info_h event_info, void *user_data)
{
	dlog_print(DLOG_INFO, LOG_TAG, "low memory");
}

static char* glob_argv[6] = {
 "atari800",
 "-run",
 "/opt/usr/apps/io.github.atari800/shared/res/DRNKCHES.XEX",
 "-config",
 "/opt/usr/apps/io.github.atari800/shared/res/atari800.cfg",
 0
};

int main(int argc, char **argv)
{
	dlog_print(DLOG_INFO, LOG_TAG, "Starting");

	AppData ad;
	ad.argc = 5; // argc;
	ad.argv = glob_argv; //argv;

	ui_app_lifecycle_callback_s event_callback = {0,};

	app_event_handler_h handlers[5] = {NULL, };

	event_callback.create = app_create;
	event_callback.terminate = app_terminate;
	event_callback.pause = app_pause;
	event_callback.resume = app_resume;
	event_callback.app_control = app_control;

	ui_app_add_event_handler(
		&handlers[0], APP_EVENT_LOW_BATTERY, ui_app_low_battery, &ad);
	ui_app_add_event_handler(
		&handlers[1], APP_EVENT_LOW_MEMORY, ui_app_low_memory, &ad);
	ui_app_add_event_handler(
		&handlers[2], APP_EVENT_DEVICE_ORIENTATION_CHANGED,
		ui_app_orient_changed, &ad);
	ui_app_add_event_handler(
		&handlers[3], APP_EVENT_LANGUAGE_CHANGED, ui_app_lang_changed, &ad);
	ui_app_add_event_handler(
		&handlers[4], APP_EVENT_REGION_FORMAT_CHANGED, ui_app_region_changed, &ad);

	int ret = ui_app_main(argc, argv, &event_callback, &ad);
	if (ret != APP_ERROR_NONE) {
		dlog_print(DLOG_ERROR, LOG_TAG, "app_main() failed. err = %d", ret);
	} else {
		dlog_print(DLOG_INFO, LOG_TAG, "app_main() finished successfully");
	}

	return 0;
}
