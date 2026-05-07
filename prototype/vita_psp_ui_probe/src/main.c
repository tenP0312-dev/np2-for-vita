#include <SDL2/SDL.h>
#include <psp2/ctrl.h>
#include <psp2/io/dirent.h>
#include <psp2/io/stat.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define SCREEN_W 960
#define SCREEN_H 544
#define PC98_W 640
#define PC98_H 400
#define APP_HOME "ux0:data/np2vita"
#define MAX_DISK_FILES 64

typedef struct {
  const char *label;
  int disabled;
  int checked;
} MenuItem;

typedef struct {
  const char *label;
  const MenuItem *items;
  int count;
} Menu;

static const MenuItem emulate_items[] = {
    {"Reset", 0, 0},
    {"Reset with HELP key", 0, 0},
    {"Configure...", 0, 0},
    {"NewDisk...", 1, 0},
    {"Font...", 1, 0},
    {"Exit", 0, 0},
};

static const MenuItem fdd_items[] = {
    {"Open...", 0, 0},
    {"Eject", 0, 0},
};

static const MenuItem hdd_items[] = {
    {"SASI1 / Open...", 0, 0},
    {"SASI1 / Remove", 0, 0},
    {"SASI2 / Open...", 0, 0},
    {"SASI2 / Remove", 0, 0},
};

static const MenuItem screen_items[] = {
    {"Disp Vsync", 0, 1},
    {"Real Palettes", 0, 0},
    {"No Wait", 0, 0},
    {"Auto frame", 0, 1},
    {"Full frame", 0, 0},
    {"1/2 frame", 0, 0},
    {"1/3 frame", 0, 0},
    {"1/4 frame", 0, 0},
};

static const MenuItem device_items[] = {
    {"Keyboard", 0, 1},
    {"JoyKey-1", 0, 0},
    {"JoyKey-2", 0, 0},
    {"Mouse-Key", 0, 0},
    {"PC-9801-26K", 0, 0},
    {"PC-9801-86", 0, 1},
    {"Memory 640KB", 0, 0},
    {"Memory 3.6MB", 0, 1},
};

static const MenuItem psp_items[] = {
    {"Clock 333MHz", 1, 0},
    {"Swap 98mouse buttons", 0, 0},
    {"432x270", 0, 0},
    {"480x272(Wide)", 0, 0},
    {"480x300", 0, 0},
    {"640x400", 0, 1},
};

static const MenuItem other_items[] = {
    {"BMP Save...", 0, 0},
    {"S98 logging...", 0, 0},
    {"Clock Disp", 0, 0},
    {"Frame Disp", 0, 1},
    {"Joy Reverse", 0, 0},
    {"Joy Rapid", 0, 0},
    {"About...", 0, 0},
};

static const Menu menus[] = {
    {"Emulate", emulate_items, (int)(sizeof(emulate_items) / sizeof(emulate_items[0]))},
    {"FDD1", fdd_items, (int)(sizeof(fdd_items) / sizeof(fdd_items[0]))},
    {"FDD2", fdd_items, (int)(sizeof(fdd_items) / sizeof(fdd_items[0]))},
    {"HDD", hdd_items, (int)(sizeof(hdd_items) / sizeof(hdd_items[0]))},
    {"Screen", screen_items, (int)(sizeof(screen_items) / sizeof(screen_items[0]))},
    {"Device", device_items, (int)(sizeof(device_items) / sizeof(device_items[0]))},
    {"PSP", psp_items, (int)(sizeof(psp_items) / sizeof(psp_items[0]))},
    {"Other", other_items, (int)(sizeof(other_items) / sizeof(other_items[0]))},
};

typedef struct {
  char name[48];
  char up[16];
  char down[16];
  char left[16];
  char right[16];
  char circle[16];
  char cross[16];
  char triangle[16];
  char square[16];
  char mm_triangle[16];
  char mm_square[16];
} KeyProfile;

enum {
  MAX_KEY_PROFILES = 12
};

static KeyProfile key_profiles[MAX_KEY_PROFILES];
static int key_profile_count = 0;

