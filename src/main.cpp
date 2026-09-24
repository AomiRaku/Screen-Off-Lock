/* ===========================================================================
 *  ScreenOffLock  ——  极简 Win32 托盘程序
 *
 *  功能：当屏幕被关闭（显示器进入 DPMS 关闭状态）时自动锁定计算机，
 *        效果等同于按下 Win+L。
 *
 *  形态：没有主窗口，只有一个系统托盘图标，右键弹出菜单。
 *        纯 Win32 API 实现，不依赖任何框架或运行时，单个小体积 exe。
 *
 *  构建：见仓库根目录 build.ps1
 * ===========================================================================
 */

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601      /* Windows 7 及以上 */
#endif

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <shellapi.h>
#include <powrprof.h>       /* GetPwrCapabilities：判断有没有盖子设备 */
#include <winhttp.h>        /* 检查更新只用到这里的类型与常量，函数运行时再取 */
#include <string.h>         /* strstr / strchr / memcpy：解析 tag_name 用 */
#include <stdlib.h>         /* malloc / free：更新说明是动态长度 */

/* ---------------------------------------------------------------------------
 * 应用信息
 * ------------------------------------------------------------------------ */

/*
 * 显示名带空格只是为了好看；文件名、窗口类名、互斥体、注册表路径一律用
 * 无空格的 ScreenOffLock，免得空格在各处惹麻烦。
 */
#define APP_NAME        L"ScreenOff Lock"
#define APP_VERSION     L"Build 1511"
#define APP_AUTHOR      L"羽梦千景 Raku Inkyetta"
#define APP_URL         L"https://github.com/AomiRaku/Screen-Off-Lock"
#define APP_LICENSE_URL L"https://github.com/AomiRaku/Screen-Off-Lock/blob/main/LICENSE"

/* ---------------------------------------------------------------------------
 * 多语言
 *
 * 界面上的每一句文案都从 g_str 里取，代码里不再出现硬编码的可见文字。
 * 要加语言就加一列、把 LANG_COUNT 加一，然后在语言窗口里补一个按钮。
 * ------------------------------------------------------------------------ */

typedef enum
{
    LANG_EN = 0,
    LANG_ZH = 1,
    LANG_COUNT
} LangId;

enum
{
    S_TAGLINE = 0,          /* 关于：一句话说明 */
    S_AUTHOR_FMT,           /* 关于：作者 */
    S_PROJECT_LABEL,        /* 关于：项目地址标签 */
    S_LICENSE_PREFIX,       /* 关于：许可证一行的前半句 */
    S_LICENSE_LINK,         /* 关于：许可证链接文字 */
    S_VERSION_FMT,          /* 关于：版本 */
    S_REG_INTRO,            /* 关于：注册表提示第一行 */
    S_REG_OUTRO,            /* 关于：注册表提示最后一行 */
    S_LANGUAGE_BTN,         /* 关于：语言按钮 */

    S_MENU_LOCK_SCREEN,
    S_MENU_LOCK_LID,
    S_MENU_LOCK_DELAY,
    S_MENU_AUTOSTART,
    S_MENU_CHECK,
    S_MENU_ABOUT,
    S_MENU_EXIT,

    /* 延迟锁定子菜单 */
    S_DELAY_NOW,
    S_DELAY_3,
    S_DELAY_5,
    S_DELAY_10,
    S_DELAY_30,

    S_CHECK_TITLE,
    S_CHECK_INTRO,
    S_CHECK_AC_FMT,
    S_CHECK_DC_FMT,
    S_CHECK_AC_LABEL,
    S_CHECK_DC_LABEL,
    S_CHECK_BUTTON_FMT,
    S_CHECK_LID_FMT,
    S_CHECK_REC_OFF,
    S_CHECK_REC_NOTHING,
    S_CHECK_REC_SLEEP,
    S_CHECK_READ_FAIL,
    S_APPLY_AC,
    S_APPLY_DC,

    S_ACTION_NOTHING,
    S_ACTION_SLEEP,
    S_ACTION_HIBERNATE,
    S_ACTION_SHUTDOWN,
    S_ACTION_OFFDISPLAY,
    S_ACTION_UNKNOWN,
    S_ACTION_LID_NOTHING,   /* 合盖语境下的“不采取任何操作” */

    S_VERDICT_BOTH,
    S_VERDICT_LID,
    S_VERDICT_BUTTON,
    S_VERDICT_OK,

    S_WARN_LINE1,
    S_WARN_LINE2,
    S_WARN_DONTASK,

    S_LANG_PROMPT,

    /* 检查更新 */
    S_BTN_CHECK_UPDATE,
    S_UPDATE_TITLE,
    S_UPDATE_CHECKING,
    S_UPDATE_FOUND,
    S_UPDATE_LATEST,
    S_UPDATE_FAILED,
    S_UPDATE_FOUND_TITLE_FMT,
    S_UPDATE_FOUND_BODY_FMT,
    S_UPDATE_REMOTE_FMT,
    S_UPDATE_IMAGE_NOTE,
    S_UPDATE_IGNORE,
    S_BTN_GO_UPDATE,
    S_BTN_DONE,
    S_BTN_CLOSE,

    S_BTN_OK,
    S_BTN_CANCEL,

    S_ERR_ALREADY_RUNNING,
    S_ERR_REGISTER_CLASS,
    S_ERR_CREATE_WINDOW,
    S_ERR_POWER_NOTIFY,

    S_COUNT
};

static const WCHAR *g_str[LANG_COUNT][S_COUNT] =
{
    /* ---------------- English ---------------- */
    {
        L"Locks the screen when the display turns off or the lid is closed.",
        L"Made with ♥ By %s",
        L"Project: ",
        L"Licensed under the ",
        L"MIT License",
        L"Version %s",
        L"Settings are stored in the registry at",
        L"You can delete them manually if you no longer need them.",
        L"Language / 语言",

        L"Lock when screen turns off",
        L"Lock when lid is closed",
        L"Lock delay",
        L"Run at startup",
        L"Power settings",
        L"About",
        L"Exit",

        L"Immediately",
        L"3 seconds",
        L"5 seconds",
        L"10 seconds",
        L"30 seconds",

        L"Power settings",
        L"With the current power settings,",
        L"On AC: %s",
        L"On battery: %s",
        L"On AC:",
        L"On battery:",
        L"Power button: %s",
        L"Lid close action: %s",
        L"Recommended: Turn off display",
        L"Recommended: Do nothing",
        L"Recommended: Sleep",
        L"Failed to read the power settings.",
        L"Apply to AC",
        L"Apply to battery",

        L"Do nothing",
        L"Sleep",
        L"Hibernate",
        L"Shut down",
        L"Turn off display",
        L"Unknown",
        L"Do nothing (screen off)",

        L"With these settings the lid and the power button cannot lock the screen; only an automatic display timeout can.",
        L"Closing the lid cannot lock the screen.",
        L"Pressing the power button cannot lock the screen.",
        L"All good.",

        L"Some features may not work with the current power settings.",
        L"Click \"Power settings\" in the tray menu for details.",
        L"Don't show this again",

        L"Choose a language",

        L"Check for updates",
        L"Check for updates",
        L"Checking for updates...",
        L"Update available",
        L"You already have the latest version.",
        L"Update check failed. Please check your network and try again.",
        L"New version available: Build %u",
        L"You are running Build %u.",
        L"Latest: Build %u",
        L"See the release page for images",
        L"Skip this version",
        L"Get update",
        L"Done",
        L"Close",

        L"OK",
        L"Cancel",

        L"Already running.",
        L"Failed to register the window class.",
        L"Failed to create the message window.",
        L"Failed to register the display power notification. The app cannot work."
    },

    /* ---------------- 简体中文 ---------------- */
    {
        L"屏幕被关闭或合盖时自动锁屏。",
        L"Made with ♥ By %s",
        L"项目地址：",
        L"本项目采用 ",
        L"MIT 协议",
        L"版本 %s",
        L"设置保存在注册表",
        L"不再使用时可手动删除。",
        L"Language / 语言",

        L"关闭屏幕时锁屏",
        L"合盖锁屏",
        L"延迟锁定",
        L"开机启动",
        L"电源设置",
        L"关于",
        L"退出",

        L"立即",
        L"3 秒",
        L"5 秒",
        L"10 秒",
        L"30 秒",

        L"电源设置",
        L"当前电脑设置下，",
        L"插电时：%s",
        L"电池时：%s",
        L"插电时：",
        L"使用电池时：",
        L"当前电源按钮：%s",
        L"当前合上盖子：%s",
        L"推荐值：关闭屏幕",
        L"推荐值：无操作（关闭屏幕）",
        L"推荐值：睡眠",
        L"读取电源设置失败。",
        L"插电时 设为推荐值",
        L"电池时 设为推荐值",

        L"不采取任何操作",
        L"睡眠",
        L"休眠",
        L"关机",
        L"关闭屏幕",
        L"未知",
        L"无操作（关闭屏幕）",

        L"合盖和按下电源按钮时不能锁屏，只有屏幕自动关闭时才能锁屏。",
        L"合盖时不能锁屏。",
        L"按下电源按钮时不能锁屏。",
        L"设置正常。",

        L"检测到当前电脑设置可能无法使用全部功能。",
        L"请点击托盘菜单的“电源设置”来查看。",
        L"不再显示",

        L"选择语言",

        L"检查更新",
        L"检查更新",
        L"正在检查更新…",
        L"发现更新",
        L"当前已是最新",
        L"更新检查失败，请检查网络情况重试",
        L"发现新版本：Build %u",
        L"当前版本：Build %u",
        L"最新版本：Build %u",
        L"请前往更新页面查看图片",
        L"忽略当前版本",
        L"前往更新",
        L"完成",
        L"关闭",

        L"确定",
        L"取消",

        L"已在运行，请勿重复运行。",
        L"注册窗口类失败。",
        L"创建消息窗口失败。",
        L"注册显示器电源通知失败，程序无法工作。"
    }
};

static LangId g_lang = LANG_ZH;

#define T(id) (g_str[g_lang][id])

#define WNDCLASS_NAME   L"ScreenOffLock_MsgWindow"
#define MUTEX_NAME      L"ScreenOffLock.SingleInstance.{4E1F9C0A-2B77-4D31-9E58-6A0C7B3D5F21}"

#define TRAY_ICON_ID    1
#define IDI_APP_ICON    1        /* app.rc 中的图标资源 ID */
#define WM_TRAYICON     (WM_APP + 1)
#define TIMER_LOCK_DELAY 1       /* 延迟锁定的计时器 ID */

#define IDM_LOCK_ON_OFF 1001
#define IDM_LID_LOCK    1002
#define IDM_CHECK       1003
#define IDM_AUTOSTART   1004
#define IDM_ABOUT       1005
#define IDM_EXIT        1006
#define IDM_UPDATE_CHECK 1007

/* 延迟锁定子菜单：ID 连续，方便用 CheckMenuRadioItem 画单选圆点 */
#define IDM_DELAY_NOW   1010
#define IDM_DELAY_3     1011
#define IDM_DELAY_5     1012
#define IDM_DELAY_10    1013
#define IDM_DELAY_30    1014

#define REG_SUBKEY         L"Software\\ScreenOffLock"
#define REG_VALUE_SCREEN   L"LockWhenScreenOff"
#define REG_VALUE_LID      L"LockWhenLidClosed"
#define REG_VALUE_DELAY    L"LockDelay"
#define REG_VALUE_SKIPWARN L"SkipSettingsWarning"
#define REG_VALUE_SKIPVER  L"SkippedVersion"
#define REG_VALUE_LANG     L"Language"

/* 开机启动写在这里；这是系统原有的键，我们只增删自己那一个值 */
#define RUN_SUBKEY         L"Software\\Microsoft\\Windows\\CurrentVersion\\Run"
#define RUN_VALUE          L"ScreenOffLock"

/* ---------------------------------------------------------------------------
 * 显示器状态通知
 *
 * GUID_CONSOLE_DISPLAY_STATE 是系统在显示器电源状态变化时使用的电源设置
 * GUID。SDK 用 DEFINE_GUID 声明它，要取用该符号还得额外链接 uuid.lib 并定义
 * INITGUID，所以这里直接以独立符号名写出同一个 GUID，省掉这层链接依赖。
 *
 * 通知携带的数据是 DWORD：0 = 显示器关闭，1 = 显示器打开，2 = 显示器变暗。
 *
 * 下面的取值已与本机 w64devkit 自带的 winnt.h 定义逐字节核对一致。
 * ------------------------------------------------------------------------ */

/* {6FE69556-704A-47A0-8F24-C28D936FDA47} */
static const GUID kGuidConsoleDisplayState =
{
    0x6FE69556, 0x704A, 0x47A0,
    { 0x8F, 0x24, 0xC2, 0x8D, 0x93, 0x6F, 0xDA, 0x47 }
};

#define DISPLAY_STATE_OFF   0
#define DISPLAY_STATE_ON    1

/* ---------------------------------------------------------------------------
 * 盖子开关状态通知
 *
 * GUID_LIDSWITCH_STATE_CHANGE = {BA3E0F4D-B817-4094-A2D1-D56379E6A0F3}
 * 数据是 DWORD：0 = 盖子已合上，1 = 盖子已打开。
 *
 * 注意：系统确实找到盖子设备之后才会发出回调；而注册本身对任何 GUID 都会
 * “成功”（它不校验 GUID 是否真实存在），所以判断机器有没有盖子必须看
 * GetPwrCapabilities 的 LidPresent，不能看注册返回值。
 * ------------------------------------------------------------------------ */

/* {BA3E0F4D-B817-4094-A2D1-D56379E6A0F3} */
static const GUID kGuidLidSwitchState =
{
    0xBA3E0F4D, 0xB817, 0x4094,
    { 0xA2, 0xD1, 0xD5, 0x63, 0x79, 0xE6, 0xA0, 0xF3 }
};

#define LID_STATE_CLOSED    0
#define LID_STATE_OPEN      1

/* ---------------------------------------------------------------------------
 * 电源计划里的“电源按钮 / 合盖”动作
 *
 * 这两项在控制面板里能改，但属于“隐藏”的电源设置——powercfg /query 不会
 * 列出它们，只能按 GUID 通过 API 读写。取值就是电源计划里的索引。
 * ------------------------------------------------------------------------ */

#define POWER_ACTION_NOTHING      0     /* 不采取任何操作 */
#define POWER_ACTION_SLEEP        1     /* 睡眠 */
#define POWER_ACTION_HIBERNATE    2     /* 休眠 */
#define POWER_ACTION_SHUTDOWN     3     /* 关机 */
#define POWER_ACTION_OFF_DISPLAY  4     /* 关闭屏幕 —— 只有电源按钮有这个选项 */

/* {4F971E89-EEBD-4455-A8DE-9E59040E7347} 子组：电源按钮和盖子 */
static const GUID kGuidSubButtons =
{
    0x4F971E89, 0xEEBD, 0x4455,
    { 0xA8, 0xDE, 0x9E, 0x59, 0x04, 0x0E, 0x73, 0x47 }
};

/* {5CA83367-6E45-459F-A27B-476B1D01C936} 合盖操作（LIDACTION） */
static const GUID kGuidLidAction =
{
    0x5CA83367, 0x6E45, 0x459F,
    { 0xA2, 0x7B, 0x47, 0x6B, 0x1D, 0x01, 0xC9, 0x36 }
};

/* {7648EFA3-DD9C-4E3E-B566-50F929386280} 电源按钮操作（PBUTTONACTION） */
static const GUID kGuidPowerButtonAction =
{
    0x7648EFA3, 0xDD9C, 0x4E3E,
    { 0xB5, 0x66, 0x50, 0xF9, 0x29, 0x38, 0x62, 0x80 }
};

/*
 * 推荐值：
 *   插电时 —— 电源按钮负责关屏，合盖什么都不做，两者都交给本程序锁屏；
 *   电池时 —— 电源按钮关屏；合盖睡眠（更省电），而“不采取任何操作”
 *             在本程序的语义下效果等价，同样算合规。
 */
#define REC_BUTTON_AC   POWER_ACTION_OFF_DISPLAY
#define REC_LID_AC      POWER_ACTION_NOTHING
#define REC_BUTTON_DC   POWER_ACTION_OFF_DISPLAY
#define REC_LID_DC      POWER_ACTION_SLEEP

/* ---------------------------------------------------------------------------
 * 全局状态
 * ------------------------------------------------------------------------ */

static HINSTANCE    g_hInstance        = NULL;
static HWND         g_hWnd             = NULL;
static HICON        g_hIcon            = NULL;
static HPOWERNOTIFY g_hPowerNotify     = NULL;
static HPOWERNOTIFY g_hLidNotify       = NULL;
static UINT         g_uTaskbarCreated  = 0;

static BOOL         g_lockOnScreenOff  = TRUE;   /* 菜单勾选状态：关屏时锁屏 */
static BOOL         g_lockOnLidClose   = TRUE;   /* 菜单勾选状态：合盖时锁屏 */
static DWORD        g_lockDelay        = 0;      /* 延迟秒数，0 = 立即锁定 */
static BOOL         g_pendingScreenOff = FALSE;  /* 屏灭触发的待定锁定 */
static BOOL         g_pendingLidClose  = FALSE;  /* 合盖触发的待定锁定 */
static BOOL         g_lockTimerArmed   = FALSE;  /* 待定锁定的计时器是否在跑 */
static BOOL         g_hasLid           = FALSE;  /* 这台机器有没有盖子设备 */
static DWORD        g_lastDisplayState = DISPLAY_STATE_ON;
static BOOL         g_hasDisplayState  = FALSE;
static DWORD        g_lastLidState     = LID_STATE_OPEN;
static BOOL         g_hasLidState      = FALSE;

/* ---------------------------------------------------------------------------
 * 高 DPI 适配
 *
 * 进程如果不声明 DPI 感知，系统就把它当成 DPI 不感知，再把窗口、菜单和对话框
 * 整体做位图拉伸，于是在高 DPI 屏幕上文字和界面都会发虚。必须在创建任何窗口
 * 之前声明。
 *
 * DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 是 Windows 10 1703 才有的，
 * 为了让老系统也能跑，这里动态取函数地址，取不到就退回 SetProcessDPIAware
 * （Vista+，系统级 DPI 感知）。
 * ------------------------------------------------------------------------ */

#define DPI_AWARENESS_PER_MONITOR_V2 ((HANDLE)(LONG_PTR)-4)

typedef BOOL (WINAPI *PFN_SetProcessDpiAwarenessContext)(HANDLE);

static void EnableHighDpi(void)
{
    HMODULE hUser32 = GetModuleHandleW(L"user32.dll");

    if (hUser32 != NULL)
    {
        PFN_SetProcessDpiAwarenessContext pfn =
            (PFN_SetProcessDpiAwarenessContext)(void *)
                GetProcAddress(hUser32, "SetProcessDpiAwarenessContext");

        if (pfn != NULL && pfn(DPI_AWARENESS_PER_MONITOR_V2))
            return;
    }

    SetProcessDPIAware();
}

/* ---------------------------------------------------------------------------
 * 设置的读写（HKCU\Software\ScreenOffLock）
 * ------------------------------------------------------------------------ */

