#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
  ERR_MEM = 1,
  ERR_IO,
  ERR_UNK_BOX,
  ERR_BOX_SIZE,
  ERR_UNK_HDLR_TYPE,
  ERR_UNK_NAL_UNIT_TYPE,
  ERR_READ_CODE,
  ERR_READ_BIT,
  ERR_READ_BITS,
  ERR_UNK_PIC_ORDER_CNT_TYPE,
  ERR_UNK_OVERSCAN_INFO_PRESENT_FLAG,
  ERR_UNK_VIDEO_SIGNAL_TYPE_PRESENT_FLAG,
  ERR_UNK_CHROMA_LOC_INFO_PRESENT_FLAG,
  ERR_UNK_ASPECT_RATIO_IDC,
  ERR_UNK_NAL_HRD_PARA_PRESENT_FLAG,
  ERR_UNK_VCL_HRD_PARA_PRESENT_FLAG,
  ERR_UNK_NUM_SLICE_GROUPS_MINUS1,
  ERR_UNK_PROFILE_IDC,
  ERR_LEN
};

union box {
  struct box_top * top;
  struct box_ftyp * ftyp;
  struct box_moov * moov;
  struct box_trak * trak;
  struct box_mdia * mdia;
  struct box_minf * minf;
  struct box_dinf * dinf;
  struct box_stsd * stsd;
  struct box_visual_sample_entry * v_entry;
};

typedef union box box_t;

struct box_ftyp {
  char m_brand[4]; /* major brand */
  unsigned int m_version; /* minor version */
  char (* c_brands)[4]; /* compatible brands */
};

struct box_mvhd {
  unsigned int c_time; /* creation time */
  unsigned int m_time; /* modification time */
  unsigned int timescale;
  unsigned int duration;
  int rate;
  short volume;
  short _;
  int matrix[9];
  unsigned int next_track_id;
};

struct box_tkhd {
  unsigned char track_enabled;
  unsigned char track_in_movie;
  unsigned char track_in_preview;
  unsigned char _;
  unsigned int c_time;
  unsigned int m_time;
  unsigned int track_id;
  unsigned int duration;
  short layer;
  short alternate_group;
  short volume;
  short _2;
  int matrix[9];
  unsigned int width;
  unsigned int height;
};

struct box_mdhd {
  unsigned int c_time;
  unsigned int m_time;
  unsigned int timescale;
  unsigned int duration;
  char _;
  char lang[3];
};

struct box_hdlr {
  char type[4];
  char * name;
};

struct box_data_entry {
  char type[3];
  unsigned char self_contained;
  char * name;
  char * location;
};

struct box_dref {
  unsigned int entry_count;
  struct box_data_entry * entry;
};

struct box_dinf {
  struct box_dref dref;
};

struct box_visual_sample_entry {
  unsigned short dref_index;
  unsigned short width;
  unsigned short height;
  unsigned int h_rez; /* horizresolution */
  unsigned int v_rez; /* vertresolution */
  unsigned short frame_count;
  unsigned short depth;
  char compressorname[32];
};

struct box_stsd {
  unsigned int entry_count;
  box_t entry;
};

struct box_stbl {
  struct box_stsd stsd;
};

struct box_minf {
  struct box_dinf dinf;
  struct box_stbl stbl;
};

struct box_mdia {
  struct box_mdhd mdhd;
  struct box_hdlr hdlr;
  struct box_minf minf;
};

struct box_trak {
  struct box_tkhd tkhd;
  struct box_mdia mdia;
};

struct box_moov {
  struct box_mvhd mvhd;
  struct box_trak * trak;
  unsigned int trak_len;
};

struct box_top {
  struct box_ftyp ftyp;
  struct box_moov moov;
};

static const char *
err_to_str(int i) {
  static const char * errors[] = {
    "",
    "Memory error",
    "IO error",
    "Unknown box",
    "Box size is too big",
    "Unknown handler type",
    "Unknown nal unit type",
    "Unable to read exp golomb code",
    "Unable to read a bit",
    "Unable to read bits",
    "Unknown pic order count type",
    "Unknown overscan_info_present_flag",
    "Unknown video_signal_type_present_flag",
    "Unknown chroma_loc_info_present_flag",
    "Unknwon aspect_ratio_idc",
    "Unknown nal_hrd_para_present_flag",
    "Unknown vcl_hrd_para_present_flag",
    "Unknown num_slice_groups_minus1",
    "Unknown profile_idc"
  };
  if (i < 0 || i >= ERR_LEN)
    return NULL;
  return errors[i];
}

static void
indent(int i) {
  static const char blank[] = "          ""          ""          ""          ";
  static int lv = 0;

  if (i == 0)
    printf("%s", blank + strlen(blank) - lv * 2);
  lv += i;
}

static void
attr(const char * name) {
  indent(0);
  printf("%-26s ", name);
}

static int
mem_alloc(void * ret, size_t size) {
  void ** ret_p;
  void * ptr;

  ptr = malloc(size);
  if (ptr == NULL)
    return ERR_MEM;

  ret_p = ret;
  * ret_p = ptr;
  return 0;
}

static int
mem_realloc(void * ret, size_t size) {
  void ** ret_p;
  void * p;

  ret_p = ret;
  p = realloc(* ret_p, size);
  if (p == NULL)
    return ERR_MEM;

  * ret_p = p;
  return 0;
}

static void
mem_free(void * ptr) {
  free(ptr);
}

static int
skip(FILE * file, long offset) {
  if (fseek(file, offset, SEEK_CUR))
    return ERR_IO;
  return 0;
}

static int
get_pos(long * ret, FILE * file) {
  long pos;

  pos = ftell(file);
  if (pos == -1)
    return ERR_IO;

  * ret = pos;
  return 0;
}

static int
read_ary(void * ptr, size_t size, size_t len, FILE * file) {
  if (fread(ptr, size, len, file) != len)
    return ERR_IO;
  return 0;
}

static int
read_c32(char * ret, FILE * file) {
  if (fread(ret, 4, 1, file) != 1)
    return ERR_IO;
  return 0;
}

static int
read_s16(short * ret, FILE * file) {
  unsigned char b16[2];

  if (fread(b16, sizeof(b16), 1, file) != 1)
    return ERR_IO;

  * ret = (short) ((b16[0] << 8) | b16[1]);
  return 0;
}

static int
read_s32(int * ret, FILE * file) {
  unsigned char b32[4];

  if (fread(b32, sizeof(b32), 1, file) != 1)
    return ERR_IO;

  * ret = (int) ((b32[0] << 24) | (b32[1] << 16) |
                 (b32[2] << 8)  | b32[3]);
  return 0;
}

static int
read_u8(unsigned char * ret, FILE * file) {
  int b8;

  b8 = fgetc(file);
  if (b8 == EOF)
    return ERR_IO;

  * ret = (unsigned char) (b8 & 0xff);
  return 0;
}

static int
read_u16(unsigned short * ret, FILE * file) {
  unsigned char b16[2];

  if (fread(b16, sizeof(b16), 1, file) != 1)
    return ERR_IO;

  * ret = (unsigned short) ((b16[0] << 8) | b16[1]);
  return 0;
}

static int
read_u32(unsigned int * ret, FILE * file) {
  unsigned char b32[4];

  if (fread(b32, sizeof(b32), 1, file) != 1)
    return ERR_IO;

  * ret = (unsigned int) ((b32[0] << 24) | (b32[1] << 16) |
                          (b32[2] << 8)  | b32[3]);
  return 0;
}

