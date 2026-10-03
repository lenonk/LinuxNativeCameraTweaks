// Linux Native Camera Tweaks, by Biiinks78. This fork runs it as a bg3le plugin: bg3le loads it
// from ~/.local/share/bg3le/plugins instead of LD_PRELOAD, and its tuning is exposed as settings.
#include <dlfcn.h>
#include <sys/types.h>
#include <stdatomic.h>
#include <math.h>
#include <time.h>
#include <SDL2/SDL.h>

#include "bg3le_plugin.h"
#include "sdl_bindings.h"
#include "utils.h"
#include "offsets.h"


#define VERSION "1.0"
#define PLUGIN_NAME "LinuxNativeCameraTweaks"
//Tested Game build:
	//4.1.1.7398727
	//4.1.1.7209685
	//4.76.31.656

#define INPUT_CONFIG "~/.local/share/Larian Studios/Baldur's Gate 3/PlayerProfiles/Public/inputconfig_p1.json"

static const bg3le_host* g_host;
static bg3le_plugin* g_self;

// Settings; the defaults are upstream's constants.
static float g_roll_sensitivity = 2.f;
static int g_invert_roll = 0;
static float g_roll_min = -89.f;
static float g_roll_max = 89.f;
static float g_zoom_step = 0.25f;
static int g_invert_zoom = 0;
static int g_zoom_limit = 0;
static float g_zoom_min = 1.f;
static float g_zoom_max = 100.f;
static int g_smooth_zoom = 1;
static float g_zoom_smoothing = 10.f;
static float g_controller_roll_speed = 2.f;
static int g_controller_deadzone = 4000;

static uint64_t g_game_build;

static float* g_zoom;
static float* g_roll;

static atomic_int g_mouse_delta_y;
static atomic_int g_mouse_wheel_y;
static atomic_int g_pending_wheel_y;
static int g_ui_wheel = 0;
static atomic_int g_roll_keydown = 0;
static atomic_int g_controller_right_stick_y;
static atomic_int g_controller_right_stick_axis_motion_y = 0;
static atomic_int g_controller_right_stick_button_down = 0;

static BindingSet g_bs;
static const ActionBindings* g_binds[1];
// The game's default; inputconfig_p1.json only lists bindings that were changed.
static const ActionBindings g_default_rotate =
	{ "CameraToggleMouseRotate", { { BINDING_MOUSE, SDL_SCANCODE_UNKNOWN, SDL_BUTTON_MIDDLE, "middle" } }, 1 };

static SDL_bool (*O_SDL_GetRelativeMouseMode)(void) = NULL;
static int (*O_SDL_SetRelativeMouseMode)(SDL_bool) = NULL;

typedef float (*CalculateCameraAngle_t)(void*, uint8_t);
static CalculateCameraAngle_t O_CalculateCameraAngle;

typedef uint8_t undefined8[8];
typedef void (*SaveToInputConfigFile_4117209685_t)(undefined8, undefined8, undefined8, undefined8, undefined8, undefined8,
		undefined8, undefined8, long*, long*, mbstate_t, uint*, mbstate_t, undefined8);
static SaveToInputConfigFile_4117209685_t O_SaveToInputConfigFile_4117209685;
typedef void (*SaveToInputConfigFile_4117398727_t)(undefined8, long, undefined8);
static SaveToInputConfigFile_4117398727_t O_SaveToInputConfigFile_4117398727;


static void LoadRotateBinding(void)
{
	if (LoadBindingsFromFile(INPUT_CONFIG, &g_bs))
	{
		const ActionBindings* found = FindAction(&g_bs, "CameraToggleMouseRotate");
		if (found && found->binding_count > 0)
		{
			g_binds[0] = found;
			return;
		}
	}
	g_binds[0] = &g_default_rotate;
	g_host->log(g_self, "CameraToggleMouseRotate isn't rebound; rotating with the middle mouse button");
}

void H_SaveToInputConfigFile_4117398727_CallSite(undefined8 p1, long p2, undefined8 p3)
{
	O_SaveToInputConfigFile_4117398727(p1, p2, p3);
	LoadRotateBinding();
}

void H_SaveToInputConfigFile_4117209685_CallSite(undefined8 p1, undefined8 p2, undefined8 p3, undefined8 p4, undefined8 p5, undefined8 p6,
		undefined8 p7, undefined8 p8, long* p9, long* p10, mbstate_t p11, uint* p12, mbstate_t p13, undefined8 p14)
{
	O_SaveToInputConfigFile_4117209685(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14);
	LoadRotateBinding();
}

