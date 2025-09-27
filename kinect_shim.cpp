// kinect_shim.cpp — Shim C++ para exponer API tipo-C al juego en C (Kinect v1, de pie)

#include <Windows.h>
#include <NuiApi.h>
#include <stdio.h>

extern "C" {
int  ks_init(void);
void ks_shutdown(void);
int  ks_update(void);
void ks_get_right_hand(float* x, float* y, float* z);
void ks_set_smoothing(float smoothing, float correction, float prediction,
                      float jitter_radius, float max_deviation);
}

// --- Estado global ---
static INuiSensor* g_sensor = nullptr;
static NUI_TRANSFORM_SMOOTH_PARAMETERS g_smooth = {0};
static int     g_tracked = 0;
static Vector4 g_hand    = {0};

static void loghr(const char* msg, HRESULT hr){
    printf("%s (hr=0x%08X)\n", msg, (unsigned)hr);
}

// Mapeo “de pie”: rango típico de hombros / altura cabeza
static void mapTo01Standing(const Vector4& p, float& nx, float& ny) {
    // X ~ [-0.6, +0.6] m → 0..1 ; Y ~ [0.0, 1.2] m (0 pecho, 1.2 cabeza) → 1..0
    nx = (p.x + 0.6f) / 1.2f; if (nx < 0) nx = 0; if (nx > 1) nx = 1;
    ny = 1.0f - (p.y / 1.2f); if (ny < 0) ny = 0; if (ny > 1) ny = 1;
}

int ks_init(void) {
    HRESULT hr;
    int count = 0;

    hr = NuiGetSensorCount(&count);
    if (FAILED(hr)) { loghr("NuiGetSensorCount fallo", hr); return 0; }
    if (count == 0) { printf("No hay sensores Kinect enumerados.\n"); return 0; }

    // Selecciona el primer sensor listo
    for (int i = 0; i < count; ++i) {
        INuiSensor* s = nullptr;
        hr = NuiCreateSensorByIndex(i, &s);
        if (FAILED(hr) || !s) { loghr("NuiCreateSensorByIndex fallo", hr); continue; }
        if (s->NuiStatus() == S_OK) { g_sensor = s; break; }
        s->Release();
    }
    if (!g_sensor) { printf("No se encontro un sensor Kinect en estado S_OK.\n"); return 0; }

    hr = g_sensor->NuiInitialize(NUI_INITIALIZE_FLAG_USES_SKELETON);
    if (FAILED(hr)) { loghr("NuiInitialize fallo", hr); g_sensor->Release(); g_sensor=nullptr; return 0; }

    hr = g_sensor->NuiSkeletonTrackingEnable(NULL, 0 /* sin seated */);
    if (FAILED(hr)) {
        loghr("NuiSkeletonTrackingEnable fallo", hr);
        g_sensor->NuiShutdown(); g_sensor->Release(); g_sensor=nullptr; return 0;
    }

    // Suavizado recomendado
    g_smooth.fSmoothing          = 0.5f;
    g_smooth.fCorrection         = 0.5f;
    g_smooth.fPrediction         = 0.0f;
    g_smooth.fJitterRadius       = 0.05f;
    g_smooth.fMaxDeviationRadius = 0.04f;

    printf("Kinect inicializado OK (modo de pie).\n");
    return 1;
}

void ks_shutdown(void) {
    if (g_sensor) {
        g_sensor->NuiShutdown();
        g_sensor->Release();
        g_sensor = nullptr;
    }
}

int ks_update(void) {
    if (!g_sensor) return 0;
    NUI_SKELETON_FRAME frame = {0};
    HRESULT hr = g_sensor->NuiSkeletonGetNextFrame(0, &frame);
    if (FAILED(hr)) { g_tracked = 0; return 0; }

    NuiTransformSmooth(&frame, &g_smooth);

    g_tracked = 0;
    for (int i = 0; i < NUI_SKELETON_COUNT; ++i) {
        const NUI_SKELETON_DATA& sk = frame.SkeletonData[i];
        if (sk.eTrackingState == NUI_SKELETON_TRACKED) {
            g_hand = sk.SkeletonPositions[NUI_SKELETON_POSITION_HAND_RIGHT];
            g_tracked = 1;
            break;
        }
    }
    return g_tracked;
}

void ks_get_right_hand(float* x, float* y, float* z) {
    if (!x || !y || !z) return;
    float nx=0, ny=0;
    mapTo01Standing(g_hand, nx, ny);
    *x = nx; *y = ny; *z = g_hand.z; // z en metros aprox
}

void ks_set_smoothing(float s, float c, float p, float jr, float mdr) {
    g_smooth.fSmoothing          = s;
    g_smooth.fCorrection         = c;
    g_smooth.fPrediction         = p;
    g_smooth.fJitterRadius       = jr;
    g_smooth.fMaxDeviationRadius = mdr;
}
