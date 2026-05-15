#include "compiler.h"
// #include <sys/time.h>
// #include <signal.h>
// #include <unistd.h>
#include "strres.h"
#include "np2.h"
#include "dosio.h"
#include "commng.h"
#include "fontmng.h"
#include "inputmng.h"
#include "scrnmng.h"
#include "soundmng.h"
#include "sysmng.h"
#include "taskmng.h"
#include "ini.h"
#include "pccore.h"
#include "cpucore.h"
#include "iocore.h"
#include "sound.h"
#include "fmboard.h"
#include "scrndraw.h"
#include "s98.h"
#include "diskdrv.h"
#include "timing.h"
#include "keystat.h"
#include "vramhdl.h"
#include "menubase.h"
#include "mousemng.h"
#include "sysmenu.h"
#ifdef USE_GPROF
#include <pspprof.h>
#endif
#include "pg.h"
#include <pspkernel.h>
#include <pspdebug.h>
#include <psppower.h>
#include <pspctrl.h>

#ifdef PSPDBG
#include "psplib.h"
#endif

#include <pspdisplay.h>

PSP_MODULE_INFO("np2 for PSP", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);
//PSP_HEAP_SIZE_KB(-256);

NP2OSCFG np2oscfg = {0, 2, 0, 0, 0, 0, 0, 0};
static UINT framecnt;
static UINT waitcnt;
static UINT framemax = 1;
static char datadir[MAX_PATH];
static int exit_sem;
char clock_buf[64];
#if defined(VITA_NP2_PORT)
int vita_np2_autorun_trace = 0;
int vita_np2_diag_pause_core = 0;
UINT8 vita_np2_frameskip = 2;
UINT8 vita_np2_core_drawskip = 0;
UINT8 vita_np2_audio_light = 0;
UINT8 vita_np2_speed_limit = 2;
UINT8 vita_np2_touhou_preset = 0;
#endif

// ---- resume

static void getstatfilename(char *path, const char *ext, int size) {

	file_cpyname(path, datadir, size);
	file_cutext(path);
	file_catname(path, "np2sdl.", size);
	file_catname(path, ext, size);
}

static int flagsave(const char *ext) {

	int		ret;
	char	path[MAX_PATH];

	getstatfilename(path, ext, sizeof(path));
	ret = statsave_save(path);
	if (ret) {
		file_delete(path);
	}
	return(ret);
}

static void flagdelete(const char *ext) {

	char	path[MAX_PATH];

	getstatfilename(path, ext, sizeof(path));
	file_delete(path);
}

static int flagload(const char *ext, const char *title, BOOL force) {

	int		ret;
	int		id;
	char	path[MAX_PATH];
	char	buf[1024];
	char	buf2[1024 + 256];

	getstatfilename(path, ext, sizeof(path));
	id = DID_YES;
	ret = statsave_check(path, buf, sizeof(buf));
	if (ret & (~STATFLAG_DISKCHG)) {
		menumbox("Couldn't restart", title, MBOX_OK | MBOX_ICONSTOP);
		id = DID_NO;
	}
	else if ((!force) && (ret & STATFLAG_DISKCHG)) {
		SPRINTF(buf2, "Conflict!\n\n%s\nContinue?", buf);
		id = menumbox(buf2, title, MBOX_YESNOCAN | MBOX_ICONQUESTION);
	}
	if (id == DID_YES) {
		statsave_load(path);
	}
	return(id);
}


// ---- proc

static void framereset(UINT cnt) {
    framecnt = 0;
#ifdef PSPDBG
    if (sysmng_workclockrenewal()) {
        sysmng_updatecaption();
    }
#endif
}

static void processwait(UINT cnt) {

	if (timing_getcount() >= cnt) {
		timing_setcount(0);
		framereset(cnt);
	}
	else {
#if defined(VITA_NP2_PORT)
		sceKernelDelayThreadCB(1000);
#else
	  //	  	taskmng_sleep(1);
#endif
	}
}


int pspeDebugWrite(const char* str,size_t length);

