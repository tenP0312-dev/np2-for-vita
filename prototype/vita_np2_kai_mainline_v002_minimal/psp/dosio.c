#include "compiler.h"
#include "dosio.h"
#include "pg.h"
#include "psplib.h"
#if defined(VITA_NP2_PORT)
#include "codecnv.h"
#endif
#if defined(VITA_NP2_PORT)
#include "psp_compat.h"
#endif

static char curpath[MAX_PATH];
static char *curfilep = curpath;
static path_list_t *pl_head = NULL;

static path_list_t *add_pl(int fd, char *s)
{
    path_list_t *p;

    p = malloc(sizeof(path_list_t));
    if (p == NULL) {
        return NULL;
    }

    ZeroMemory(p, sizeof(path_list_t));

    p->fd = fd;
    strcpy(p->path, s);

    // 先頭に追加
    p->next = pl_head;
    pl_head = p;

    return p;
}

static void del_pl(int fd)
{
    path_list_t *p, *prev;

    for (p = pl_head, prev = NULL; p != NULL; prev = p, p = p->next) {
        if (p->fd == fd) {
            if (prev != NULL) {
                prev->next = p->next;
            } else {
                pl_head = p->next;
            }
            free(p);
            return;
        }
    }
    plPrintErr("fd not found");
}

// fdからpathの文字列を返す。見つかれなければNULL
static char *fd2path(int fd)
{
    path_list_t *p;

    for (p = pl_head; p != NULL; p = p->next) {
        if (p->fd == fd) {
            return p->path;
        }
    }
    return NULL;
}

#if defined(VITA_NP2_PORT)
static int vita_path_is_utf8_nonascii(const char *s)
{
    int nonascii;

    if (s == NULL) {
        return 0;
    }
    nonascii = 0;
    while (*s) {
        unsigned char c;

        c = (unsigned char)*s++;
        if (c < 0x80) {
            continue;
        }
        nonascii = 1;
        if ((c & 0xe0) == 0xc0) {
            if (((unsigned char)s[0] & 0xc0) != 0x80) {
                return 0;
            }
            s += 1;
        }
        else if ((c & 0xf0) == 0xe0) {
            if ((((unsigned char)s[0] & 0xc0) != 0x80) ||
                (((unsigned char)s[1] & 0xc0) != 0x80)) {
                return 0;
            }
            s += 2;
        }
        else {
            return 0;
        }
    }
    return nonascii;
}

static void vita_euc_to_sjis_pair(UINT8 euc1, UINT8 euc2, char *dst)
{
    UINT8 ku;
    UINT8 ten;
    UINT8 lead;
    UINT8 trail;

    ku = euc1 - 0x80;
    ten = euc2 - 0x80;
    lead = (UINT8)(((ku - 0x21) >> 1) + 0x81);
    if (lead > 0x9f) {
        lead += 0x40;
    }
    if (ku & 1) {
        trail = ten + 0x1f;
        if (trail >= 0x7f) {
            trail++;
        }
    }
    else {
        trail = ten + 0x7e;
    }
    dst[0] = (char)lead;
    dst[1] = (char)trail;
}

static void vita_sjis_to_euc_pair(UINT8 sjis1, UINT8 sjis2, char *dst)
{
    UINT8 adjust;
    UINT8 row;
    UINT8 cell;

    adjust = (sjis1 < 0xa0) ? 0x81 : 0xc1;
    if (sjis2 >= 0x9f) {
        row = (UINT8)(((sjis1 - adjust) << 1) + 0x22);
        cell = (UINT8)(sjis2 - 0x7e);
    }
    else {
        row = (UINT8)(((sjis1 - adjust) << 1) + 0x21);
        if (sjis2 > 0x7e) {
            sjis2--;
        }
        cell = (UINT8)(sjis2 - 0x1f);
    }
    dst[0] = (char)(row + 0x80);
    dst[1] = (char)(cell + 0x80);
}

static int vita_ucs2_to_sjis_char(UINT16 ucs, char *dst, int remain)
{
    int h;
    int l;

    if (ucs < 0x80) {
        if (remain < 1) {
            return 0;
        }
        dst[0] = (char)ucs;
        return 1;
    }
    if ((ucs >= 0xff61) && (ucs <= 0xff9f)) {
        if (remain < 1) {
            return 0;
        }
        dst[0] = (char)(ucs - 0xff61 + 0xa1);
        return 1;
    }
    for (h = 0xa1; h <= 0xfe; h++) {
        for (l = 0xa1; l <= 0xfe; l++) {
            char euc[3];
            UINT16 chk[2];

            euc[0] = (char)h;
            euc[1] = (char)l;
            euc[2] = '\0';
            codecnv_euctoucs2(chk, NELEMENTS(chk), euc, 2);
            if (chk[0] == ucs) {
                if (remain < 2) {
                    return 0;
                }
                vita_euc_to_sjis_pair((UINT8)h, (UINT8)l, dst);
                return 2;
            }
        }
    }
    if (remain < 1) {
        return 0;
    }
    dst[0] = '?';
    return 1;
}