static DWORD LoadDwordSetting(const WCHAR *name, DWORD def)
{
    HKEY  hKey   = NULL;
    DWORD dwVal  = 0;
    DWORD cbVal  = sizeof(dwVal);
    DWORD dwType = 0;
    LONG  lr;

    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_SUBKEY, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return def;                     /* 没有记录过，用默认值 */

    lr = RegQueryValueExW(hKey, name, NULL, &dwType, (LPBYTE)&dwVal, &cbVal);
    RegCloseKey(hKey);

    if (lr != ERROR_SUCCESS || dwType != REG_DWORD || cbVal != sizeof(dwVal))
        return def;

    return dwVal;
}

static void SaveDwordSetting(const WCHAR *name, DWORD value)
{
    HKEY hKey = NULL;

    if (RegCreateKeyExW(HKEY_CURRENT_USER, REG_SUBKEY, 0, NULL,
                        REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL,
                        &hKey, NULL) != ERROR_SUCCESS)
        return;

    RegSetValueExW(hKey, name, 0, REG_DWORD, (const BYTE *)&value, sizeof(value));
    RegCloseKey(hKey);
}

/* 开关类设置就是取值 0 / 1 的 DWORD */
static BOOL LoadBoolSetting(const WCHAR *name, BOOL def)
{
    return LoadDwordSetting(name, def ? 1u : 0u) ? TRUE : FALSE;
}

static void SaveBoolSetting(const WCHAR *name, BOOL value)
{
    SaveDwordSetting(name, value ? 1u : 0u);
}

/* ---------------------------------------------------------------------------
 * 延迟锁定
 *
 * 菜单里只有下面这几档，表里的顺序就是菜单里的顺序；注册表要是被人手工改成
 * 别的值，就退回“立即”，免得子菜单里出现一项都没被选中的状态。
 *
 * 档位、命令 ID 和文案全在这一张表里，以后加档位只改这一处。
 * ------------------------------------------------------------------------ */

typedef struct
{
    DWORD seconds;          /* 0 = 立即 */
    UINT  commandId;
    int   strId;            /* S_DELAY_* */
} DelayItem;

static const DelayItem kDelayItems[] =
{
    { 0,  IDM_DELAY_NOW, S_DELAY_NOW },
    { 3,  IDM_DELAY_3,   S_DELAY_3   },
    { 5,  IDM_DELAY_5,   S_DELAY_5   },
    { 10, IDM_DELAY_10,  S_DELAY_10  },
    { 30, IDM_DELAY_30,  S_DELAY_30  }
};

#define DELAY_ITEM_COUNT ((int)(sizeof(kDelayItems) / sizeof(kDelayItems[0])))

static BOOL IsValidLockDelay(DWORD seconds)
{
    int i;

    for (i = 0; i < DELAY_ITEM_COUNT; ++i)
    {
        if (kDelayItems[i].seconds == seconds)
            return TRUE;
    }

    return FALSE;
}

/* 档位 -> 命令 ID（弹菜单时决定哪一项画单选圆点） */
static UINT DelayCommandForSeconds(DWORD seconds)
{
    int i;

    for (i = 0; i < DELAY_ITEM_COUNT; ++i)
    {
        if (kDelayItems[i].seconds == seconds)
            return kDelayItems[i].commandId;
    }

    return IDM_DELAY_NOW;
}

/* 命令 ID -> 档位（点了菜单之后换算成秒数） */
static DWORD DelaySecondsForCommand(UINT id)
{
    int i;

    for (i = 0; i < DELAY_ITEM_COUNT; ++i)
    {
        if (kDelayItems[i].commandId == id)
            return kDelayItems[i].seconds;
    }

    return 0;
}

/* ---------------------------------------------------------------------------
 * 界面语言
 *
 * 存成 DWORD；值不存在就说明是第一次运行，要先让用户选一次。
 * ------------------------------------------------------------------------ */

static BOOL LoadLanguage(LangId *out)
{
    HKEY  hKey   = NULL;
    DWORD dwVal  = 0;
    DWORD cbVal  = sizeof(dwVal);
    DWORD dwType = 0;
    LONG  lr;

    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_SUBKEY, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return FALSE;

    lr = RegQueryValueExW(hKey, REG_VALUE_LANG, NULL, &dwType, (LPBYTE)&dwVal, &cbVal);
    RegCloseKey(hKey);

    if (lr != ERROR_SUCCESS || dwType != REG_DWORD || cbVal != sizeof(dwVal))
        return FALSE;

    *out = (dwVal < (DWORD)LANG_COUNT) ? (LangId)dwVal : LANG_ZH;
    return TRUE;
}

static void SaveLanguage(LangId lang)
{
    HKEY  hKey  = NULL;
    DWORD dwVal = (DWORD)lang;

    if (RegCreateKeyExW(HKEY_CURRENT_USER, REG_SUBKEY, 0, NULL,
                        REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL,
                        &hKey, NULL) != ERROR_SUCCESS)
        return;

    RegSetValueExW(hKey, REG_VALUE_LANG, 0, REG_DWORD, (const BYTE *)&dwVal, sizeof(dwVal));
    RegCloseKey(hKey);
}

/* ---------------------------------------------------------------------------
 * 开机启动（注册表方式）
 *
 * 值写在 HKCU\...\CurrentVersion\Run 下。关闭时直接把这个值删掉，不留任何
 * 残留；Run 键本身是系统原有的，我们既不创建也不删除它。
 * ------------------------------------------------------------------------ */