static int Deadzoned(int value)
{
	return (value > -g_controller_deadzone && value < g_controller_deadzone) ? 0 : value;
}

static float ClampZoom(float zoom)
{
	if (!g_zoom_limit || g_zoom_min >= g_zoom_max)
		return zoom;
	return zoom < g_zoom_min ? g_zoom_min : zoom > g_zoom_max ? g_zoom_max : zoom;
}

// The wheel moves a target; the camera eases toward it, framerate-independently.
static float g_zoom_target;
static float g_zoom_written;
static void* g_zoom_camera;

static void UpdateZoom(void* camera, float delta)
{
	static struct timespec last;
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	float dt = last.tv_sec ? (float)(now.tv_sec - last.tv_sec) + (float)(now.tv_nsec - last.tv_nsec) * 1e-9f : 0.f;
	last = now;
	if (dt > 0.25f)
		dt = 0.25f;

	// A new camera, or the game moved it (a cutscene, a level load): start from where it is.
	if (camera != g_zoom_camera || *g_zoom != g_zoom_written)
	{
		g_zoom_camera = camera;
		g_zoom_target = *g_zoom;
	}
	g_zoom_target = ClampZoom(g_zoom_target + delta);
	if (g_smooth_zoom && g_zoom_smoothing > 0.f && fabsf(g_zoom_target - *g_zoom) > 1e-4f)
		*g_zoom += (g_zoom_target - *g_zoom) * (1.f - expf(-g_zoom_smoothing * dt));
	else
		*g_zoom = g_zoom_target;
	g_zoom_written = *g_zoom;
}

float H_CalculateCameraAngle_CallSite(void* pCameraObject, uint8_t angle)
{
	g_zoom = (float*)((uint8_t*)pCameraObject + 0x58);
	const float zoom_sign = g_invert_zoom ? -1.f : 1.f;
	int right_stick_button_down = atomic_load(&g_controller_right_stick_button_down);
	int right_stick_axis_motion_y = atomic_load(&g_controller_right_stick_axis_motion_y);
	if (right_stick_button_down && right_stick_axis_motion_y)
	{
		int right_stick_y = Deadzoned(atomic_load(&g_controller_right_stick_y));
		UpdateZoom(pCameraObject, -((float)right_stick_y / 32767.f * g_zoom_step) * zoom_sign);
		return O_CalculateCameraAngle(pCameraObject, angle);
	}
	int wheel = atomic_exchange(&g_mouse_wheel_y, 0);
	UpdateZoom(pCameraObject, -((float)wheel * g_zoom_step) * zoom_sign);

	int roll_keydown = atomic_load(&g_roll_keydown);
	if (!roll_keydown && !right_stick_axis_motion_y)
	{
		if (O_SDL_GetRelativeMouseMode() == SDL_TRUE)
			O_SDL_SetRelativeMouseMode(SDL_FALSE);
		return O_CalculateCameraAngle(pCameraObject, angle);
	}
	if (O_SDL_GetRelativeMouseMode() != SDL_TRUE)
		O_SDL_SetRelativeMouseMode(SDL_TRUE);

	g_roll = (float*)((uint8_t*)pCameraObject + 0x164);
	float roll = *g_roll;
	const float roll_sign = g_invert_roll ? -1.f : 1.f;

	if (right_stick_axis_motion_y)
	{
		float norm = (float)Deadzoned(atomic_load(&g_controller_right_stick_y)) / 32767.f;
		roll += norm * g_controller_roll_speed * roll_sign;
	}
	else
	{
		int delta = atomic_exchange(&g_mouse_delta_y, 0);
		roll += (float)delta * g_roll_sensitivity * roll_sign;
	}

	// Past +-89 the camera flips over.
	float lo = g_roll_min < g_roll_max ? g_roll_min : -89.f;
	float hi = g_roll_min < g_roll_max ? g_roll_max : 89.f;
	if (roll > hi)
		roll = hi;
	if (roll < lo)
		roll = lo;
	*g_roll = roll;

	return O_CalculateCameraAngle(pCameraObject, angle);
}

