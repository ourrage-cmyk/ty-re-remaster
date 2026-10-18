// debugcam.inl — debug free-camera offset for reproducible close-up test views (not a gameplay feature).
// Create <game>\tyrender\cam.txt containing "dx dy dz yawDeg pitchDeg" (floats): the scene camera is moved by (dx right, dy up, dz forward) world units
// in its own frame and then turned by yaw (about world Y) and pitch (about its right axis). Delete the file for the normal view. Polled every 8 frames.
// The scene view-projection matrix is modified before anything else sees it, so AO/GI/atmosphere/TAA/PBR all use the shifted camera.
// The engine itself still culls/LODs from Ty's real camera, so keep moves modest (< ~1500 units) to avoid popping geometry.
static float g_dcam[5]; static bool g_dcamOn;
static bool MotionActive();
static void PollDebugCam() {
    if (MotionActive()) return;   // a motion capture owns the camera
    char p[MAX_PATH]; snprintf(p, sizeof p, "%styrender\\cam.txt", g_dir); std::string s;
    if (ReadFileStr(p, s)) { float v[5] = {0, 0, 0, 0, 0}; int n = sscanf(s.c_str(), "%f %f %f %f %f", &v[0], &v[1], &v[2], &v[3], &v[4]); if (n >= 3) { if (!g_dcamOn || memcmp(g_dcam, v, sizeof v)) logf("[dcam] camera offset %.1f %.1f %.1f yaw %.1f pitch %.1f", v[0], v[1], v[2], v[3], v[4]); memcpy(g_dcam, v, sizeof v); g_dcamOn = true; return; } }
    if (g_dcamOn) logf("[dcam] off");
    g_dcamOn = false;
}
static void DcMul4(const float* a, const float* b, float* o) { for (int c = 0; c < 4; c++) for (int r = 0; r < 4; r++) { float s = 0; for (int k = 0; k < 4; k++) s += a[k * 4 + r] * b[c * 4 + k]; o[c * 4 + r] = s; } }
static bool DebugCamApply(float* m) {
    if (!g_dcamOn) return false;
    float inv[16]; if (!Invert4(m, inv)) return false;
    const float* q = m;
    float r0[3] = {q[0], q[4], q[8]}, r1[3] = {q[1], q[5], q[9]}, r3[3] = {q[3], q[7], q[11]};
    float p00 = sqrtf(r0[0]*r0[0] + r0[1]*r0[1] + r0[2]*r0[2]), p11 = sqrtf(r1[0]*r1[0] + r1[1]*r1[1] + r1[2]*r1[2]), l3 = sqrtf(r3[0]*r3[0] + r3[1]*r3[1] + r3[2]*r3[2]);
    if (p00 < 1e-6f || p11 < 1e-6f || l3 < 1e-6f) return false;
    float R[3] = {r0[0]/p00, r0[1]/p00, r0[2]/p00}, U[3] = {r1[0]/p11, r1[1]/p11, r1[2]/p11}, B[3] = {-r3[0]/l3, -r3[1]/l3, -r3[2]/l3};
    float zn = g_aoZMode == 0 ? -1.0f : 0.0f, nc[4], nt[4];
    for (int i = 0; i < 4; i++) { nc[i] = inv[8 + i] * zn + inv[12 + i]; nt[i] = inv[4 + i] + nc[i]; }
    float N3[3] = {nc[0]/nc[3], nc[1]/nc[3], nc[2]/nc[3]}, NT[3] = {nt[0]/nt[3], nt[1]/nt[3], nt[2]/nt[3]};
    float halfH = sqrtf((NT[0]-N3[0])*(NT[0]-N3[0]) + (NT[1]-N3[1])*(NT[1]-N3[1]) + (NT[2]-N3[2])*(NT[2]-N3[2])), nearDist = halfH * p11;
    float c[3] = {N3[0] + B[0]*nearDist, N3[1] + B[1]*nearDist, N3[2] + B[2]*nearDist};
    float c2[3]; for (int i = 0; i < 3; i++) c2[i] = c[i] + R[i] * g_dcam[0] + U[i] * g_dcam[1] - B[i] * g_dcam[2];
    const float d2r = 3.14159265f / 180.0f; float yaw = g_dcam[3] * d2r, pit = g_dcam[4] * d2r;
    // Rot = Rn^-1 with Rn = Rpitch(axis R) * Ryaw(world Y): inverse = Ryaw(-yaw) * Rpitch(-pit); built as 3x3 row-major
    float cy = cosf(-yaw), sy = sinf(-yaw), Ry[9] = {cy, 0, sy,  0, 1, 0,  -sy, 0, cy};
    float cp = cosf(-pit), sp = sinf(-pit), ax = R[0], ay = R[1], az = R[2], t = 1 - cp;
    float Rp[9] = {t*ax*ax + cp, t*ax*ay - sp*az, t*ax*az + sp*ay,  t*ax*ay + sp*az, t*ay*ay + cp, t*ay*az - sp*ax,  t*ax*az - sp*ay, t*ay*az + sp*ax, t*az*az + cp};
    float Rot[9]; for (int r = 0; r < 3; r++) for (int k = 0; k < 3; k++) { float s = 0; for (int j = 0; j < 3; j++) s += Ry[r*3 + j] * Rp[j*3 + k]; Rot[r*3 + k] = s; }
    float tr[3]; for (int r = 0; r < 3; r++) tr[r] = c[r] - (Rot[r*3] * c2[0] + Rot[r*3 + 1] * c2[1] + Rot[r*3 + 2] * c2[2]);   // M = T(c) * Rot * T(-c2)
    float M[16] = {Rot[0], Rot[3], Rot[6], 0,  Rot[1], Rot[4], Rot[7], 0,  Rot[2], Rot[5], Rot[8], 0,  tr[0], tr[1], tr[2], 1};
    float out[16]; DcMul4(m, M, out); memcpy(m, out, sizeof out); memcpy(g_sceneVP, out, sizeof out);
    return true;
}