static void exit_proc(void)
{
    //終了処理

    #if defined(VITA_NP2_PORT)
    vita_np2_log("exit_proc: begin");
#endif
    taskmng_exit();

    /* main threadのループ抜け待ち、または
       menuのexitとPSPのHOMEキーor電源断での終了処理の排他 */
    sceKernelWaitSema(exit_sem, 1, 0); //lock

#ifdef USE_GPROF
	gprof_cleanup();
#endif

    pccore_cfgupdate();
    if (np2oscfg.resume) {
        flagsave(str_sav);
    }
    else {
        flagdelete(str_sav);
    }

    pccore_term();
    S98_trash();
    soundmng_deinitialize();

    if (sys_updates & (SYS_UPDATECFG | SYS_UPDATEOSCFG)) {
        initsave();
    }
}

int exit_callback(int arg1, int arg2, void *common)
{
    exit_proc();

    sceKernelExitGame();
    return 0;
}

volatile int suspending = 0;

int power_callback(int unknown, int pwrflags, void *common)
{
    if (pwrflags &
        (PSP_POWER_CB_POWER_SWITCH | PSP_POWER_CB_SUSPENDING)) {
            suspending = 1;
            sceKernelSignalSema(exit_sem,1); // unlock
    }

    if (pwrflags & PSP_POWER_CB_RESUME_COMPLETE) {
        sceKernelWaitSema(exit_sem, 1, 0); // lock
        suspending = 0;
    }

    return 0;
}

int CallbackThread(SceSize args, void *argp)
{
    int cbid;

    cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
    sceKernelRegisterExitCallback(cbid); //SetExitCallback(cbid);
    cbid = sceKernelCreateCallback("Power Callback", power_callback, NULL);
    scePowerRegisterCallback(0, cbid);

    sceKernelSleepThreadCB(); //KernelPollCallbacks();

    return 0;
}

int SetupCallbacks(void)
{
    int thid;

    thid = sceKernelCreateThread("update_thread", CallbackThread,
                                0x11, 0xFA0, 0, 0);
    if (thid >= 0) {
        sceKernelStartThread(thid, 0, 0);
    }

    return thid;
}

// ms0:/psp/game/hoge/eboot.pbp -> ms0:/psp/game/hoge/ ???
static void set_datadir(char *s)
{
#if defined(VITA_NP2_PORT)
    (void)s;
    sceIoMkdir("ux0:data", 0777);
    sceIoMkdir("ux0:data/np2vita", 0777);
    sceIoMkdir("ux0:data/np2vita/disks", 0777);
    file_cpyname(datadir, "ux0:data/np2vita/", sizeof(datadir));
#else
    char *p = '\0', *d;

    if (s == NULL || *s == '\0') {
        file_cpyname(datadir, "app0:/", sizeof(datadir));
        return;
    }

    for (d = datadir; *s != '\0'; s++) {
        *d++ = *s;
        if (*s == '/') {
            p = d;
        }
    }
    if (p != NULL) {
        *p = '\0';
    }
    else {
        file_cpyname(datadir, "app0:/", sizeof(datadir));
    }
#endif
}
void Gu_Finish_Callback(int id, void *arg)
{
    if (menuvram) {
        scrnmng_menudraw2(NULL);

        // メニューを開いているならマウスカーソルを描画する
        scrnmng_draw_cursor();
    }

    taskmng_prt_onscrn();

#ifdef PSPDBG
    plClrChars(40, 0, 35);
    plPrint(40, 0, 0xffff, clock_buf);
#endif

    /* 表<->裏画面切替 */
    sceDisplayWaitVblank();
    pgScreenFlip();
}

