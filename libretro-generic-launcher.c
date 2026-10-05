/*
 * Generic external-emulator launcher core for RetroArch (Windows).
 *
 * Based on libretro-dolphin-launcher by Rob Loach (MIT).
 *
 * One DLL, any emulator: all details come from a text file placed next to the
 * DLL, with the same name as the DLL but ending in .cfg
 *   launcher_rpcs3_libretro.dll  ->  launcher_rpcs3_libretro.cfg
 * To add another emulator, copy + rename the DLL and write a new .cfg.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>
#include "libretro.h"

#ifdef _WIN32
#include <windows.h>
#endif

static uint32_t *frame_buf;
static struct retro_log_callback logging;
static retro_log_printf_t log_cb;
static retro_video_refresh_t video_cb;
static retro_audio_sample_t audio_cb;
static retro_audio_sample_batch_t audio_batch_cb;
static retro_environment_t environ_cb;
static retro_input_poll_t input_poll_cb;
static retro_input_state_t input_state_cb;

/* ---------------------------------------------------------------- config */

#define CFG_STR 1024
static struct {
   bool  loaded;
   char  cfg_path[MAX_PATH + 8];
   char  name[CFG_STR];        /* name=       */
   char  extensions[CFG_STR];  /* extensions= */
   char  exe[CFG_STR];         /* exe=        */
   char  args[CFG_STR * 2];    /* args=       */
   char  workdir[CFG_STR];     /* workdir=    */
   int   wait;                 /* wait=1/0    */
} cfg;

static void logmsg(const char *fmt, ...)
{
   char buf[2048];
   va_list va;
   va_start(va, fmt);
   vsnprintf(buf, sizeof(buf), fmt, va);
   va_end(va);
   if (log_cb)
      log_cb(RETRO_LOG_INFO, "[Launcher] %s\n", buf);
   fprintf(stderr, "[Launcher] %s\n", buf);
}

static char *trim(char *s)
{
   char *end;
   if ((unsigned char)s[0] == 0xEF && (unsigned char)s[1] == 0xBB && (unsigned char)s[2] == 0xBF)
      s += 3;
   while (*s == ' ' || *s == '\t')
      s++;
   end = s + strlen(s);
   while (end > s && (end[-1] == '\n' || end[-1] == '\r' || end[-1] == ' ' || end[-1] == '\t'))
      end--;
   *end = '\0';
   return s;
}

/* Removes one pair of surrounding quotes, if present. */
static char *unquote(char *s)
{
   size_t n = strlen(s);
   if (n >= 2 && s[0] == '"' && s[n - 1] == '"') {
      s[n - 1] = '\0';
      return s + 1;
   }
   return s;
}

#ifdef _WIN32
static void dll_anchor(void) {}

static void config_load(void)
{
   HMODULE self = NULL;
   char line[CFG_STR * 3];
   char *dot, *slash;
   FILE *f;

   if (cfg.loaded)
      return;
   cfg.loaded = true;
   cfg.wait = 1;

   if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCSTR)&dll_anchor, &self) ||
       GetModuleFileNameA(self, cfg.cfg_path, MAX_PATH) == 0) {
      cfg.cfg_path[0] = '\0';
      return;
   }

   /* Default name = DLL file name without "_libretro.dll". */
   slash = strrchr(cfg.cfg_path, '\\');
   snprintf(cfg.name, sizeof(cfg.name), "%s", slash ? slash + 1 : cfg.cfg_path);
   dot = strstr(cfg.name, "_libretro");
   if (dot == NULL)
      dot = strrchr(cfg.name, '.');
   if (dot)
      *dot = '\0';

   /* cfg path = DLL path with the extension replaced by .cfg */
   dot = strrchr(cfg.cfg_path, '.');
   if (dot == NULL || (slash && dot < slash))
      dot = cfg.cfg_path + strlen(cfg.cfg_path);
   strcpy(dot, ".cfg");

   f = fopen(cfg.cfg_path, "r");
   if (f == NULL)
      return;

   while (fgets(line, sizeof(line), f) != NULL) {
      char *key = trim(line);
      char *val;
      if (key[0] == '\0' || key[0] == '#' || key[0] == ';')
         continue;
      val = strchr(key, '=');
      if (val == NULL)
         continue;
      *val++ = '\0';
      key = trim(key);
      val = trim(val);

      if (!strcmp(key, "name"))            snprintf(cfg.name, sizeof(cfg.name), "%s", val);
      else if (!strcmp(key, "extensions")) snprintf(cfg.extensions, sizeof(cfg.extensions), "%s", val);
      else if (!strcmp(key, "exe"))        snprintf(cfg.exe, sizeof(cfg.exe), "%s", unquote(val));
      else if (!strcmp(key, "args"))       snprintf(cfg.args, sizeof(cfg.args), "%s", val);
      else if (!strcmp(key, "workdir"))    snprintf(cfg.workdir, sizeof(cfg.workdir), "%s", unquote(val));
      else if (!strcmp(key, "wait"))       cfg.wait = atoi(val);
   }
   fclose(f);
}