// Turns the game's `movss [field], xmm` store of its smoothed pitch/zoom into a load of the same field.
// NOPing it (upstream) left the game drawing with its smoothed value, a frame-time-dependent step off ours: shake.
static uint8_t StoreToLoad(uint8_t* movss, const char* what)
{
	int op = -1;
	for (int i = 1; i < 3; i++)
		if (movss[0] == 0xF3 && movss[i] == 0x0F && movss[i + 1] == 0x11)
			op = i + 1;
	if (op < 0)
	{
		g_host->warn(g_self, "PatchUpdateCamera(): %s isn't a movss store", what);
		return 0;
	}

	long page_size = sysconf(_SC_PAGESIZE);
	uint64_t page_start = (uint64_t)(movss + op) & ~(page_size - 1);
	if (mprotect((void*)page_start, page_size, PROT_READ | PROT_WRITE | PROT_EXEC) != 0)
	{
		g_host->warn(g_self, "PatchUpdateCamera(): %s mprotect() failed", what);
		return 0;
	}
	movss[op] = 0x10;
	mprotect((void*)page_start, page_size, PROT_READ | PROT_EXEC);
	return 1;
}

uint8_t PatchUpdateCamera()
{
	return StoreToLoad((uint8_t*)GetAddresses()->roll_movss, "roll movss")
		&& StoreToLoad((uint8_t*)GetAddresses()->zoom_movss, "zoom movss");
}

uint8_t SetupCallSitesTrampoline()
{
	size_t page_size = sysconf(_SC_PAGESIZE);

	void* callsite = (void*)GetAddresses()->CalculateCameraAngle_CallSite;
	void* trampoline = AllocNear(callsite, page_size);
	if (!trampoline)
	{
		g_host->warn(g_self, "SetupCallSitesTrampoline(): AllocNear() failed for CalculateCameraAngle()");
		return 0;
	}

	if (!PatchCallSite(callsite, trampoline, H_CalculateCameraAngle_CallSite))
	{
		g_host->warn(g_self, "SetupCallSitesTrampoline(): PatchCallSite() failed for CalculateCameraAngle()");
		munmap(trampoline, page_size);
		return 0;
	}

	callsite = (void*)GetAddresses()->SaveToInputConfigFile_CallSite;
	trampoline = AllocNear(callsite, page_size);
	if (!trampoline)
	{
		g_host->warn(g_self, "SetupCallSitesTrampoline(): AllocNear() failed for SaveToInputConfigFile()");
		return 0;
	}

	void* hook = g_game_build == 4117209685
		? (void*)H_SaveToInputConfigFile_4117209685_CallSite
		: (void*)H_SaveToInputConfigFile_4117398727_CallSite;
	if (!PatchCallSite(callsite, trampoline, hook))
	{
		g_host->warn(g_self, "SetupCallSitesTrampoline(): PatchCallSite() failed for SaveToInputConfigFile()");
		munmap(trampoline, page_size);
		return 0;
	}

	return 1;
}

uint64_t FNV1a_Hash(const uint8_t* data, size_t len)
{
    uint64_t hash = 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < len; i++)
    {
        hash ^= data[i];
        hash *= 0x100000001b3ULL;
    }
    return hash;
}

uint64_t GetBuild()
{
	size_t size;
	uint8_t* file = MapSelfExe(&size);
	if (!file)
		return 0;
    uint64_t hash = FNV1a_Hash(file, size);
    g_host->log(g_self, "binary FNV1a hash: %#lx (size: %zu)", hash, size);
    munmap(file, size);
	switch (hash)
	{
		case 0x59b4428151c778e5:
			return 4117209685;
		case 0x142d91e7bfe067cb:
			return 4117398727;
		default:
			return 0;
	}
}

static void TrackRollBinding(int is_mouse, int code, int down)
{
	const ActionBindings* binds = g_binds[0];
	if (!binds)
		return;
	for (int i = 0; i < binds->binding_count; i++)
	{
		const Binding* b = &binds->bindings[i];
		if (is_mouse ? (b->type == BINDING_MOUSE && code == b->mouse_button)
				: (b->type == BINDING_KEY && code == (int)b->scancode))
			atomic_store(&g_roll_keydown, down);
	}
}

// Noesis gets the wheel first, so scrollable windows scroll; the camera zooms only when no UI element used it.
typedef uint8_t (*MouseWheel_t)(void*, int, int, int);
static MouseWheel_t O_View_MouseWheel, O_IView_MouseWheel;

static void AfterUIWheel(uint8_t handled)
{
	int pending = atomic_exchange(&g_pending_wheel_y, 0);
	if (!handled && pending)
		atomic_fetch_add(&g_mouse_wheel_y, pending);
}

static uint8_t H_View_MouseWheel(void* view, int x, int y, int rotation)
{
	uint8_t handled = O_View_MouseWheel(view, x, y, rotation);
	AfterUIWheel(handled);
	return handled;
}