int main(int argc, char *argv[])
{
    int id;

    vita_np2_log_reset();
    vita_np2_log("main: enter");
    vita_np2_log("main: build vita-np2-kai-fullcore-rc009-rebuild-nofmgen 20260515");
    set_datadir((argc > 0 && argv != NULL) ? argv[0] : NULL);
    vita_np2_log("main: datadir set");
    vita_np2_log(datadir);

    pgGeInit();
    vita_np2_log("main: pgGeInit ok");
    //    scrnmng_gu_init0();
    sceDisplaySetMode(0, SCREEN_WIDTH, SCREEN_HEIGHT);
    pgScreenFrame(2, 0);
    vita_np2_log("main: screen frame ok");

    exit_sem = sceKernelCreateSema("exitcsec", 0, 1, 1, NULL);
    vita_np2_log("main: sema created");
    SetupCallbacks();
    vita_np2_log("main: callbacks setup");

    GeCB gecb;
    gecb.signal_func = NULL;
    gecb.signal_arg = NULL;
    gecb.finish_func = Gu_Finish_Callback;
    gecb.finish_arg = NULL;
    gecbid = sceGeSetCallback(&gecb);
    vita_np2_log("main: ge callback set");

    /* カレントファイル操作 */
    file_setcd(datadir);
    vita_np2_log("main: file_setcd ok");
    /* np2.cfgファイルの読み込み? */
    initload();
    vita_np2_log("main: initload ok");
#if defined(VITA_NP2_PORT)
    if (vita_np2_frameskip > 10) {
        vita_np2_frameskip = 10;
    }
    if (vita_np2_speed_limit > 2) {
        vita_np2_speed_limit = 2;
    }
    if (vita_np2_touhou_preset) {
        np2cfg.baseclock = 2457600;
        if (np2cfg.multiple < 8) {
            np2cfg.multiple = 8;
        }
        np2cfg.samplingrate = 44100;
        np2cfg.delayms = 0;
        np2cfg.skipline = 1;
        np2cfg.skiplight = 255;
        vita_np2_frameskip = 0;
        vita_np2_core_drawskip = 0;
        vita_np2_speed_limit = 1;
        vita_np2_log("main: vita touhou preset on 56.4fps/44100hz");
    }
    np2oscfg.DRAW_SKIP = vita_np2_frameskip;
    np2oscfg.NOWAIT = 1;
    {
        char vita_cfg_log[192];
        snprintf(vita_cfg_log, sizeof(vita_cfg_log), "main: vita cfg present_skip=%u core_drawskip=%u audio_light=%u speed_limit=%u touhou=%u clk=%ldx%ld hz=%u latency=%u skiplight=%d", vita_np2_frameskip, vita_np2_core_drawskip, vita_np2_audio_light, vita_np2_speed_limit, vita_np2_touhou_preset, (long)np2cfg.baseclock, (long)np2cfg.multiple, np2cfg.samplingrate, np2cfg.delayms, np2cfg.skiplight);
        vita_np2_log(vita_cfg_log);
    }
    if (vita_np2_audio_light) {
        np2cfg.samplingrate = 11025;
        if (np2cfg.delayms < 750) {
            np2cfg.delayms = 750;
        }
        vita_np2_log("main: vita audio light 11025hz latency>=750");
    }
    vita_np2_log("main: vita force nowait=1");
#if defined(CPUCORE_IA32)
    vita_np2_log("main: vita cpu core=ia32");
#else
    vita_np2_log("main: vita cpu core=i286");
#endif
    if (np2cfg.EXTMEM < 13) {
        np2cfg.EXTMEM = 13;
        vita_np2_log("main: vita force extmem=13");
    }
    if (!np2cfg.skipline) {
        np2cfg.skipline = 1;
        vita_np2_log("main: vita force skipline=1");
    }
    if (np2cfg.dipsw[1] != 0xf3) {
        np2cfg.dipsw[1] = 0xf3;
        vita_np2_log("main: vita force dipsw gdc=2.5mhz");
    }
#endif

    scrnmng_change_scrn(np2oscfg.pspscrn);
    vita_np2_log("main: change_scrn ok");

    TRACEINIT();
    vita_np2_log("main: trace init ok");

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    vita_np2_log("main: ctrl init ok");

    if (fontmng_init() != SUCCESS) {
        vita_np2_log("main: fontmng_init failed");
        goto np2main_err2;
    }
    vita_np2_log("main: fontmng_init ok");
    keystat_initialize();
    vita_np2_log("main: keystat init ok");

    if (sysmenu_create() != SUCCESS) {
        vita_np2_log("main: sysmenu_create failed");
        goto np2main_err3;
    }

    vita_np2_log("main: sysmenu_create ok");
    scrnmng_gu_init();
    vita_np2_log("main: scrnmng_gu_init ok");

    scrnmng_initialize();
    vita_np2_log("main: scrnmng_initialize ok");
    if (scrnmng_create(FULLSCREEN_WIDTH, FULLSCREEN_HEIGHT) != SUCCESS) {
        vita_np2_log("main: scrnmng_create failed");
        goto np2main_err4;
    }

    vita_np2_log("main: scrnmng_create ok");
    soundmng_initialize();
    vita_np2_log("main: soundmng init ok");
    commng_initialize();
    vita_np2_log("main: commng init ok");
    sysmng_initialize();
    vita_np2_log("main: sysmng init ok");
    taskmng_initialize();
    vita_np2_log("main: taskmng init ok");
    mousemng_initialize();
    vita_np2_log("main: mousemng init ok");

    pccore_init();
    vita_np2_log("main: pccore_init ok");
    S98_init();
    vita_np2_log("main: S98_init ok");

    scrndraw_redraw();
    vita_np2_log("main: scrndraw_redraw ok");

    pccore_reset();
    vita_np2_log("main: pccore_reset ok");
    sysmenu_menuopen(0, 0, 0);
    vita_np2_log("main: startup menu open");
    scrnmng_gu_update();
    vita_np2_log("main: startup menu draw");

    if (np2oscfg.resume) {
        id = flagload(str_sav, str_resume, FALSE);
        if (id == DID_CANCEL) {
            goto np2main_err5;
        }
    }

#ifdef PSPDBG
    sysmng_workclockreset();
#endif

#if defined(VITA_NP2_PORT)
    vita_np2_log("main: skip startup semaphore lock on vita");
#else
    sceKernelWaitSema(exit_sem, 1, 0); // lock
#endif

#if defined(VITA_NP2_PORT)
    task_avail = TRUE;
    vita_np2_log(taskmng_isavail() ? "main: loop enter avail=1" : "main: loop enter avail=0");
#else
    vita_np2_log("main: loop enter");
#endif

#if defined(VITA_NP2_PORT)
    {
        UINT vita_task_flip_logs;
        UINT vita_exec_flip_logs;
        UINT vita_loop_counter;
        UINT vita_last_drawcount;
        UINT vita_draw_stall;
        UINT vita_redraw_logs;

                UINT vita_present_period;
vita_task_flip_logs = 0;
        vita_exec_flip_logs = 0;
        vita_loop_counter = 0;
        vita_last_drawcount = drawcount;
        vita_draw_stall = 0;
        vita_redraw_logs = 0;
                vita_present_period = (UINT)np2oscfg.DRAW_SKIP + 1;
while (1) {
            if (suspending == 1) {
                sceDisplayWaitVblankStart();
                vita_loop_counter++;
                continue;
            }
            taskmng_rol();
            if (!task_avail) {
#if VITA_NP2_DIAG
                if (vita_task_flip_logs < 2) {
                    vita_np2_log("main: task_avail flipped after task; restoring");
                    vita_task_flip_logs++;
                }
#endif
                task_avail = TRUE;
            }
            if (menuvram) {
                scrnmng_gu_update();
                sceDisplayWaitVblankStart();
                vita_loop_counter++;
                continue;
            }
            pccore_exec(TRUE);
            {
                UINT vita_draw_changed = (drawcount != vita_last_drawcount);
                if (!vita_draw_changed) {
                    vita_draw_stall++;
                }
                else {
                    vita_last_drawcount = drawcount;
                    vita_draw_stall = 0;
                }
            if (vita_draw_stall >= 600) {
#if VITA_NP2_DIAG
                if (vita_redraw_logs < 16) {
                    vita_np2_log("main: redraw watchdog");
                    iocore_vita_diag_log_recent();
                    vita_redraw_logs++;
                }
#endif
                scrndraw_redraw();
                vita_draw_stall = 0;
            }
            #if VITA_NP2_DIAG
            if ((vita_loop_counter != 0) && ((vita_loop_counter % 600) == 0)) {
                scrnmng_vita_diag_log(vita_loop_counter, drawcount, pccore.realclock, pcstat.screenupdate, soundrenewal, CPU_CLOCK, CPU_BASECLOCK, CPU_REMCLOCK, CPU_CS, CPU_EIP, g_nevent.readyevents, g_nevent.waitevents, g_nevent.readyevents ? g_nevent.level[0] : 99, g_nevent.readyevents ? g_nevent.item[g_nevent.level[0]].clock : 0);
            }
#endif
            if ((vita_present_period <= 1) || ((vita_loop_counter % vita_present_period) == 0)) {
                scrnmng_gu_update();
                sceDisplayWaitVblankStart();
            }
            else if ((vita_np2_speed_limit == 1) ||
                     (vita_np2_speed_limit == 2 && !vita_draw_changed)) {
                sceDisplayWaitVblankStart();
            }
            }
            vita_loop_counter++;
            if (!task_avail) {
#if VITA_NP2_DIAG
                if (vita_exec_flip_logs < 2) {
                    vita_np2_log("main: task_avail flipped after exec; restoring");
                    vita_exec_flip_logs++;
                }
#endif
                task_avail = TRUE;
            }
        }
    }
#else
    while(taskmng_isavail()) {
#if defined(VITA_NP2_PORT)
        static UINT vita_top_logs = 0;
        if ((vita_top_logs < 32) || (vita_np2_autorun_trace > 0)) {
            vita_np2_log("main: loop top");
        }
#endif
        if (suspending == 1) {
            sceDisplayWaitVblankStart();
            continue;
	}
#if defined(VITA_NP2_PORT)
        if ((vita_top_logs < 32) || (vita_np2_autorun_trace > 0)) {
            vita_np2_log("main: taskmng_rol begin");
        }
#endif
        taskmng_rol();
#if defined(VITA_NP2_PORT)
        if ((vita_top_logs < 32) || (vita_np2_autorun_trace > 0)) {
            vita_np2_log("main: taskmng_rol end");
            vita_top_logs++;
        }
#endif
        if (menuvram) {
#if defined(VITA_NP2_PORT)
            if (vita_np2_autorun_trace > 0) {
                vita_np2_log("main: menu active after autorun");
                vita_np2_autorun_trace--;
            }
#endif
            // menuを出している時は、emulationを停止する
		sceDisplayWaitVblankStart();
		continue;
        }
#if defined(VITA_NP2_PORT)
        {
            static UINT vita_loop_logs = 0;
            if ((vita_loop_logs < 32) || (vita_np2_autorun_trace > 0)) {
                vita_np2_log("main: vita simple loop begin");
            }
            pccore_exec(TRUE);
            scrnmng_gu_update();
            if ((vita_loop_logs < 32) || (vita_np2_autorun_trace > 0)) {
                vita_np2_log("main: vita simple loop end");
                vita_loop_logs++;
                if (vita_np2_autorun_trace > 0) {
                    vita_np2_autorun_trace--;
                }
            }
            continue;
        }
#endif
        if (np2oscfg.NOWAIT) {
            pccore_exec(framecnt == 0);
            if (np2oscfg.DRAW_SKIP) { // nowait frame skip
                framecnt++;
                if (framecnt >= np2oscfg.DRAW_SKIP) {
                    processwait(0);
                }
            }
            else { // nowait auto skip
                framecnt = 1;
                if (timing_getcount()) {
                    processwait(0);
                }
            }
        }
        else if (np2oscfg.DRAW_SKIP) { // frame skip
            if (framecnt < np2oscfg.DRAW_SKIP) {
                pccore_exec(framecnt == 0);
                framecnt++;
            }
            else {
                processwait(np2oscfg.DRAW_SKIP);
            }
        }
        else { // auto skip
            if (!waitcnt) {
                UINT cnt;
                pccore_exec(framecnt == 0);
                framecnt++;
                cnt = timing_getcount();
                if (framecnt > cnt) {
                    waitcnt = framecnt;
                    if (framemax > 1) {
                        framemax--;
                    }
                }
                else if (framecnt >= framemax) {
                    if (framemax < 12) {
                        framemax++;
                    }
                    if (cnt >= 12) {
                        timing_reset();
                    }
                    else {
                        timing_setcount(cnt - framecnt);
                    }
                    framereset(0);
                }
            }
            else {
                processwait(waitcnt);
                waitcnt = framecnt;
            }
        }
    }
#endif

    sceKernelSignalSema(exit_sem,1); // unlock
    exit_callback(0, 0, 0);

 np2main_err5:
    pccore_term();
    S98_trash();
    soundmng_deinitialize();

 np2main_err4:
    scrnmng_destroy();

 np2main_err3:
    sysmenu_destroy();

 np2main_err2:
    TRACETERM();

    // np2main_err1:
    return(FAILURE);
}

