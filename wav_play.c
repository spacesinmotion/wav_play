//------------------------------------------------------------------------------
//  debugtext-sapp.c
//  Text rendering with sokol_debugtext.h, test builtin fonts.
//---------------------------------------------------------------------------
#include <time.h>
#define DR_WAV_IMPLEMENTATION
#include "dr/dr_wav.h"

#define _POSIX_C_SOURCE 200809L
#include <sys/stat.h>
#include <sys/types.h>
// #include <unistd.h>

#ifdef __TINYC__
#include <math.h>
#define fmodf fmod
#define sinf sin
#endif

#include <stdio.h>
#define SOKOL_GLCORE
#define SOKOL_DEBUGTEXT_IMPL

#include "sokol_app.h"
#include "sokol_audio.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "sokol_log.h"
#include "util/sokol_debugtext.h"

#define FONT_KC853 (0)
#define FONT_KC854 (1)
#define FONT_Z1013 (2)
#define FONT_CPC (3)
#define FONT_C64 (4)
#define FONT_ORIC (5)

typedef struct {
  unsigned int channels;
  unsigned int sample_rate;
  drwav_uint64 frame_count;
  float *sample_data;
} WaveFile;

WaveFile WaveFile_load(const char *path) {
  WaveFile wf = (WaveFile){0};
  wf.sample_data = drwav_open_file_and_read_pcm_frames_f32(
      path, &wf.channels, &wf.sample_rate, &wf.frame_count, NULL);
  return wf;
}

void WaveFile_dispose(WaveFile *wf) {
  if (wf->sample_data)
    drwav_free(wf->sample_data, NULL);
  *wf = (WaveFile){0};
}

static struct {
  const char *path;
  WaveFile wave_file;
  WaveFile wave_file_new;
  time_t last_modification;
  int frame;
  sg_pass_action pass_action;
} state = {
    .path = NULL,
    .wave_file = (WaveFile){},
    .frame = 0,
    .pass_action =
        {
            .colors[0] =
                {
                    .load_action = SG_LOADACTION_CLEAR,
                    .clear_value = {0.0f, 0.125f, 0.25f, 1.0f},
                },
        },
};

static void audio_cb(float *buffer, int num_frames, int num_channels,
                     void *user_data) {

  if (state.wave_file_new.sample_data) {
    printf("swap wav \n");
    WaveFile_dispose(&state.wave_file);
    state.wave_file = state.wave_file_new;
    state.wave_file_new = (WaveFile){};
    printf("wav swapped %d\n", (int)state.wave_file.frame_count);
  }

  for (int i = 0; i < num_frames; ++i) {
    // float t = (float)state.frame / 44100.0f;
    // t = 0.025f * sinf(220.0f * 2.0f * M_PI * t);
    for (int j = 0; j < num_channels; ++j)
      buffer[i * num_channels + j] = 0.0f;
    if (state.wave_file.sample_data && state.wave_file.frame_count > 0) {
      int o = (state.frame % state.wave_file.frame_count);
      if (o < state.wave_file.frame_count)
        for (int j = 0; j < num_channels; ++j)
          buffer[i * num_channels + j] +=
              state.wave_file.sample_data[o * state.wave_file.channels];
    }

    ++state.frame;
  }
}

static void init(void) {
  // setup sokol-gfx
  sg_setup(&(sg_desc){
      .environment = sglue_environment(),
      .logger.func = slog_func,
  });
  // __dbgui_setup(sapp_sample_count());

  saudio_setup(&(saudio_desc){
      .sample_rate = 44100, .num_channels = 2, .stream_userdata_cb = audio_cb});

  // setup sokol-debugtext
  sdtx_setup(&(sdtx_desc_t){
      .fonts = {[FONT_KC853] = sdtx_font_kc853(),
                [FONT_KC854] = sdtx_font_kc854(),
                [FONT_Z1013] = sdtx_font_z1013(),
                [FONT_CPC] = sdtx_font_cpc(),
                [FONT_C64] = sdtx_font_c64(),
                [FONT_ORIC] = sdtx_font_oric()},
      .logger.func = slog_func,
  });
}

static void frame(void) {

  // set virtual canvas size to half display size so that
  // glyphs are 16x16 display pixels
  sdtx_canvas(sapp_width() * 0.5f, sapp_height() * 0.5f);
  // sdtx_origin(0.0f, 2.0f);
  sdtx_home();
  sdtx_font(FONT_KC853);
  sdtx_color3b(0xf4, 0x43, 0x36);
  sdtx_puts("\n ");
  if (state.path)
    sdtx_puts(state.path);
  if (state.wave_file.sample_data && state.wave_file.frame_count > 0) {
    int f = state.frame % state.wave_file.frame_count;
    f = state.wave_file.frame_count - f;
    int s = f / state.wave_file.sample_rate;
    int m = s / 60;
    s = s % 60;
    sdtx_color3b(0x43, 0x36, 0xf4);
    sdtx_printf("\n -%0.2d:%0.2d", m, s);
    sdtx_printf("\n %s", ctime(&state.last_modification));
  }

  sg_begin_pass(
      &(sg_pass){.action = state.pass_action, .swapchain = sglue_swapchain()});
  sdtx_draw();
  // __dbgui_draw();
  sg_end_pass();
  sg_commit();

  if (state.path != NULL && state.wave_file_new.sample_data == NULL) {
    struct stat st = {0};
    if (stat(state.path, &st) != -1) {
      if (state.last_modification != st.st_mtime) {
        state.wave_file_new = WaveFile_load(state.path);
        printf("load wav '%s' %d %d %d (%p)\n", state.path,
               state.wave_file_new.channels, state.wave_file_new.sample_rate,
               (int)state.wave_file_new.frame_count,
               state.wave_file_new.sample_data);
        if (state.wave_file_new.frame_count == 0) {
          WaveFile_dispose(&state.wave_file_new);
          printf(" fail '%s' %d %d %d (%p)\n", state.path,
                 state.wave_file_new.channels, state.wave_file_new.sample_rate,
                 (int)state.wave_file_new.frame_count,
                 state.wave_file_new.sample_data);
        } else
          state.last_modification = st.st_mtime;
      }
    }
  }
}

static void cleanup(void) {
  sdtx_shutdown();
  saudio_shutdown();
  // __dbgui_shutdown();
  WaveFile_dispose(&state.wave_file);
  sg_shutdown();
}

int main(int argc, char *argv[]) {

  if (argc > 1)
    state.path = argv[1];
  sapp_run(&(sapp_desc){
      .init_cb = init,
      .frame_cb = frame,
      .cleanup_cb = cleanup,
      // .event_cb = __dbgui_event,
      .width = 600,
      .height = 200,
      .window_title = "debugtext-sapp",
      .icon.sokol_default = true,
      .logger.func = slog_func,
  });

  return 0;
}