static BOOL IsAutoStartEnabled(void)
{
    HKEY  hKey = NULL;
    BOOL  enabled = FALSE;
    DWORD type = 0;
    DWORD cb;
    WCHAR buf[MAX_PATH + 8];

    if (RegOpenKeyExW(HKEY_CURRENT_USER, RUN_SUBKEY, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return FALSE;

    cb = sizeof(buf);
    if (RegQueryValueExW(hKey, RUN_VALUE, NULL, &type, (LPBYTE)buf, &cb) == ERROR_SUCCESS
        && type == REG_SZ)
    {
        enabled = TRUE;
    }

    RegCloseKey(hKey);
    return enabled;
}

static BOOL SetAutoStart(BOOL enable)
{
    HKEY hKey = NULL;

    if (!enable)
    {
        /* 关闭：删掉这个值。键不存在、值本来就没有，同样算成功——总之没有残留。 */
        if (RegOpenKeyExW(HKEY_CURRENT_USER, RUN_SUBKEY, 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS)
        {
            RegDeleteValueW(hKey, RUN_VALUE);
            RegCloseKey(hKey);
        }

        return IsAutoStartEnabled() ? FALSE : TRUE;
    }

    if (RegCreateKeyExW(HKEY_CURRENT_USER, RUN_SUBKEY, 0, NULL,
                        REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, NULL,
                        &hKey, NULL) != ERROR_SUCCESS)
        return FALSE;

    {
        WCHAR exePath[MAX_PATH];
        WCHAR quoted[MAX_PATH + 4];
        DWORD cb;

        if (GetModuleFileNameW(NULL, exePath, MAX_PATH) == 0)
        {
            RegCloseKey(hKey);
            return FALSE;
        }

        /* 路径里可能有空格，按惯例整个用引号包起来 */
        wsprintfW(quoted, L"\"%s\"", exePath);
        cb = (DWORD)((lstrlenW(quoted) + 1) * sizeof(WCHAR));

        if (RegSetValueExW(hKey, RUN_VALUE, 0, REG_SZ,
                           (const BYTE *)quoted, cb) != ERROR_SUCCESS)
        {
            RegCloseKey(hKey);
            return FALSE;
        }
    }

    RegCloseKey(hKey);
    return TRUE;
}

/* ---------------------------------------------------------------------------
 * 电源计划里的“电源按钮 / 合盖”动作
 *
 * 读取不需要管理员权限，写入需要——所以写入只在提权后的子进程里调用。
 * SchemeGuid 必须传具体的活动方案：传 NULL 会返回 ERROR_INVALID_PARAMETER。
 * ------------------------------------------------------------------------ */

typedef struct
{
    BOOL  valid;        /* 四项是否全部读到 */
    DWORD buttonAc;     /* 插电：电源按钮 */
    DWORD lidAc;        /* 插电：合盖 */
    DWORD buttonDc;     /* 电池：电源按钮 */
    DWORD lidDc;        /* 电池：合盖 */
} PowerSettings;

static BOOL ReadPowerSettings(PowerSettings *ps)
{
    GUID *scheme = NULL;
    DWORD idx;
    BOOL  ok = TRUE;

    ZeroMemory(ps, sizeof(*ps));

    if (PowerGetActiveScheme(NULL, &scheme) != ERROR_SUCCESS || scheme == NULL)
        return FALSE;

    idx = 0;
    if (PowerReadACValueIndex(NULL, scheme, &kGuidSubButtons, &kGuidPowerButtonAction, &idx) == ERROR_SUCCESS)
        ps->buttonAc = idx;
    else
        ok = FALSE;

    idx = 0;
    if (PowerReadACValueIndex(NULL, scheme, &kGuidSubButtons, &kGuidLidAction, &idx) == ERROR_SUCCESS)
        ps->lidAc = idx;
    else
        ok = FALSE;

    idx = 0;
    if (PowerReadDCValueIndex(NULL, scheme, &kGuidSubButtons, &kGuidPowerButtonAction, &idx) == ERROR_SUCCESS)
        ps->buttonDc = idx;
    else
        ok = FALSE;

    idx = 0;
    if (PowerReadDCValueIndex(NULL, scheme, &kGuidSubButtons, &kGuidLidAction, &idx) == ERROR_SUCCESS)
        ps->lidDc = idx;
    else
        ok = FALSE;

    LocalFree(scheme);

    ps->valid = ok;
    return ok;
}

static BOOL WriteRecommendedSettings(BOOL ac)
{
    GUID *scheme = NULL;
    BOOL  ok;

    if (PowerGetActiveScheme(NULL, &scheme) != ERROR_SUCCESS || scheme == NULL)
        return FALSE;

    if (ac)
    {
        ok  = (PowerWriteACValueIndex(NULL, scheme, &kGuidSubButtons,
                                      &kGuidPowerButtonAction, REC_BUTTON_AC) == ERROR_SUCCESS);
        ok &= (PowerWriteACValueIndex(NULL, scheme, &kGuidSubButtons,
                                      &kGuidLidAction, REC_LID_AC) == ERROR_SUCCESS);
    }
    else
    {
        ok  = (PowerWriteDCValueIndex(NULL, scheme, &kGuidSubButtons,
                                      &kGuidPowerButtonAction, REC_BUTTON_DC) == ERROR_SUCCESS);
        ok &= (PowerWriteDCValueIndex(NULL, scheme, &kGuidSubButtons,
                                      &kGuidLidAction, REC_LID_DC) == ERROR_SUCCESS);
    }

    if (ok)
        PowerSetActiveScheme(NULL, scheme);     /* 让改动立即生效 */

    LocalFree(scheme);
    return ok;
}

/* 动作索引 -> 界面上的说法 */
static const WCHAR *ActionName(DWORD action)
{
    switch (action)
    {
    case POWER_ACTION_NOTHING:      return T(S_ACTION_NOTHING);
    case POWER_ACTION_SLEEP:        return T(S_ACTION_SLEEP);
    case POWER_ACTION_HIBERNATE:    return T(S_ACTION_HIBERNATE);
    case POWER_ACTION_SHUTDOWN:     return T(S_ACTION_SHUTDOWN);
    case POWER_ACTION_OFF_DISPLAY:  return T(S_ACTION_OFFDISPLAY);
    default:                        return T(S_ACTION_UNKNOWN);
    }
}

/*
 * 合盖动作的说法：值为“不采取任何操作”时补上效果说明，与推荐值列的写法保持
 * 一致——合盖不做动作时屏幕本来就会被关掉，这样左右一对比就知道是否合规。
 */
static const WCHAR *LidActionName(DWORD action)
{
    if (action == POWER_ACTION_NOTHING)
        return T(S_ACTION_LID_NOTHING);

    return ActionName(action);
}

static BOOL AcButtonOk(const PowerSettings *ps) { return ps->buttonAc == REC_BUTTON_AC; }
static BOOL AcLidOk(const PowerSettings *ps)    { return ps->lidAc    == REC_LID_AC; }
static BOOL DcButtonOk(const PowerSettings *ps) { return ps->buttonDc == REC_BUTTON_DC; }

/* 电池时合盖若是“不采取任何操作”，在本程序的语义下与推荐值效果一致 */
static BOOL DcLidOk(const PowerSettings *ps)
{
    return (ps->lidDc == REC_LID_DC || ps->lidDc == POWER_ACTION_NOTHING) ? TRUE : FALSE;
}

/* 按合规情况给出那一行结论 */
static const WCHAR *VerdictText(BOOL buttonOk, BOOL lidOk)
{
    if (!buttonOk && !lidOk)
        return T(S_VERDICT_BOTH);

    if (!lidOk)
        return T(S_VERDICT_LID);

    if (!buttonOk)
        return T(S_VERDICT_BUTTON);

    return T(S_VERDICT_OK);
}

/* ---------------------------------------------------------------------------
 * 托盘图标
 * ------------------------------------------------------------------------ */

static void AddTrayIcon(void)
{
    NOTIFYICONDATAW nid;

    ZeroMemory(&nid, sizeof(nid));
    nid.cbSize           = sizeof(nid);
    nid.hWnd             = g_hWnd;
    nid.uID              = TRAY_ICON_ID;
    nid.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon            = g_hIcon;
    lstrcpynW(nid.szTip, APP_NAME, ARRAYSIZE(nid.szTip));

    Shell_NotifyIconW(NIM_ADD, &nid);
}

static void RemoveTrayIcon(void)
{
    NOTIFYICONDATAW nid;

    ZeroMemory(&nid, sizeof(nid));
    nid.cbSize = sizeof(nid);
    nid.hWnd   = g_hWnd;
    nid.uID    = TRAY_ICON_ID;

    Shell_NotifyIconW(NIM_DELETE, &nid);
}

/* ---------------------------------------------------------------------------
 * 关于窗口
 *
 * 这里刻意不用 MessageBox：它的图标只能是 IDI_* 那套老式位图，窗口外观也停留
 * 在经典控件样式。改成自己画一个 Win11 风格的窗口——DWM 圆角、Segoe UI
 * Variable 字体、图标取自 Segoe Fluent Icons 字形（U+E946 ⓘ）。
 * ------------------------------------------------------------------------ */

#define ABOUT_CLASS     L"ScreenOffLock_AboutWindow"
#define IDC_ABOUT_OK    2001
#define IDC_ABOUT_LANG  2002
#define IDC_ABOUT_UPDATE 2003
#define GLYPH_INFO      0xE946      /* Segoe Fluent Icons: ⓘ */

#define ABOUT_CX        640         /* 客户区逻辑宽度：要放得下 Run 键那行长路径 */
#define ABOUT_CY        340

static HWND  g_hAboutWnd    = NULL;
static BOOL  g_aboutLinkHot = FALSE;    /* 鼠标是否悬在“项目地址”链接上 */
static BOOL  g_aboutLicenseHot = FALSE; /* 鼠标是否悬在“许可证”链接上 */
static UINT  g_uiDpi        = 96;       /* 全局 UI DPI，所有自绘对话框共用 */
static HFONT g_fontTitle = NULL;
static HFONT g_fontBody  = NULL;
static HFONT g_fontSmall = NULL;
static HFONT g_fontGlyph = NULL;
static HFONT g_fontGlyphSm = NULL;      /* 小号状态图标（对勾 / 感叹号） */

/* 下面这些在文件靠后定义，但前面的窗口过程要先引用，这里提前声明 */
static int  TextWidth(HDC hdc, HFONT font, const WCHAR *text);
static int  InfoPanelHeightLog(HDC hdc, const WCHAR *text, int cxLog);
static int  DrawInfoPanel(HDC hdc, const WCHAR *text, int x, int y, int cx);
static void DrawInfoPanelFrame(HDC hdc, int x, int y, int cx, int cy);
static void ShowLanguageWindow(BOOL modal);
static void ShowUpdateCheckWindow(void);
static HWND g_hCheckWnd;

/* 逻辑像素（96 DPI 基准）-> 实际像素 */
static int AS(int v) { return MulDiv(v, (int)g_uiDpi, 96); }

/* --- 当前 DPI ----------------------------------------------------------- */

typedef UINT (WINAPI *PFN_GetDpiForWindow)(HWND);

static UINT QueryDpi(HWND hWnd)
{
    HMODULE hUser32 = GetModuleHandleW(L"user32.dll");

    if (hUser32 != NULL && hWnd != NULL)
    {
        PFN_GetDpiForWindow pfn =
            (PFN_GetDpiForWindow)(void *)GetProcAddress(hUser32, "GetDpiForWindow");

        if (pfn != NULL)
        {
            UINT dpi = pfn(hWnd);
            if (dpi != 0)
                return dpi;
        }
    }

    HDC  hdc = GetDC(NULL);
    UINT dpi = (UINT)GetDeviceCaps(hdc, LOGPIXELSY);
    ReleaseDC(NULL, hdc);

    return dpi ? dpi : 96;
}

/* --- 字体 --------------------------------------------------------------- */

/* 指定的字体不存在时 CreateFontIndirect 会悄悄返回一个替代字体，
   把真实字体名取回来比对，就能判断它到底在不在。 */
static BOOL FontExists(const WCHAR *face)
{
    HDC      hdc = GetDC(NULL);
    WCHAR    actual[LF_FACESIZE] = L"";
    LOGFONTW lf;
    BOOL     found = FALSE;

    ZeroMemory(&lf, sizeof(lf));
    lstrcpynW(lf.lfFaceName, face, LF_FACESIZE);

    HFONT hf = CreateFontIndirectW(&lf);
    if (hf != NULL)
    {
        HFONT hOld = (HFONT)SelectObject(hdc, hf);
        GetTextFaceW(hdc, LF_FACESIZE, actual);
        SelectObject(hdc, hOld);
        DeleteObject(hf);
        found = (lstrcmpiW(actual, face) == 0);
    }

    ReleaseDC(NULL, hdc);
    return found;
}

/* 界面字体：Windows 11 是 Segoe UI Variable Text，老系统退回 Segoe UI */
static const WCHAR *UiFontFace(void)
{
    static WCHAR face[LF_FACESIZE] = L"";
    static BOOL  resolved = FALSE;

    if (!resolved)
    {
        resolved = TRUE;

        if (FontExists(L"Segoe UI Variable Text"))
            lstrcpynW(face, L"Segoe UI Variable Text", LF_FACESIZE);
        else
            lstrcpynW(face, L"Segoe UI", LF_FACESIZE);
    }

    return face;
}

/*
 * 图标字形所用的字体：
 *   Windows 11   Segoe Fluent Icons
 *   Windows 10   Segoe MDL2 Assets —— 码点与 Fluent 兼容，外观几乎一致
 *   Windows 7/8  两者都没有，返回空串，绘制时改用系统自带的位图图标
 */
static const WCHAR *GlyphFontFace(void)
{
    static WCHAR face[LF_FACESIZE] = L"";
    static BOOL  resolved = FALSE;

    if (!resolved)
    {
        resolved = TRUE;

        if (FontExists(L"Segoe Fluent Icons"))
            lstrcpynW(face, L"Segoe Fluent Icons", LF_FACESIZE);
        else if (FontExists(L"Segoe MDL2 Assets"))
            lstrcpynW(face, L"Segoe MDL2 Assets", LF_FACESIZE);
    }

    return face;
}

/* 字体全局只创建一次，所有自绘对话框共用；进程退出时由系统回收 */
static void CreateAboutFonts(void)
{
    if (g_fontTitle != NULL)
        return;

    g_fontTitle = CreateFontW(-AS(20), 0, 0, 0, 600 /* semibold */, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                              UiFontFace());

    g_fontBody  = CreateFontW(-AS(14), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                              UiFontFace());

    g_fontSmall = CreateFontW(-AS(13), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                              UiFontFace());

    {
        const WCHAR *glyphFace = GlyphFontFace();

        if (glyphFace[0] != L'\0')
        {
            g_fontGlyph = CreateFontW(-AS(30), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                      DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                                      CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                                      glyphFace);

            g_fontGlyphSm = CreateFontW(-AS(16), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                        DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                                        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                                        glyphFace);
        }
        else
        {
            g_fontGlyph   = NULL;   /* 没有图标字体，关于窗口改用系统位图图标 */
            g_fontGlyphSm = NULL;   /* 状态图标则直接不画 */
        }
    }
}

/* --- 绘制 --------------------------------------------------------------- */

static void AboutText(HDC hdc, HFONT font, COLORREF color,
                      const WCHAR *text, int x, int y, int cx)
{
    HFONT hOld = (HFONT)SelectObject(hdc, font);
    RECT  r;

    SetTextColor(hdc, color);
    SetBkMode(hdc, TRANSPARENT);

    r.left   = AS(x);
    r.top    = AS(y);
    r.right  = AS(x + cx);
    r.bottom = AS(y + 40);

    /* DT_NOPREFIX 是必需的：应用名里带 &，否则会被当成助记符前缀吃掉 */
    DrawTextW(hdc, text, -1, &r, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);

    SelectObject(hdc, hOld);
}

/* 「关于」窗口里各行的纵向位置（逻辑像素） */
#define ABOUT_TAGLINE_Y  100            /* 一句话说明 */
#define ABOUT_AUTHOR_Y   136            /* 作者（和上面那行拉开一点） */
#define ABOUT_PROJECT_Y  159            /* 项目地址 */
#define ABOUT_LICENSE_Y  182            /* 许可证（这三行彼此收紧一点） */

/*
 * 「关于」里“标签 + 可点击链接”那一行的链接位置（实际像素），
 * 绘制和命中测试共用同一套算法，免得两边对不上。
 */
static void AboutLinkRectAt(HWND hWnd, const WCHAR *prefix, const WCHAR *link,
                            int yLogic, RECT *out)
{
    HDC    hdc     = GetDC(hWnd);
    HFONT  hOld    = (HFONT)SelectObject(hdc, g_fontBody);
    SIZE   szLabel = { 0, 0 };
    SIZE   szLink  = { 0, 0 };

    GetTextExtentPoint32W(hdc, prefix, lstrlenW(prefix), &szLabel);
    GetTextExtentPoint32W(hdc, link,   lstrlenW(link),   &szLink);

    SelectObject(hdc, hOld);
    ReleaseDC(hWnd, hdc);

    out->left   = AS(28) + szLabel.cx;
    out->top    = AS(yLogic - 6);
    out->right  = out->left + szLink.cx;
    out->bottom = AS(yLogic + 24);
}

/* 项目地址那一行的链接位置 */
static void AboutLinkRect(HWND hWnd, RECT *out)
{
    AboutLinkRectAt(hWnd, T(S_PROJECT_LABEL), APP_URL, ABOUT_PROJECT_Y, out);
}

/* 许可证那一行的链接位置 */
static void AboutLicenseRect(HWND hWnd, RECT *out)
{
    AboutLinkRectAt(hWnd, T(S_LICENSE_PREFIX), T(S_LICENSE_LINK), ABOUT_LICENSE_Y, out);
}

/* 「关于」里那两行注册表路径：写全称 */
#define REG_PATH_APP  L"HKEY_CURRENT_USER\\Software\\ScreenOffLock"
#define REG_PATH_RUN  L"HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run → ScreenOffLock"

#define ABOUT_PANEL_Y    210            /* 注册表块的顶边（逻辑像素） */
#define ABOUT_REG_LINE_H 20             /* 块里每行的高度 */
#define ABOUT_REG_LINES  4              /* 四行：说明 + 两条路径 + 结尾 */
#define ABOUT_REG_PANEL_H (ABOUT_REG_LINES * ABOUT_REG_LINE_H + 24)  /* 上下各留 12 */

/* 「关于」窗口客户区要多高：注册表块的底 + 间距 + 按钮 + 下边距 */
static int AboutClientHeight(void)
{
    return ABOUT_PANEL_Y + ABOUT_REG_PANEL_H + 34 + 32 + 20;
}

/* 画一行“标签 + 可点击链接”：链接带下划线，悬停时颜色加深 */
static void AboutDrawLinkLine(HDC hdc, const WCHAR *prefix, const WCHAR *link,
                              int y, BOOL hot, const RECT *linkRect)
{
    HFONT    hOld;
    HPEN     hPen, hOldPen;
    RECT     r;
    COLORREF clrLink = hot ? RGB(0x00, 0x4E, 0x99) : RGB(0x00, 0x5F, 0xB8);

    AboutText(hdc, g_fontBody, GetSysColor(COLOR_WINDOWTEXT), prefix, 28, y, 300);

    hOld = (HFONT)SelectObject(hdc, g_fontBody);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, clrLink);

    r.left   = linkRect->left;
    r.top    = AS(y);
    r.right  = linkRect->right + AS(4);
    r.bottom = r.top + AS(26);

    DrawTextW(hdc, link, -1, &r, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);

    /* 链接下划线 */
    hPen    = CreatePen(PS_SOLID, 1, clrLink);
    hOldPen = (HPEN)SelectObject(hdc, hPen);
    MoveToEx(hdc, linkRect->left, AS(y) + AS(19), NULL);
    LineTo(hdc, linkRect->right, AS(y) + AS(19));
    SelectObject(hdc, hOldPen);
    DeleteObject(hPen);

    SelectObject(hdc, hOld);
}

static void AboutPaint(HWND hWnd)
{
    PAINTSTRUCT ps;
    HDC         hdc = BeginPaint(hWnd, &ps);
    RECT        rc;
    COLORREF    clrText   = GetSysColor(COLOR_WINDOWTEXT);
    COLORREF    clrMuted  = RGB(0x60, 0x5E, 0x5C);
    COLORREF    clrAccent = RGB(0x0F, 0x6C, 0xBD);
    COLORREF    clrPath   = RGB(0x8A, 0x88, 0x86);   /* 两行注册表路径：比正文淡 */
    HPEN        hPen, hOldPen;
    WCHAR       buf[256];

    GetClientRect(hWnd, &rc);
    FillRect(hdc, &rc, GetSysColorBrush(COLOR_WINDOW));

    SetBkMode(hdc, TRANSPARENT);

    /* 图标：优先用系统图标字体的字形；老系统没有该字体，退回位图图标 */
    if (g_fontGlyph != NULL)
    {
        HFONT hOld = (HFONT)SelectObject(hdc, g_fontGlyph);
        RECT  r;

        SetTextColor(hdc, clrAccent);

        r.left   = AS(28);
        r.top    = AS(22);
        r.right  = AS(28 + 44);
        r.bottom = AS(22 + 44);

        DrawTextW(hdc, L"\xE946", -1, &r,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        SelectObject(hdc, hOld);
    }
    else
    {
        HICON hInfo = LoadIconW(NULL, IDI_INFORMATION);

        if (hInfo != NULL)
            DrawIconEx(hdc, AS(29), AS(23), hInfo, AS(42), AS(42), 0, NULL, DI_NORMAL);
    }

    /* 标题与版本 */
    AboutText(hdc, g_fontTitle, clrText, APP_NAME, 80, 26, ABOUT_CX - 104);

    wsprintfW(buf, T(S_VERSION_FMT), APP_VERSION);
    AboutText(hdc, g_fontSmall, clrMuted, buf, 81, 52, 200);

    /* 分隔线 */
    hPen    = CreatePen(PS_SOLID, 1, RGB(0xE5, 0xE5, 0xE5));
    hOldPen = (HPEN)SelectObject(hdc, hPen);
    MoveToEx(hdc, AS(28), AS(84), NULL);
    LineTo(hdc, AS(ABOUT_CX - 28), AS(84));
    SelectObject(hdc, hOldPen);
    DeleteObject(hPen);

    /* 正文：先一句话说明，再作者、项目地址、许可证 */
    AboutText(hdc, g_fontBody, clrText,
              T(S_TAGLINE),
              28, ABOUT_TAGLINE_Y, ABOUT_CX - 56);

    wsprintfW(buf, T(S_AUTHOR_FMT), APP_AUTHOR);
    AboutText(hdc, g_fontBody, clrText, buf, 28, ABOUT_AUTHOR_Y, ABOUT_CX - 56);

    {
        RECT lr;

        AboutLinkRect(hWnd, &lr);
        AboutDrawLinkLine(hdc, T(S_PROJECT_LABEL), APP_URL,
                          ABOUT_PROJECT_Y, g_aboutLinkHot, &lr);
    }

    {
        RECT lr;

        AboutLicenseRect(hWnd, &lr);
        AboutDrawLinkLine(hdc, T(S_LICENSE_PREFIX), T(S_LICENSE_LINK),
                          ABOUT_LICENSE_Y, g_aboutLicenseHot, &lr);
    }

    /*
     * 注册表提示整块放进淡灰衬底里，样式和「电源设置」那个块一致。
     * 四行分开画：中间两行路径是给人照着去用的，颜色压淡一点。
     */
    {
        int y = ABOUT_PANEL_Y + 12;

        DrawInfoPanelFrame(hdc, 28, ABOUT_PANEL_Y, ABOUT_CX - 56, ABOUT_REG_PANEL_H);

        AboutText(hdc, g_fontSmall, clrText, T(S_REG_INTRO), 42, y, ABOUT_CX - 84);
        AboutText(hdc, g_fontSmall, clrPath, REG_PATH_APP,   42, y + ABOUT_REG_LINE_H,     ABOUT_CX - 84);
        AboutText(hdc, g_fontSmall, clrPath, REG_PATH_RUN,   42, y + ABOUT_REG_LINE_H * 2, ABOUT_CX - 84);
        AboutText(hdc, g_fontSmall, clrText, T(S_REG_OUTRO), 42, y + ABOUT_REG_LINE_H * 3, ABOUT_CX - 84);
    }

    EndPaint(hWnd, &ps);
}

/* --- 窗口 --------------------------------------------------------------- */

static void CreateAboutButton(HWND hWnd)
{
    RECT rc;
    HWND hBtn;
    int  bh = AS(32), gap = AS(10), w, x, y;

    GetClientRect(hWnd, &rc);
    y = rc.bottom - AS(20) - bh;

    /* 「确定」在最右 */
    w = AS(96);
    x = rc.right - AS(24) - w;

    hBtn = CreateWindowExW(0, L"BUTTON", T(S_BTN_OK),
                           WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                           x, y, w, bh,
                           hWnd, (HMENU)(INT_PTR)IDC_ABOUT_OK, g_hInstance, NULL);

    if (hBtn != NULL)
    {
        SendMessageW(hBtn, WM_SETFONT, (WPARAM)g_fontBody, TRUE);
        SetFocus(hBtn);
    }

    /* 语言按钮摆在它左边 */
    {
        HDC hdc = GetDC(hWnd);

        w = TextWidth(hdc, g_fontBody, T(S_LANGUAGE_BTN)) + AS(30);
        ReleaseDC(hWnd, hdc);
    }

    x -= (gap + w);

    hBtn = CreateWindowExW(0, L"BUTTON", T(S_LANGUAGE_BTN),
                           WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                           x, y, w, bh,
                           hWnd, (HMENU)(INT_PTR)IDC_ABOUT_LANG, g_hInstance, NULL);

    if (hBtn != NULL)
        SendMessageW(hBtn, WM_SETFONT, (WPARAM)g_fontBody, TRUE);

    /* 「检查更新」摆到最左边 */
    {
        HDC hdc = GetDC(hWnd);

        w = TextWidth(hdc, g_fontBody, T(S_BTN_CHECK_UPDATE)) + AS(30);
        ReleaseDC(hWnd, hdc);
    }

    x -= (gap + w);

    hBtn = CreateWindowExW(0, L"BUTTON", T(S_BTN_CHECK_UPDATE),
                           WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                           x, y, w, bh,
                           hWnd, (HMENU)(INT_PTR)IDC_ABOUT_UPDATE, g_hInstance, NULL);

    if (hBtn != NULL)
        SendMessageW(hBtn, WM_SETFONT, (WPARAM)g_fontBody, TRUE);
}

/* Win11 的窗口圆角由 DWM 负责，这里显式要求圆角 */
static void ApplyRoundCorners(HWND hWnd)
{
    typedef HRESULT (WINAPI *PFN_DwmSetWindowAttribute)(HWND, DWORD, LPCVOID, DWORD);
    HMODULE hDwm = LoadLibraryW(L"dwmapi.dll");
    PFN_DwmSetWindowAttribute pfn;

    if (hDwm == NULL)
        return;

    pfn = (PFN_DwmSetWindowAttribute)(void *)GetProcAddress(hDwm, "DwmSetWindowAttribute");

    if (pfn != NULL)
    {
        DWORD pref = 2;             /* DWMWCP_ROUND */
        pfn(hWnd, 33 /* DWMWA_WINDOW_CORNER_PREFERENCE */, &pref, sizeof(pref));
    }
}

static LRESULT CALLBACK AboutWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
        CreateAboutFonts();
        CreateAboutButton(hWnd);
        ApplyRoundCorners(hWnd);
        return 0;

    case WM_ERASEBKGND:
        return 1;                   /* 背景统一在 WM_PAINT 里画，避免闪烁 */

    case WM_PAINT:
        AboutPaint(hWnd);
        return 0;

    /* 两条链接都是可点的：悬停变手型、颜色加深 */
    case WM_MOUSEMOVE:
        {
            RECT  lr, lc;
            POINT pt;
            BOOL  hotLink, hotLic;

            pt.x = (int)(short)LOWORD(lParam);
            pt.y = (int)(short)HIWORD(lParam);

            AboutLinkRect(hWnd, &lr);
            AboutLicenseRect(hWnd, &lc);

            hotLink = PtInRect(&lr, pt);
            hotLic  = PtInRect(&lc, pt);

            if (hotLink != g_aboutLinkHot)
            {
                g_aboutLinkHot = hotLink;
                InvalidateRect(hWnd, &lr, FALSE);
            }

            if (hotLic != g_aboutLicenseHot)
            {
                g_aboutLicenseHot = hotLic;
                InvalidateRect(hWnd, &lc, FALSE);
            }

            SetCursor(LoadCursorW(NULL, (hotLink || hotLic) ? IDC_HAND : IDC_ARROW));
        }
        return 0;

    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT && (g_aboutLinkHot || g_aboutLicenseHot))
        {
            SetCursor(LoadCursorW(NULL, IDC_HAND));
            return TRUE;
        }
        break;

    case WM_LBUTTONDOWN:
        {
            RECT  lr, lc;
            POINT pt;

            pt.x = (int)(short)LOWORD(lParam);
            pt.y = (int)(short)HIWORD(lParam);

            AboutLinkRect(hWnd, &lr);
            AboutLicenseRect(hWnd, &lc);

            if (PtInRect(&lr, pt))
                ShellExecuteW(NULL, L"open", APP_URL, NULL, NULL, SW_SHOWNORMAL);
            else if (PtInRect(&lc, pt))
                ShellExecuteW(NULL, L"open", APP_LICENSE_URL, NULL, NULL, SW_SHOWNORMAL);
        }
        return 0;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_ABOUT_OK)
            DestroyWindow(hWnd);
        else if (LOWORD(wParam) == IDC_ABOUT_LANG)
            ShowLanguageWindow(FALSE);
        else if (LOWORD(wParam) == IDC_ABOUT_UPDATE)
            ShowUpdateCheckWindow();
        return 0;

    case WM_CLOSE:
        DestroyWindow(hWnd);
        return 0;

    case WM_DESTROY:
        g_hAboutWnd        = NULL;
        g_aboutLinkHot     = FALSE;
        g_aboutLicenseHot  = FALSE;
        return 0;

    default:
        break;
    }

    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

static void ShowAboutWindow(void)
{
    WNDCLASSEXW wc;
    RECT        rc, wa;
    int         x, y, w, h;

    if (g_hAboutWnd != NULL)        /* 已经开着就置前 */
    {
        SetForegroundWindow(g_hAboutWnd);
        return;
    }

    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = AboutWndProc;
    wc.hInstance     = g_hInstance;
    wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
    wc.lpszClassName = ABOUT_CLASS;

    /* 标题栏图标：不指定的话系统会塞一个默认占位图标 */
    wc.hIcon   = (HICON)LoadImageW(g_hInstance, MAKEINTRESOURCEW(IDI_APP_ICON),
                                   IMAGE_ICON,
                                   GetSystemMetrics(SM_CXICON),
                                   GetSystemMetrics(SM_CYICON),
                                   LR_DEFAULTCOLOR);
    wc.hIconSm = (HICON)LoadImageW(g_hInstance, MAKEINTRESOURCEW(IDI_APP_ICON),
                                   IMAGE_ICON,
                                   GetSystemMetrics(SM_CXSMICON),
                                   GetSystemMetrics(SM_CYSMICON),
                                   LR_DEFAULTCOLOR);

    RegisterClassExW(&wc);

    g_uiDpi = QueryDpi(g_hWnd);

    rc.left   = 0;
    rc.top    = 0;
    rc.right  = AS(ABOUT_CX);
    rc.bottom = AS(AboutClientHeight());
    AdjustWindowRectEx(&rc, WS_CAPTION | WS_SYSMENU | WS_POPUP, FALSE, 0);

    w = rc.right - rc.left;
    h = rc.bottom - rc.top;

    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    x = wa.left + ((wa.right - wa.left) - w) / 2;
    y = wa.top + ((wa.bottom - wa.top) - h) / 2;

    g_hAboutWnd = CreateWindowExW(0, ABOUT_CLASS, T(S_MENU_ABOUT),
                                  WS_CAPTION | WS_SYSMENU | WS_POPUP,
                                  x, y, w, h,
                                  g_hWnd, NULL, g_hInstance, NULL);

    if (g_hAboutWnd == NULL)
        return;

    ShowWindow(g_hAboutWnd, SW_SHOW);
    UpdateWindow(g_hAboutWnd);
    SetForegroundWindow(g_hAboutWnd);
}

/* ---------------------------------------------------------------------------
 * 对话框公共辅助
 * ------------------------------------------------------------------------ */

static int TextWidth(HDC hdc, HFONT font, const WCHAR *text)
{
    HFONT hOld = (HFONT)SelectObject(hdc, font);
    SIZE  sz = { 0, 0 };

    GetTextExtentPoint32W(hdc, text, lstrlenW(text), &sz);
    SelectObject(hdc, hOld);
    return sz.cx;
}

/*
 * 行首状态图标：正常是绿色对勾圆圈（U+E930），异常是橙色感叹号圆圈（U+E783）。
 * 老系统没有图标字体时直接不画，文字照常显示。
 */