static int
read_str(char ** ret, FILE * file) {
  long pos;
  size_t len;
  char * str;
  int err;

  err = 0;

  pos = ftell(file);
  if (pos == -1) {
    err = ERR_IO;
    goto exit;
  }

  len = 0;

  for (;;) {
    int c;

    c = fgetc(file);
    if (c == EOF) {
      err = ERR_IO;
      goto exit;
    }
    if (c == '\0')
      break;

    len++;
  }

  str = malloc(len + 1);
  if (str == NULL) {
    err = ERR_MEM;
    goto exit;
  }

  if (fseek(file, pos, SEEK_SET) == -1) {
    err = ERR_IO;
    goto free;
  }

  if (fread(str, len + 1, 1, file) != 1) {
    err = ERR_IO;
    goto free;
  }

  * ret = str;
free:
  if (err)
    mem_free(str);
exit:
  return err;
}

static int
read_ver(unsigned char * version, unsigned int * flags, FILE * file) {
  unsigned int b32;
  int ret;

  if ((ret = read_u32(&b32, file)) != 0)
    return ret;

  * version = (b32 >> 8) & 0xff;
  * flags = b32 & 0x00ffffff;

#if 0
  attr("version:"); printf("%u\n", * version);
  attr("flags:"); printf("%u\n", * flags);
#endif

  return 0;
}

static int
read_mat(int * matrix, FILE * file) {
  int i;
  int ret;

  for (i = 0; i < 9; i++)
    if ((ret = read_s32(&matrix[i], file)) != 0)
      return ret;

  return 0;
}

struct box_func {
  const char * name;
  int (* func)(FILE * file, unsigned int box_size, box_t p_box);
};


static int
dispatch(FILE * file, struct box_func * funcs, unsigned int p_box_size,
         box_t p_box) {
  long pos;
  long now;
  unsigned int box_size;
  char box_type[4];
  int found;
  int ret;
  int i;

  if ((ret = get_pos(&pos, file)) != 0)
    return ret;

  for (;;) {

    if ((ret = get_pos(&now, file)) != 0)
      return ret;

    /* if this box is not the top level box */
    if (p_box_size != (unsigned int) -1) {

      if ((unsigned long) now > (unsigned long) pos + p_box_size)
        return ERR_BOX_SIZE;

      /* return if no space for child boxes */
      if ((unsigned long) now == (unsigned long) pos + p_box_size)
        break;
    }

    /* box size */
    /* box type */
    if ((ret = read_u32(&box_size, file)) != 0) {

      /* if this box is the top level box */
      if (p_box_size == (unsigned int) -1)
        return 0;

      return ret;
    }
    if ((ret = read_c32(box_type, file)) != 0)
      return ret;

    indent(0); printf("[box size]:  %u\n", box_size);
    indent(0); printf("[box type]:  %.4s\n", box_type);

    found = 0;
    for (i = 0; funcs[i].name != NULL; i++) {
      if (strncmp(funcs[i].name, box_type, 4) == 0) {
        indent(1);
        if ((ret = funcs[i].func(file, box_size - 8, p_box)) != 0)
          return ret;
        indent(-1);
        found = 1;
        break;
      }
    }
    if (!found)
      return ERR_UNK_BOX;
  }
  indent(0); printf("[        ]:  %u\n", p_box_size);
  return 0;
}

static int
read_ftyp(FILE * file, unsigned int box_size, box_t p_box) {
  long pos;
  long now;

  char m_brand[4]; /* major brand */
  unsigned int m_version; /* minor version */
  char (* c_brands)[4]; /* compatible brands */

  struct box_ftyp * ftyp;

  unsigned int i;
  unsigned int len;
  int ret;

  ret = 0;
  c_brands = NULL;

  if ((ret = get_pos(&pos, file)) != 0 ||
      (ret = read_c32(m_brand, file)) != 0 ||
      (ret = read_u32(&m_version, file)) != 0 ||
      (ret = get_pos(&now, file)) != 0)
    goto exit;

  attr("major_brand:"); printf("%.4s\n", m_brand);
  attr("minor_version:"); printf("%u\n", m_version);

  len = (box_size - (unsigned int) (now - pos)) / 4;
  if (len > 0) {

    /* compatible brands */
    if ((ret = mem_alloc(&c_brands, len * sizeof(c_brands[0]))) != 0 ||
        (ret = read_ary(c_brands, sizeof(c_brands[0]), len, file)) != 0)
      goto exit;

    for (i = 0; i < len; i++) {
      attr("compatible brand:"); printf("%.4s\n", c_brands[i]);
    }
  }
  ftyp = &p_box.top->ftyp;
  memcpy(ftyp->m_brand, m_brand, sizeof(m_brand));
  ftyp->m_version = m_version;
  ftyp->c_brands = c_brands;
exit:
  return ret;
}

static int
read_mvhd(FILE * file, unsigned int box_size, box_t p_box) {
  unsigned char version;
  unsigned int flags;
  unsigned int c_time;
  unsigned int m_time;
  unsigned int timescale;
  unsigned int duration;
  int rate;
  short volume;
  int matrix[9];
  unsigned int next_track_id;
  unsigned int i;
  struct box_mvhd * mvhd;
  int ret;

  (void) box_size;

  if ((ret = read_ver(&version, &flags, file)) != 0 ||
      (ret = read_u32(&c_time, file)) != 0 ||
      (ret = read_u32(&m_time, file)) != 0 ||
      (ret = read_u32(&timescale, file)) != 0 ||
      (ret = read_u32(&duration, file)) != 0 ||
      (ret = read_s32(&rate, file)) != 0 ||
      (ret = read_s16(&volume, file)) != 0 ||
      (ret = skip(file, 2 + 4 * 2)) != 0 || /* reserved */
      (ret = read_mat(matrix, file)) != 0 ||
      (ret = skip(file, 4 * 6)) != 0 || /* pre defined */
      (ret = read_u32(&next_track_id, file)) != 0)
    return ret;

  attr("creation time:"); printf("%u\n", c_time);
  attr("modification time:"); printf("%u\n", m_time);
  attr("timescale:"); printf("%u\n", timescale);
  attr("duration:"); printf("%u\n", duration);
  attr("rate:"); printf("%d.%x\n", rate >> 16, rate & 0xffff);
  attr("volume:"); printf("%d.%x\n", volume >> 8, volume & 0xff);

  for (i = 0; i < 3; i++) {
    attr(i == 0 ? "matrix:" : "       ");
    printf("%d.%x %d.%x %d.%x\n",
           matrix[i] >> 16, matrix[i] & 0xffff,
           matrix[i+3] >> 16, matrix[i+3] & 0xffff,
           matrix[i+6] >> 30, matrix[i+6] & 0x3fffffff);
  }

  attr("next_track_id:"); printf("%u\n", next_track_id);

  mvhd = &p_box.moov->mvhd;
  mvhd->c_time = c_time;
  mvhd->m_time = m_time;
  mvhd->timescale = timescale;
  mvhd->duration = duration;
  mvhd->rate = rate;
  mvhd->volume = volume;
  for (i = 0; i < 9; i++)
    mvhd->matrix[i] = matrix[i];
  mvhd->next_track_id = next_track_id;

  return 0;
}