static const char *soft_rows[] = {
    "ESC  1 2 3 4 5 6 7 8 9 0 - ^ \\ BS",
    "TAB  Q W E R T Y U I O P @ [ RET",
    "CTRL  A S D F G H J K L ; : ]",
    "SHIFT  Z X C V B N M , . / _ SHIFT",
    "GRPH KANA NFER XFER SPACE HELP COPY STOP",
};

typedef struct {
  char name[256];
} DiskFile;

static DiskFile disk_files[MAX_DISK_FILES];
static int disk_file_count = 0;
static int disk_file_selected = 0;
static int file_browser_open = 0;

static const uint8_t font5x7[96][7] = {
    {0,0,0,0,0,0,0},{4,4,4,4,0,4,0},{10,10,0,0,0,0,0},{10,31,10,10,31,10,0},
    {4,15,20,14,5,30,4},{24,25,2,4,8,19,3},{12,18,20,8,21,18,13},{4,4,8,0,0,0,0},
    {2,4,8,8,8,4,2},{8,4,2,2,2,4,8},{0,10,4,31,4,10,0},{0,4,4,31,4,4,0},
    {0,0,0,0,4,4,8},{0,0,0,31,0,0,0},{0,0,0,0,0,12,12},{1,2,4,8,16,0,0},
    {14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},{30,1,1,14,1,1,30},
    {2,6,10,18,31,2,2},{31,16,30,1,1,17,14},{6,8,16,30,17,17,14},{31,1,2,4,8,8,8},
    {14,17,17,14,17,17,14},{14,17,17,15,1,2,12},{0,12,12,0,12,12,0},{0,12,12,0,4,4,8},
    {2,4,8,16,8,4,2},{0,0,31,0,31,0,0},{8,4,2,1,2,4,8},{14,17,1,2,4,0,4},
    {14,17,23,21,23,16,14},{14,17,17,31,17,17,17},{30,17,17,30,17,17,30},{14,17,16,16,16,17,14},
    {30,17,17,17,17,17,30},{31,16,16,30,16,16,31},{31,16,16,30,16,16,16},{14,17,16,23,17,17,15},
    {17,17,17,31,17,17,17},{14,4,4,4,4,4,14},{1,1,1,1,17,17,14},{17,18,20,24,20,18,17},
    {16,16,16,16,16,16,31},{17,27,21,21,17,17,17},{17,25,21,19,17,17,17},{14,17,17,17,17,17,14},
    {30,17,17,30,16,16,16},{14,17,17,17,21,18,13},{30,17,17,30,20,18,17},{15,16,16,14,1,1,30},
    {31,4,4,4,4,4,4},{17,17,17,17,17,17,14},{17,17,17,17,17,10,4},{17,17,17,21,21,21,10},
    {17,17,10,4,10,17,17},{17,17,10,4,4,4,4},{31,1,2,4,8,16,31},{14,8,8,8,8,8,14},
    {16,8,4,2,1,0,0},{14,2,2,2,2,2,14},{4,10,17,0,0,0,0},{0,0,0,0,0,0,31},
    {8,4,2,0,0,0,0},{0,0,14,1,15,17,15},{16,16,30,17,17,17,30},{0,0,14,16,16,16,14},
    {1,1,15,17,17,17,15},{0,0,14,17,31,16,14},{6,8,8,30,8,8,8},{0,0,15,17,17,15,1},
    {16,16,30,17,17,17,17},{4,0,12,4,4,4,14},{2,0,6,2,2,18,12},{16,16,18,20,24,20,18},
    {12,4,4,4,4,4,14},{0,0,26,21,21,21,21},{0,0,30,17,17,17,17},{0,0,14,17,17,17,14},
    {0,0,30,17,17,30,16},{0,0,15,17,17,15,1},{0,0,22,24,16,16,16},{0,0,15,16,14,1,30},
    {8,8,30,8,8,8,6},{0,0,17,17,17,17,15},{0,0,17,17,17,10,4},{0,0,17,17,21,21,10},
    {0,0,17,10,4,10,17},{0,0,17,17,17,15,1},{0,0,31,2,4,8,31},{2,4,4,8,4,4,2},
    {4,4,4,0,4,4,4},{8,4,4,2,4,4,8},{8,21,2,0,0,0,0},{31,31,31,31,31,31,31}
};