static void DrawStatusIcon(HDC hdc, int x, int y, int size, BOOL ok)
{
    HFONT hOld;
    RECT  r;

    if (g_fontGlyphSm == NULL)
        return;

    hOld = (HFONT)SelectObject(hdc, g_fontGlyphSm);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, ok ? RGB(0x0F, 0x7B, 0x0F) : RGB(0xC4, 0x3E, 0x1C));

    r.left   = x;
    r.top    = y;
    r.right  = x + size;
    r.bottom = y + size;

    DrawTextW(hdc, ok ? L"\xE930" : L"\xE783", -1, &r,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    SelectObject(hdc, hOld);
}

static void SetupDialogClass(WNDCLASSEXW *wc, WNDPROC proc, const WCHAR *cls)
{
    ZeroMemory(wc, sizeof(*wc));
    wc->cbSize        = sizeof(*wc);
    wc->lpfnWndProc   = proc;
    wc->hInstance     = g_hInstance;
    wc->hCursor       = LoadCursorW(NULL, IDC_ARROW);
    wc->lpszClassName = cls;
    wc->hIcon         = (HICON)LoadImageW(g_hInstance, MAKEINTRESOURCEW(IDI_APP_ICON),
                                          IMAGE_ICON,
                                          GetSystemMetrics(SM_CXICON),
                                          GetSystemMetrics(SM_CYICON),
                                          LR_DEFAULTCOLOR);
    wc->hIconSm       = (HICON)LoadImageW(g_hInstance, MAKEINTRESOURCEW(IDI_APP_ICON),
                                          IMAGE_ICON,
                                          GetSystemMetrics(SM_CXSMICON),
                                          GetSystemMetrics(SM_CYSMICON),
                                          LR_DEFAULTCOLOR);
    RegisterClassExW(wc);
}

/* 居中创建一个固定大小、不可调整的自绘对话框 */
static HWND CreateCenteredDialog(const WCHAR *cls, const WCHAR *title,
                                 int cx, int cy, DWORD style)
{
    RECT wa, rc;
    int  x, y, w, h;

    rc.left   = 0;
    rc.top    = 0;
    rc.right  = AS(cx);
    rc.bottom = AS(cy);
    AdjustWindowRectEx(&rc, style, FALSE, 0);

    w = rc.right - rc.left;
    h = rc.bottom - rc.top;

    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    x = wa.left + ((wa.right - wa.left) - w) / 2;
    y = wa.top + ((wa.bottom - wa.top) - h) / 2;

    return CreateWindowExW(0, cls, title, style, x, y, w, h,
                           g_hWnd, NULL, g_hInstance, NULL);
}

/* ---------------------------------------------------------------------------
 * 语言选择窗口
 *
 * 两个按钮各自用母语写着，不随界面语言变化。第一次运行时以模态方式弹出
 * （走一个局部消息循环等用户选完）；在「关于」里点按钮则是非模态的。
 * ------------------------------------------------------------------------ */

#define LANG_CLASS      L"ScreenOffLock_LangWindow"
#define IDC_LANG_EN     2301
#define IDC_LANG_ZH     2302

#define LANG_CX         340
#define LANG_CY         124

static HWND g_hLangWnd    = NULL;
static BOOL g_langPicked  = FALSE;      /* 用户这一次是否真的做了选择 */

static void LangPaint(HWND hWnd)
{
    PAINTSTRUCT ps;
    HDC         hdc = BeginPaint(hWnd, &ps);
    RECT        rc, r;
    HFONT       hOld;

    GetClientRect(hWnd, &rc);
    FillRect(hdc, &rc, GetSysColorBrush(COLOR_WINDOW));
    SetBkMode(hdc, TRANSPARENT);

    hOld = (HFONT)SelectObject(hdc, g_fontBody);
    SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));

    /* 提示文字整行居中 */
    r.left   = 0;
    r.top    = AS(24);
    r.right  = rc.right;
    r.bottom = r.top + AS(30);

    DrawTextW(hdc, T(S_LANG_PROMPT), -1, &r,
              DT_CENTER | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);

    SelectObject(hdc, hOld);

    EndPaint(hWnd, &ps);
}

static void CreateLangButtons(HWND hWnd)
{
    HDC  hdc = GetDC(hWnd);
    RECT rc;
    HWND hBtn;
    int  bh = AS(34), gap = AS(12);
    int  w1, w2, x, y;

    GetClientRect(hWnd, &rc);

    /* 两个语言名都用各自的母语写，任何界面语言下都长这样 */
    w1 = TextWidth(hdc, g_fontBody, L"English")   + AS(38);
    w2 = TextWidth(hdc, g_fontBody, L"简体中文")  + AS(38);
    x  = (rc.right - (w1 + gap + w2)) / 2;
    y  = rc.bottom - AS(20) - bh;

    hBtn = CreateWindowExW(0, L"BUTTON", L"English",
                           WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                           x, y, w1, bh, hWnd,
                           (HMENU)(INT_PTR)IDC_LANG_EN, g_hInstance, NULL);
    if (hBtn)
    {
        SendMessageW(hBtn, WM_SETFONT, (WPARAM)g_fontBody, TRUE);
        SetFocus(hBtn);
    }

    x += w1 + gap;
    hBtn = CreateWindowExW(0, L"BUTTON", L"简体中文",
                           WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                           x, y, w2, bh, hWnd,
                           (HMENU)(INT_PTR)IDC_LANG_ZH, g_hInstance, NULL);
    if (hBtn)
        SendMessageW(hBtn, WM_SETFONT, (WPARAM)g_fontBody, TRUE);

    ReleaseDC(hWnd, hdc);
}

static void ApplyLanguage(HWND hWnd, LangId lang)
{
    g_lang       = lang;
    g_langPicked = TRUE;
    SaveLanguage(lang);

    /*
     * 已经打开的对话框里，文字和按钮都是按旧语言建好的，直接关掉最省事——
     * 用户重新打开就是新语言了。菜单是每次弹出时重建的，不用管。
     */
    if (g_hAboutWnd != NULL)
        DestroyWindow(g_hAboutWnd);

    if (g_hCheckWnd != NULL)
        DestroyWindow(g_hCheckWnd);

    DestroyWindow(hWnd);
}

static LRESULT CALLBACK LangWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
        CreateAboutFonts();
        CreateLangButtons(hWnd);
        ApplyRoundCorners(hWnd);
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
        LangPaint(hWnd);
        return 0;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_LANG_EN)
            ApplyLanguage(hWnd, LANG_EN);
        else if (LOWORD(wParam) == IDC_LANG_ZH)
            ApplyLanguage(hWnd, LANG_ZH);
        return 0;

    case WM_CLOSE:
        DestroyWindow(hWnd);
        return 0;

    case WM_DESTROY:
        g_hLangWnd = NULL;
        return 0;

    default:
        break;
    }

    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

static void ShowLanguageWindow(BOOL modal)
{
    WNDCLASSEXW wc;
    MSG         msg;

    if (g_hLangWnd != NULL)
    {
        SetForegroundWindow(g_hLangWnd);
        return;
    }

    SetupDialogClass(&wc, LangWndProc, LANG_CLASS);

    g_uiDpi = QueryDpi(g_hWnd);

    g_hLangWnd = CreateCenteredDialog(LANG_CLASS, T(S_LANGUAGE_BTN),
                                      LANG_CX, LANG_CY,
                                      WS_CAPTION | WS_SYSMENU | WS_POPUP);

    if (g_hLangWnd == NULL)
        return;

    ShowWindow(g_hLangWnd, SW_SHOW);
    UpdateWindow(g_hLangWnd);
    SetForegroundWindow(g_hLangWnd);

    if (modal)
    {
        /* 第一次运行：就地跑消息循环，等用户选完再继续启动流程 */
        while (g_hLangWnd != NULL && GetMessageW(&msg, NULL, 0, 0) > 0)
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
}

/* ---------------------------------------------------------------------------
 * 「电源设置」对话框
 *
 * 列出插电/电池两种供电模式下电源按钮与合盖的实际取值，给出结论，并提供两个
 * 一键写入推荐值的按钮。
 *
 * 写入电源计划需要管理员权限，所以按钮不直接写，而是用 runas 启动一个提权
 * 子进程（带 --apply-ac / --apply-dc）去完成，完成后刷新本窗口。
 * ------------------------------------------------------------------------ */

#define CHECK_CLASS      L"ScreenOffLock_CheckWindow"
#define IDC_CHECK_CANCEL 2101
#define IDC_CHECK_AC     2102
#define IDC_CHECK_DC     2103

#define CHECK_CX         580
#define CHECK_CY         322

static void CheckPaint(HWND hWnd)
{
    PAINTSTRUCT   ps;
    HDC           hdc = BeginPaint(hWnd, &ps);
    RECT          rc;
    PowerSettings cfg;
    WCHAR         buf[256];
    COLORREF      clrText  = GetSysColor(COLOR_WINDOWTEXT);
    COLORREF      clrMuted = RGB(0x60, 0x5E, 0x5C);
    HPEN          hPen, hOldPen;
    int           colRec = 330;     /* “推荐值”那一列的起始 x */

    GetClientRect(hWnd, &rc);
    FillRect(hdc, &rc, GetSysColorBrush(COLOR_WINDOW));
    SetBkMode(hdc, TRANSPARENT);

    if (!ReadPowerSettings(&cfg))
    {
        AboutText(hdc, g_fontBody, clrText, T(S_CHECK_READ_FAIL),
                  28, 24, CHECK_CX - 56);
        EndPaint(hWnd, &ps);
        return;
    }

    AboutText(hdc, g_fontBody, clrText, T(S_CHECK_INTRO), 28, 20, CHECK_CX - 56);

    /* 两行结论，行首图标表示该供电模式是否全部合规 */
    {
        BOOL acOk = (AcButtonOk(&cfg) && AcLidOk(&cfg));
        BOOL dcOk = (DcButtonOk(&cfg) && DcLidOk(&cfg));

        DrawStatusIcon(hdc, AS(28), AS(45), AS(16), acOk);
        wsprintfW(buf, T(S_CHECK_AC_FMT), VerdictText(AcButtonOk(&cfg), AcLidOk(&cfg)));
        AboutText(hdc, g_fontBody, clrText, buf, 52, 44, CHECK_CX - 80);

        DrawStatusIcon(hdc, AS(28), AS(69), AS(16), dcOk);
        wsprintfW(buf, T(S_CHECK_DC_FMT), VerdictText(DcButtonOk(&cfg), DcLidOk(&cfg)));
        AboutText(hdc, g_fontBody, clrText, buf, 52, 68, CHECK_CX - 80);
    }

    hPen    = CreatePen(PS_SOLID, 1, RGB(0xE5, 0xE5, 0xE5));
    hOldPen = (HPEN)SelectObject(hdc, hPen);
    MoveToEx(hdc, AS(28), AS(96), NULL);
    LineTo(hdc, AS(CHECK_CX - 28), AS(96));
    SelectObject(hdc, hOldPen);
    DeleteObject(hPen);

    /* 副区域：整块放进一个淡灰色衬底里，字号比上面的结论小一号 */
    {
        HBRUSH hbrPanel = CreateSolidBrush(RGB(0xF5, 0xF5, 0xF5));
        HPEN   hPnPanel = CreatePen(PS_SOLID, 1, RGB(0xE8, 0xE8, 0xE8));
        HBRUSH hOldBr   = (HBRUSH)SelectObject(hdc, hbrPanel);
        HPEN   hOldPn   = (HPEN)SelectObject(hdc, hPnPanel);

        RoundRect(hdc, AS(18), AS(104), AS(CHECK_CX - 18), AS(248), AS(12), AS(12));

        SelectObject(hdc, hOldBr);
        SelectObject(hdc, hOldPn);
        DeleteObject(hbrPanel);
        DeleteObject(hPnPanel);
    }

    /* 插电 */
    AboutText(hdc, g_fontSmall, clrMuted, T(S_CHECK_AC_LABEL), 32, 116, 200);

    DrawStatusIcon(hdc, AS(32), AS(137), AS(16), AcButtonOk(&cfg));
    wsprintfW(buf, T(S_CHECK_BUTTON_FMT), ActionName(cfg.buttonAc));
    AboutText(hdc, g_fontSmall, clrText,  buf, 56, 136, 268);
    AboutText(hdc, g_fontSmall, clrMuted, T(S_CHECK_REC_OFF), colRec, 136, 220);

    DrawStatusIcon(hdc, AS(32), AS(157), AS(16), AcLidOk(&cfg));
    wsprintfW(buf, T(S_CHECK_LID_FMT), LidActionName(cfg.lidAc));
    AboutText(hdc, g_fontSmall, clrText,  buf, 56, 156, 268);
    AboutText(hdc, g_fontSmall, clrMuted, T(S_CHECK_REC_NOTHING), colRec, 156, 240);

    /* 电池 */
    AboutText(hdc, g_fontSmall, clrMuted, T(S_CHECK_DC_LABEL), 32, 182, 200);

    DrawStatusIcon(hdc, AS(32), AS(203), AS(16), DcButtonOk(&cfg));
    wsprintfW(buf, T(S_CHECK_BUTTON_FMT), ActionName(cfg.buttonDc));
    AboutText(hdc, g_fontSmall, clrText,  buf, 56, 202, 268);
    AboutText(hdc, g_fontSmall, clrMuted, T(S_CHECK_REC_OFF), colRec, 202, 220);

    DrawStatusIcon(hdc, AS(32), AS(223), AS(16), DcLidOk(&cfg));
    wsprintfW(buf, T(S_CHECK_LID_FMT), LidActionName(cfg.lidDc));
    AboutText(hdc, g_fontSmall, clrText,  buf, 56, 222, 268);
    AboutText(hdc, g_fontSmall, clrMuted, T(S_CHECK_REC_SLEEP), colRec, 222, 220);

    EndPaint(hWnd, &ps);
}

static void CreateCheckButtons(HWND hWnd)
{
    HDC    hdc = GetDC(hWnd);
    RECT   rc;
    HWND   hBtn;
    int    bh = AS(32), gap = AS(10), x, y, bw;

    GetClientRect(hWnd, &rc);
    y = rc.bottom - AS(18) - bh;

    /* 从右往左依次摆放 */
    bw = TextWidth(hdc, g_fontBody, T(S_APPLY_DC)) + AS(30);
    x  = rc.right - AS(24) - bw;
    hBtn = CreateWindowExW(0, L"BUTTON", T(S_APPLY_DC),
                           WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                           x, y, bw, bh, hWnd,
                           (HMENU)(INT_PTR)IDC_CHECK_DC, g_hInstance, NULL);
    if (hBtn) SendMessageW(hBtn, WM_SETFONT, (WPARAM)g_fontBody, TRUE);

    bw = TextWidth(hdc, g_fontBody, T(S_APPLY_AC)) + AS(30);
    x -= (gap + bw);
    hBtn = CreateWindowExW(0, L"BUTTON", T(S_APPLY_AC),
                           WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                           x, y, bw, bh, hWnd,
                           (HMENU)(INT_PTR)IDC_CHECK_AC, g_hInstance, NULL);
    if (hBtn) SendMessageW(hBtn, WM_SETFONT, (WPARAM)g_fontBody, TRUE);

    bw = TextWidth(hdc, g_fontBody, T(S_BTN_CANCEL)) + AS(30);
    if (bw < AS(84)) bw = AS(84);
    x -= (gap + bw);
    hBtn = CreateWindowExW(0, L"BUTTON", T(S_BTN_CANCEL),
                           WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                           x, y, bw, bh, hWnd,
                           (HMENU)(INT_PTR)IDC_CHECK_CANCEL, g_hInstance, NULL);
    if (hBtn)
    {
        SendMessageW(hBtn, WM_SETFONT, (WPARAM)g_fontBody, TRUE);
        SetFocus(hBtn);
    }

    ReleaseDC(hWnd, hdc);
}

/* 用提权子进程写入推荐值；用户拒绝 UAC 时 ShellExecuteEx 会失败，静默返回 */
static void ApplyRecommendedViaElevation(HWND hWnd, BOOL ac)
{
    WCHAR             exePath[MAX_PATH];
    SHELLEXECUTEINFOW sei;

    if (GetModuleFileNameW(NULL, exePath, MAX_PATH) == 0)
        return;

    ZeroMemory(&sei, sizeof(sei));
    sei.cbSize       = sizeof(sei);
    sei.fMask        = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
    sei.lpVerb       = L"runas";
    sei.lpFile       = exePath;
    sei.lpParameters = ac ? L"--apply-ac" : L"--apply-dc";
    sei.nShow        = SW_HIDE;

    if (!ShellExecuteExW(&sei))
        return;

    WaitForSingleObject(sei.hProcess, 30000);
    CloseHandle(sei.hProcess);

    InvalidateRect(hWnd, NULL, TRUE);       /* 重新读取并刷新显示 */
    UpdateWindow(hWnd);
}

static LRESULT CALLBACK CheckWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
        CreateAboutFonts();
        CreateCheckButtons(hWnd);
        ApplyRoundCorners(hWnd);
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
        CheckPaint(hWnd);
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDC_CHECK_CANCEL:
            DestroyWindow(hWnd);
            break;

        case IDC_CHECK_AC:
        case IDC_CHECK_DC:
            EnableWindow(hWnd, FALSE);      /* 提权期间挡住重复点击 */
            ApplyRecommendedViaElevation(hWnd, LOWORD(wParam) == IDC_CHECK_AC);
            EnableWindow(hWnd, TRUE);
            SetForegroundWindow(hWnd);
            break;

        default:
            break;
        }
        return 0;

    case WM_CLOSE:
        DestroyWindow(hWnd);
        return 0;

    case WM_DESTROY:
        g_hCheckWnd = NULL;
        return 0;

    default:
        break;
    }

    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

static void ShowCheckWindow(void)
{
    WNDCLASSEXW wc;

    if (g_hCheckWnd != NULL)
    {
        SetForegroundWindow(g_hCheckWnd);
        return;
    }

    SetupDialogClass(&wc, CheckWndProc, CHECK_CLASS);

    g_uiDpi = QueryDpi(g_hWnd);

    g_hCheckWnd = CreateCenteredDialog(CHECK_CLASS, T(S_MENU_CHECK),
                                       CHECK_CX, CHECK_CY,
                                       WS_CAPTION | WS_SYSMENU | WS_POPUP);

    if (g_hCheckWnd == NULL)
        return;

    ShowWindow(g_hCheckWnd, SW_SHOW);
    UpdateWindow(g_hCheckWnd);
    SetForegroundWindow(g_hCheckWnd);
}

/* ---------------------------------------------------------------------------
 * 首次启动提示
 *
 * 启动时如果发现设置不全在推荐值，提醒一次。勾选“不再显示”并确定后写入
 * 注册表，之后不再打扰。
 * ------------------------------------------------------------------------ */

#define WARN_CLASS       L"ScreenOffLock_WarnWindow"
#define IDC_WARN_DONTASK 2201
#define IDC_WARN_OK      2202

#define WARN_CX          480
#define WARN_CY          192

static HWND g_hWarnWnd    = NULL;
static BOOL g_warnDontAsk = FALSE;

static BOOL SettingsAllRecommended(const PowerSettings *ps)
{
    return (AcButtonOk(ps) && AcLidOk(ps) && DcButtonOk(ps) && DcLidOk(ps)) ? TRUE : FALSE;
}