static int
read_tkhd(FILE * file, unsigned int box_size, box_t p_box) {
  unsigned char version;
  unsigned int flags;
  unsigned char track_enabled;
  unsigned char track_in_movie;
  unsigned char track_in_preview;
  unsigned int c_time;
  unsigned int m_time;
  unsigned int track_id;
  unsigned int duration;
  short layer;
  short alternate_group;
  short volume;
  int matrix[9];
  unsigned int width;
  unsigned int height;
  unsigned int i;
  struct box_tkhd * tkhd;
  int ret;

  (void) box_size;

  if ((ret = read_ver(&version, &flags, file)) != 0 ||
      (ret = read_u32(&c_time, file)) != 0 ||
      (ret = read_u32(&m_time, file)) != 0 ||
      (ret = read_u32(&track_id, file)) != 0 ||
      (ret = skip(file, 4)) != 0 || /* reserved */
      (ret = read_u32(&duration, file)) != 0 ||
      (ret = skip(file, 4 * 2)) != 0 || /* reserved */
      (ret = read_s16(&layer, file)) != 0 ||
      (ret = read_s16(&alternate_group, file)) != 0 ||
      (ret = read_s16(&volume, file)) != 0 ||
      (ret = skip(file, 2)) != 0 || /* reserved */
      (ret = read_mat(matrix, file)) != 0 ||
      (ret = read_u32(&width, file)) != 0 ||
      (ret = read_u32(&height, file)) != 0)
    return ret;

  track_enabled = flags & 0x000001;
  track_in_movie = (flags & 0x000002) != 0;
  track_in_preview= (flags & 0x000004) != 0;

  attr("track_enabled:"); printf("%u\n", track_enabled);
  attr("track_in_movie:"); printf("%u\n", track_in_movie);
  attr("track_in_preview:"); printf("%u\n", track_in_preview);
  attr("creation time:"); printf("%u\n", c_time);
  attr("modification time:"); printf("%u\n", m_time);
  attr("track_id:"); printf("%u\n", track_id);
  attr("duration:"); printf("%u\n", duration);
  attr("layer:"); printf("%d\n", layer);
  attr("alternate_group:"); printf("%d\n", alternate_group);
  attr("volume:"); printf("%d.%x\n", volume >> 8, volume & 0xff);

  for (i = 0; i < 3; i++) {
    attr(i == 0 ? "matrix:" : "       ");
    printf("%d.%x %d.%x %d.%x\n",
           matrix[i] >> 16, matrix[i] & 0xffff,
           matrix[i+3] >> 16, matrix[i+3] & 0xffff,
           matrix[i+6] >> 30, matrix[i+6] & 0x3fffffff);
  }

  attr("width:"); printf("%u.%x\n", width >> 16, width & 0xffff);
  attr("height:"); printf("%u.%x\n", height >> 16, height & 0xffff);

  tkhd = &p_box.trak->tkhd;
  tkhd->track_enabled = track_enabled;
  tkhd->track_in_movie = track_in_movie;
  tkhd->track_in_preview = track_in_preview;
  tkhd->c_time = c_time;
  tkhd->m_time = m_time;
  tkhd->track_id = track_id;
  tkhd->duration = duration;
  tkhd->layer = layer;
  tkhd->alternate_group = alternate_group;
  tkhd->volume = volume;
  for (i = 0; i < 9; i++)
    tkhd->matrix[i] = matrix[i];
  tkhd->width = width;
  tkhd->height = height;

  return 0;
}

static int
read_mdhd(FILE * file, unsigned int box_size, box_t p_box) {
  unsigned char version;
  unsigned int flags;
  unsigned int c_time;
  unsigned int m_time;
  unsigned int timescale;
  unsigned int duration;
  unsigned short lang_pack;
  char lang[3];
  struct box_mdhd * mdhd;
  int ret;

  (void) box_size;

  if ((ret = read_ver(&version, &flags, file)) != 0 ||
      (ret = read_u32(&c_time, file)) != 0 ||
      (ret = read_u32(&m_time, file)) != 0 ||
      (ret = read_u32(&timescale, file)) != 0 ||
      (ret = read_u32(&duration, file)) != 0 ||
      (ret = read_u16(&lang_pack, file)) != 0 ||
      (ret = skip(file, 2)) != 0) /* pre defined */
    return ret;

  lang[0] = (char) ((lang_pack >> 10) + 0x60);
  lang[1] = (char) (((lang_pack >> 5) & 0x1f) + 0x60);
  lang[2] = (char) ((lang_pack & 0x1f) + 0x60);

  attr("creation time:"); printf("%u\n", c_time);
  attr("modification time:"); printf("%u\n", m_time);
  attr("timescale:"); printf("%u\n", timescale);
  attr("duration:"); printf("%u\n", duration);
  attr("lang:"); printf("%.3s\n", lang);

  mdhd = &p_box.mdia->mdhd;
  mdhd->c_time = c_time;
  mdhd->m_time = m_time;
  mdhd->timescale = timescale;
  mdhd->duration = duration;
  memcpy(mdhd->lang, lang, sizeof(lang));

  return 0;
}

static int
read_hdlr(FILE * file, unsigned int box_size, box_t p_box) {
  unsigned char version;
  unsigned int flags;
  char type[4]; /* handler type */
  char * name;
  struct box_hdlr * hdlr;
  int ret;

  (void) box_size;

  if ((ret = read_ver(&version, &flags, file)) != 0 ||
      (ret = skip(file, 4)) != 0 || /* pre defined */
      (ret = read_c32(type, file)) != 0 ||
      (ret = skip(file, 4 * 3)) != 0 || /* reserved */
      (ret = read_str(&name, file)) != 0)
    return ret;

  attr("handler_type:"); printf("%.4s\n", type);
  attr("name:"); printf("%s\n", name);

  hdlr = &p_box.mdia->hdlr;
  memcpy(hdlr->type, type, sizeof(type));
  hdlr->name = name;

  return 0;
}

static int
read_data_entry(FILE * file, struct box_data_entry * entry) {
  unsigned int box_size;
  char box_type[4];
  unsigned char version;
  unsigned int flags;
  unsigned char self_contained;
  char * name;
  char * location;
  int ret;

  ret = 0;

  if ((ret = read_u32(&box_size, file)) != 0 ||
      (ret = read_c32(box_type, file)) != 0)
    goto exit;

  indent(0); printf("[box size]:  %u\n", box_size);
  indent(0); printf("[box type]:  %.4s\n", box_type);

  indent(1);

  if (strncmp(box_type, "url ", 4) != 0 &&
      strncmp(box_type, "urn ", 4) != 0) {
    ret = ERR_UNK_BOX;
    goto exit;
  }

  if ((ret = read_ver(&version, &flags, file)) != 0)
    goto exit;
  self_contained = flags & 0x1;

  attr("self_contained:"); printf("%u\n", self_contained);

  name = NULL;
  location = NULL;

  if (strncmp(box_type, "url ", 4) == 0) {

    if (self_contained == 0) {

      if ((ret = read_str(&location, file)) != 0)
        goto exit;

      attr("location:"); printf("%s\n", location);
    }
  } else { /* urn */

    if ((ret = read_str(&name, file)) != 0)
      goto exit;
    if ((ret = read_str(&location, file)) != 0)
      goto free;

    attr("name:"); printf("%s\n", name);
    attr("location:"); printf("%s\n", location);
free:
    if (ret) {
      mem_free(name);
      goto exit;
    }
  }
  memcpy(entry->type, box_type, sizeof(entry->type));
  entry->self_contained = self_contained;
  entry->name = name;
  entry->location = location;
exit:
  indent(-1);
  return ret;
}

static void
free_data_entry(struct box_data_entry * entry) {
  mem_free(entry->name);
  mem_free(entry->location);
}

