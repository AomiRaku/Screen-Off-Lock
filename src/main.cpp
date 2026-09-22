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

/* ---------------------------------------------------------------------------
 * 应用信息
 * ------------------------------------------------------------------------ */

/*
 * 显示名带空格只是为了好看；文件名、窗口类名、互斥体、注册表路径一律用
 * 无空格的 ScreenOffLock，免得空格在各处惹麻烦。
 */
#define APP_NAME        L"ScreenOff Lock"
#define APP_VERSION     L"Build 1400"
#define APP_AUTHOR      L"Raku Inkyetta 羽梦千景"
#define APP_URL         L"https://github.com/AomiRaku/Screen-Off-Lock"

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
        L"Author: %s",
        L"Project: ",
        L"Version %s",
        L"Settings are stored in the registry at",
        L"You can delete them manually if you no longer need them.",
        L"Language / 语言",

        L"Lock when screen turns off",
        L"Lock when lid is closed",
        L"Lock delay",
        L"Run at startup",
        L"Check settings",
        L"About",
        L"Exit",

        L"Immediately",
        L"3 seconds",
        L"5 seconds",
        L"10 seconds",
        L"30 seconds",

        L"Check settings",
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
        L"Click \"Check settings\" in the tray menu for details.",
        L"Don't show this again",

        L"Choose a language",

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
        L"作者：%s",
        L"项目地址：",
        L"版本 %s",
        L"设置保存在注册表",
        L"不再使用时可手动删除。",
        L"Language / 语言",

        L"关闭屏幕时锁屏",
        L"合盖锁屏",
        L"延迟锁定",
        L"开机启动",
        L"检查设置",
        L"关于",
        L"退出",

        L"立即",
        L"3 秒",
        L"5 秒",
        L"10 秒",
        L"30 秒",

        L"检查设置",
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
        L"请点击托盘菜单的“检查设置”来查看。",
        L"不再显示",

        L"选择语言",

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
#define GLYPH_INFO      0xE946      /* Segoe Fluent Icons: ⓘ */

#define ABOUT_CX        500         /* 客户区逻辑宽度：要放得下 Run 键那行长路径 */
#define ABOUT_CY        340

static HWND  g_hAboutWnd    = NULL;
static BOOL  g_aboutLinkHot = FALSE;    /* 鼠标是否悬在“项目地址”链接上 */
static UINT  g_uiDpi        = 96;       /* 全局 UI DPI，所有自绘对话框共用 */
static HFONT g_fontTitle = NULL;
static HFONT g_fontBody  = NULL;
static HFONT g_fontSmall = NULL;
static HFONT g_fontGlyph = NULL;
static HFONT g_fontGlyphSm = NULL;      /* 小号状态图标（对勾 / 感叹号） */

/* 下面这些在文件靠后定义，但前面的窗口过程要先引用，这里提前声明 */
static int  TextWidth(HDC hdc, HFONT font, const WCHAR *text);
static void ShowLanguageWindow(BOOL modal);
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

/* 关于窗口里那条链接的位置（实际像素），绘制与命中测试共用 */
static void AboutLinkRect(HWND hWnd, RECT *out)
{
    const WCHAR *label = T(S_PROJECT_LABEL);
    HDC   hdc     = GetDC(hWnd);
    HFONT hOld    = (HFONT)SelectObject(hdc, g_fontBody);
    SIZE  szLabel = { 0, 0 };
    SIZE  szLink  = { 0, 0 };

    GetTextExtentPoint32W(hdc, label, lstrlenW(label), &szLabel);
    GetTextExtentPoint32W(hdc, APP_URL, lstrlenW(APP_URL), &szLink);

    SelectObject(hdc, hOld);
    ReleaseDC(hWnd, hdc);

    out->left   = AS(28) + szLabel.cx;
    out->top    = AS(118);
    out->right  = out->left + szLink.cx;
    out->bottom = AS(148);
}