static void vita_sjis_bytes_from_ucs2(char *dst, int dstlen, const UINT16 *ucs)
{
    UINT i;
    int out;

    if (dstlen <= 0) {
        return;
    }
    dst[0] = '\0';
    out = 0;
    for (i = 0; ucs[i] != 0; i++) {
        int wrote;

        wrote = vita_ucs2_to_sjis_char(ucs[i], dst + out, dstlen - 1 - out);
        if (wrote <= 0) {
            break;
        }
        out += wrote;
    }
    dst[out] = '\0';
}

static int vita_ucs2_has_mojibake_hint(const UINT16 *ucs)
{
    UINT i;

    for (i = 0; ucs[i] != 0; i++) {
        UINT16 c;

        c = ucs[i];
        if (((c >= 0xff61) && (c <= 0xff9f)) ||
            (c == 0x7e32) || (c == 0x7e3a) || (c == 0x8b5a) ||
            (c == 0x8b41) || (c == 0x8349) || (c == 0x8353) ||
            (c == 0x835f) || (c == 0x8376) || (c == 0x837b)) {
            return 1;
        }
    }
    return 0;
}

static void vita_utf8_to_sjis_name(char *dst, int dstlen, const char *src)
{
    UINT16 ucs[MAX_PATH];
    UINT16 fixeducs[MAX_PATH];
    char rawbytes[MAX_PATH];
    UINT n;

    if (dstlen <= 0) {
        return;
    }
    dst[0] = '\0';
    n = codecnv_utf8toucs2(ucs, NELEMENTS(ucs), src, (UINT)-1);
    if (n == 0) {
        milstr_ncpy(dst, src, dstlen);
        return;
    }

    /* Some transfers/log viewers expose UTF-8 names after a CP932 mojibake pass.
       When the CP932 bytes form valid UTF-8 again, recover the intended name. */
    if (vita_ucs2_has_mojibake_hint(ucs)) {
        vita_sjis_bytes_from_ucs2(rawbytes, sizeof(rawbytes), ucs);
        if (vita_path_is_utf8_nonascii(rawbytes)) {
            UINT rn;

            rn = codecnv_utf8toucs2(fixeducs, NELEMENTS(fixeducs), rawbytes, (UINT)-1);
            if (rn > 0) {
                vita_sjis_bytes_from_ucs2(dst, dstlen, fixeducs);
                vita_np2_log("file: jpname repaired mojibake");
                return;
            }
        }
    }
    vita_sjis_bytes_from_ucs2(dst, dstlen, ucs);
}

static UINT vita_sjis_to_ucs2(UINT16 *dst, UINT dcnt, const char *src)
{
    UINT out;

    if ((dst == NULL) || (dcnt == 0)) {
        return 0;
    }
    out = 0;
    while ((out + 1 < dcnt) && (*src != '\0')) {
        UINT8 c;

        c = (UINT8)*src++;
        if (c < 0x80) {
            dst[out++] = c;
        }
        else if ((c >= 0xa1) && (c <= 0xdf)) {
            dst[out++] = (UINT16)(0xff61 + c - 0xa1);
        }
        else if (((((c ^ 0x20) - 0xa1) & 0xff) < 0x3c) && (*src != '\0')) {
            char euc[3];
            UINT16 ucs[2];

            vita_sjis_to_euc_pair(c, (UINT8)*src++, euc);
            euc[2] = '\0';
            codecnv_euctoucs2(ucs, NELEMENTS(ucs), euc, 2);
            dst[out++] = ucs[0];
        }
        else {
            dst[out++] = '?';
        }
    }
    dst[out] = '\0';
    return out + 1;
}