static int
read_dref(FILE * file, unsigned int box_size, box_t p_box) {
  unsigned char version;
  unsigned int flags;
  unsigned int entry_count;
  unsigned int i;
  unsigned int j;
  struct box_data_entry * entry;
  struct box_dref * dref;
  int ret;

  (void) box_size;

  ret = 0;

  if ((ret = read_ver(&version, &flags, file)) != 0 ||
      (ret = read_u32(&entry_count, file)) != 0)
    goto exit;

  attr("entry_count:"); printf("%u\n", entry_count);

  entry = NULL;

  if (entry_count) {

    if ((ret = mem_alloc(&entry, entry_count * sizeof(* entry))) != 0)
      goto exit;

    for (i = 0; i < entry_count; i++) {
      if ((ret = read_data_entry(file, &entry[i])) != 0)
        goto free;
    }
free:
    if (ret) {
      for (j = 0; j < i; j++)
        free_data_entry(&entry[j]);
      goto exit;
    }
  }

  dref = &p_box.dinf->dref;
  dref->entry_count = entry_count;
  dref->entry = entry;

exit:
  if (ret)
    mem_free(entry);
  return ret;
}

static int
read_dinf(FILE * file, unsigned int box_size, box_t p_box) {
  static struct box_func funcs[] = {
    {"dref", read_dref},
    {NULL, NULL}
  };
  box_t box;
  box.dinf = &p_box.mdia->minf.dinf;
  return dispatch(file, funcs, box_size, box);
}

static int
read_visual_sample_entry(FILE * file, struct box_visual_sample_entry * entry) {
  unsigned int box_size;
  char box_type[4];
  unsigned short dref_index; /* data reference index */
  unsigned short width;
  unsigned short height;
  unsigned int h_rez;
  unsigned int v_rez;
  unsigned short frame_count;
  unsigned char len;
  char compressorname[32];
  unsigned short depth;
  int ret;

  ret = 0;

  if ((ret = read_u32(&box_size, file)) != 0 ||
      (ret = read_c32(box_type, file)) != 0 ||
      (ret = skip(file, 6)) != 0 || /* reserved */
      (ret = read_u16(&dref_index, file)) != 0 ||
      (ret = skip(file, 2 + 2 + 4 * 3)) != 0 || /* pre / reserved / pre */
      (ret = read_u16(&width, file)) != 0 ||
      (ret = read_u16(&height, file)) != 0 ||
      (ret = read_u32(&h_rez, file)) != 0 ||
      (ret = read_u32(&v_rez, file)) != 0 ||
      (ret = skip(file, 4)) != 0 || /* reserved */
      (ret = read_u16(&frame_count, file)) != 0 ||
      (ret = read_u8(&len, file)) != 0 ||
      (ret = read_ary(compressorname,
                      sizeof(compressorname) - 1, 1, file)) != 0 ||
      (ret = read_u16(&depth, file)) != 0 ||
      (ret = skip(file, 2)) != 0) /* pre defined */
    goto exit;

  compressorname[len] = '\0';

  indent(0); printf("[box size]:  %u\n", box_size);
  indent(0); printf("[box type]:  %.4s\n", box_type);

  indent(1);

  attr("dref_index:"); printf("%u\n", dref_index);
  attr("width:"); printf("%u\n", width);
  attr("height:"); printf("%u\n", height);
  attr("h_rez:"); printf("%u.%x\n", h_rez >> 16, h_rez & 0xffff);
  attr("v_rez:"); printf("%u.%x\n", v_rez >> 16, v_rez & 0xffff);
  attr("frame_count:"); printf("%u\n", frame_count);
  attr("compressorname:"); printf("%s\n", compressorname);
  attr("depth:"); printf("%u\n", depth);

  entry->dref_index = dref_index;
  entry->width = width;
  entry->height = height;
  entry->h_rez = h_rez;
  entry->v_rez = v_rez;
  entry->frame_count = frame_count;
  memcpy(entry->compressorname, compressorname, sizeof(compressorname));
  entry->depth = depth;
exit:
  indent(-1);
  return ret;
}

static void
free_visual_sample_entry(struct box_visual_sample_entry * entry) {
  (void) entry;
}

int
read_code(unsigned int * ret,
          unsigned int * ret_bi,
          unsigned int * ret_pos,
          unsigned int * ret_buf,
          unsigned int size, unsigned char * bits) {
  unsigned int bi; /* byte index */
  unsigned int pos; /* bit position */

  unsigned int z; /* number of zeros */
  unsigned int u; /* unsigned byte = byte buf */
  signed int s; /* signed byte */

  unsigned int l; /* last n bits */

  bi = * ret_bi;
  pos = * ret_pos;
  u = * ret_buf;

  /* read zeros */
  z = 0;
  u &= (unsigned int) 0xff >> pos;
  if (u == 0) { /* the first leading '1' (bit) is not in this byte */
    z += 8 - pos;
    pos = 0;
    for (;;) { /* read next byte to find the first leading '1' */
      if (bi >= size)
        return ERR_READ_CODE; /* out of bound */

      u = bits[bi++];
      if (u)
        break; /* the first leading '1' is found in this byte */
      z += 8;
    }
  }
  s = (int) u << pos;
  while ((signed char) s > 0) { /* read & count zeros (bit) */
    s <<= 1;
    z++;
    pos++;
  }

  if (z > 32)
    return ERR_READ_CODE; /* exceed the maximum of unsigned int */

  /* first non-zero bit */
  pos++;

  if (bi + ((pos + z - 1) >> 3) > size)
    return ERR_READ_CODE; /* out of bound */

  /* last n bit */
  if (z > 8 - pos) {
    l = u & ((unsigned int) 0xff >> pos);
    pos = z - (8 - pos);
    for (; pos > 8; pos -= 8) {
      u = bits[bi++];
      l <<= 8;
      l |= u;
    }
    if (pos) {
      u = bits[bi++];
      l <<= pos;
      l |= u >> (8 - pos);
    }
  } else {
    l = ((u << pos) & 0xff) >> (8 - z);
    pos += z;
  }

  if (z == 32 && l)
    return ERR_READ_CODE; /* exceed the maximum of unsigned int */

  if (z == 32)
    l = 0xffffffff;
  else
    l += ~(0xffffffff << z);

  * ret_buf = u;
  * ret_pos = pos;
  * ret_bi = bi;
  * ret = l;

  return 0;
}

static int
read_bit(unsigned char * ret,
         unsigned int * ret_bi,
         unsigned int * ret_pos,
         unsigned int * ret_buf,
         unsigned int size, unsigned char * bits) {
  unsigned int bi;
  unsigned int pos;
  unsigned int buf;
  unsigned char bit;

  bi = * ret_bi;
  pos = * ret_pos;
  buf = * ret_buf;

  if (pos == 8) {
    if (bi >= size)
      return ERR_READ_BIT;

    buf = bits[bi++];
    pos = 0;
  }

  bit = (buf & ((unsigned int) 0x80 >> pos)) != 0;

  * ret_bi = bi;
  * ret_pos = pos + 1;
  * ret_buf = buf;
  * ret = bit;

  return 0;
}

static int
read_bits(unsigned int * ret,
          unsigned char w, /* width */
          unsigned int * ret_bi,
          unsigned int * ret_pos,
          unsigned int * ret_buf,
          unsigned int size, unsigned char * bits) {
  unsigned int bi;
  unsigned int pos;
  unsigned int u; /* unsigned byte */
  unsigned int r; /* result */

  bi = * ret_bi;
  pos = * ret_pos;
  u = * ret_buf;

  /* width valid range: 1~32 */

  if (bi + ((pos + w - 1) >> 3) > size)
    return ERR_READ_BITS; /* out of bound */

  if (w > 8 - pos) {
    r = u & ((unsigned int) 0xff >> pos);
    pos = w - (8 - pos);
    for (; pos > 8; pos -= 8) {
      u = bits[bi++];
      r <<= 8;
      r |= u;
    }
    if (pos) {
      u = bits[bi++];
      r <<= pos;
      r |= u >> (8 - pos);
    }
  } else {
    r = ((u << pos) & 0xff) >> (8 - w);
    pos += w;
  }

  * ret_bi = bi;
  * ret_pos = pos;
  * ret_buf = u;
  * ret = r;

  return 0;
}