static void set_color(SDL_Renderer *r, uint32_t rgba) {
  SDL_SetRenderDrawColor(r, (rgba >> 24) & 0xff, (rgba >> 16) & 0xff, (rgba >> 8) & 0xff, rgba & 0xff);
}

static void fill_rect(SDL_Renderer *r, int x, int y, int w, int h, uint32_t rgba) {
  SDL_Rect rect = {x, y, w, h};
  set_color(r, rgba);
  SDL_RenderFillRect(r, &rect);
}

static void draw_rect(SDL_Renderer *r, int x, int y, int w, int h, uint32_t rgba) {
  SDL_Rect rect = {x, y, w, h};
  set_color(r, rgba);
  SDL_RenderDrawRect(r, &rect);
}

static void draw_char(SDL_Renderer *r, int x, int y, char ch, int scale, uint32_t rgba) {
  unsigned c = (unsigned char)ch;
  if (c < 32 || c > 127) {
    c = '?';
  }
  set_color(r, rgba);
  for (int row = 0; row < 7; row++) {
    uint8_t bits = font5x7[c - 32][row];
    for (int col = 0; col < 5; col++) {
      if (bits & (1u << (4 - col))) {
        SDL_Rect px = {x + col * scale, y + row * scale, scale, scale};
        SDL_RenderFillRect(r, &px);
      }
    }
  }
}

static void draw_text(SDL_Renderer *r, int x, int y, const char *text, int scale, uint32_t rgba) {
  while (*text) {
    draw_char(r, x, y, *text++, scale, rgba);
    x += 6 * scale;
  }
}

static void draw_pc98(SDL_Renderer *r) {
  int x = (SCREEN_W - PC98_W) / 2;
  int y = 72;
  fill_rect(r, x - 1, y - 1, PC98_W + 2, PC98_H + 2, 0x06080cff);
  fill_rect(r, x, y, PC98_W, PC98_H, 0x10151dff);
  for (int yy = 0; yy < PC98_H; yy += 16) {
    uint32_t color = (yy % 32) ? 0x17202aff : 0x1d2733ff;
    fill_rect(r, x, y + yy, PC98_W, 8, color);
  }
  draw_text(r, x + 24, y + 24, "PC-9801 framebuffer placeholder", 2, 0x75c789ff);
  draw_text(r, x + 24, y + 54, "PSP UI probe - emulator core not attached", 2, 0xd7dee8ff);
}

static int menu_x_for(int active) {
  int x = 8;
  for (int i = 0; i < active; i++) {
    x += (int)strlen(menus[i].label) * 12 + 24;
  }
  return x;
}

static void draw_menu_bar(SDL_Renderer *r, int active, int open) {
  fill_rect(r, 0, 0, SCREEN_W, 30, 0xd7d7d7ff);
  draw_rect(r, 0, 0, SCREEN_W, 30, 0x777777ff);
  int x = 8;
  for (int i = 0; i < (int)(sizeof(menus) / sizeof(menus[0])); i++) {
    int w = (int)strlen(menus[i].label) * 12 + 18;
    if (i == active) {
      fill_rect(r, x - 4, 4, w, 22, open ? 0x263852ff : 0xbac7d8ff);
    }
    draw_text(r, x, 9, menus[i].label, 2, (i == active && open) ? 0xffffffff : 0x101010ff);
    x += w + 6;
  }
}

static void draw_dropdown(SDL_Renderer *r, int active, int item) {
  const Menu *m = &menus[active];
  int x = menu_x_for(active);
  int y = 31;
  int w = 250;
  int h = m->count * 26 + 10;
  fill_rect(r, x, y, w, h, 0xe9e9e9ff);
  draw_rect(r, x, y, w, h, 0x4b4b4bff);
  for (int i = 0; i < m->count; i++) {
    int iy = y + 6 + i * 26;
    if (i == item) {
      fill_rect(r, x + 4, iy - 2, w - 8, 24, 0x2f5d92ff);
    }
    uint32_t fg = m->items[i].disabled ? 0x8a8a8aff : ((i == item) ? 0xffffffff : 0x111111ff);
    if (m->items[i].checked) {
      draw_text(r, x + 10, iy + 4, "*", 2, fg);
    }
    draw_text(r, x + 30, iy + 4, m->items[i].label, 2, fg);
  }
}