/*
 * 自绘复选框。
 * 带视觉样式的原生复选框不接受 WM_CTLCOLORBTN，背景由主题绘制，放在白色
 * 对话框里会露出一块系统灰；所以这里自己画一个 Win11 样式的。
 */
static void DrawCheckbox(HDC hdc, int x, int y, int size, BOOL checked)
{
    RECT   r;
    HPEN   hPen, hOldPen;
    HBRUSH hbr, hOldBr;

    r.left   = x;
    r.top    = y;
    r.right  = x + size;
    r.bottom = y + size;

    if (checked)
    {
        hbr = CreateSolidBrush(RGB(0x0F, 0x6C, 0xBD));
        FillRect(hdc, &r, hbr);
        DeleteObject(hbr);

        hPen    = CreatePen(PS_SOLID, MulDiv(2, (int)g_uiDpi, 96), RGB(0xFF, 0xFF, 0xFF));
        hOldPen = (HPEN)SelectObject(hdc, hPen);
        MoveToEx(hdc, x + size * 24 / 100, y + size * 52 / 100, NULL);
        LineTo(hdc,   x + size * 43 / 100, y + size * 71 / 100);
        LineTo(hdc,   x + size * 77 / 100, y + size * 29 / 100);
        SelectObject(hdc, hOldPen);
        DeleteObject(hPen);
    }
    else
    {
        hbr = CreateSolidBrush(GetSysColor(COLOR_WINDOW));
        FillRect(hdc, &r, hbr);
        DeleteObject(hbr);

        hPen    = CreatePen(PS_SOLID, 1, RGB(0x8A, 0x8A, 0x8A));
        hOldPen = (HPEN)SelectObject(hdc, hPen);
        hOldBr  = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, x, y, x + size, y + size);
        SelectObject(hdc, hOldBr);
        SelectObject(hdc, hOldPen);
        DeleteObject(hPen);
    }
}

static void WarnPaint(HWND hWnd)
{
    PAINTSTRUCT ps;
    HDC         hdc = BeginPaint(hWnd, &ps);
    RECT        rc;
    COLORREF    clrText   = GetSysColor(COLOR_WINDOWTEXT);
    COLORREF    clrAccent = RGB(0xD8, 0x3B, 0x01);

    GetClientRect(hWnd, &rc);
    FillRect(hdc, &rc, GetSysColorBrush(COLOR_WINDOW));
    SetBkMode(hdc, TRANSPARENT);

    /* 警告图标：系统图标字体可用时画 U+E7BA（⚠），否则省略 */
    if (g_fontGlyph != NULL)
    {
        HFONT hOld = (HFONT)SelectObject(hdc, g_fontGlyph);
        RECT  r;

        SetTextColor(hdc, clrAccent);
        r.left   = AS(28);
        r.top    = AS(26);
        r.right  = AS(28 + 40);
        r.bottom = AS(26 + 40);

        DrawTextW(hdc, L"\xE7BA", -1, &r,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        SelectObject(hdc, hOld);
    }

    AboutText(hdc, g_fontBody, clrText,
              T(S_WARN_LINE1), 80, 28, WARN_CX - 108);

    AboutText(hdc, g_fontSmall, RGB(0x60, 0x5E, 0x5C),
              T(S_WARN_LINE2), 80, 54, WARN_CX - 108);

    DrawCheckbox(hdc, AS(28), AS(98), AS(20), g_warnDontAsk);
    AboutText(hdc, g_fontBody, clrText, T(S_WARN_DONTASK), 58, 100, 200);

    EndPaint(hWnd, &ps);
}

static void CreateWarnControls(HWND hWnd)
{
    HDC  hdc = GetDC(hWnd);
    RECT rc;
    HWND hCtl;
    int  bh = AS(32), bw, x, y;

    GetClientRect(hWnd, &rc);

    bw = TextWidth(hdc, g_fontBody, T(S_BTN_OK)) + AS(30);
    if (bw < AS(96)) bw = AS(96);
    x = rc.right - AS(24) - bw;
    y = rc.bottom - AS(18) - bh;

    hCtl = CreateWindowExW(0, L"BUTTON", T(S_BTN_OK),
                           WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                           x, y, bw, bh, hWnd,
                           (HMENU)(INT_PTR)IDC_WARN_OK, g_hInstance, NULL);
    if (hCtl)
    {
        SendMessageW(hCtl, WM_SETFONT, (WPARAM)g_fontBody, TRUE);
        SetFocus(hCtl);
    }

    ReleaseDC(hWnd, hdc);
}

static LRESULT CALLBACK WarnWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
        CreateAboutFonts();
        g_warnDontAsk = FALSE;          /* 复选框默认不勾选 */
        CreateWarnControls(hWnd);
        ApplyRoundCorners(hWnd);
        return 0;

    case WM_ERASEBKGND:
        return 1;

    /* 让复选框的背景跟窗口的白色一致，否则会露出一块系统灰 */
    case WM_CTLCOLORBTN:
        SetBkColor((HDC)wParam, GetSysColor(COLOR_WINDOW));
        return (LRESULT)GetSysColorBrush(COLOR_WINDOW);

    case WM_PAINT:
        WarnPaint(hWnd);
        return 0;

    case WM_LBUTTONDOWN:
        {
            int mx  = (int)(short)LOWORD(lParam);
            int my  = (int)(short)HIWORD(lParam);
            int bx  = AS(28), by = AS(98), bs = AS(20), pad = AS(6);

            /* 点方框本身或右边的“不再显示”文字都算 */
            if (mx >= bx - pad && mx <= bx + bs + AS(130) &&
                my >= by - pad && my <= by + bs + pad)
            {
                g_warnDontAsk = !g_warnDontAsk;
                InvalidateRect(hWnd, NULL, FALSE);
            }
        }
        return 0;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_WARN_OK)
        {
            if (g_warnDontAsk)
                SaveBoolSetting(REG_VALUE_SKIPWARN, TRUE);

            DestroyWindow(hWnd);
        }
        return 0;

    case WM_CLOSE:
        DestroyWindow(hWnd);
        return 0;

    case WM_DESTROY:
        g_hWarnWnd = NULL;
        return 0;

    default:
        break;
    }

    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

static void ShowSettingsWarning(void)
{
    WNDCLASSEXW wc;

    if (g_hWarnWnd != NULL)
    {
        SetForegroundWindow(g_hWarnWnd);
        return;
    }

    SetupDialogClass(&wc, WarnWndProc, WARN_CLASS);

    g_uiDpi = QueryDpi(g_hWnd);

    g_hWarnWnd = CreateCenteredDialog(WARN_CLASS, APP_NAME,
                                      WARN_CX, WARN_CY,
                                      WS_CAPTION | WS_SYSMENU | WS_POPUP);

    if (g_hWarnWnd == NULL)
        return;

    ShowWindow(g_hWarnWnd, SW_SHOW);
    UpdateWindow(g_hWarnWnd);
    SetForegroundWindow(g_hWarnWnd);
}

/* ---------------------------------------------------------------------------
 * 检查更新
 *
 * 版本号取 APP_VERSION 里 Build 后面那段数字，跟 GitHub 最新 release 的 tag
 * 比大小（tag 就写成 "1400" 这种纯数字）。
 *
 * 网络这层只用 WinHTTP，而且是从 winhttp.dll 里运行时取函数指针——静态导入表
 * 保持原来那六个系统 DLL 不变。请求跑在工作线程上、结果用 PostMessage 回投，
 * 免得同步的网络调用把消息循环卡住。
 *
 * 启动后静默查一次（最多试 UPDATE_TRY_COUNT 次，全失败就彻底安静）；用户也
 * 可以在「关于」里手动查，结果显示在窗口上。
 * ------------------------------------------------------------------------ */

#define UPDATE_API_HOST     L"api.github.com"
#define UPDATE_API_PATH     L"/repos/AomiRaku/Screen-Off-Lock/releases/latest"
#define UPDATE_PAGE_URL     L"https://github.com/AomiRaku/Screen-Off-Lock/releases/latest"

#define WM_APP_UPDATE_DONE  (WM_APP + 2)

#define UPDATE_TRY_COUNT    3           /* 启动时最多试几次 */
#define UPDATE_RETRY_WAIT   3000        /* 两次尝试之间等多久（毫秒） */
#define UPDATE_TIMEOUT      8000        /* 单次请求的超时（毫秒） */

/* 原子读一个后台线程会写的变量 */
#define ATOMIC_READ(p)      InterlockedCompareExchange((p), 0, 0)

static void ShowUpdateAvailableWindow(void);

static HWND g_hUpdateWarnWnd = NULL;        /* 启动时那个更新提示窗 */
static HWND g_hUpdateChkWnd  = NULL;        /* 手动检查更新那个窗口 */

/* --- 从 winhttp.dll 里取函数 ---------------------------------------------
 * 不静态链接 winhttp，导入表里就不会多出这个 DLL。
 * ------------------------------------------------------------------------ */

typedef HINTERNET (WINAPI *PFN_WinHttpOpen)(LPCWSTR, DWORD, LPCWSTR, LPCWSTR, DWORD);
typedef HINTERNET (WINAPI *PFN_WinHttpConnect)(HINTERNET, LPCWSTR, INTERNET_PORT, DWORD);
typedef HINTERNET (WINAPI *PFN_WinHttpOpenRequest)(HINTERNET, LPCWSTR, LPCWSTR, LPCWSTR,
                                                   LPCWSTR, LPCWSTR *, DWORD);
typedef BOOL (WINAPI *PFN_WinHttpSetOption)(HINTERNET, DWORD, LPVOID, DWORD);
typedef BOOL (WINAPI *PFN_WinHttpSetTimeouts)(HINTERNET, int, int, int, int);
typedef BOOL (WINAPI *PFN_WinHttpSendRequest)(HINTERNET, LPCWSTR, DWORD, LPVOID,
                                              DWORD, DWORD, DWORD_PTR);
typedef BOOL (WINAPI *PFN_WinHttpReceiveResponse)(HINTERNET, LPVOID);
typedef BOOL (WINAPI *PFN_WinHttpQueryHeaders)(HINTERNET, DWORD, LPCWSTR, LPVOID,
                                               LPDWORD, LPDWORD);
typedef BOOL (WINAPI *PFN_WinHttpReadData)(HINTERNET, LPVOID, DWORD, LPDWORD);
typedef BOOL (WINAPI *PFN_WinHttpCloseHandle)(HINTERNET);

typedef struct
{
    PFN_WinHttpOpen            Open;
    PFN_WinHttpConnect         Connect;
    PFN_WinHttpOpenRequest     OpenRequest;
    PFN_WinHttpSetOption       SetOption;
    PFN_WinHttpSetTimeouts     SetTimeouts;
    PFN_WinHttpSendRequest     SendRequest;
    PFN_WinHttpReceiveResponse ReceiveResponse;
    PFN_WinHttpQueryHeaders    QueryHeaders;
    PFN_WinHttpReadData        ReadData;
    PFN_WinHttpCloseHandle     CloseHandle;
} WinHttpApi;

/* 取不到全部函数就算不可用；winhttp.dll 保持加载，不再 FreeLibrary */
static BOOL LoadWinHttp(WinHttpApi *api)
{
    HMODULE hLib = LoadLibraryW(L"winhttp.dll");

    if (hLib == NULL)
        return FALSE;

    api->Open            = (PFN_WinHttpOpen)           (void *)GetProcAddress(hLib, "WinHttpOpen");
    api->Connect         = (PFN_WinHttpConnect)        (void *)GetProcAddress(hLib, "WinHttpConnect");
    api->OpenRequest     = (PFN_WinHttpOpenRequest)    (void *)GetProcAddress(hLib, "WinHttpOpenRequest");
    api->SetOption       = (PFN_WinHttpSetOption)      (void *)GetProcAddress(hLib, "WinHttpSetOption");
    api->SetTimeouts     = (PFN_WinHttpSetTimeouts)    (void *)GetProcAddress(hLib, "WinHttpSetTimeouts");
    api->SendRequest     = (PFN_WinHttpSendRequest)    (void *)GetProcAddress(hLib, "WinHttpSendRequest");
    api->ReceiveResponse = (PFN_WinHttpReceiveResponse)(void *)GetProcAddress(hLib, "WinHttpReceiveResponse");
    api->QueryHeaders    = (PFN_WinHttpQueryHeaders)   (void *)GetProcAddress(hLib, "WinHttpQueryHeaders");
    api->ReadData        = (PFN_WinHttpReadData)       (void *)GetProcAddress(hLib, "WinHttpReadData");
    api->CloseHandle     = (PFN_WinHttpCloseHandle)    (void *)GetProcAddress(hLib, "WinHttpCloseHandle");

    return (api->Open != NULL && api->Connect != NULL && api->OpenRequest != NULL &&
            api->SetOption != NULL && api->SetTimeouts != NULL && api->SendRequest != NULL &&
            api->ReceiveResponse != NULL && api->QueryHeaders != NULL &&
            api->ReadData != NULL && api->CloseHandle != NULL) ? TRUE : FALSE;
}

/* --- 版本号 --------------------------------------------------------------- */

/* APP_VERSION 里的数字："Build 1400" -> 1400 */
static DWORD LocalBuildNumber(void)
{
    const WCHAR *p    = APP_VERSION;
    DWORD        v    = 0;
    BOOL         seen = FALSE;

    while (*p != L'\0')
    {
        if (*p >= L'0' && *p <= L'9')
        {
            v = v * 10 + (DWORD)(*p - L'0');
            seen = TRUE;
        }
        else if (seen)
        {
            break;
        }

        ++p;
    }

    return seen ? v : 0;
}

/* 从 JSON 里抠出 "tag_name":"xxx"。不引 JSON 库，扫一遍就够了 */
static BOOL ExtractTagName(const char *json, char *out, int outSize)
{
    const char *key = "\"tag_name\":\"";
    const char *p   = strstr(json, key);
    const char *end;

    if (p == NULL)
        return FALSE;

    p  += 12;                           /* strlen(key) */
    end = strchr(p, '"');

    if (end == NULL || (int)(end - p) >= outSize)
        return FALSE;

    memcpy(out, p, (size_t)(end - p));
    out[end - p] = '\0';
    return TRUE;
}

/* "1400" / "v1400" / "Build 1400" 一律只取第一段连续数字 */
static DWORD FirstNumberA(const char *s)
{
    DWORD v    = 0;
    BOOL  seen = FALSE;

    while (*s != '\0')
    {
        if (*s >= '0' && *s <= '9')
        {
            v = v * 10 + (DWORD)(*s - '0');
            seen = TRUE;
        }
        else if (seen)
        {
            break;
        }

        ++s;
    }

    return seen ? v : 0;
}

/* --- 更新说明（release 的 body）-----------------------------------------
 *
 * API 返回的是 JSON：body 里的换行、引号都是转义过的，所以得真正解码一遍；
 * 解出来是 Markdown，还得把标记去掉变成纯文本。作者把中英文用一条分割线
 * （单独一行 ---）分开写，于是按它切两段，再挑语言对应的那段。
 * ------------------------------------------------------------------------ */

#define BODY_RAW_SIZE   16384       /* 解码后的原始正文 */
#define BODY_SEG_SIZE   8192        /* 切出来的单语段 */

/* 十六进制四位数，例如 \u4e2d 里的 4e2d */
static unsigned Hex4(const char *s)
{
    unsigned v = 0;
    int      i;

    for (i = 0; i < 4; ++i)
    {
        char     c = s[i];
        unsigned d;

        if (c >= '0' && c <= '9')      d = (unsigned)(c - '0');
        else if (c >= 'a' && c <= 'f') d = (unsigned)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') d = (unsigned)(c - 'A' + 10);
        else break;

        v = (v << 4) | d;
    }

    return v;
}