static uint8_t H_IView_MouseWheel(void* view, int x, int y, int rotation)
{
	uint8_t handled = O_IView_MouseWheel(view, x, y, rotation);
	AfterUIWheel(handled);
	return handled;
}

// Swaps View::MouseWheel and its IView thunk in Noesis::View's vtables.
static int HookUIWheel(void)
{
	const char* names[] = { "_ZTVN6Noesis4ViewE", "_ZN6Noesis4View10MouseWheelEiii", "_ZThn16_N6Noesis4View10MouseWheelEiii" };
	uint64_t addresses[3], sizes[3];
	FindSymbols(names, 3, addresses, sizes);
	if (!addresses[0] || !sizes[0] || !addresses[1] || !addresses[2])
		return 0;

	uint64_t* slots = (uint64_t*)addresses[0];
	size_t count = sizes[0] / sizeof(uint64_t);
	uint64_t* view_slot = NULL;
	uint64_t* iview_slot = NULL;
	for (size_t i = 0; i < count; i++)
	{
		if (slots[i] == addresses[1] && !view_slot)
			view_slot = &slots[i];
		else if (slots[i] == addresses[2] && !iview_slot)
			iview_slot = &slots[i];
	}
	if (!view_slot || !iview_slot)
		return 0;

	long page_size = sysconf(_SC_PAGESIZE);
	uint64_t first = (uint64_t)(view_slot < iview_slot ? view_slot : iview_slot) & ~(page_size - 1);
	uint64_t last = (uint64_t)(view_slot < iview_slot ? iview_slot : view_slot) & ~(page_size - 1);
	size_t len = last - first + page_size;
	if (mprotect((void*)first, len, PROT_READ | PROT_WRITE) != 0)
		return 0;
	O_View_MouseWheel = (MouseWheel_t)*view_slot;
	O_IView_MouseWheel = (MouseWheel_t)*iview_slot;
	*view_slot = (uint64_t)H_View_MouseWheel;
	*iview_slot = (uint64_t)H_IView_MouseWheel;
	mprotect((void*)first, len, PROT_READ);
	return 1;
}

// Upstream's SDL_PollEvent body; returning 1 keeps the event from the game.
static int OnEvent(void* user, SDL_Event* event)
{
	(void)user;
	switch (event->type)
	{
		case SDL_MOUSEMOTION:
			atomic_store(&g_mouse_delta_y, event->motion.yrel);
			return 0;
		case SDL_MOUSEWHEEL:
			if (g_ui_wheel)
			{
				atomic_fetch_add(&g_pending_wheel_y, event->wheel.y);
				return 0;
			}
			atomic_fetch_add(&g_mouse_wheel_y, event->wheel.y);
			return 1;
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
			TrackRollBinding(1, event->button.button, event->type == SDL_MOUSEBUTTONDOWN);
			return 0;
		case SDL_KEYDOWN:
		case SDL_KEYUP:
			TrackRollBinding(0, event->key.keysym.scancode, event->type == SDL_KEYDOWN);
			return 0;
		case SDL_CONTROLLERAXISMOTION:
			if (event->caxis.axis != SDL_CONTROLLER_AXIS_RIGHTY)
				return 0;
			if (event->caxis.value > g_controller_deadzone || event->caxis.value < -g_controller_deadzone)
			{
				atomic_store(&g_controller_right_stick_axis_motion_y, 1);
				atomic_store(&g_controller_right_stick_y, event->caxis.value);
			}
			else
				atomic_store(&g_controller_right_stick_axis_motion_y, 0);
			return 1;
		case SDL_CONTROLLERBUTTONDOWN:
		case SDL_CONTROLLERBUTTONUP:
			if (event->cbutton.button == SDL_CONTROLLER_BUTTON_RIGHTSTICK)
				atomic_store(&g_controller_right_stick_button_down, event->type == SDL_CONTROLLERBUTTONDOWN);
			return 0;
		default:
			return 0;
	}
}