static void AboutPaint(HWND hWnd)
{
    PAINTSTRUCT ps;
    HDC         hdc = BeginPaint(hWnd, &ps);
    RECT        rc;
    COLORREF    clrText   = GetSysColor(COLOR_WINDOWTEXT);
    COLORREF    clrMuted  = RGB(0x60, 0x5E, 0x5C);
    COLORREF    clrAccent = RGB(0x0F, 0x6C, 0xBD);
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

    /* 正文 */
    wsprintfW(buf, T(S_AUTHOR_FMT), APP_AUTHOR);
    AboutText(hdc, g_fontBody, clrText, buf, 28, 100, ABOUT_CX - 56);

    /* 项目地址：纯文本标签 + 可点击的链接 */
    AboutText(hdc, g_fontBody, clrText, T(S_PROJECT_LABEL), 28, 124, 200);
    {
        RECT     lr, r;
        HFONT    hOld;
        HPEN     hPen, hOldPen;
        COLORREF clrLink = g_aboutLinkHot ? RGB(0x00, 0x4E, 0x99)
                                          : RGB(0x00, 0x5F, 0xB8);
        int      yUnder;

        AboutLinkRect(hWnd, &lr);

        hOld = (HFONT)SelectObject(hdc, g_fontBody);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, clrLink);

        r.left   = lr.left;
        r.top    = AS(124);
        r.right  = lr.right + AS(4);
        r.bottom = r.top + AS(26);

        DrawTextW(hdc, APP_URL, -1, &r,
                  DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);

        /* 链接下划线 */
        yUnder  = AS(124) + AS(19);
        hPen    = CreatePen(PS_SOLID, 1, clrLink);
        hOldPen = (HPEN)SelectObject(hdc, hPen);
        MoveToEx(hdc, lr.left, yUnder, NULL);
        LineTo(hdc, lr.right, yUnder);
        SelectObject(hdc, hOldPen);
        DeleteObject(hPen);

        SelectObject(hdc, hOld);
    }

    AboutText(hdc, g_fontBody, clrText,
              T(S_TAGLINE),
              28, 164, ABOUT_CX - 56);

    AboutText(hdc, g_fontSmall, clrMuted,
              T(S_REG_INTRO),
              28, 190, ABOUT_CX - 56);

    AboutText(hdc, g_fontSmall, clrMuted,
              L"HKEY_CURRENT_USER\\Software\\ScreenOffLock",
              28, 210, ABOUT_CX - 56);

    AboutText(hdc, g_fontSmall, clrMuted,
              L"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Run → ScreenOffLock",
              28, 230, ABOUT_CX - 56);

    AboutText(hdc, g_fontSmall, clrMuted,
              T(S_REG_OUTRO),
              28, 250, ABOUT_CX - 56);

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

    /* “项目地址”那一行是可以点的：悬停变手型、颜色加深，点击打开浏览器 */
    case WM_MOUSEMOVE:
        {
            RECT  lr;
            POINT pt;

            pt.x = (int)(short)LOWORD(lParam);
            pt.y = (int)(short)HIWORD(lParam);

            AboutLinkRect(hWnd, &lr);

            if (PtInRect(&lr, pt))
            {
                if (!g_aboutLinkHot)
                {
                    g_aboutLinkHot = TRUE;
                    InvalidateRect(hWnd, &lr, FALSE);
                }
                SetCursor(LoadCursorW(NULL, IDC_HAND));
            }
            else
            {
                if (g_aboutLinkHot)
                {
                    g_aboutLinkHot = FALSE;
                    InvalidateRect(hWnd, &lr, FALSE);
                }
                SetCursor(LoadCursorW(NULL, IDC_ARROW));
            }
        }
        return 0;

    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT && g_aboutLinkHot)
        {
            SetCursor(LoadCursorW(NULL, IDC_HAND));
            return TRUE;
        }
        break;

    case WM_LBUTTONDOWN:
        {
            RECT  lr;
            POINT pt;

            pt.x = (int)(short)LOWORD(lParam);
            pt.y = (int)(short)HIWORD(lParam);

            AboutLinkRect(hWnd, &lr);

            if (PtInRect(&lr, pt))
                ShellExecuteW(NULL, L"open", APP_URL, NULL, NULL, SW_SHOWNORMAL);
        }
        return 0;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_ABOUT_OK)
            DestroyWindow(hWnd);
        else if (LOWORD(wParam) == IDC_ABOUT_LANG)
            ShowLanguageWindow(FALSE);
        return 0;

    case WM_CLOSE:
        DestroyWindow(hWnd);
        return 0;

    case WM_DESTROY:
        g_hAboutWnd    = NULL;
        g_aboutLinkHot = FALSE;
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
    rc.bottom = AS(ABOUT_CY);
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
 * 「检查设置」对话框
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

    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, IDM_CHECK, T(S_MENU_CHECK));
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