/* 一个码点写成 UTF-8，返回字节数 */
static int Utf8Encode(unsigned cp, char *out)
{
    if (cp < 0x80)
    {
        out[0] = (char)cp;
        return 1;
    }

    if (cp < 0x800)
    {
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    }

    if (cp < 0x10000)
    {
        out[0] = (char)(0xE0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    }

    out[0] = (char)(0xF0 | (cp >> 18));
    out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
    out[3] = (char)(0x80 | (cp & 0x3F));
    return 4;
}

/* 从开引号之后开始解码一段 JSON 字符串，输出 UTF-8 */
static void JsonDecodeTo(const char *src, char *out, int outSize)
{
    int o = 0;

    while (*src != '\0' && *src != '"' && o < outSize - 8)
    {
        if (*src != '\\')
        {
            out[o++] = *src++;
            continue;
        }

        ++src;                          /* 吃掉反斜杠 */

        switch (*src)
        {
        case 'n':  out[o++] = '\n'; ++src; break;
        case 'r':  out[o++] = '\r'; ++src; break;
        case 't':  out[o++] = '\t'; ++src; break;
        case 'b':  out[o++] = '\b'; ++src; break;
        case 'f':  out[o++] = '\f'; ++src; break;
        case '/':  out[o++] = '/';  ++src; break;
        case '\\': out[o++] = '\\'; ++src; break;
        case '"':  out[o++] = '"';  ++src; break;

        case 'u':
            {
                unsigned cp = Hex4(src + 1);

                src += 5;               /* 跳过 \uXXXX */

                /* 代理对：高低位拼成一个码点 */
                if (cp >= 0xD800 && cp <= 0xDBFF && src[0] == '\\' && src[1] == 'u')
                {
                    unsigned lo = Hex4(src + 2);

                    if (lo >= 0xDC00 && lo <= 0xDFFF)
                    {
                        cp   = 0x10000u + ((cp - 0xD800u) << 10) + (lo - 0xDC00u);
                        src += 6;
                    }
                }

                o += Utf8Encode(cp, out + o);
            }
            break;

        default:
            if (*src == '\0')
            {
                out[o] = '\0';
                return;
            }
            out[o++] = *src++;
            break;
        }
    }

    out[o] = '\0';
}

/* 取出 release 正文并解码；响应里没有这个字段就返回 FALSE */
static BOOL ExtractReleaseBody(const char *json, char *out, int outSize)
{
    const char *key = "\"body\":\"";
    const char *p   = strstr(json, key);

    out[0] = '\0';

    if (p == NULL)
        return FALSE;

    JsonDecodeTo(p + strlen(key), out, outSize);
    return TRUE;
}

/* 一整行只有 - * _ 且至少三个，就是 Markdown 的分割线 */
static BOOL IsMdRule(const char *line, int len)
{
    int i, n = 0;

    for (i = 0; i < len; ++i)
    {
        char c = line[i];

        if (c == ' ' || c == '\t' || c == '\r')
            continue;

        if (c == '-' || c == '*' || c == '_')
        {
            ++n;
            continue;
        }

        return FALSE;
    }

    return (n >= 3) ? TRUE : FALSE;
}

/* 行首是列表符号（- * + 后面跟空格）时，返回符号后面的偏移 */
static int SkipListMark(const char *line, int len)
{
    if (len >= 2 && (line[0] == '-' || line[0] == '*' || line[0] == '+') && line[1] == ' ')
        return 2;

    return 0;
}

/* 把一句提示按当前界面语言写成 UTF-8 塞进缓冲 */
static void AppendWideAsUtf8(const WCHAR *w, char *out, int outSize, int *used)
{
    char tmp[512];
    int  n = WideCharToMultiByte(CP_UTF8, 0, w, -1, tmp, (int)sizeof(tmp) - 1, NULL, NULL);
    int  i;

    if (n <= 0)
        return;

    for (i = 0; i < n - 1 && *used < outSize - 4; ++i)
        out[(*used)++] = tmp[i];
}

/*
 * 把一行 Markdown 变成纯文本，追加进 out。
 * 只处理写 release 说明常用的那些：标题 #、引用 >、列表符号、强调 ** __ *、
 * 行内代码 `、链接 [文字](url)。图片这里显示不出来，换成一句提示。
 */
static void CleanMdLine(const char *line, int len, const WCHAR *imgNote,
                        char *out, int outSize, int *used)
{
    int i = 0, o = *used;
    int stars = 0, k;

    while (i < len && (line[i] == ' ' || line[i] == '\t'))
        ++i;

    /* 标题的 # */
    if (i < len && line[i] == '#')
    {
        while (i < len && line[i] == '#') ++i;
        while (i < len && line[i] == ' ') ++i;
    }

    /* 引用的 > */
    if (i < len && line[i] == '>')
    {
        ++i;
        if (i < len && line[i] == ' ') ++i;
    }

    /* 列表符号统一成 "- " */
    k = SkipListMark(line + i, len - i);

    if (k > 0)
    {
        if (o < outSize - 4) { out[o++] = '-'; out[o++] = ' '; }
        i += k;
    }

    /* 这一行有几个星号：够两个才当强调标记抹掉 */
    for (k = i; k < len; ++k)
    {
        if (line[k] == '*')
            ++stars;
    }

    while (i < len && o < outSize - 8)
    {
        /* 图片 ![alt](url) */
        if (line[i] == '!' && i + 1 < len && line[i + 1] == '[')
        {
            int j = i + 2;

            while (j < len && line[j] != ']') ++j;

            if (j < len && j + 1 < len && line[j + 1] == '(')
            {
                int m = j + 2;

                while (m < len && line[m] != ')') ++m;

                if (m < len)
                {
                    if (o < outSize - 4) out[o++] = '[';
                    AppendWideAsUtf8(imgNote, out, outSize, &o);
                    if (o < outSize - 4) out[o++] = ']';

                    i = m + 1;
                    continue;
                }
            }
        }

        /* 链接 [文字](url)：只留文字 */
        if (line[i] == '[')
        {
            int j = i + 1, m;

            while (j < len && line[j] != ']') ++j;

            if (j < len && j + 1 < len && line[j + 1] == '(')
            {
                m = j + 2;
                while (m < len && line[m] != ')') ++m;

                if (m < len)
                {
                    int t;

                    for (t = i + 1; t < j && o < outSize - 4; ++t)
                        out[o++] = line[t];

                    i = m + 1;
                    continue;
                }
            }
        }

        /* 行内代码 */
        if (line[i] == '`')
        {
            ++i;
            continue;
        }

        /* 成对的星号（强调） */
        if (line[i] == '*' && stars >= 2)
        {
            ++i;
            continue;
        }

        /* 删除线 ~~ */
        if (line[i] == '~' && i + 1 < len && line[i + 1] == '~')
        {
            i += 2;
            continue;
        }

        out[o++] = line[i++];
    }

    *used = o;
}

/* 去掉首尾的空行/空白，返回能用的起点 */
static char *TrimBlank(char *s)
{
    char *e;

    while (*s == '\n' || *s == '\r' || *s == ' ' || *s == '\t')
        ++s;

    e = s + strlen(s);

    while (e > s && (e[-1] == '\n' || e[-1] == '\r' || e[-1] == ' ' || e[-1] == '\t'))
        --e;

    *e = '\0';
    return s;
}

/* 中文字符占可见字符的百分比：用来认哪一段是中文，而不是假设谁在前 */
static int CjkScore(const char *utf8)
{
    int total = 0, cjk = 0;

    while (*utf8 != '\0')
    {
        unsigned char c = (unsigned char)*utf8;

        if (c < 0x80)
        {
            if (c > ' ')
                ++total;

            ++utf8;
        }
        else if ((c & 0xE0) == 0xC0)
        {
            utf8 += 2;
            ++total;
        }
        else if ((c & 0xF0) == 0xE0)
        {
            if (c >= 0xE4 && c <= 0xE9)     /* 三字节里 U+4E00~U+9FFF 的头字节 */
                ++cjk;

            utf8 += 3;
            ++total;
        }
        else if ((c & 0xF8) == 0xF0)
        {
            utf8 += 4;
            ++total;
        }
        else
        {
            ++utf8;
        }
    }

    return (total == 0) ? 0 : (cjk * 100 / total);
}

static WCHAR *Utf8ToWide(const char *s)
{
    WCHAR *w;
    int    n;

    if (s == NULL || s[0] == '\0')
        return NULL;

    n = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);

    if (n <= 0)
        return NULL;

    w = (WCHAR *)malloc((size_t)n * sizeof(WCHAR));

    if (w == NULL)
        return NULL;

    if (MultiByteToWideChar(CP_UTF8, 0, s, -1, w, n) <= 0)
    {
        free(w);
        return NULL;
    }

    return w;
}

/*
 * 正文切成中英两段：以第一条单独成行的 --- 为界，各段去掉 Markdown 标记，
 * 再按 CJK 比例判断哪段是中文。返回的两段可能有一个是 NULL。
 */
static void SplitReleaseBody(const char *body, WCHAR **outZh, WCHAR **outEn)
{
    char       *segA = (char *)malloc(BODY_SEG_SIZE);
    char       *segB = (char *)malloc(BODY_SEG_SIZE);
    int         lenA = 0, lenB = 0;
    BOOL        second = FALSE;
    const char *p = body;
    const WCHAR *imgNote = T(S_UPDATE_IMAGE_NOTE);

    *outZh = NULL;
    *outEn = NULL;

    if (segA == NULL || segB == NULL)
    {
        free(segA);
        free(segB);
        return;
    }

    segA[0] = '\0';
    segB[0] = '\0';

    while (*p != '\0')
    {
        const char *eol = p;
        int         len;

        while (*eol != '\0' && *eol != '\n')
            ++eol;

        len = (int)(eol - p);

        if (!second && IsMdRule(p, len))
        {
            second = TRUE;              /* 分割线之后都算第二段 */
        }
        else
        {
            char *dst  = second ? segB : segA;
            int  *used = second ? &lenB : &lenA;

            if (IsMdRule(p, len))
            {
                if (*used < BODY_SEG_SIZE - 2)      /* 第二段里再见到就留个空行 */
                    dst[(*used)++] = '\n';
            }
            else
            {
                CleanMdLine(p, len, imgNote, dst, BODY_SEG_SIZE, used);

                if (*used < BODY_SEG_SIZE - 2)
                    dst[(*used)++] = '\n';
            }
        }

        p = (*eol == '\0') ? eol : eol + 1;
    }

    segA[lenA] = '\0';
    segB[lenB] = '\0';

    if (CjkScore(segA) >= CjkScore(segB))
    {
        *outZh = Utf8ToWide(TrimBlank(segA));
        *outEn = Utf8ToWide(TrimBlank(segB));
    }
    else
    {
        *outZh = Utf8ToWide(TrimBlank(segB));
        *outEn = Utf8ToWide(TrimBlank(segA));
    }

    free(segA);
    free(segB);
}

/* --- 拉一次最新 release --------------------------------------------------- */

static BOOL FetchLatestBuild(const WinHttpApi *api, DWORD *outBuild,
                             char *outBody, int bodySize)
{
    HINTERNET hSession, hConnect, hRequest;
    char      resp[BODY_RAW_SIZE];
    char      tag[64];
    DWORD     status = 0, len = sizeof(status), total = 0;
    BOOL      ok = FALSE;

    *outBuild = 0;

    if (outBody != NULL && bodySize > 0)
        outBody[0] = '\0';

    hSession = api->Open(L"ScreenOffLock/" APP_VERSION,
                         WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                         WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);

    if (hSession == NULL)
        return FALSE;

    /* 解析、连接、发送、接收各给个上限，网络卡住时线程不会一直挂着 */
    api->SetTimeouts(hSession, UPDATE_TIMEOUT, UPDATE_TIMEOUT,
                     UPDATE_TIMEOUT, UPDATE_TIMEOUT);

    /*
     * GitHub 已经不吃 TLS 1.0/1.1 了，显式打开 TLS 1.2。注意这个选项必须设在
     * session 句柄上；设在 request 句柄上只会返回 12018（句柄类型不对）。
     */
    {
        DWORD proto = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;

        api->SetOption(hSession, WINHTTP_OPTION_SECURE_PROTOCOLS, &proto, sizeof(proto));
    }

    hConnect = api->Connect(hSession, UPDATE_API_HOST, INTERNET_DEFAULT_HTTPS_PORT, 0);

    if (hConnect != NULL)
    {
        hRequest = api->OpenRequest(hConnect, L"GET", UPDATE_API_PATH, NULL,
                                    WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                    WINHTTP_FLAG_SECURE);

        if (hRequest != NULL)
        {
            if (api->SendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                 WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
                api->ReceiveResponse(hRequest, NULL) &&
                api->QueryHeaders(hRequest,
                                  WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                  WINHTTP_HEADER_NAME_BY_INDEX,
                                  &status, &len, WINHTTP_NO_HEADER_INDEX) &&
                status == 200)
            {
                for (;;)
                {
                    DWORD read = 0;

                    if (total + 1 >= sizeof(resp))
                        break;          /* 响应比缓冲大就只认前面这些 */

                    if (!api->ReadData(hRequest, resp + total,
                                       (DWORD)(sizeof(resp) - total - 1), &read) || read == 0)
                        break;

                    total += read;
                }

                resp[total] = '\0';

                if (ExtractTagName(resp, tag, sizeof(tag)))
                {
                    *outBuild = FirstNumberA(tag);
                    ok = (*outBuild != 0) ? TRUE : FALSE;
                }

                if (outBody != NULL && bodySize > 0)
                    ExtractReleaseBody(resp, outBody, bodySize);
            }

            api->CloseHandle(hRequest);
        }

        api->CloseHandle(hConnect);
    }

    api->CloseHandle(hSession);
    return ok;
}

/* --- 后台线程 ------------------------------------------------------------- */

static volatile LONG g_updateRunning   = 0;     /* 有线程在跑 */
static volatile LONG g_updateCancel    = 0;     /* 用户取消了，结果作废 */
static volatile LONG g_updateOk        = 0;     /* 成功拿到远端版本 */
static volatile LONG g_updateBuild     = 0;     /* 远端 build 号 */
static HWND          g_updateNotify    = NULL;  /* 结果投给哪个窗口 */
static BOOL          g_updateRetryMode = FALSE; /* 失败要不要重试 */
static WCHAR        *g_updateTextZh    = NULL;  /* 更新说明：中文段（已去 Markdown） */
static WCHAR        *g_updateTextEn    = NULL;  /* 更新说明：英文段 */

/* 当前界面语言对应的那段说明；缺一段就退回另一段 */
static const WCHAR *CurrentUpdateText(void)
{
    if (g_lang == LANG_ZH)
        return (g_updateTextZh != NULL) ? g_updateTextZh : g_updateTextEn;

    return (g_updateTextEn != NULL) ? g_updateTextEn : g_updateTextZh;
}

/* 可打断的等待：用户一取消，就不用把这几秒睡完 */
static void UpdateSleep(DWORD ms)
{
    DWORD waited = 0;

    while (waited < ms && ATOMIC_READ(&g_updateCancel) == 0)
    {
        Sleep(100);
        waited += 100;
    }
}

static DWORD WINAPI UpdateThreadProc(LPVOID param)
{
    WinHttpApi api;
    char      *rawBody  = (char *)malloc(BODY_RAW_SIZE);
    WCHAR     *textZh   = NULL;
    WCHAR     *textEn   = NULL;
    DWORD      build    = 0;
    BOOL       ok       = FALSE;
    int        tries    = 0;
    int        maxTries = g_updateRetryMode ? UPDATE_TRY_COUNT : 1;

    (void)param;

    if (LoadWinHttp(&api))
    {
        while (tries < maxTries)
        {
            if (ATOMIC_READ(&g_updateCancel) != 0)
                break;

            ++tries;

            if (FetchLatestBuild(&api, &build, rawBody, BODY_RAW_SIZE))
            {
                ok = TRUE;
                break;
            }

            if (tries < maxTries)
                UpdateSleep(UPDATE_RETRY_WAIT);
        }
    }

    /* 说明只在成功那一次解析；失败时留空，免得显示上一次的旧内容 */
    if (ok && rawBody != NULL)
        SplitReleaseBody(rawBody, &textZh, &textEn);

    if (rawBody != NULL)
        free(rawBody);

    {
        WCHAR *oldZh = (WCHAR *)InterlockedExchangePointer((PVOID volatile *)&g_updateTextZh, textZh);
        WCHAR *oldEn = (WCHAR *)InterlockedExchangePointer((PVOID volatile *)&g_updateTextEn, textEn);

        if (oldZh != NULL) free(oldZh);
        if (oldEn != NULL) free(oldEn);
    }

    InterlockedExchange(&g_updateOk,      ok ? 1 : 0);
    InterlockedExchange(&g_updateBuild,   (LONG)build);
    InterlockedExchange(&g_updateRunning, 0);

    if (g_updateNotify != NULL)
        PostMessageW(g_updateNotify, WM_APP_UPDATE_DONE, 0, 0);

    return 0;
}

/*
 * 起一次后台检查。notifyWnd 是结果投给哪个窗口；retryMode 为 TRUE 时失败会按
 * UPDATE_TRY_COUNT 重试（启动时的静默检查用这个）。已经有线程在跑就只把收件人
 * 换掉，不重复起线程。
 */
static void StartUpdateCheck(HWND notifyWnd, BOOL retryMode)
{
    HANDLE hThread;

    g_updateNotify = notifyWnd;

    if (InterlockedCompareExchange(&g_updateRunning, 1, 0) != 0)
        return;

    InterlockedExchange(&g_updateCancel, 0);
    g_updateRetryMode = retryMode;

    hThread = CreateThread(NULL, 0, UpdateThreadProc, NULL, 0, NULL);

    if (hThread == NULL)
    {
        InterlockedExchange(&g_updateRunning, 0);
        return;
    }

    CloseHandle(hThread);               /* 线程自己会跑完，用不着它的句柄 */
}

/* 启动后静默查一次 */
static void StartStartupUpdateCheck(void)
{
    StartUpdateCheck(g_hWnd, TRUE);
}

/*
 * 启动检查的结果：确实有新版本、而且这个版本没被“忽略当前版本”记下过，才弹
 * 提示。失败、没更新、已经忽略过，都什么都不做。
 */
static void OnStartupUpdateResult(void)
{
    DWORD remote, skipped;

    if (ATOMIC_READ(&g_updateOk) == 0)
        return;                         /* 静默失败 */

    remote = (DWORD)ATOMIC_READ(&g_updateBuild);

    if (remote == 0 || remote <= LocalBuildNumber())
        return;

    /* 记的是被忽略过的那个版本号：线上再出更高的版本时，提示会自己回来 */
    skipped = LoadDwordSetting(REG_VALUE_SKIPVER, 0);

    if (remote <= skipped)
        return;

    /* 已经有别的窗口开着就别叠上去，下次启动再说 */
    if (g_hWarnWnd != NULL || g_hAboutWnd != NULL || g_hUpdateWarnWnd != NULL)
        return;

    ShowUpdateAvailableWindow();
}

/* ---------------------------------------------------------------------------
 * 灰色信息块
 *
 * 和「电源设置」里那块一个样式：淡灰圆角衬底 + 小一号的字。
 * 更新说明长短不定，所以窗口高度得按它的实际行数算出来。
 * ------------------------------------------------------------------------ */

/* 文本在给定实际宽度下占多高（实际像素）；没有文本就是 0 */
static int PanelTextHeight(HDC hdc, const WCHAR *text, int cxPhys)
{
    RECT  r;
    HFONT hOld;

    if (text == NULL)
        return 0;

    r.left   = 0;
    r.top    = 0;
    r.right  = cxPhys;
    r.bottom = 0;

    hOld = (HFONT)SelectObject(hdc, g_fontSmall);
    DrawTextW(hdc, text, -1, &r, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
    SelectObject(hdc, hOld);

    return r.bottom - r.top;
}

/* 同样的高度，换算成逻辑像素 */
static int PanelTextHeightLog(HDC hdc, const WCHAR *text, int cxLog)
{
    return MulDiv(PanelTextHeight(hdc, text, AS(cxLog)), 96, (int)g_uiDpi);
}

/* 只算不画：内容块要占多高（逻辑像素），没有内容返回 0 */
static int InfoPanelHeightLog(HDC hdc, const WCHAR *text, int cxLog)
{
    if (text == NULL || text[0] == L'\0')
        return 0;

    return PanelTextHeightLog(hdc, text, cxLog - 28) + 24;   /* 上下各留 12 */
}

/* 把内容块画出来，返回它占掉的高度（逻辑像素） */
static int DrawInfoPanel(HDC hdc, const WCHAR *text, int x, int y, int cx)
{
    HBRUSH hbr, hOldBr;
    HPEN   hpn, hOldPn;
    HFONT  hOld;
    RECT   r;
    int    cy = InfoPanelHeightLog(hdc, text, cx);

    if (cy == 0)
        return 0;

    hbr    = CreateSolidBrush(RGB(0xF5, 0xF5, 0xF5));
    hpn    = CreatePen(PS_SOLID, 1, RGB(0xE8, 0xE8, 0xE8));
    hOldBr = (HBRUSH)SelectObject(hdc, hbr);
    hOldPn = (HPEN)SelectObject(hdc, hpn);

    RoundRect(hdc, AS(x), AS(y), AS(x + cx), AS(y + cy), AS(12), AS(12));

    SelectObject(hdc, hOldBr);
    SelectObject(hdc, hOldPn);
    DeleteObject(hbr);
    DeleteObject(hpn);

    r.left   = AS(x + 14);
    r.top    = AS(y + 12);
    r.right  = AS(x + cx - 14);
    r.bottom = AS(y + cy - 12);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));

    hOld = (HFONT)SelectObject(hdc, g_fontSmall);
    DrawTextW(hdc, text, -1, &r, DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);
    SelectObject(hdc, hOld);

    return cy;
}

/* 只画灰块的框；高度自己给，文字由调用方按行画 */
static void DrawInfoPanelFrame(HDC hdc, int x, int y, int cx, int cy)
{
    HBRUSH hbr    = CreateSolidBrush(RGB(0xF5, 0xF5, 0xF5));
    HPEN   hpn    = CreatePen(PS_SOLID, 1, RGB(0xE8, 0xE8, 0xE8));
    HBRUSH hOldBr = (HBRUSH)SelectObject(hdc, hbr);
    HPEN   hOldPn = (HPEN)SelectObject(hdc, hpn);

    RoundRect(hdc, AS(x), AS(y), AS(x + cx), AS(y + cy), AS(12), AS(12));

    SelectObject(hdc, hOldBr);
    SelectObject(hdc, hOldPn);
    DeleteObject(hbr);
    DeleteObject(hpn);
}