/* no need to implement now */
#if 0
static int
read_hrd_para(unsigned int * bi,
              unsigned int * pos,
              unsigned int * buf,
              unsigned int size, unsigned char * bits) {
  unsigned int cpb_cnt_minus1;
#if 0
  unsigned char bit_rate_scale;
  unsigned char cpb_rate_scale;
#endif
  int ret;

  ret = 0;

  if ((ret = read_code(&cpb_cnt_minus1,
                       bi, pos, buf, size, bits)) != 0)
    goto exit;

  attr("cpb_cnt_minus1:"); printf("%u\n", cpb_cnt_minus1);

exit:
  return ret;
}
#endif

static int
read_vui_para(unsigned int * bi,
              unsigned int * pos,
              unsigned int * buf,
              unsigned int size, unsigned char * bits) {
  unsigned char aspect_ratio_info_present_flag;
  unsigned char aspect_ratio_idc;

  unsigned char overscan_info_present_flag;
  unsigned char video_signal_type_present_flag;
  unsigned char chroma_loc_info_present_flag;

  unsigned char timing_info_present_flag;
  unsigned int num_units_in_tick;
  unsigned int time_scale;
  unsigned char fixed_frame_rate_flag;

  unsigned char nal_hrd_para_present_flag;
  unsigned char vcl_hrd_para_present_flag;
  unsigned char pic_struct_present_flag;

  unsigned char bitstream_restriction_flag;
  unsigned char motion_vectors_over_pic_boundaries_flag;
  unsigned int max_bytes_per_pic_denom;
  unsigned int max_bits_per_mb_denom;
  unsigned int log2_max_mv_length_horizontal;
  unsigned int log2_max_mv_length_vertical;
  unsigned int num_reorder_frames;
  unsigned int max_dec_frame_buffering;

  unsigned int n;
  int ret;

  ret = 0;

  if ((ret = read_bit(&aspect_ratio_info_present_flag,
                      bi, pos, buf, size, bits)) != 0)
    goto exit;

  attr("aspect_ratio_info_present_flag:");
  printf("%u\n", aspect_ratio_info_present_flag);

  if (aspect_ratio_info_present_flag) {
    if ((ret = read_bits(&n, 8, bi, pos, buf, size, bits)) != 0)
      goto exit;

    aspect_ratio_idc = n & 0xff;

    indent(1);
    attr("aspect_ratio_idc:"); printf("%u\n", aspect_ratio_idc);
    indent(-1);

    if (aspect_ratio_idc == 0xff) {
      ret = ERR_UNK_ASPECT_RATIO_IDC;
      goto exit;
    }
  }

  if ((ret = read_bit(&overscan_info_present_flag,
                      bi, pos, buf, size, bits)) != 0)
    goto exit;

  attr("overscan_info_present_flag:");
  printf("%u\n", overscan_info_present_flag);

  if (overscan_info_present_flag) {
    ret = ERR_UNK_OVERSCAN_INFO_PRESENT_FLAG;
    goto exit;
  }

  if ((ret = read_bit(&video_signal_type_present_flag,
                      bi, pos, buf, size, bits)) != 0)
    goto exit;

  attr("video_signal_type_present_flag:");
  printf("%u\n", video_signal_type_present_flag);

  if (video_signal_type_present_flag) {
    ret = ERR_UNK_VIDEO_SIGNAL_TYPE_PRESENT_FLAG;
    goto exit;
  }

  if ((ret = read_bit(&chroma_loc_info_present_flag,
                      bi, pos, buf, size, bits)) != 0)
    goto exit;

  attr("chroma_loc_info_present_flag:");
  printf("%u\n", chroma_loc_info_present_flag);

  if (chroma_loc_info_present_flag) {
    ret = ERR_UNK_CHROMA_LOC_INFO_PRESENT_FLAG;
    goto exit;
  }

  if ((ret = read_bit(&timing_info_present_flag,
                      bi, pos, buf, size, bits)) != 0)
    goto exit;

  attr("timing_info_present_flag:");
  printf("%u\n", timing_info_present_flag);

  if (timing_info_present_flag) {
    if ((ret = read_bits(&num_units_in_tick, 32,
                         bi, pos, buf, size, bits)) != 0 ||
        (ret = read_bits(&time_scale, 32,
                         bi, pos, buf, size, bits)) != 0 ||
        (ret = read_bit(&fixed_frame_rate_flag,
                        bi, pos, buf, size, bits)) != 0)
      goto exit;

    indent(1);
    attr("num_units_in_tick:"); printf("%u\n", num_units_in_tick);
    attr("time_scale:"); printf("%u\n", time_scale);
    attr("fixed_frame_rate_flag:"); printf("%u\n", fixed_frame_rate_flag);
    indent(-1);
  }

  if ((ret = read_bit(&nal_hrd_para_present_flag,
                      bi, pos, buf, size, bits)) != 0)
    goto exit;

  attr("nal_hrd_para_present_flag:");
  printf("%u\n", nal_hrd_para_present_flag);

  if (nal_hrd_para_present_flag) {
    ret = ERR_UNK_NAL_HRD_PARA_PRESENT_FLAG;
    goto exit;
  }

  if ((ret = read_bit(&vcl_hrd_para_present_flag,
                      bi, pos, buf, size, bits)) != 0)
    goto exit;

  attr("vcl_hrd_para_present_flag:");
  printf("%u\n", vcl_hrd_para_present_flag);

  if (vcl_hrd_para_present_flag) {
    ret = ERR_UNK_VCL_HRD_PARA_PRESENT_FLAG;
    goto exit;
  }
  if (nal_hrd_para_present_flag ||
      vcl_hrd_para_present_flag) {
    ret = ERR_UNK_VCL_HRD_PARA_PRESENT_FLAG;
    goto exit;
  }

  if ((ret = read_bit(&pic_struct_present_flag,
                      bi, pos, buf, size, bits)) != 0)
    goto exit;

  attr("pic_struct_present_flag:");
  printf("%u\n", pic_struct_present_flag);

  if ((ret = read_bit(&bitstream_restriction_flag,
                      bi, pos, buf, size, bits)) != 0)
    goto exit;

  attr("bitstream_restriction_flag:");
  printf("%u\n", bitstream_restriction_flag);

  if (bitstream_restriction_flag) {
    if ((ret = read_bit(&motion_vectors_over_pic_boundaries_flag,
                        bi, pos, buf, size, bits)) != 0 ||
        (ret = read_code(&max_bytes_per_pic_denom,
                         bi, pos, buf, size, bits)) != 0 ||
        (ret = read_code(&max_bits_per_mb_denom,
                         bi, pos, buf, size, bits)) != 0 ||
        (ret = read_code(&log2_max_mv_length_horizontal,
                         bi, pos, buf, size, bits)) != 0 ||
        (ret = read_code(&log2_max_mv_length_vertical,
                         bi, pos, buf, size, bits)) != 0 ||
        (ret = read_code(&num_reorder_frames,
                         bi, pos, buf, size, bits)) != 0 ||
        (ret = read_code(&max_dec_frame_buffering,
                         bi, pos, buf, size, bits)) != 0)
      goto exit;

    indent(1);
    attr("motion_vectors_over_pic_boundaries_flag:");
    printf("%u\n", motion_vectors_over_pic_boundaries_flag);
    attr("max_bytes_per_pic_denom:");
    printf("%u\n", max_bytes_per_pic_denom);
    attr("max_bits_per_mb_denom:");
    printf("%u\n", max_bits_per_mb_denom);
    attr("log2_max_mv_length_horizontal:");
    printf("%u\n", log2_max_mv_length_horizontal);
    attr("log2_max_mv_length_vertical:");
    printf("%u\n", log2_max_mv_length_vertical);
    attr("num_reorder_frames:");
    printf("%u\n", num_reorder_frames);
    attr("max_dec_frame_buffering:");
    printf("%u\n", max_dec_frame_buffering);
    indent(-1);
  }
exit:
  return ret;
}