// ---- motion-stability capture (test aid). Create <game>\tyrender\motion.req containing "name yawStepDeg frames [x y z yaw pitch]" -> for the next N frames the debug camera yaws by
// yawStepDeg per FRAME (smooth constant motion) and every frame's back buffer is stored as 640x360 gray; written to <game>\tyrender\snaps\motion_<name>.raw
// (int32 W,H,K then K*W*H bytes). Offline: temporal second difference of the frames = flicker (see TOOLS\motion.ps1). The previous debug camera is restored afterwards.
static struct { bool on; int n, i, W, H; float step, base, fwd, side; int sleepMs; char name[64]; std::vector<unsigned char> buf; bool savedOn; float savedCam[5]; } g_mt;
static bool MotionActive() { return g_mt.on; }
static void MotionPoll() {
    if (g_mt.on) return;
    char p[MAX_PATH]; snprintf(p, sizeof p, "%styrender\\motion.req", g_dir); std::string s; if (!ReadFileStr(p, s)) return; logf("[motion] request seen: %s", s.c_str()); DeleteFileA(p);
    char nm[64] = "m"; float step = 0.1f, v[5] = {0, 0, 0, 0, 0}, fw = 0, sd = 0, slp = 0; int n = 14; int k = sscanf(s.c_str(), "%63s %f %d %f %f %f %f %f %f %f %f", nm, &step, &n, &v[0], &v[1], &v[2], &v[3], &v[4], &fw, &sd, &slp);
    if (k < 1) return; if (n < 4) n = 4; if (n > 40) n = 40;
    g_mt.on = true; g_mt.n = n; g_mt.i = 0; g_mt.step = step; g_mt.fwd = fw; g_mt.side = sd; g_mt.sleepMs = (int)slp;   // extra args: fwd side (world units per frame), sleepMs (low-fps emulation)
     strncpy(g_mt.name, nm, 63); g_mt.name[63] = 0; g_mt.W = 640; g_mt.H = 360; g_mt.buf.assign((size_t)n * g_mt.W * g_mt.H, 0);
    g_mt.savedOn = g_dcamOn; memcpy(g_mt.savedCam, g_dcam, sizeof g_dcam);
    if (k >= 6) memcpy(g_dcam, v, sizeof v); else if (!g_dcamOn) memset(g_dcam, 0, sizeof g_dcam);
    g_dcamOn = true; g_mt.base = g_dcam[3]; logf("[motion] start %s step %.3f deg/frame frames %d", g_mt.name, step, n);
}
static void MotionFrame(HDC dc) {
    if (!g_mt.on) return;
    RECT rc; HWND wnd = WindowFromDC(dc); if (!wnd || !GetClientRect(wnd, &rc) || rc.right < 64 || rc.bottom < 64) { g_mt.on = false; return; }
    int w = rc.right, h = rc.bottom; if (o_glBindFramebuffer) o_glBindFramebuffer(0x8CA8, 0);
    std::string px((size_t)w * h * 3, 0);
    typedef void(WINAPI* glPixelStorei_t)(GLenum, GLint); static glPixelStorei_t ps = (glPixelStorei_t)GetProcAddress(GetModuleHandleA("opengl32.dll"), "glPixelStorei"); ps(0x0D05, 1);
    o_glReadPixels(0, 0, w, h, 0x80E0, 0x1401, &px[0]);
    unsigned char* dst = &g_mt.buf[(size_t)g_mt.i * g_mt.W * g_mt.H];
    for (int j = 0; j < g_mt.H; j++) for (int i = 0; i < g_mt.W; i++) {   // box downsample; GL rows are bottom-up, store top-down
        int x0 = i * w / g_mt.W, x1 = (i + 1) * w / g_mt.W, y0 = (g_mt.H - 1 - j) * h / g_mt.H, y1 = (g_mt.H - j) * h / g_mt.H; float sum = 0; int cnt = 0;
        for (int y = y0; y < y1; y += 2) for (int x = x0; x < x1; x += 2) { const unsigned char* q = (const unsigned char*)&px[((size_t)y * w + x) * 3]; sum += 0.0722f * q[0] + 0.7152f * q[1] + 0.2126f * q[2]; cnt++; }
        dst[j * g_mt.W + i] = (unsigned char)(cnt ? sum / cnt : 0); }
    g_mt.i++; g_dcam[3] = g_mt.base + g_mt.step * g_mt.i; g_dcam[2] += g_mt.fwd; g_dcam[0] += g_mt.side; if (g_mt.sleepMs > 0) Sleep(g_mt.sleepMs);
    if (g_mt.i >= g_mt.n) {
        char dir[MAX_PATH], out[MAX_PATH]; snprintf(dir, sizeof dir, "%styrender\\snaps", g_dir); CreateDirectoryA(dir, nullptr); snprintf(out, sizeof out, "%s\\motion_%s.raw", dir, g_mt.name);
        if (FILE* f = fopen(out, "wb")) { int hdr[3] = {g_mt.W, g_mt.H, g_mt.n}; fwrite(hdr, 4, 3, f); fwrite(g_mt.buf.data(), 1, g_mt.buf.size(), f); fclose(f); }
        g_dcamOn = g_mt.savedOn; memcpy(g_dcam, g_mt.savedCam, sizeof g_dcam); g_mt.on = false; logf("[motion] done %s -> %s", g_mt.name, LogPath(out).c_str());
    }
}