/* First non-empty line of the content file (for small text "stub" files).
 * Returns false for big or binary files (e.g. a real EBOOT.BIN), so the
 * caller falls back to using the file path itself. */
static bool read_first_line(const char *path, char *out, size_t out_size)
{
   char buf[2048];
   size_t n, i;
   char *line, *next;
   FILE *f = fopen(path, "rb");
   if (f == NULL)
      return false;
   n = fread(buf, 1, sizeof(buf) - 1, f);
   if (fgetc(f) != EOF) { fclose(f); return false; }   /* file too big */
   fclose(f);
   buf[n] = '\0';
   for (i = 0; i < n; i++)
      if (buf[i] == '\0')
         return false;                                  /* binary file */

   for (line = buf; line && *line; line = next) {
      char *t;
      next = strchr(line, '\n');
      if (next) *next++ = '\0';
      t = unquote(trim(line));
      if (t[0] != '\0') {
         snprintf(out, out_size, "%s", t);
         return true;
      }
   }
   return false;
}

/* Appends src to dst (dst capacity cap). */
static void append(char *dst, size_t cap, const char *src)
{
   size_t len = strlen(dst);
   if (len + 1 < cap)
      snprintf(dst + len, cap - len, "%s", src);
}

/* Replaces {rom} {rom_dir} {rom_file} {rom_name} {target} inside 'tpl'. */
static void expand(const char *tpl, const char *rom, char *out, size_t cap)
{
   char rom_dir[CFG_STR] = "", rom_file[CFG_STR] = "", rom_name[CFG_STR] = "", target[CFG_STR * 2] = "";
   char *s, *dot;
   const char *p = tpl;

   snprintf(rom_file, sizeof(rom_file), "%s", rom);
   s = strrchr(rom, '\\');
   if (s == NULL) s = strrchr(rom, '/');
   if (s) {
      snprintf(rom_dir, sizeof(rom_dir), "%.*s", (int)(s - rom), rom);
      snprintf(rom_file, sizeof(rom_file), "%s", s + 1);
   }
   snprintf(rom_name, sizeof(rom_name), "%s", rom_file);
   dot = strrchr(rom_name, '.');
   if (dot) *dot = '\0';
   if (strstr(tpl, "{target}") && !read_first_line(rom, target, sizeof(target)))
      snprintf(target, sizeof(target), "%s", rom);

   out[0] = '\0';
   while (*p) {
      if (*p == '{') {
         if      (!strncmp(p, "{rom}", 5))      { append(out, cap, rom);      p += 5; continue; }
         else if (!strncmp(p, "{rom_dir}", 9))  { append(out, cap, rom_dir);  p += 9; continue; }
         else if (!strncmp(p, "{rom_file}", 10)){ append(out, cap, rom_file); p += 10; continue; }
         else if (!strncmp(p, "{rom_name}", 10)){ append(out, cap, rom_name); p += 10; continue; }
         else if (!strncmp(p, "{target}", 8))   { append(out, cap, target);   p += 8; continue; }
      }
      {
         char one[2] = { *p, '\0' };
         append(out, cap, one);
         p++;
      }
   }
}