static int
read_nalu(unsigned int size /* size of NALU */, FILE * file) {
  unsigned char ref_idc;
  unsigned char type;
  unsigned int rbsp_size;
  unsigned char * nalu;
  unsigned char * rbsp;
  unsigned int bi;
  unsigned int pos;
  unsigned int buf;
  unsigned int n;
  unsigned int i;
  int ret;

  ret = 0;

  if ((ret = mem_alloc(&nalu, size)) != 0)
    goto exit;

  if ((ret = read_ary(nalu, size, 1, file)) != 0)
    goto free;

  ref_idc = (unsigned char) ((nalu[0] >> 5) & 0x3);
  type = (unsigned char) (nalu[0] & 0x1f);

  attr("nal_ref_idc:"); printf("%u\n", ref_idc);
  attr("nal_unit_type:"); printf("%u\n", type);

  rbsp = nalu;
  rbsp_size = 0;
  for (i = 1; i < size;) {
    if (i + 2 < size && !nalu[i] && !nalu[i+1] && nalu[i+2] == 0x03) {
      rbsp[rbsp_size++] = nalu[i++];
      rbsp[rbsp_size++] = nalu[i++];
      i++; /* emulation_prevention_tree_byte == 0x03 */
    } else {
      rbsp[rbsp_size++] = nalu[i++];
    }
  }

  if (type == 0x7) {
    unsigned char profile_idc; /* AVC profile indication */
    unsigned char profile_comp; /* profile compatibility */
    unsigned char c_set0_flag; /* constraint set 0 flag */
    unsigned char c_set1_flag; /* constraint set 1 flag */
    unsigned char c_set2_flag; /* constraint set 2 flag */
    unsigned char level_idc; /* AVC level indication */
    unsigned char seq_para_set_id; /* seq parameter set id */
    unsigned char log2_max_frame_num_minus4; /* MaxFrameNum used in frame_num */
    unsigned char pic_order_cnt_type; /* to decode picture order count */
    unsigned int num_ref_frames;
    unsigned char gaps_in_frame_num_value_allowed_flag;
    unsigned int pic_width_in_mbs_minus_1;
    unsigned int pic_height_in_mbs_minus_1;
    unsigned char frame_mbs_only_flag;
    unsigned char mb_adaptive_frame_field_flag;
    unsigned char direct_8x8_inference_flag;
    unsigned char frame_cropping_flag;
    unsigned int frame_crop_left_offset;
    unsigned int frame_crop_right_offset;
    unsigned int frame_crop_top_offset;
    unsigned int frame_crop_bottom_offset;
    unsigned char vui_para_present_flag;

    bi = 0;

    if (bi + 3 > rbsp_size)
      goto free;

    profile_idc = rbsp[bi++];
    profile_comp = rbsp[bi++];
    level_idc = rbsp[bi++];

    c_set0_flag = (unsigned char) ((profile_comp >> 7) & 0x1);
    c_set1_flag = (unsigned char) ((profile_comp >> 6) & 0x1);
    c_set2_flag = (unsigned char) ((profile_comp >> 5) & 0x1);

    attr("profile_idc:"); printf("%u\n", profile_idc);
    attr("constraint_set0_flag:"); printf("%u\n", c_set0_flag);
    attr("constraint_set1_flag:"); printf("%u\n", c_set1_flag);
    attr("constraint_set2_flag:"); printf("%u\n", c_set2_flag);
    attr("level_idc:"); printf("%u\n", level_idc);

    if (bi + 1 > rbsp_size)
      goto free;

    pos = 0;
    buf = rbsp[bi++];
    if ((ret = read_code(&n, &bi, &pos, &buf, rbsp_size, rbsp)) != 0)
      goto free;

    seq_para_set_id = n & 0x1f;

    if ((ret = read_code(&n, &bi, &pos, &buf, rbsp_size, rbsp)) != 0)
      goto free;

    log2_max_frame_num_minus4 = n & 0xf;

    if ((ret = read_code(&n, &bi, &pos, &buf, rbsp_size, rbsp)) != 0)
      goto free;

    pic_order_cnt_type = n & 0x3;

    attr("seq_para_set_id:"); printf("%u\n", seq_para_set_id);
    attr("log2_max_frame_num_minus4:");
    printf("%u\n", log2_max_frame_num_minus4);
    attr("pic_order_cnt_type:"); printf("%u\n", pic_order_cnt_type);

    if (pic_order_cnt_type == 2) {
    } else {
      ret = ERR_UNK_PIC_ORDER_CNT_TYPE;
      goto free;
    }

    if ((ret = read_code(&num_ref_frames,
                         &bi, &pos, &buf, rbsp_size, rbsp)) != 0 ||
        (ret = read_bit(&gaps_in_frame_num_value_allowed_flag,
                        &bi, &pos, &buf, rbsp_size, rbsp)) != 0 ||
        (ret = read_code(&pic_width_in_mbs_minus_1,
                         &bi, &pos, &buf, rbsp_size, rbsp)) != 0 ||
        (ret = read_code(&pic_height_in_mbs_minus_1,
                         &bi, &pos, &buf, rbsp_size, rbsp)) != 0 ||
        (ret = read_bit(&frame_mbs_only_flag,
                        &bi, &pos, &buf, rbsp_size, rbsp)) != 0)
      goto free;

    attr("num_ref_frames:"); printf("%u\n", num_ref_frames);
    attr("gaps_in_frame_num_value_allowed_flag:");
    printf("%u\n", gaps_in_frame_num_value_allowed_flag);
    attr("pic_width_in_mbs_minus_1:");
    printf("%u\n", pic_width_in_mbs_minus_1);
    attr("pic_height_in_mbs_minus_1:");
    printf("%u\n", pic_height_in_mbs_minus_1);
    attr("frame_mbs_only_flag:"); printf("%u\n", frame_mbs_only_flag);

    if (!frame_mbs_only_flag) {
      if ((ret = read_bit(&mb_adaptive_frame_field_flag,
                          &bi, &pos, &buf, rbsp_size, rbsp)) != 0)
        goto free;

      attr("mb_adaptive_frame_field_flag:");
      printf("%u\n", mb_adaptive_frame_field_flag);
    }

    if ((ret = read_bit(&direct_8x8_inference_flag,
                        &bi, &pos, &buf, rbsp_size, rbsp)) != 0 ||
        (ret = read_bit(&frame_cropping_flag,
                        &bi, &pos, &buf, rbsp_size, rbsp)) != 0)
      goto free;

    attr("direct_8x8_inference_flag:");
    printf("%u\n", direct_8x8_inference_flag);
    attr("frame_cropping_flag:");
    printf("%u\n", frame_cropping_flag);

    if (frame_cropping_flag) {
      if ((ret = read_code(&frame_crop_left_offset,
                           &bi, &pos, &buf, rbsp_size, rbsp)) != 0 ||
          (ret = read_code(&frame_crop_right_offset,
                           &bi, &pos, &buf, rbsp_size, rbsp)) != 0 ||
          (ret = read_code(&frame_crop_top_offset,
                           &bi, &pos, &buf, rbsp_size, rbsp)) != 0 ||
          (ret = read_code(&frame_crop_bottom_offset,
                           &bi, &pos, &buf, rbsp_size, rbsp)) != 0)
        goto free;

      attr("frame_crop_left_offset:");
      printf("%u\n", frame_crop_left_offset);
      attr("frame_crop_right_offset:");
      printf("%u\n", frame_crop_right_offset);
      attr("frame_crop_top_offset:");
      printf("%u\n", frame_crop_top_offset);
      attr("frame_crop_bottom_offset:");
      printf("%u\n", frame_crop_bottom_offset);
    }

    if ((ret = read_bit(&vui_para_present_flag,
                        &bi, &pos, &buf, rbsp_size, rbsp)) != 0)
      goto free;
    attr("vui_para_present_flag:"); printf("%u\n", vui_para_present_flag);

    if (vui_para_present_flag) {
      indent(1);
      if ((ret = read_vui_para(&bi, &pos, &buf, rbsp_size, rbsp)) != 0)
        goto free;
      indent(-1);
    }
  } else if (type == 0x8) {
    unsigned int pic_para_set_id;
    unsigned int seq_para_set_id;
    unsigned char entropy_coding_mode_flag;
    unsigned char pic_order_present_flag;
    unsigned int num_slice_groups_minus1;
    unsigned int num_ref_idx_10_active_minus1;
    unsigned int num_ref_idx_11_active_minus1;
    unsigned char weighted_pred_flag;
    unsigned int weighted_bipred_idc;
    unsigned int pic_init_qp_minus26;
    unsigned int pic_init_qs_minus26;
    unsigned int chroma_qp_index_offset;
    unsigned char deblocking_filter_control_present_flag;
    unsigned char constrained_intra_pred_flag;
    unsigned char redundant_pic_cnt_present_flag;

    if (rbsp_size < 1)
      goto free;

    bi = 0;
    pos = 0;
    buf = rbsp[bi++];

    if ((ret = read_code(&pic_para_set_id,
                         &bi, &pos, &buf, rbsp_size, rbsp)) != 0 ||
        (ret = read_code(&seq_para_set_id,
                         &bi, &pos, &buf, rbsp_size, rbsp)) != 0 ||
        (ret = read_bit(&entropy_coding_mode_flag,
                        &bi, &pos, &buf, rbsp_size, rbsp)) != 0 ||
        (ret = read_bit(&pic_order_present_flag,
                        &bi, &pos, &buf, rbsp_size, rbsp)) != 0 ||
        (ret = read_code(&num_slice_groups_minus1,
                         &bi, &pos, &buf, rbsp_size, rbsp)) != 0)
      goto free;

    attr("pic_para_set_id:");
    printf("%u\n", pic_para_set_id);
    attr("seq_para_set_id:");
    printf("%u\n", seq_para_set_id);
    attr("entropy_coding_mode_flag:");
    printf("%u\n", entropy_coding_mode_flag);
    attr("pic_order_present_flag:");
    printf("%u\n", pic_order_present_flag);
    attr("num_slice_groups_minus1:");
    printf("%u\n", num_slice_groups_minus1);

    if (num_slice_groups_minus1 > 0) {
      ret = ERR_UNK_NUM_SLICE_GROUPS_MINUS1;
      goto free;
    }

    if ((ret = read_code(&num_ref_idx_10_active_minus1,
                         &bi, &pos, &buf, rbsp_size, rbsp)) != 0 ||
        (ret = read_code(&num_ref_idx_11_active_minus1,
                         &bi, &pos, &buf, rbsp_size, rbsp)) != 0 ||
        (ret = read_bit(&weighted_pred_flag,
                        &bi, &pos, &buf, rbsp_size, rbsp)) != 0 ||
        (ret = read_bits(&weighted_bipred_idc, 2,
                         &bi, &pos, &buf, rbsp_size, rbsp)) != 0 ||
        (ret = read_code(&pic_init_qp_minus26,
                         &bi, &pos, &buf, rbsp_size, rbsp)) != 0 ||
        (ret = read_code(&pic_init_qs_minus26,
                         &bi, &pos, &buf, rbsp_size, rbsp)) != 0 ||
        (ret = read_code(&chroma_qp_index_offset,
                         &bi, &pos, &buf, rbsp_size, rbsp)) != 0 ||
        (ret = read_bit(&deblocking_filter_control_present_flag,
                        &bi, &pos, &buf, rbsp_size, rbsp)) != 0 ||
        (ret = read_bit(&constrained_intra_pred_flag,
                        &bi, &pos, &buf, rbsp_size, rbsp)) != 0 ||
        (ret = read_bit(&redundant_pic_cnt_present_flag,
                        &bi, &pos, &buf, rbsp_size, rbsp)) != 0)
      goto free;

    attr("num_ref_idx_10_active_minus1:");
    printf("%u\n", num_ref_idx_10_active_minus1);
    attr("num_ref_idx_11_active_minus1:");
    printf("%u\n", num_ref_idx_11_active_minus1);
    attr("weighted_pred_flag:");
    printf("%u\n", weighted_pred_flag);
    attr("weighted_bipred_idc:");
    printf("%u\n", weighted_bipred_idc);
    attr("pic_init_qp_minus26:");
    printf("%u\n", pic_init_qp_minus26);
    attr("pic_init_qs_minus26:");
    printf("%u\n", pic_init_qs_minus26);
    attr("chroma_qp_index_offset:");
    printf("%u\n", chroma_qp_index_offset);
    attr("deblocking_filter_control_present_flag:");
    printf("%u\n", deblocking_filter_control_present_flag);
    attr("constrained_intra_pred_flag:");
    printf("%u\n", constrained_intra_pred_flag);
    attr("redundant_pic_cnt_present_flag:");
    printf("%u\n", redundant_pic_cnt_present_flag);
  } else {
    ret = ERR_UNK_NAL_UNIT_TYPE;
    goto exit;
  }

free:
  if (ret)
    mem_free(rbsp);
exit:
  return ret;
}