static void RegisterSettings(void)
{
	g_host->add_setting(g_self, "roll_sensitivity", BG3LE_SETTING_FLOAT, &g_roll_sensitivity, 0.1, 10.0);
	g_host->add_setting(g_self, "invert_roll", BG3LE_SETTING_BOOL, &g_invert_roll, 0, 0);
	g_host->add_setting(g_self, "roll_min", BG3LE_SETTING_FLOAT, &g_roll_min, -89.0, 89.0);
	g_host->add_setting(g_self, "roll_max", BG3LE_SETTING_FLOAT, &g_roll_max, -89.0, 89.0);
	g_host->add_setting(g_self, "zoom_step", BG3LE_SETTING_FLOAT, &g_zoom_step, 0.01, 5.0);
	g_host->add_setting(g_self, "invert_zoom", BG3LE_SETTING_BOOL, &g_invert_zoom, 0, 0);
	g_host->add_setting(g_self, "zoom_limit", BG3LE_SETTING_BOOL, &g_zoom_limit, 0, 0);
	g_host->add_setting(g_self, "zoom_min", BG3LE_SETTING_FLOAT, &g_zoom_min, 0.0, 200.0);
	g_host->add_setting(g_self, "zoom_max", BG3LE_SETTING_FLOAT, &g_zoom_max, 0.0, 200.0);
	g_host->add_setting(g_self, "smooth_zoom", BG3LE_SETTING_BOOL, &g_smooth_zoom, 0, 0);
	g_host->add_setting(g_self, "zoom_smoothing", BG3LE_SETTING_FLOAT, &g_zoom_smoothing, 1.0, 30.0);
	g_host->add_setting(g_self, "controller_roll_speed", BG3LE_SETTING_FLOAT, &g_controller_roll_speed, 0.1, 10.0);
	g_host->add_setting(g_self, "controller_deadzone", BG3LE_SETTING_INT, &g_controller_deadzone, 0, 32000);
}

int bg3le_plugin_init(const bg3le_host* host, bg3le_plugin* self)
{
	g_host = host;
	g_self = self;
	if (host->abi < BG3LE_PLUGIN_ABI || host->size < sizeof(bg3le_host))
		return 1;
	host->describe(self, PLUGIN_NAME, VERSION);
	host->log(self, "Linux Native Camera Tweaks %s, by Biiinks78", VERSION);

	O_SDL_GetRelativeMouseMode = (SDL_bool (*)(void))host->sdl_function("SDL_GetRelativeMouseMode");
	O_SDL_SetRelativeMouseMode = (int (*)(SDL_bool))host->sdl_function("SDL_SetRelativeMouseMode");
	if (!O_SDL_GetRelativeMouseMode || !O_SDL_SetRelativeMouseMode)
	{
		host->warn(self, "SDL's relative mouse functions aren't available");
		return 2;
	}

	RegisterSettings();
	LoadRotateBinding();
	g_game_build = GetBuild();

	struct Sigs* sigs = GetSigs();
	struct Addresses* addresses = GetAddresses();
	addresses->CalculateCameraAngle_CallSite = PatternScanSection(sigs->CalculateCameraAngle_Callsite, ".text") + 39;
	addresses->SaveToInputConfigFile_CallSite = PatternScanSection(sigs->SaveToInputConfigFile_CallSite, ".text");
	addresses->roll_movss = PatternScanSection(sigs->roll_movss, ".text");
	addresses->zoom_movss = PatternScanSection(sigs->zoom_movss, ".text");
	if (addresses->CalculateCameraAngle_CallSite == 39 || !addresses->SaveToInputConfigFile_CallSite
			|| !addresses->roll_movss || !addresses->zoom_movss)
	{
		host->warn(self, "pattern scan failed; a game update broke the patterns");
		return 3;
	}

	O_CalculateCameraAngle = ResolveCallTarget((void*)addresses->CalculateCameraAngle_CallSite);
	if (g_game_build == 4117209685)
		O_SaveToInputConfigFile_4117209685 = ResolveCallTarget((void*)addresses->SaveToInputConfigFile_CallSite);
	else
		O_SaveToInputConfigFile_4117398727 = ResolveCallTarget((void*)addresses->SaveToInputConfigFile_CallSite);
	if (!O_CalculateCameraAngle || !(O_SaveToInputConfigFile_4117209685 || O_SaveToInputConfigFile_4117398727))
	{
		host->warn(self, "a patched call site isn't a call");
		return 4;
	}

	g_ui_wheel = HookUIWheel();
	if (!g_ui_wheel)
		host->warn(self, "couldn't hook Noesis' mouse wheel; the wheel always zooms, even over scrollable windows");
	if (host->add_event_handler(self, OnEvent, NULL) != 0)
		return 5;
	if (!PatchUpdateCamera())
		return 6;
	if (!SetupCallSitesTrampoline())
		return 7;

	host->log(self, "initialised; settings in Ext.Plugins.GetSettings(\"%s\")", PLUGIN_NAME);
	return 0;
}