static bool launch(const char *rom)
{
   char args[CFG_STR * 4];
   char command[CFG_STR * 5];
   char workdir[CFG_STR];
   STARTUPINFOA si;
   PROCESS_INFORMATION pi;
   DWORD code = 0;
   const char *wd = NULL;

   config_load();

   if (cfg.exe[0] == '\0') {
      logmsg("No 'exe=' found. Expected config file: %s", cfg.cfg_path[0] ? cfg.cfg_path : "(unknown)");
      return false;
   }

   expand(cfg.args, rom ? rom : "", args, sizeof(args));
   snprintf(command, sizeof(command), "\"%s\"%s%s", cfg.exe, args[0] ? " " : "", args);

   if (cfg.workdir[0] != '\0') {
      wd = cfg.workdir;
   } else {
      char *slash;
      snprintf(workdir, sizeof(workdir), "%s", cfg.exe);
      slash = strrchr(workdir, '\\');
      if (slash) { *slash = '\0'; wd = workdir; }
   }

   logmsg("Running: %s", command);

   memset(&si, 0, sizeof(si));
   si.cb = sizeof(si);
   memset(&pi, 0, sizeof(pi));
   if (!CreateProcessA(NULL, command, NULL, NULL, FALSE, 0, NULL, wd, &si, &pi)) {
      logmsg("Could not start '%s' (Windows error %lu)", cfg.exe, (unsigned long)GetLastError());
      return false;
   }
   if (cfg.wait) {
      WaitForSingleObject(pi.hProcess, INFINITE);
      GetExitCodeProcess(pi.hProcess, &code);
      logmsg("Finished (exit code %lu)", (unsigned long)code);
   }
   CloseHandle(pi.hProcess);
   CloseHandle(pi.hThread);
   return true;
}
#else
static void config_load(void) { cfg.loaded = true; }
static bool launch(const char *rom) { (void)rom; return false; }
#endif

/* --------------------------------------------------------------- libretro */

static void fallback_log(enum retro_log_level level, const char *fmt, ...)
{
   (void)level;
   va_list va;
   va_start(va, fmt);
   vfprintf(stderr, fmt, va);
   va_end(va);
}

void retro_init(void)   { frame_buf = calloc(320 * 240, sizeof(uint32_t)); }
void retro_deinit(void) { free(frame_buf); frame_buf = NULL; }
unsigned retro_api_version(void) { return RETRO_API_VERSION; }
void retro_set_controller_port_device(unsigned port, unsigned device) { (void)port; (void)device; }

void retro_get_system_info(struct retro_system_info *info)
{
   config_load();
   memset(info, 0, sizeof(*info));
   info->library_name     = cfg.name[0] ? cfg.name : "External Launcher";
   info->library_version  = "2.0";
   info->need_fullpath    = true;
   info->valid_extensions = cfg.extensions[0] ? cfg.extensions : NULL;
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
   info->timing = (struct retro_system_timing) { .fps = 60.0, .sample_rate = 30000.0 };
   info->geometry = (struct retro_game_geometry) {
      .base_width = 320, .base_height = 240, .max_width = 320, .max_height = 240,
      .aspect_ratio = 4.0f / 3.0f,
   };
}

void retro_set_environment(retro_environment_t cb)
{
   bool no_content = true;
   environ_cb = cb;
   cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_content);
   if (cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &logging))
      log_cb = logging.log;
   else
      log_cb = fallback_log;
}

void retro_set_audio_sample(retro_audio_sample_t cb)             { audio_cb = cb; }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { audio_batch_cb = cb; }
void retro_set_input_poll(retro_input_poll_t cb)                 { input_poll_cb = cb; }
void retro_set_input_state(retro_input_state_t cb)               { input_state_cb = cb; }
void retro_set_video_refresh(retro_video_refresh_t cb)           { video_cb = cb; }
void retro_reset(void) {}

void retro_run(void)
{
   video_cb(frame_buf, 320, 240, 320 << 2);
   environ_cb(RETRO_ENVIRONMENT_SHUTDOWN, NULL);
}

bool retro_load_game(const struct retro_game_info *info)
{
   const char *rom = (info && info->path) ? info->path : "";
   return launch(rom);
}

void retro_unload_game(void) {}
unsigned retro_get_region(void) { return RETRO_REGION_NTSC; }
bool retro_load_game_special(unsigned t, const struct retro_game_info *info, size_t n) { (void)t; (void)n; return retro_load_game(info); }
size_t retro_serialize_size(void) { return 0; }
bool retro_serialize(void *d, size_t s) { (void)d; (void)s; return true; }
bool retro_unserialize(const void *d, size_t s) { (void)d; (void)s; return true; }
void *retro_get_memory_data(unsigned id) { (void)id; return NULL; }
size_t retro_get_memory_size(unsigned id) { (void)id; return 0; }
void retro_cheat_reset(void) {}
void retro_cheat_set(unsigned i, bool e, const char *c) { (void)i; (void)e; (void)c; }