static void draw_key_profile(SDL_Renderer *r, int profile, int mouse_mode) {
  const KeyProfile *k = &key_profiles[profile];
  int x = 18;
  int y = 486;
  fill_rect(r, x, y, 520, 48, 0x000000bb);
  draw_rect(r, x, y, 520, 48, 0x9bb7d5ff);
  draw_text(r, x + 12, y + 8, mouse_mode ? "Mode: PC98 mouse" : "Mode: config key", 2, 0xffffffff);
  draw_text(r, x + 12, y + 28, k->name, 2, 0x75c789ff);
  draw_text(r, x + 178, y + 28, "O=", 2, 0xd7dee8ff);
  draw_text(r, x + 206, y + 28, k->circle, 2, 0xd7dee8ff);
  draw_text(r, x + 284, y + 28, "X=", 2, 0xd7dee8ff);
  draw_text(r, x + 312, y + 28, k->cross, 2, 0xd7dee8ff);
  draw_text(r, x + 390, y + 28, "SEL cycles", 2, 0xd7dee8ff);
}

static void draw_soft_keyboard(SDL_Renderer *r) {
  int x = 84;
  int y = 316;
  int w = 792;
  int h = 188;
  fill_rect(r, x, y, w, h, 0x111a25ee);
  draw_rect(r, x, y, w, h, 0xcad6e4ff);
  draw_text(r, x + 16, y + 12, "Soft keyboard (PSP R trigger behavior)", 2, 0xffffffff);
  for (int row = 0; row < 5; row++) {
    draw_text(r, x + 24, y + 46 + row * 26, soft_rows[row], 2, 0xd7dee8ff);
  }
}

static void draw_status(SDL_Renderer *r, const char *status) {
  fill_rect(r, 568, 486, 374, 48, 0x000000bb);
  draw_rect(r, 568, 486, 374, 48, 0x9bb7d5ff);
  draw_text(r, 582, 496, "L menu  R softkbd  START mode", 2, 0xd7dee8ff);
  draw_text(r, 582, 516, status, 2, 0x75c789ff);
}

static int has_disk_extension(const char *name) {
  const char *dot = strrchr(name, '.');
  if (!dot) {
    return 0;
  }
  char ext[8];
  snprintf(ext, sizeof(ext), "%s", dot + 1);
  for (char *p = ext; *p; p++) {
    *p = (char)tolower((unsigned char)*p);
  }
  return strcmp(ext, "d88") == 0 || strcmp(ext, "fdi") == 0 || strcmp(ext, "xdf") == 0 ||
         strcmp(ext, "hdm") == 0 || strcmp(ext, "hdi") == 0 || strcmp(ext, "thd") == 0 ||
         strcmp(ext, "nfd") == 0 || strcmp(ext, "fdd") == 0;
}

static void ensure_home_dir(void) {
  sceIoMkdir("ux0:data", 0777);
  sceIoMkdir(APP_HOME, 0777);
}

static void scan_disk_files(void) {
  disk_file_count = 0;
  disk_file_selected = 0;
  SceUID fd = sceIoDopen(APP_HOME);
  if (fd < 0) {
    return;
  }

  SceIoDirent ent;
  memset(&ent, 0, sizeof(ent));
  while (disk_file_count < MAX_DISK_FILES && sceIoDread(fd, &ent) > 0) {
    if (ent.d_name[0] == '.') {
      memset(&ent, 0, sizeof(ent));
      continue;
    }
    if (has_disk_extension(ent.d_name)) {
      snprintf(disk_files[disk_file_count].name, sizeof(disk_files[disk_file_count].name), "%s", ent.d_name);
      disk_file_count++;
    }
    memset(&ent, 0, sizeof(ent));
  }
  sceIoDclose(fd);
}