static int
read_avcc(FILE * file, unsigned int box_size, box_t p_box) {
  unsigned char conf_version; /* configuration version */
  unsigned char profile_idc; /* AVC profile indication */
  unsigned char profile_comp; /* profile compatibility */
  unsigned char c_set0_flag; /* constraint set 0 flag */
  unsigned char c_set1_flag; /* constraint set 1 flag */
  unsigned char c_set2_flag; /* constraint set 2 flag */
  unsigned char level_idc; /* AVC level indication */
  unsigned char len_size_minus_one; /* NALU length size - 1 */

  unsigned char num_of_sps; /* number of sequence parameter sets */
  unsigned short sps_len; /* sequence parameter set length */

  unsigned char num_of_pps; /* number of picture parameter sets */
  unsigned short pps_len; /* picture parameter set length */

/* no need to implement now */
#if 0
  unsigned char chroma_format;
  unsigned char bit_depth_luma_minus8;
  unsigned char bit_depth_chroma_minus8;

  unsigned char num_of_sps_ext; /* number of sequence parameter set ext */
  unsigned short sps_ext_len; /* sequence parameter set ext length */
  unsigned char * sps_ext_nalu; /* sequence parameter set ext NALU */
#endif

  unsigned int i;
  int ret;

  (void) box_size;
  (void) p_box;

  ret = 0;

  if ((ret = read_u8(&conf_version, file)) != 0 ||
      (ret = read_u8(&profile_idc, file)) != 0 ||
      (ret = read_u8(&profile_comp, file)) != 0 ||
      (ret = read_u8(&level_idc, file)) != 0 ||
      (ret = read_u8(&len_size_minus_one, file)) != 0 ||
      (ret = read_u8(&num_of_sps, file)) != 0)
    goto exit;

  c_set0_flag = (unsigned char) ((profile_comp >> 7) & 0x1);
  c_set1_flag = (unsigned char) ((profile_comp >> 6) & 0x1);
  c_set2_flag = (unsigned char) ((profile_comp >> 5) & 0x1);
  len_size_minus_one = (unsigned char) (len_size_minus_one & 0x3);
  num_of_sps = (unsigned char) (num_of_sps & 0x1f);

  attr("conf_version:"); printf("%u\n", conf_version);
  attr("profile_idc:"); printf("%u\n", profile_idc);
  attr("constraint_set0_flag:"); printf("%u\n", c_set0_flag);
  attr("constraint_set1_flag:"); printf("%u\n", c_set1_flag);
  attr("constraint_set2_flag:"); printf("%u\n", c_set2_flag);
  attr("level_idc:"); printf("%u\n", level_idc);
  attr("len_size_minus_one:"); printf("%u\n", len_size_minus_one);
  attr("num_of_sps:"); printf("%u\n", num_of_sps);

  indent(1);
  for (i = 0; i < num_of_sps; i++) {
    if ((ret = read_u16(&sps_len, file)) != 0)
      goto exit;

    attr("sps_len:"); printf("%u\n", sps_len);

    if ((ret = read_nalu(sps_len, file)) != 0)
      goto exit;
  }
  indent(-1);

  if ((ret = read_u8(&num_of_pps, file)) != 0)
    goto exit;

  attr("num_of_pps:"); printf("%u\n", num_of_pps);

  indent(1);
  for (i = 0; i < num_of_sps; i++) {
    if ((ret = read_u16(&pps_len, file)) != 0)
      goto exit;

    attr("pps_len:"); printf("%u\n", pps_len);

    if ((ret = read_nalu(pps_len, file)) != 0)
      goto exit;
  }
  indent(-1);

  if (profile_idc == 100 ||
      profile_idc == 110 ||
      profile_idc == 122 ||
      profile_idc == 144) {
    ret = ERR_UNK_PROFILE_IDC;
    goto exit;
  }
exit:
  return ret;
}