static void vita_path_to_os(char *dst, int dstlen, const char *src)
{
    UINT16 ucs[MAX_PATH];

    if (dstlen <= 0) {
        return;
    }
    dst[0] = '\0';
    if (src == NULL) {
        return;
    }
    if (vita_path_is_utf8_nonascii(src)) {
        milstr_ncpy(dst, src, dstlen);
        return;
    }
    vita_sjis_to_ucs2(ucs, NELEMENTS(ucs), src);
    codecnv_ucs2toutf8(dst, dstlen, ucs, (UINT)-1);
}
#endif
static FILEH _file_open(const char* file, int mode, int fmode)
{
    int fd;

#if defined(VITA_NP2_PORT)
    {
        char osfile[MAX_PATH];

        vita_path_to_os(osfile, sizeof(osfile), file);
        fd = sceIoOpen(osfile, mode, fmode);
    }
#else
    fd = sceIoOpen(file, mode, fmode);
#endif
    if (fd < 0) {
        return FILEH_INVALID;
    }

    if (add_pl(fd, (char *)file) == NULL) {
        return FILEH_INVALID;
    }

    return fd;
}



/* ファイル操作 */

FILEH file_open(const char *path)
{
    return(_file_open(path, PSP_O_RDWR, 0777));
}

FILEH file_open_rb(const char *path)
{
    return(_file_open(path, PSP_O_RDONLY, 0777));
}

FILEH file_create(const char *path)
{
    return(_file_open(path, PSP_O_RDWR | PSP_O_CREAT, 0777));
}

long file_seek(FILEH handle, long pointer, int method)
{
    return(sceIoLseek(handle, (long long)pointer, method));
}

UINT file_read(FILEH handle, void *data, UINT length)
{
    return (sceIoRead(handle, data, length));
}

UINT file_write(FILEH handle, const void *data, UINT length)
{
    return (sceIoWrite(handle, data, length));
}

short file_close(FILEH handle)
{
    sceIoClose(handle);
    del_pl(handle);

    return(0);
}

#if 0
/** Structure to hold the status information about a file */
typedef struct SceIoStat {
    unsigned int st_mode;
    unsigned int st_attr;
    /** Size of the file in bytes. */
    unsigned int st_size;
    /** Creation time. */
    struct dirent_tm st_ctime;
    /** Access time. */
    struct dirent_tm st_atime;
    /** Modification time. */
    struct dirent_tm st_mtime;
    /** Device-specific data. */
    unsigned int    st_private[6];
} SceIoStat;
#endif

UINT file_getsize(FILEH handle)
{
    char *s;
    SceIoStat p;

    s = fd2path(handle);
    if (s == NULL) {
        return 0;
    }

#if defined(VITA_NP2_PORT)
    {
        char ospath[MAX_PATH];

        vita_path_to_os(ospath, sizeof(ospath), s);
        sceIoGetstat(ospath, &p);
    }
#else
    sceIoGetstat(s, &p);
#endif

    return(p.st_size);
}

short file_getdatetime(FILEH handle, DOSDATE *dosdate, DOSTIME *dostime)
{
	return(-1);
}

short file_delete(const char *path)
{
	return(-1);
}

short file_attr(const char *path)
{
    return 0; // とりあえず0 typeはdosio.hで定義FILEATTR_
}

short file_dircreate(const char *path)
{
#if defined(VITA_NP2_PORT)
    {
        char ospath[MAX_PATH];

        vita_path_to_os(ospath, sizeof(ospath), path);
        return(sceIoMkdir(ospath, 0777));
    }
#else
	return(-1);
#endif
}


/* カレントファイル操作 */
void file_setcd(const char *exepath)
{
    file_cpyname(curpath, exepath, sizeof(curpath));
    curfilep = file_getname(curpath);
    *curfilep = '\0';
}

char *file_getcd(const char *path)
{
    file_cpyname(curfilep, path, sizeof(curpath) - (curfilep - curpath));
    return(curpath);
}

FILEH file_open_c(const char *path)
{
    file_cpyname(curfilep, path, sizeof(curpath) - (curfilep - curpath));
    return(file_open(curpath));
}

FILEH file_open_rb_c(const char *path)
{
    file_cpyname(curfilep, path, sizeof(curpath) - (curfilep - curpath));
    return(file_open_rb(curpath));
}

FILEH file_create_c(const char *path)
{
    file_cpyname(curfilep, path, sizeof(curpath) - (curfilep - curpath));
    return(file_create(curpath));
}

short file_delete_c(const char *path)
{
    file_cpyname(curfilep, path, sizeof(curpath) - (curfilep - curpath));
    return(file_delete(curpath));
}

short file_attr_c(const char *path)
{
    file_cpyname(curfilep, path, sizeof(curpath) - (curfilep - curpath));
    return(file_attr(curpath));
}