static void draw_file_browser(SDL_Renderer *r) {
  int x = 128;
  int y = 74;
  int w = 704;
  int h = 392;
  fill_rect(r, x, y, w, h, 0x10151df5);
  draw_rect(r, x, y, w, h, 0xd7dee8ff);
  draw_text(r, x + 18, y + 16, "Open disk image", 2, 0xffffffff);
  draw_text(r, x + 18, y + 38, APP_HOME, 2, 0x75c789ff);

  if (disk_file_count == 0) {
    draw_text(r, x + 18, y + 86, "No disk images found.", 2, 0xffd27dff);
    draw_text(r, x + 18, y + 112, "Put .d88 .fdi .xdf .hdm .hdi files there.", 2, 0xd7dee8ff);
    draw_text(r, x + 18, y + 340, "Cross: close", 2, 0xd7dee8ff);
    return;
  }

  int max_visible = 10;
  int first = disk_file_selected - max_visible + 1;
  if (first < 0) {
    first = 0;
  }
  for (int i = 0; i < max_visible && first + i < disk_file_count; i++) {
    int idx = first + i;
    int iy = y + 76 + i * 26;
    if (idx == disk_file_selected) {
      fill_rect(r, x + 14, iy - 2, w - 28, 24, 0x2f5d92ff);
    }
    draw_text(r, x + 28, iy + 4, disk_files[idx].name, 2, idx == disk_file_selected ? 0xffffffff : 0xd7dee8ff);
  }
  draw_text(r, x + 18, y + 340, "Circle: choose   Cross: close", 2, 0xd7dee8ff);
}

static void trim(char *s) {
  char *start = s;
  while (*start && isspace((unsigned char)*start)) {
    start++;
  }
  if (start != s) {
    memmove(s, start, strlen(start) + 1);
  }
  size_t len = strlen(s);
  while (len > 0 && isspace((unsigned char)s[len - 1])) {
    s[--len] = '\0';
  }
}

static int is_ascii_printable(const char *s) {
  while (*s) {
    unsigned char c = (unsigned char)*s++;
    if (c < 32 || c > 126) {
      return 0;
    }
  }
  return 1;
}

static void copy_value(char *dst, size_t dst_size, const char *src) {
  if (!src || !*src) {
    src = "-";
  }
  snprintf(dst, dst_size, "%s", src);
}

static void key_profile_defaults(KeyProfile *p, int index) {
  snprintf(p->name, sizeof(p->name), "Profile %d", index + 1);
  copy_value(p->up, sizeof(p->up), "UP");
  copy_value(p->down, sizeof(p->down), "DOWN");
  copy_value(p->left, sizeof(p->left), "LEFT");
  copy_value(p->right, sizeof(p->right), "RIGHT");
  copy_value(p->circle, sizeof(p->circle), "RET");
  copy_value(p->cross, sizeof(p->cross), "ESC");
  copy_value(p->triangle, sizeof(p->triangle), "SHIFT");
  copy_value(p->square, sizeof(p->square), "SPC");
  copy_value(p->mm_triangle, sizeof(p->mm_triangle), "SHIFT");
  copy_value(p->mm_square, sizeof(p->mm_square), "SPC");
}