static int
read_stsd(FILE * file, unsigned int box_size, box_t p_box) {
  unsigned char version;
  unsigned int flags;
  unsigned int entry_count;
  unsigned int i;
  unsigned int j;
  box_t entry;
  struct box_visual_sample_entry * v_entry;
  struct box_hdlr * hdlr;
  struct box_stsd * stsd;
  box_t box;
  static struct box_func funcs[] = {
    {"avcC", read_avcc},
    {NULL, NULL}
  };
  int ret;

  (void) box_size;

  ret = 0;

  if ((ret = read_ver(&version, &flags, file)) != 0 ||
      (ret = read_u32(&entry_count, file)) != 0)
    goto exit;

  attr("entry_count:"); printf("%u\n", entry_count);

  entry.v_entry = NULL;

  if (entry_count) {

    hdlr = &p_box.mdia->hdlr;
    if (strncmp(hdlr->type, "vide", 4) == 0) {

      if ((ret = mem_alloc(&v_entry, entry_count * sizeof(* v_entry))) != 0)
        goto exit;

      for (i = 0; i < entry_count; i++)
        if ((ret = read_visual_sample_entry(file, &v_entry[i])) != 0)
          goto free_entry;

      entry.v_entry = v_entry;
free_entry:
      if (ret) {
        for (j = 0; j < i; j++)
          free_visual_sample_entry(&v_entry[j]);
        mem_free(v_entry);
      }
    } else {
      ret = ERR_UNK_HDLR_TYPE;
      goto exit;
    }
  }

  box.stsd = &p_box.mdia->minf.stbl.stsd;
  if ((ret = dispatch(file, funcs, box_size, box)) != 0)
    goto free;

  stsd = box.stsd;
  stsd->entry_count = entry_count;
  stsd->entry = entry;
free:
  if (ret) {
    for (j = 0; j < entry_count; j++)
      free_visual_sample_entry(&entry.v_entry[j]);
    mem_free(entry.v_entry);
  }
exit:
  return ret;
}

static int
read_stbl(FILE * file, unsigned int box_size, box_t p_box) {
  static struct box_func funcs[] = {
    {"stsd", read_stsd},
    {NULL, NULL}
  };
  return dispatch(file, funcs, box_size, p_box);
}

static int
read_minf(FILE * file, unsigned int box_size, box_t p_box) {
  static struct box_func funcs[] = {
    {"dinf", read_dinf},
    {"stbl", read_stbl},
    {NULL, NULL}
  };
  return dispatch(file, funcs, box_size, p_box);
}

static int
read_mdia(FILE * file, unsigned int box_size, box_t p_box) {
  static struct box_func funcs[] = {
    {"mdhd", read_mdhd},
    {"hdlr", read_hdlr},
    {"minf", read_minf},
    {NULL, NULL}
  };
  box_t box;
  box.mdia = &p_box.trak->mdia;
  return dispatch(file, funcs, box_size, box);
}

static int
read_trak(FILE * file, unsigned int box_size, box_t p_box) {
  static struct box_func funcs[] = {
    {"tkhd", read_tkhd},
    {"mdia", read_mdia},
    {NULL, NULL}
  };
  struct box_moov * moov;
  box_t box;
  int ret;

  moov = p_box.moov;
  if ((ret = mem_realloc(&moov->trak,
                         (moov->trak_len + 1) * sizeof(* moov->trak))) != 0)
    return ret;
  moov->trak_len++;

  box.trak = &moov->trak[moov->trak_len - 1];
  return dispatch(file, funcs, box_size, box);
}

static int
read_moov(FILE * file, unsigned int box_size, box_t p_box) {
  static struct box_func funcs[] = {
    {"mvhd", read_mvhd},
    {"trak", read_trak},
    {NULL, NULL}
  };
  box_t box;
  box.moov = &p_box.top->moov;
  return dispatch(file, funcs, box_size, box);
}

int
main(int argc, char ** argv) {
  const char * fname;
  FILE * file;
  static struct box_func funcs[] = {
    {"ftyp", read_ftyp},
    {"moov", read_moov},
    {NULL, NULL}
  };
  struct box_top top;
  box_t box;
  unsigned int i;
  unsigned int j;
  int ret;

  (void) argc;
  (void) argv;

  /* open file */
  fname = "standbyme.mp4";
  file = fopen(fname, "rb");
  if (file == NULL) {
    ret = ERR_IO;
    goto exit;
  }

  box.top = &top;

  top.ftyp.c_brands = NULL;
  top.moov.trak = NULL;
  top.moov.trak_len = 0;

  ret = dispatch(file, funcs, (unsigned int) -1, box);

  for (i = 0; i < top.moov.trak_len; i++) {
    for (j = 0; j < top.moov.trak[i].mdia.minf.stbl.stsd.entry_count; j++)
      free_visual_sample_entry(
          top.moov.trak[i].mdia.minf.stbl.stsd.entry.v_entry);
    for (j = 0; j < top.moov.trak[i].mdia.minf.dinf.dref.entry_count; j++)
      free_data_entry(&top.moov.trak[i].mdia.minf.dinf.dref.entry[j]);
    mem_free(top.moov.trak[i].mdia.minf.dinf.dref.entry);
    mem_free(top.moov.trak[i].mdia.hdlr.name);
  }

  mem_free(top.moov.trak);
  mem_free(top.ftyp.c_brands);

  fclose(file);
exit:
  if (ret)
    printf("Error: %s\n", err_to_str(ret));
  return ret;
}