/* 按客户区逻辑尺寸调整窗口，并保持屏幕居中 */
static void ResizeDialogCentered(HWND hWnd, int cxLogical, int cyLogical)
{
    RECT rc, wa;
    int  w, h, x, y;

    rc.left   = 0;
    rc.top    = 0;
    rc.right  = AS(cxLogical);
    rc.bottom = AS(cyLogical);
    AdjustWindowRectEx(&rc, WS_CAPTION | WS_SYSMENU | WS_POPUP, FALSE, 0);

    w = rc.right - rc.left;
    h = rc.bottom - rc.top;

    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    x = wa.left + ((wa.right - wa.left) - w) / 2;
    y = wa.top + ((wa.bottom - wa.top) - h) / 2;

    SetWindowPos(hWnd, NULL, x, y, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
}

/* ---------------------------------------------------------------------------
 * 启动时的更新提示窗
 *
 * 跟“设置提醒”那个窗一个路数：自绘图标 + 文案 + 自绘复选框。勾上
 * “忽略当前版本”并关掉窗口，就把这次看到的远端版本号记下来，以后不再为
 * 这个版本提示；等线上出了更高的版本，提示会自己回来。
 * ------------------------------------------------------------------------ */

#define UPDWARN_CLASS     L"ScreenOffLock_UpdateNotice"
#define IDC_UPDWARN_GO    2401
#define IDC_UPDWARN_CLOSE 2402

#define UPDWARN_CX        480
#define UPDWARN_PANEL_Y   84            /* 内容块从哪儿开始（逻辑像素） */

static BOOL g_updateWarnSkip = FALSE;       /* 勾了“忽略当前版本” */
static int  g_warnPanelH     = 0;           /* 内容块高度（逻辑像素），0 = 没有内容 */
static int  g_warnCbY        = 0;           /* 复选框的 y（逻辑像素） */

/* 客户区要多高：复选框 + 间距 + 按钮 + 下边距 */
static int UpdateWarnClientHeight(void)
{
    return g_warnCbY + 20 + 18 + 32 + 18;
}

/* 按说明文本的长度算好内容块高度和复选框位置 */
static void UpdateWarnLayout(void)
{
    HDC hdc = GetDC(NULL);

    g_warnPanelH = InfoPanelHeightLog(hdc, CurrentUpdateText(), UPDWARN_CX - 56);
    g_warnCbY    = UPDWARN_PANEL_Y + g_warnPanelH + 14;

    ReleaseDC(NULL, hdc);
}

static void UpdateWarnPaint(HWND hWnd)
{
    PAINTSTRUCT ps;
    HDC         hdc = BeginPaint(hWnd, &ps);
    RECT        rc;
    COLORREF    clrText   = GetSysColor(COLOR_WINDOWTEXT);
    COLORREF    clrAccent = RGB(0x0F, 0x6C, 0xBD);
    WCHAR       buf[160];

    GetClientRect(hWnd, &rc);
    FillRect(hdc, &rc, GetSysColorBrush(COLOR_WINDOW));
    SetBkMode(hdc, TRANSPARENT);

    if (g_fontGlyph != NULL)
    {
        HFONT hOld = (HFONT)SelectObject(hdc, g_fontGlyph);
        RECT  r;

        SetTextColor(hdc, clrAccent);
        r.left   = AS(28);
        r.top    = AS(26);
        r.right  = AS(28 + 40);
        r.bottom = AS(26 + 40);

        DrawTextW(hdc, L"\xE946", -1, &r,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        SelectObject(hdc, hOld);
    }

    wsprintfW(buf, T(S_UPDATE_FOUND_TITLE_FMT), (DWORD)ATOMIC_READ(&g_updateBuild));
    AboutText(hdc, g_fontBody, clrText, buf, 80, 28, UPDWARN_CX - 108);

    wsprintfW(buf, T(S_UPDATE_FOUND_BODY_FMT), LocalBuildNumber());
    AboutText(hdc, g_fontSmall, RGB(0x60, 0x5E, 0x5C), buf, 80, 54, UPDWARN_CX - 108);

    /* 更新说明（挑当前语言那一段），放在淡灰衬底里 */
    if (g_warnPanelH > 0)
        DrawInfoPanel(hdc, CurrentUpdateText(), 28, UPDWARN_PANEL_Y, UPDWARN_CX - 56);

    DrawCheckbox(hdc, AS(28), AS(g_warnCbY), AS(20), g_updateWarnSkip);
    AboutText(hdc, g_fontBody, clrText, T(S_UPDATE_IGNORE), 58, g_warnCbY + 2, 260);

    EndPaint(hWnd, &ps);
}

static void CreateUpdateWarnControls(HWND hWnd)
{
    HDC  hdc = GetDC(hWnd);
    RECT rc;
    HWND hCtl;
    int  bh = AS(32), gap = AS(10), y, x, wGo, wClose;

    GetClientRect(hWnd, &rc);

    wGo    = TextWidth(hdc, g_fontBody, T(S_BTN_GO_UPDATE)) + AS(36);
    wClose = TextWidth(hdc, g_fontBody, T(S_BTN_CLOSE))     + AS(36);

    y = rc.bottom - AS(18) - bh;

    /* 「关闭」在最右 */
    x = rc.right - AS(24) - wClose;

    hCtl = CreateWindowExW(0, L"BUTTON", T(S_BTN_CLOSE),
                           WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                           x, y, wClose, bh, hWnd,
                           (HMENU)(INT_PTR)IDC_UPDWARN_CLOSE, g_hInstance, NULL);
    if (hCtl != NULL)
        SendMessageW(hCtl, WM_SETFONT, (WPARAM)g_fontBody, TRUE);

    /* 「前往更新」在它左边，作为默认按钮 */
    x -= (gap + wGo);

    hCtl = CreateWindowExW(0, L"BUTTON", T(S_BTN_GO_UPDATE),
                           WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                           x, y, wGo, bh, hWnd,
                           (HMENU)(INT_PTR)IDC_UPDWARN_GO, g_hInstance, NULL);
    if (hCtl != NULL)
    {
        SendMessageW(hCtl, WM_SETFONT, (WPARAM)g_fontBody, TRUE);
        SetFocus(hCtl);
    }

    ReleaseDC(hWnd, hdc);
}

static LRESULT CALLBACK UpdateWarnWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
        CreateAboutFonts();
        g_updateWarnSkip = FALSE;
        CreateUpdateWarnControls(hWnd);
        ApplyRoundCorners(hWnd);
        return 0;

    case WM_ERASEBKGND:
        return 1;

    /* 让复选框的背景跟窗口的白色一致，否则会露出一块系统灰 */
    case WM_CTLCOLORBTN:
        SetBkColor((HDC)wParam, GetSysColor(COLOR_WINDOW));
        return (LRESULT)GetSysColorBrush(COLOR_WINDOW);

    case WM_PAINT:
        UpdateWarnPaint(hWnd);
        return 0;

    case WM_LBUTTONDOWN:
        {
            int mx = (int)(short)LOWORD(lParam);
            int my = (int)(short)HIWORD(lParam);
            int bx = AS(28), by = AS(g_warnCbY), bs = AS(20), pad = AS(6);

            /* 点方框本身或右边的文字都算 */
            if (mx >= bx - pad && mx <= bx + bs + AS(160) &&
                my >= by - pad && my <= by + bs + pad)
            {
                g_updateWarnSkip = !g_updateWarnSkip;
                InvalidateRect(hWnd, NULL, FALSE);
            }
        }
        return 0;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_UPDWARN_GO)
        {
            ShellExecuteW(NULL, L"open", UPDATE_PAGE_URL, NULL, NULL, SW_SHOWNORMAL);
            DestroyWindow(hWnd);
        }
        else if (LOWORD(wParam) == IDC_UPDWARN_CLOSE)
        {
            DestroyWindow(hWnd);
        }
        return 0;

    case WM_CLOSE:
        DestroyWindow(hWnd);
        return 0;

    case WM_DESTROY:
        /* 勾了“忽略当前版本”就把这个远端版本号记下来；从哪条路关的都算数 */
        if (g_updateWarnSkip)
            SaveDwordSetting(REG_VALUE_SKIPVER, (DWORD)ATOMIC_READ(&g_updateBuild));

        g_hUpdateWarnWnd = NULL;
        return 0;

    default:
        break;
    }

    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

static void ShowUpdateAvailableWindow(void)
{
    WNDCLASSEXW wc;
    HWND        hWnd;

    if (g_hUpdateWarnWnd != NULL)
    {
        SetForegroundWindow(g_hUpdateWarnWnd);
        return;
    }

    SetupDialogClass(&wc, UpdateWarnWndProc, UPDWARN_CLASS);

    g_uiDpi = QueryDpi(g_hWnd);
    UpdateWarnLayout();

    hWnd = CreateCenteredDialog(UPDWARN_CLASS, APP_NAME,
                                UPDWARN_CX, UpdateWarnClientHeight(),
                                WS_CAPTION | WS_SYSMENU | WS_POPUP);

    if (hWnd == NULL)
        return;

    g_hUpdateWarnWnd = hWnd;

    ShowWindow(hWnd, SW_SHOW);
    UpdateWindow(hWnd);
    SetForegroundWindow(hWnd);
}

/* ---------------------------------------------------------------------------
 * 手动检查更新窗口
 *
 * 一个窗口走完整个过程：先显示“正在检查更新”，结果回来后就地变成
 * “发现更新 / 当前已是最新 / 更新检查失败”，按钮文字和动作跟着换。
 * ------------------------------------------------------------------------ */

#define UPDCHK_CLASS      L"ScreenOffLock_UpdateCheck"
#define IDC_UPDCHK_ACTION 2501

#define UPDCHK_CX         480           /* 和启动时的提示窗同宽 */
#define UPDCHK_PANEL_Y    84            /* 内容块从哪儿开始（逻辑像素） */

typedef enum
{
    UPDCHK_CHECKING = 0,
    UPDCHK_FOUND,
    UPDCHK_LATEST,
    UPDCHK_FAILED
} UpdateCheckState;

static UpdateCheckState g_updateChkState = UPDCHK_CHECKING;
static int              g_chkPanelH      = 0;   /* 内容块高度（逻辑像素） */

/* 只有“发现更新”才有说明可显示 */
static const WCHAR *UpdateCheckPanelText(void)
{
    return (g_updateChkState == UPDCHK_FOUND) ? CurrentUpdateText() : NULL;
}

/* 文字区占到哪一行：标题下面那一行是当前版本，其余状态只有标题 */
static int UpdateCheckTextBottom(void)
{
    switch (g_updateChkState)
    {
    case UPDCHK_FOUND:
    case UPDCHK_LATEST: return 80;
    default:            return 74;
    }
}

/* 客户区要多高 */
static int UpdateCheckClientHeight(void)
{
    int bottom = UpdateCheckTextBottom();

    if (g_chkPanelH > 0)
        bottom = UPDCHK_PANEL_Y + g_chkPanelH;      /* 一直算到内容块底 */

    return bottom + 16 + 32 + 18;
}

/* 按钮靠右下角，和启动时那个提示窗摆法一致 */
static void UpdateCheckPlaceButton(HWND hWnd)
{
    HWND hBtn = GetDlgItem(hWnd, IDC_UPDCHK_ACTION);
    RECT rc, br;
    int  bw;

    if (hBtn == NULL)
        return;

    GetClientRect(hWnd, &rc);
    GetWindowRect(hBtn, &br);
    bw = br.right - br.left;

    SetWindowPos(hBtn, NULL,
                 rc.right - AS(24) - bw, rc.bottom - AS(18) - AS(32),
                 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

/* 说明有没有、窗口就多高：状态一变就重排一次 */
static void UpdateCheckRelayout(HWND hWnd)
{
    HDC hdc = GetDC(hWnd);

    g_chkPanelH = InfoPanelHeightLog(hdc, UpdateCheckPanelText(), UPDCHK_CX - 56);
    ReleaseDC(hWnd, hdc);

    ResizeDialogCentered(hWnd, UPDCHK_CX, UpdateCheckClientHeight());
    UpdateCheckPlaceButton(hWnd);
}

static void UpdateCheckActionText(HWND hWnd)
{
    HWND         hBtn = GetDlgItem(hWnd, IDC_UPDCHK_ACTION);
    const WCHAR *text;

    switch (g_updateChkState)
    {
    case UPDCHK_FOUND:  text = T(S_BTN_GO_UPDATE); break;
    case UPDCHK_LATEST: text = T(S_BTN_DONE);      break;
    case UPDCHK_FAILED: text = T(S_BTN_CLOSE);     break;
    default:            text = T(S_BTN_CANCEL);    break;
    }

    if (hBtn != NULL)
    {
        SetWindowTextW(hBtn, text);
        InvalidateRect(hBtn, NULL, TRUE);
    }
}

static void UpdateCheckSetState(HWND hWnd, UpdateCheckState state)
{
    g_updateChkState = state;
    UpdateCheckActionText(hWnd);
    UpdateCheckRelayout(hWnd);          /* 说明有没有、窗口就跟着变高变矮 */
    InvalidateRect(hWnd, NULL, FALSE);
}

static void UpdateCheckPaint(HWND hWnd)
{
    PAINTSTRUCT  ps;
    HDC          hdc = BeginPaint(hWnd, &ps);
    RECT         rc, r;
    HFONT        hOld;
    const WCHAR *msg;
    WCHAR        buf[96];

    GetClientRect(hWnd, &rc);
    FillRect(hdc, &rc, GetSysColorBrush(COLOR_WINDOW));
    SetBkMode(hdc, TRANSPARENT);

    switch (g_updateChkState)
    {
    case UPDCHK_FOUND:                      /* 标题直接带新版本号，和启动提示窗一字不差 */
        wsprintfW(buf, T(S_UPDATE_FOUND_TITLE_FMT), (DWORD)ATOMIC_READ(&g_updateBuild));
        msg = buf;
        break;

    case UPDCHK_LATEST: msg = T(S_UPDATE_LATEST);    break;
    case UPDCHK_FAILED: msg = T(S_UPDATE_FAILED);    break;
    default:            msg = T(S_UPDATE_CHECKING);  break;
    }

    /* 版式和启动时的更新提示窗一致：左边一个图标，文字都左对齐 */
    if (g_fontGlyph != NULL)
    {
        HFONT hOldGlyph = (HFONT)SelectObject(hdc, g_fontGlyph);
        RECT  ri;

        SetTextColor(hdc, RGB(0x0F, 0x6C, 0xBD));

        ri.left   = AS(28);
        ri.top    = AS(26);
        ri.right  = AS(28 + 40);
        ri.bottom = AS(26 + 40);

        DrawTextW(hdc, L"\xE946", -1, &ri,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        SelectObject(hdc, hOldGlyph);
    }

    hOld = (HFONT)SelectObject(hdc, g_fontBody);
    SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));

    r.left   = AS(80);
    r.top    = AS(28);
    r.right  = rc.right - AS(28);
    r.bottom = r.top + AS(46);

    DrawTextW(hdc, msg, -1, &r,
              DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);

    SelectObject(hdc, hOld);

    if (g_updateChkState == UPDCHK_FOUND || g_updateChkState == UPDCHK_LATEST)
    {
        hOld = (HFONT)SelectObject(hdc, g_fontSmall);
        SetTextColor(hdc, RGB(0x60, 0x5E, 0x5C));

        /* 当前版本：两种状态都写在标题下面那一行 */
        wsprintfW(buf, T(S_UPDATE_FOUND_BODY_FMT), LocalBuildNumber());

        r.top    = AS(56);
        r.bottom = r.top + AS(24);

        DrawTextW(hdc, buf, -1, &r,
                  DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);

        SelectObject(hdc, hOld);

        /* 再下面是更新说明，放在淡灰衬底里 */
        if (g_updateChkState == UPDCHK_FOUND && g_chkPanelH > 0)
            DrawInfoPanel(hdc, CurrentUpdateText(), 28, UPDCHK_PANEL_Y, UPDCHK_CX - 56);
    }

    EndPaint(hWnd, &ps);
}

static void CreateUpdateCheckControls(HWND hWnd)
{
    HWND hCtl;
    HDC  hdc = GetDC(hWnd);
    int  bh = AS(32), bw;

    /* 宽度跟着四态里最长的那个按钮文字走，再留一点内边距 */
    bw = TextWidth(hdc, g_fontBody, T(S_BTN_GO_UPDATE));
    {
        int w;

        w = TextWidth(hdc, g_fontBody, T(S_BTN_CANCEL));
        if (w > bw) bw = w;

        w = TextWidth(hdc, g_fontBody, T(S_BTN_DONE));
        if (w > bw) bw = w;

        w = TextWidth(hdc, g_fontBody, T(S_BTN_CLOSE));
        if (w > bw) bw = w;
    }

    ReleaseDC(hWnd, hdc);

    bw += AS(36);

    /* 位置先随便给，随后由 UpdateCheckPlaceButton 居中贴底 */
    hCtl = CreateWindowExW(0, L"BUTTON", L"",
                           WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                           0, 0, bw, bh, hWnd,
                           (HMENU)(INT_PTR)IDC_UPDCHK_ACTION, g_hInstance, NULL);
    if (hCtl != NULL)
    {
        SendMessageW(hCtl, WM_SETFONT, (WPARAM)g_fontBody, TRUE);
        SetFocus(hCtl);
    }

    UpdateCheckPlaceButton(hWnd);
}

static LRESULT CALLBACK UpdateCheckWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
        CreateAboutFonts();
        g_updateChkState = UPDCHK_CHECKING;
        CreateUpdateCheckControls(hWnd);
        UpdateCheckActionText(hWnd);
        ApplyRoundCorners(hWnd);

        /* 启动那次检查还在跑就直接等它的结果，否则现在起一个 */
        if (ATOMIC_READ(&g_updateRunning) != 0)
            g_updateNotify = hWnd;
        else
            StartUpdateCheck(hWnd, FALSE);
        return 0;

    case WM_APP_UPDATE_DONE:
        /* 用户已经按过“取消”就什么都不改 */
        if (ATOMIC_READ(&g_updateCancel) == 0)
        {
            if (ATOMIC_READ(&g_updateOk) == 0)
                UpdateCheckSetState(hWnd, UPDCHK_FAILED);
            else if ((DWORD)ATOMIC_READ(&g_updateBuild) > LocalBuildNumber())
                UpdateCheckSetState(hWnd, UPDCHK_FOUND);
            else
                UpdateCheckSetState(hWnd, UPDCHK_LATEST);
        }
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
        UpdateCheckPaint(hWnd);
        return 0;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_UPDCHK_ACTION)
        {
            switch (g_updateChkState)
            {
            case UPDCHK_FOUND:          /* 前往更新 */
                ShellExecuteW(NULL, L"open", UPDATE_PAGE_URL, NULL, NULL, SW_SHOWNORMAL);
                DestroyWindow(hWnd);
                break;

            case UPDCHK_CHECKING:       /* 取消：这次结果不要了 */
                InterlockedExchange(&g_updateCancel, 1);
                DestroyWindow(hWnd);
                break;

            default:                    /* 已是最新 / 检查失败 */
                DestroyWindow(hWnd);
                break;
            }
        }
        return 0;

    case WM_CLOSE:
        if (g_updateChkState == UPDCHK_CHECKING)
            InterlockedExchange(&g_updateCancel, 1);

        DestroyWindow(hWnd);
        return 0;

    case WM_DESTROY:
        g_hUpdateChkWnd = NULL;

        /* 结果别再投给一个已经没了的窗口 */
        if (g_updateNotify == hWnd)
            g_updateNotify = NULL;
        return 0;

    default:
        break;
    }

    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