static void key_profile_fallback(void) {
  key_profile_count = 3;
  key_profile_defaults(&key_profiles[0], 0);
  copy_value(key_profiles[0].name, sizeof(key_profiles[0].name), "Cursor keys");
  key_profile_defaults(&key_profiles[1], 1);
  copy_value(key_profiles[1].name, sizeof(key_profiles[1].name), "Ten-key move");
  copy_value(key_profiles[1].up, sizeof(key_profiles[1].up), "[8]");
  copy_value(key_profiles[1].down, sizeof(key_profiles[1].down), "[2]");
  copy_value(key_profiles[1].left, sizeof(key_profiles[1].left), "[4]");
  copy_value(key_profiles[1].right, sizeof(key_profiles[1].right), "[6]");
  key_profile_defaults(&key_profiles[2], 2);
  copy_value(key_profiles[2].name, sizeof(key_profiles[2].name), "Template");
  copy_value(key_profiles[2].up, sizeof(key_profiles[2].up), "-");
  copy_value(key_profiles[2].down, sizeof(key_profiles[2].down), "-");
  copy_value(key_profiles[2].left, sizeof(key_profiles[2].left), "-");
  copy_value(key_profiles[2].right, sizeof(key_profiles[2].right), "-");
  copy_value(key_profiles[2].circle, sizeof(key_profiles[2].circle), "-");
  copy_value(key_profiles[2].cross, sizeof(key_profiles[2].cross), "-");
  copy_value(key_profiles[2].triangle, sizeof(key_profiles[2].triangle), "-");
  copy_value(key_profiles[2].square, sizeof(key_profiles[2].square), "-");
}

static int load_key_profiles(void) {
  const char *paths[] = {
      APP_HOME "/psp_key.txt",
      "app0:/psp_key.txt",
      "psp_key.txt",
      "prototype/vita_psp_ui_probe/data/psp_key.txt",
  };
  FILE *fp = NULL;
  for (unsigned i = 0; i < sizeof(paths) / sizeof(paths[0]); i++) {
    fp = fopen(paths[i], "r");
    if (fp) {
      break;
    }
  }
  if (!fp) {
    key_profile_fallback();
    return 0;
  }

  char line[160];
  KeyProfile *current = NULL;
  key_profile_count = 0;
  while (fgets(line, sizeof(line), fp)) {
    trim(line);
    if (!line[0] || line[0] == ';' || line[0] == '#') {
      continue;
    }
    if (strcmp(line, "[KeySetting]") == 0) {
      if (key_profile_count >= MAX_KEY_PROFILES) {
        current = NULL;
        continue;
      }
      current = &key_profiles[key_profile_count];
      key_profile_defaults(current, key_profile_count);
      key_profile_count++;
      continue;
    }
    if (!current) {
      continue;
    }

    char *eq = strchr(line, '=');
    if (!eq) {
      continue;
    }
    *eq = '\0';
    char *key = line;
    char *value = eq + 1;
    trim(key);
    trim(value);
    if (value[0] == '"' && value[strlen(value) - 1] == '"') {
      value[strlen(value) - 1] = '\0';
      value++;
    }

    if (strcmp(key, "up") == 0) {
      copy_value(current->up, sizeof(current->up), value);
    } else if (strcmp(key, "down") == 0) {
      copy_value(current->down, sizeof(current->down), value);
    } else if (strcmp(key, "left") == 0) {
      copy_value(current->left, sizeof(current->left), value);
    } else if (strcmp(key, "right") == 0) {
      copy_value(current->right, sizeof(current->right), value);
    } else if (strcmp(key, "circle") == 0) {
      copy_value(current->circle, sizeof(current->circle), value);
    } else if (strcmp(key, "cross") == 0) {
      copy_value(current->cross, sizeof(current->cross), value);
    } else if (strcmp(key, "triangle") == 0) {
      copy_value(current->triangle, sizeof(current->triangle), value);
    } else if (strcmp(key, "square") == 0) {
      copy_value(current->square, sizeof(current->square), value);
    } else if (strcmp(key, "mm_triangle") == 0) {
      copy_value(current->mm_triangle, sizeof(current->mm_triangle), value);
    } else if (strcmp(key, "mm_square") == 0) {
      copy_value(current->mm_square, sizeof(current->mm_square), value);
    } else if (strcmp(key, "comment") == 0) {
      if (is_ascii_printable(value) && value[0]) {
        copy_value(current->name, sizeof(current->name), value);
      }
    }
  }
  fclose(fp);

  if (key_profile_count <= 0) {
    key_profile_fallback();
    return 0;
  }
  return 1;
}