static void setflist(SceIoDirent *de, FLINFO *fli)
{
    fli->caps = FLICAPS_SIZE | FLICAPS_ATTR;
    fli->size = de->d_stat.st_size;
    fli->attr = FILEATTR_ARCHIVE;
#if defined(VITA_NP2_PORT)
    if (SCE_S_ISDIR(de->d_stat.st_mode) || SCE_SO_ISDIR(de->d_stat.st_attr)) {
        fli->attr = FILEATTR_DIRECTORY;
    }
#else
    if (de->d_stat.st_attr == FIO_SO_IFDIR) {
        fli->attr = FILEATTR_DIRECTORY;
    }
#endif
#if defined(VITA_NP2_PORT)
    vita_utf8_to_sjis_name(fli->path, NELEMENTS(fli->path), de->d_name);
#else
    milstr_ncpy(fli->path, de->d_name, NELEMENTS(fli->path));
#endif
}

SceIoDirent files;

FLISTH file_list1st(const char *dir, FLINFO *fli)
{
    int fd;

    ZeroMemory(&files, sizeof(files));

#if defined(VITA_NP2_PORT)
    {
        char osdir[MAX_PATH];

        vita_path_to_os(osdir, sizeof(osdir), dir);
        fd = sceIoDopen(osdir);
    }
#else
    fd = sceIoDopen(dir);
#endif
    if (fd < 0) {
        plPrintErr("sceIoDopen() failed");
        return FLISTH_INVALID;
    }

    while(1) {
        if (sceIoDread(fd, &files) <= 0) {
            plPrintErr("sceIoDread() faild");
            break;
        }
        if (files.d_name[0] == '.')
            continue;
#if defined(VITA_NP2_PORT)
        if (SCE_S_ISDIR(files.d_stat.st_mode) || SCE_SO_ISDIR(files.d_stat.st_attr)) {
            strcat(files.d_name, "/");
        }
#else
        if (files.d_stat.st_attr == FIO_SO_IFDIR) {
            strcat(files.d_name, "/");
        }
#endif
        setflist(&files, fli);
        return fd;
    }
    return FLISTH_INVALID;
}

BOOL file_listnext(FLISTH hdl, FLINFO *fli)
{
    while (1) {
        if (sceIoDread(hdl, &files) <= 0)
            return FAILURE;
        if (files.d_name[0] == '.')
            continue;
#if defined(VITA_NP2_PORT)
        if (SCE_S_ISDIR(files.d_stat.st_mode) || SCE_SO_ISDIR(files.d_stat.st_attr)) {
            strcat(files.d_name, "/");
        }
#else
        if (files.d_stat.st_attr == FIO_SO_IFDIR) {
            strcat(files.d_name, "/");
        }
#endif
        setflist(&files, fli);
        return SUCCESS;
    }
}

void file_listclose(FLISTH hdl)
{
    sceIoDclose(hdl);
}

void file_catname(char *path, const char *name, int maxlen) {

	int		csize;

	while(maxlen > 0) {
		if (*path == '\0') {
			break;
		}
		path++;
		maxlen--;
	}
	file_cpyname(path, name, maxlen);
	while((csize = milstr_charsize(path)) != 0) {
		if ((csize == 1) && (*path == '\\')) {
			*path = '/';
		}
		path += csize;
	}
}

char *file_getname(const char *path) {

const char	*ret;
	int		csize;

	ret = path;
	while((csize = milstr_charsize(path)) != 0) {
		if ((csize == 1) && (*path == '/')) {
			ret = path + 1;
		}
		path += csize;
	}
	return((char *)ret);
}

void file_cutname(char *path) {

	char	*p;

	p = file_getname(path);
	*p = '\0';
}

char *file_getext(const char *path) {

const char	*p;
const char	*q;

	p = file_getname(path);
	q = NULL;
	while(*p != '\0') {
		if (*p == '.') {
			q = p + 1;
		}
		p++;
	}
	if (q == NULL) {
		q = p;
	}
	return((char *)q);
}

void file_cutext(char *path) {

	char	*p;
	char	*q;

	p = file_getname(path);
	q = NULL;
	while(*p != '\0') {
		if (*p == '.') {
			q = p;
		}
		p++;
	}
	if (q != NULL) {
		*q = '\0';
	}
}

void file_cutseparator(char *path) {

	int		pos;

	pos = strlen(path) - 1;
	if ((pos > 0) &&							// 2文字以上でー
		(path[pos] == '/') &&					// ケツが \ でー
		((pos != 1) || (path[0] != '.'))) {		// './' ではなかったら
		path[pos] = '\0';
	}
}

void file_setseparator(char *path, int maxlen) {

	int		pos;

	pos = strlen(path);
	if ((pos) && (path[pos-1] != '/') && ((pos + 2) < maxlen)) {
		path[pos++] = '/';
		path[pos] = '\0';
	}
}