static void ShowUpdateCheckWindow(void)
{
    WNDCLASSEXW wc;
    HWND        hWnd;

    if (g_hUpdateChkWnd != NULL)        /* 已经开着就置前 */
    {
        SetForegroundWindow(g_hUpdateChkWnd);
        return;
    }

    SetupDialogClass(&wc, UpdateCheckWndProc, UPDCHK_CLASS);

    g_uiDpi = QueryDpi(g_hWnd);
    g_chkPanelH = 0;                    /* 新窗口先从“正在检查”的小尺寸开始 */

    hWnd = CreateCenteredDialog(UPDCHK_CLASS, T(S_UPDATE_TITLE),
                                UPDCHK_CX, UpdateCheckClientHeight(),
                                WS_CAPTION | WS_SYSMENU | WS_POPUP);

    if (hWnd == NULL)
        return;

    g_hUpdateChkWnd = hWnd;

    ShowWindow(hWnd, SW_SHOW);
    UpdateWindow(hWnd);
    SetForegroundWindow(hWnd);
}

/* ---------------------------------------------------------------------------
 * 托盘右键菜单
 *
 * 菜单命令统一用 PostMessage 回投，避免在菜单还处于弹出状态时弹出对话框。
 * ------------------------------------------------------------------------ */

static void ShowContextMenu(void)
{
    HMENU hMenu = CreatePopupMenu();
    POINT pt;

    if (!hMenu)
        return;

    AppendMenuW(hMenu, MF_STRING | (g_lockOnScreenOff ? MF_CHECKED : MF_UNCHECKED),
                IDM_LOCK_ON_OFF, T(S_MENU_LOCK_SCREEN));

    /* 台式机没有盖子设备，这一项没有意义，直接置灰 */
    AppendMenuW(hMenu,
                MF_STRING
                    | (g_lockOnLidClose ? MF_CHECKED : MF_UNCHECKED)
                    | (g_hasLid ? 0 : MF_GRAYED),
                IDM_LID_LOCK, T(S_MENU_LOCK_LID));

    /* 延迟锁定：单选子菜单，勾当前档位 */
    {
        HMENU hDelay = CreatePopupMenu();

        if (hDelay)
        {
            int i;

            for (i = 0; i < DELAY_ITEM_COUNT; ++i)
            {
                /* 第一档「立即」和其余档位之间来一条分隔线 */
                if (i == 1)
                    AppendMenuW(hDelay, MF_SEPARATOR, 0, NULL);

                AppendMenuW(hDelay, MF_STRING,
                            kDelayItems[i].commandId, T(kDelayItems[i].strId));
            }

            /* 画成单选圆点；范围里夹着分隔线没关系 */
            CheckMenuRadioItem(hDelay, IDM_DELAY_NOW, IDM_DELAY_30,
                               DelayCommandForSeconds(g_lockDelay), MF_BYCOMMAND);

            AppendMenuW(hMenu, MF_STRING | MF_POPUP,
                        (UINT_PTR)hDelay, T(S_MENU_LOCK_DELAY));
        }
    }

    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);

    /* 每次弹菜单都重读一次，免得和外部改动脱节 */
    AppendMenuW(hMenu,
                MF_STRING | (IsAutoStartEnabled() ? MF_CHECKED : MF_UNCHECKED),
                IDM_AUTOSTART, T(S_MENU_AUTOSTART));

    AppendMenuW(hMenu, MF_STRING, IDM_CHECK, T(S_MENU_CHECK));

    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);

    /* 和「关于」窗口里那个按钮干同一件事 */
    AppendMenuW(hMenu, MF_STRING, IDM_UPDATE_CHECK, T(S_BTN_CHECK_UPDATE));

    AppendMenuW(hMenu, MF_STRING, IDM_ABOUT, T(S_MENU_ABOUT));
    AppendMenuW(hMenu, MF_STRING, IDM_EXIT,  T(S_MENU_EXIT));

    GetCursorPos(&pt);

    /* 必须先置前台，否则菜单在点击别处时不会自动消失 */
    SetForegroundWindow(g_hWnd);

    UINT uCmd = TrackPopupMenu(hMenu,
                               TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY,
                               pt.x, pt.y, 0, g_hWnd, NULL);

    DestroyMenu(hMenu);

    if (uCmd)
        PostMessageW(g_hWnd, WM_COMMAND, (WPARAM)uCmd, 0);
}

/* ---------------------------------------------------------------------------
 * 延迟锁定
 *
 * 屏灭 / 合盖命中之后，如果设了延迟就先起一个计时器，到点再锁；屏幕重新亮起
 * 或盖子重新打开会打断计时，这次就不锁了。
 *
 * 两条路径各自记一个“待定”标记：笔记本合盖时往往关屏和合盖两条通知都会来，
 * 其中一条恢复（比如开盖）只取消它自己那一条，另一条仍照常计时；两条都取消
 * 了才真正拆掉计时器。
 * ------------------------------------------------------------------------ */

static void UpdateLockTimer(void)
{
    if (g_pendingScreenOff || g_pendingLidClose)
    {
        /* 同一个 ID 再 SetTimer 就是重新计时，所以总是按当前档位走 */
        if (SetTimer(g_hWnd, TIMER_LOCK_DELAY, g_lockDelay * 1000u, NULL) == 0)
        {
            /* 计时器起不来（句柄无效、定时器耗尽）就直接锁，
               别把这一次等待整个丢掉 */
            g_pendingScreenOff = FALSE;
            g_pendingLidClose  = FALSE;
            g_lockTimerArmed   = FALSE;
            LockWorkStation();
            return;
        }

        g_lockTimerArmed = TRUE;
    }
    else if (g_lockTimerArmed)
    {
        KillTimer(g_hWnd, TIMER_LOCK_DELAY);
        g_lockTimerArmed = FALSE;
    }
}

/* 屏幕重新亮起：取消“关屏”这条路径的待定锁定 */
static void CancelScreenLock(void)
{
    if (g_pendingScreenOff)
    {
        g_pendingScreenOff = FALSE;
        UpdateLockTimer();
    }
}

/* 盖子重新打开：取消“合盖”这条路径的待定锁定 */
static void CancelLidLock(void)
{
    if (g_pendingLidClose)
    {
        g_pendingLidClose = FALSE;
        UpdateLockTimer();
    }
}

/* 待定锁定的来源 */
typedef enum
{
    LOCK_SOURCE_SCREEN = 0,     /* 屏幕被关掉 */
    LOCK_SOURCE_LID             /* 盖子被合上 */
} LockSource;

/* 命中一次“该锁屏了”：没设延迟就立刻锁，否则记下待定并起计时 */
static void RequestLock(LockSource source)
{
    if (g_lockDelay == 0)
    {
        LockWorkStation();
        return;
    }

    if (source == LOCK_SOURCE_SCREEN)
        g_pendingScreenOff = TRUE;
    else
        g_pendingLidClose  = TRUE;

    UpdateLockTimer();
}

/* 计时到点：按开关和状态再核对一次，然后锁定 */
static void OnLockDelayElapsed(void)
{
    BOOL shouldLock =
        (g_pendingScreenOff && g_lockOnScreenOff
             && g_hasDisplayState && g_lastDisplayState == DISPLAY_STATE_OFF)
        || (g_pendingLidClose && g_lockOnLidClose
             && g_hasLidState && g_lastLidState == LID_STATE_CLOSED);

    g_pendingScreenOff = FALSE;
    g_pendingLidClose  = FALSE;
    UpdateLockTimer();                  /* 标记都清了，顺带拆掉计时器 */

    if (shouldLock)
        LockWorkStation();
}

/* ---------------------------------------------------------------------------
 * 显示器状态变化
 * ------------------------------------------------------------------------ */

static void OnDisplayStateChanged(DWORD newState)
{
    DWORD prevState = g_hasDisplayState ? g_lastDisplayState : DISPLAY_STATE_ON;

    g_lastDisplayState = newState;
    g_hasDisplayState  = TRUE;

    if (newState != DISPLAY_STATE_OFF)
    {
        /* 屏幕又亮了（或转为变暗）：人回来了，这次不锁 */
        CancelScreenLock();
        return;
    }

    /* 只在“由亮转灭”的那一次边沿动作，重复的通知不重复锁屏 */
    if (prevState == DISPLAY_STATE_OFF || !g_lockOnScreenOff)
        return;

    RequestLock(LOCK_SOURCE_SCREEN);
}

/* ---------------------------------------------------------------------------
 * 盖子状态变化
 * ------------------------------------------------------------------------ */

static void OnLidStateChanged(DWORD newState)
{
    DWORD prevState = g_hasLidState ? g_lastLidState : LID_STATE_OPEN;

    g_lastLidState = newState;
    g_hasLidState  = TRUE;

    if (newState != LID_STATE_CLOSED)
    {
        /* 盖子重新打开：取消这次合盖锁定 */
        CancelLidLock();
        return;
    }

    /* 只在“由开转合”的那一次边沿动作 */
    if (prevState == LID_STATE_CLOSED || !g_lockOnLidClose)
        return;

    RequestLock(LOCK_SOURCE_LID);
}

/* ---------------------------------------------------------------------------
 * 窗口过程
 * ------------------------------------------------------------------------ */

static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_TRAYICON:
        /* 只要右键，左键与双击一律忽略 */
        if (LOWORD(lParam) == WM_RBUTTONUP)
            ShowContextMenu();
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDM_LOCK_ON_OFF:
            g_lockOnScreenOff = !g_lockOnScreenOff;
            SaveBoolSetting(REG_VALUE_SCREEN, g_lockOnScreenOff);

            if (!g_lockOnScreenOff)         /* 功能关掉了，就别再等着锁 */
                CancelScreenLock();
            break;

        case IDM_LID_LOCK:
            if (g_hasLid)                   /* 没有盖子设备就不响应 */
            {
                g_lockOnLidClose = !g_lockOnLidClose;
                SaveBoolSetting(REG_VALUE_LID, g_lockOnLidClose);

                if (!g_lockOnLidClose)
                    CancelLidLock();
            }
            break;

        case IDM_DELAY_NOW:
        case IDM_DELAY_3:
        case IDM_DELAY_5:
        case IDM_DELAY_10:
        case IDM_DELAY_30:
            g_lockDelay = DelaySecondsForCommand(LOWORD(wParam));
            SaveDwordSetting(REG_VALUE_DELAY, g_lockDelay);

            /* 正等着的话按新档位重新计时；选“立即”就马上了结 */
            UpdateLockTimer();
            break;

        case IDM_CHECK:
            ShowCheckWindow();
            break;

        case IDM_UPDATE_CHECK:
            ShowUpdateCheckWindow();
            break;

        case IDM_AUTOSTART:
            /* 取反之前先读一次实际状态，避免和外部改动脱节 */
            SetAutoStart(IsAutoStartEnabled() ? FALSE : TRUE);
            break;

        case IDM_ABOUT:
            ShowAboutWindow();
            break;

        case IDM_EXIT:
            DestroyWindow(hWnd);
            break;

        default:
            break;
        }
        return 0;

    case WM_TIMER:
        if (wParam == TIMER_LOCK_DELAY)
            OnLockDelayElapsed();
        return 0;

    case WM_APP_UPDATE_DONE:
        OnStartupUpdateResult();
        return 0;

    case WM_POWERBROADCAST:
        if (wParam == PBT_POWERSETTINGCHANGE && lParam)
        {
            const POWERBROADCAST_SETTING *pbs = (const POWERBROADCAST_SETTING *)lParam;

            if (pbs->DataLength >= sizeof(DWORD))
            {
                DWORD value = *(const DWORD *)pbs->Data;

                if (IsEqualGUID(pbs->PowerSetting, kGuidConsoleDisplayState))
                    OnDisplayStateChanged(value);
                else if (IsEqualGUID(pbs->PowerSetting, kGuidLidSwitchState))
                    OnLidStateChanged(value);
            }
        }
        return TRUE;

    case WM_DESTROY:
        RemoveTrayIcon();

        if (g_lockTimerArmed)
        {
            KillTimer(hWnd, TIMER_LOCK_DELAY);
            g_lockTimerArmed = FALSE;
        }

        if (g_hPowerNotify)
        {
            UnregisterPowerSettingNotification(g_hPowerNotify);
            g_hPowerNotify = NULL;
        }

        if (g_hLidNotify)
        {
            UnregisterPowerSettingNotification(g_hLidNotify);
            g_hLidNotify = NULL;
        }

        PostQuitMessage(0);
        return 0;

    default:
        /* 资源管理器重启后需要重新添加托盘图标 */
        if (g_uTaskbarCreated != 0 && uMsg == g_uTaskbarCreated)
        {
            AddTrayIcon();
            return 0;
        }
        break;
    }

    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

/* ---------------------------------------------------------------------------
 * 入口
 * ------------------------------------------------------------------------ */

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                    LPWSTR lpCmdLine, int nCmdShow)
{
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;

    /* --- 声明 DPI 感知，必须早于任何窗口的创建 --------------------------- */

    EnableHighDpi();

    /*
     * 提权子进程：用户点了「插电时 设为推荐值」/「电池时 设为推荐值」之后，
     * ShellExecuteEx 会以 runas 方式再启动一个自己并带上下面这两个参数。
     * 它只写电源计划就退出——不建窗口、不进消息循环，也不参与单实例检查。
     */
    if (lpCmdLine != NULL && wcsstr(lpCmdLine, L"--apply-ac") != NULL)
        return WriteRecommendedSettings(TRUE) ? 0 : 1;

    if (lpCmdLine != NULL && wcsstr(lpCmdLine, L"--apply-dc") != NULL)
        return WriteRecommendedSettings(FALSE) ? 0 : 1;

    /* --- 单实例：已经在运行就直接提示并退出 ------------------------------ */

    HANDLE hMutex = CreateMutexW(NULL, FALSE, MUTEX_NAME);

    if (hMutex != NULL && GetLastError() == ERROR_ALREADY_EXISTS)
    {
        MessageBoxW(NULL, T(S_ERR_ALREADY_RUNNING), APP_NAME,
                    MB_OK | MB_ICONINFORMATION);
        CloseHandle(hMutex);
        return 0;
    }

    g_hInstance = hInstance;

    /* --- 注册窗口类并创建不可见的消息窗口 -------------------------------- */

    WNDCLASSEXW wc;

    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = WNDCLASS_NAME;

    if (!RegisterClassExW(&wc))
    {
        MessageBoxW(NULL, T(S_ERR_REGISTER_CLASS), APP_NAME, MB_OK | MB_ICONERROR);
        if (hMutex) CloseHandle(hMutex);
        return 1;
    }

    g_hWnd = CreateWindowExW(0, WNDCLASS_NAME, APP_NAME, 0,
                             0, 0, 0, 0, NULL, NULL, hInstance, NULL);

    if (g_hWnd == NULL)
    {
        MessageBoxW(NULL, T(S_ERR_CREATE_WINDOW), APP_NAME, MB_OK | MB_ICONERROR);
        if (hMutex) CloseHandle(hMutex);
        return 1;
    }

    g_uTaskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");

    /* --- 界面语言：第一次运行先让用户选一次 ------------------------------ */

    if (!LoadLanguage(&g_lang))
    {
        g_lang = LANG_ZH;                   /* 先给个默认值，仅用于窗口标题 */
        ShowLanguageWindow(TRUE);           /* 模态：等用户点完才继续启动 */
        SaveLanguage(g_lang);               /* 直接关掉窗口也算选过，下次不再问 */
    }

    /* --- 读取设置 -------------------------------------------------------- */

    g_lockOnScreenOff = LoadBoolSetting(REG_VALUE_SCREEN, TRUE);
    g_lockOnLidClose  = LoadBoolSetting(REG_VALUE_LID,    TRUE);

    g_lockDelay = LoadDwordSetting(REG_VALUE_DELAY, 0);

    if (!IsValidLockDelay(g_lockDelay))     /* 注册表被手工改坏就退回“立即” */
        g_lockDelay = 0;

    /* --- 托盘图标 -------------------------------------------------------- */

    /* 按当前 DPI 下的小图标尺寸加载，免得系统再缩放一次导致发虚 */
    g_hIcon = (HICON)LoadImageW(hInstance,
                                MAKEINTRESOURCEW(IDI_APP_ICON),
                                IMAGE_ICON,
                                GetSystemMetrics(SM_CXSMICON),
                                GetSystemMetrics(SM_CYSMICON),
                                LR_DEFAULTCOLOR);

    if (g_hIcon == NULL)                 /* 资源缺失时退回系统默认图标 */
        g_hIcon = LoadIconW(NULL, IDI_APPLICATION);

    AddTrayIcon();

    /* --- 订阅显示器状态变化 ---------------------------------------------- */

    g_hPowerNotify = RegisterPowerSettingNotification(
                         g_hWnd,
                         &kGuidConsoleDisplayState,
                         DEVICE_NOTIFY_WINDOW_HANDLE);

    if (g_hPowerNotify == NULL)
    {
        MessageBoxW(NULL,
                    T(S_ERR_POWER_NOTIFY),
                    APP_NAME, MB_OK | MB_ICONERROR);
        DestroyWindow(g_hWnd);
        if (hMutex) CloseHandle(hMutex);
        return 1;
    }

    /* --- 订阅盖子开关状态变化 -------------------------------------------- */

    /*
     * 注意：RegisterPowerSettingNotification 不校验 GUID 是否真实存在，
     * 随手编一个 GUID 它照样返回成功句柄。所以“这台机器有没有盖子”只能问
     * GetPwrCapabilities，不能看注册结果。
     */
    {
        SYSTEM_POWER_CAPABILITIES caps;

        ZeroMemory(&caps, sizeof(caps));

        if (GetPwrCapabilities(&caps))
            g_hasLid = caps.LidPresent ? TRUE : FALSE;
        else
            g_hasLid = TRUE;        /* 查询失败时假定有盖子，免得误伤笔记本 */
    }

    /* 台式机上注册它也不会报错，只是永远等不到回调，所以照常注册 */
    g_hLidNotify = RegisterPowerSettingNotification(
                       g_hWnd,
                       &kGuidLidSwitchState,
                       DEVICE_NOTIFY_WINDOW_HANDLE);

    /* --- 便捷入口：命令行带 --about 时直接打开关于窗口 ------------------- */

    if (lpCmdLine != NULL && wcsstr(lpCmdLine, L"--about") != NULL)
        ShowAboutWindow();

    /* --- 设置不全在推荐值时提醒一次（勾过“不再显示”就不再打扰） ---------- */

    if (!LoadBoolSetting(REG_VALUE_SKIPWARN, FALSE))
    {
        PowerSettings cfg;

        if (ReadPowerSettings(&cfg) && !SettingsAllRecommended(&cfg))
            ShowSettingsWarning();
    }

    /* --- 启动后静默查一次更新（失败就安静地算了） ------------------------ */

    StartStartupUpdateCheck();

    /* --- 消息循环 -------------------------------------------------------- */

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (hMutex)
        CloseHandle(hMutex);

    return 0;
}