int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;

  sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) {
    return 1;
  }

  SDL_Window *window = SDL_CreateWindow("NP2 PSP UI Probe", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                        SCREEN_W, SCREEN_H, SDL_WINDOW_SHOWN);
  if (!window) {
    SDL_Quit();
    return 2;
  }
  SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
  if (!renderer) {
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 3;
  }

  int running = 1;
  int menu_open = 1;
  int active_menu = 0;
  int active_item = 0;
  int softkbd = 0;
  int profile = 0;
  int mouse_mode = 0;
  char status[96] = "PSP menu assets mapped for Vita";
  uint32_t last_buttons = 0;
  ensure_home_dir();
  scan_disk_files();
  int loaded_key_file = load_key_profiles();
  snprintf(status, sizeof(status), loaded_key_file ? "loaded psp_key.txt (%d profiles)" : "using fallback key profiles",
           key_profile_count);

  while (running) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) {
        running = 0;
      }
    }

    SceCtrlData pad;
    sceCtrlPeekBufferPositive(0, &pad, 1);
    uint32_t now = pad.buttons;
    uint32_t pressed = now & ~last_buttons;
    last_buttons = now;

    if (file_browser_open) {
      if (pressed & SCE_CTRL_UP && disk_file_count > 0) {
        disk_file_selected = (disk_file_selected + disk_file_count - 1) % disk_file_count;
      }
      if (pressed & SCE_CTRL_DOWN && disk_file_count > 0) {
        disk_file_selected = (disk_file_selected + 1) % disk_file_count;
      }
      if (pressed & SCE_CTRL_CROSS) {
        file_browser_open = 0;
      }
      if (pressed & SCE_CTRL_CIRCLE && disk_file_count > 0) {
        snprintf(status, sizeof(status), "selected %s", disk_files[disk_file_selected].name);
        file_browser_open = 0;
      }
    } else if (pressed & SCE_CTRL_LTRIGGER) {
      menu_open = !menu_open;
    }
    if (pressed & SCE_CTRL_RTRIGGER) {
      softkbd = !softkbd;
    }
    if (pressed & SCE_CTRL_START) {
      mouse_mode = !mouse_mode;
    }
    if (pressed & SCE_CTRL_SELECT) {
      profile = (profile + 1) % key_profile_count;
    }
    if (pressed & SCE_CTRL_TRIANGLE) {
      running = 0;
    }

    if (!file_browser_open && menu_open) {
      if (pressed & SCE_CTRL_LEFT) {
        active_menu = (active_menu + (int)(sizeof(menus) / sizeof(menus[0])) - 1) %
                      (int)(sizeof(menus) / sizeof(menus[0]));
        active_item = 0;
      }
      if (pressed & SCE_CTRL_RIGHT) {
        active_menu = (active_menu + 1) % (int)(sizeof(menus) / sizeof(menus[0]));
        active_item = 0;
      }
      if (pressed & SCE_CTRL_UP) {
        active_item = (active_item + menus[active_menu].count - 1) % menus[active_menu].count;
      }
      if (pressed & SCE_CTRL_DOWN) {
        active_item = (active_item + 1) % menus[active_menu].count;
      }
      if (pressed & SCE_CTRL_CIRCLE) {
        snprintf(status, sizeof(status), "%s / %s", menus[active_menu].label,
                 menus[active_menu].items[active_item].label);
        if ((active_menu == 1 || active_menu == 2 || active_menu == 3) && active_item == 0) {
          scan_disk_files();
          file_browser_open = 1;
          menu_open = 0;
        }
      }
      if (pressed & SCE_CTRL_CROSS) {
        menu_open = 0;
      }
    }

    fill_rect(renderer, 0, 0, SCREEN_W, SCREEN_H, 0x202833ff);
    draw_pc98(renderer);
    draw_menu_bar(renderer, active_menu, menu_open);
    if (menu_open) {
      draw_dropdown(renderer, active_menu, active_item);
    }
    if (softkbd) {
      draw_soft_keyboard(renderer);
    }
    if (file_browser_open) {
      draw_file_browser(renderer);
    }
    draw_key_profile(renderer, profile, mouse_mode);
    draw_status(renderer, status);
    SDL_RenderPresent(renderer);
    SDL_Delay(16);
  }

  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
