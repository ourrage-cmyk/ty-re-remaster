// gpuprof.inl — GPU-side timing with GL_TIMESTAMP queries (ring of 4 frames, read 3 frames late so nothing stalls).
// Marks: 0 frame start, 1-2 AO, 3-4 GI, 5-6 atmosphere, 7-8 post/TAA, 9 frame end, 10-19 sub-stages, 20-21 emitter light pass (RunEmit, emit.inl).
// Logged every 300 frames as "[gpu] ..." and "[gpusub] ...". Disable: [debug] GpuProf=0.
#include "stateguard.inl"
static int g_gpEnable = 1;
static struct { GLuint q[4][22]; unsigned mask[4]; bool init, failed; int cur; double acc[12]; int n; } gp;
typedef void(WINAPI* gp_gen_t)(GLsizei, GLuint*); typedef void(WINAPI* gp_qc_t)(GLuint, GLenum); typedef void(WINAPI* gp_geti_t)(GLuint, GLenum, GLint*); typedef void(WINAPI* gp_get64_t)(GLuint, GLenum, unsigned long long*);
static gp_gen_t gp_gen; static gp_qc_t gp_qc; static gp_geti_t gp_geti; static gp_get64_t gp_get64;
static bool GProfInit() {
    if (gp.init || gp.failed) return gp.init;
    gp_gen = (gp_gen_t)o_wglGetProcAddress("glGenQueries"); gp_qc = (gp_qc_t)o_wglGetProcAddress("glQueryCounter");
    gp_geti = (gp_geti_t)o_wglGetProcAddress("glGetQueryObjectiv"); gp_get64 = (gp_get64_t)o_wglGetProcAddress("glGetQueryObjectui64v");
    if (!gp_gen || !gp_qc || !gp_geti || !gp_get64) { gp.failed = true; return false; }
    for (int s = 0; s < 4; s++) gp_gen(22, gp.q[s]);
    gp.init = true; return true;
}
static void GProf(int m) {
    SGMark(m);
    if (!g_gpEnable || !GProfInit()) return;
    gp_qc(gp.q[gp.cur][m], 0x8E28 /*TIMESTAMP*/); gp.mask[gp.cur] |= 1u << m;
}
// call right after SwapBuffers: mark frame end of the slot just used, collect the oldest slot, start the next frame
static void GProfNextFrame() {
    if (!g_gpEnable || !GProfInit()) return;
    gp.cur = (gp.cur + 1) & 3;                         // oldest slot (3 frames back)
    unsigned mk = gp.mask[gp.cur];
    if ((mk & 1) && (mk & 512)) {
        GLint ok = 0; gp_geti(gp.q[gp.cur][9], 0x8867 /*RESULT_AVAILABLE*/, &ok);
        if (ok) {
            unsigned long long t[22] = {};
            for (int m = 0; m < 22; m++) if (mk & (1u << m)) gp_get64(gp.q[gp.cur][m], 0x8866, &t[m]);
            auto d = [&](int a, int b) { return ((mk & (1u << a)) && (mk & (1u << b)) && t[b] >= t[a]) ? (t[b] - t[a]) / 1e6 : 0.0; };
            gp.acc[0] += d(0, 9); gp.acc[1] += d(1, 2); gp.acc[2] += d(3, 4); gp.acc[3] += d(5, 6); gp.acc[4] += d(7, 8); gp.acc[5] += d(10, 11); gp.acc[6] += d(12, 13); gp.acc[7] += d(14, 15); gp.acc[8] += d(16, 17); gp.acc[9] += d(18, 19); gp.acc[10] += d(20, 21); gp.n++;
                SessGpuSample(d(0, 9), d(1, 2), d(3, 4), d(5, 6), d(7, 8));
            if (gp.n >= 300) {
                double f = gp.acc[0] / gp.n, o = f - (gp.acc[1] + gp.acc[2] + gp.acc[3] + gp.acc[4]) / gp.n;
                logf("[gpu] frame %.2f ms | ao %.2f gi %.2f atmos %.2f post %.2f | engine+rest %.2f ms (%u draws)", f, gp.acc[1] / gp.n, gp.acc[2] / gp.n, gp.acc[3] / gp.n, gp.acc[4] / gp.n, o, g_draws);
                logf("[gpusub] sub0 %.2f sub1 %.2f sub2 %.2f sub3 %.2f sub4 %.2f | emit %.3f", gp.acc[5] / gp.n, gp.acc[6] / gp.n, gp.acc[7] / gp.n, gp.acc[8] / gp.n, gp.acc[9] / gp.n, gp.acc[10] / gp.n);
                memset(gp.acc, 0, sizeof gp.acc); gp.n = 0;
            }
        }
    }
    gp.mask[gp.cur] = 0;
    GProf(0);
}

// Fog coordination for AO: base density and height falloff exactly as RunAtmos feeds the froxel/raymarch (density relative to camera height).
static void AtmosFogParams(float* dens0, float* falloff) {
    *dens0 = g_atEnable ? g_atDensity * g_atIntensity * g_atFogMul * g_fogAmount / fmaxf(g_fogDistance, 0.05f) : 0.0f; *falloff = g_atFalloff;
}
