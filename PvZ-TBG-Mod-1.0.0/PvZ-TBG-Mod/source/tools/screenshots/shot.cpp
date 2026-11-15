// Снимки меню для README: рисует настоящий код меню (drawUi из src/ui.cpp) программным растеризатором на Linux и сохраняет RGBA-файлы.
// Запуск: python tools/screenshots/make_screenshots.py
#define PVZ_SHOT 1
#include "../../src/ui.cpp"
#include "imgui_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <math.h>

Shared S;                                        // то, что в игре определено в mod.cpp
std::wstring g_dataDir = L".";
void ModSay(const char* m) { strncpy(S.msg, m, sizeof S.msg - 1); }

struct Fb { int w, h; std::vector<float> px; };   // RGBA, 0..1

static void sample(ImTextureData* t, float u, float v, float out[4]) {
    float x = u * t->Width - 0.5f, y = v * t->Height - 0.5f;
    int x0 = (int)floorf(x), y0 = (int)floorf(y); float fx = x - x0, fy = y - y0;
    float acc[4] = {0, 0, 0, 0};
    for (int j = 0; j < 2; j++) for (int i = 0; i < 2; i++) {
        int xx = std::min(std::max(x0 + i, 0), t->Width - 1), yy = std::min(std::max(y0 + j, 0), t->Height - 1);
        float w = (i ? fx : 1 - fx) * (j ? fy : 1 - fy);
        const unsigned char* p = (const unsigned char*)t->GetPixelsAt(xx, yy);
        if (t->Format == ImTextureFormat_Alpha8) { acc[0] += w; acc[1] += w; acc[2] += w; acc[3] += w * p[0] / 255.f; }
        else for (int c = 0; c < 4; c++) acc[c] += w * p[c] / 255.f;
    }
    for (int c = 0; c < 4; c++) out[c] = acc[c];
}

static void render(ImDrawData* dd, Fb& fb) {
    for (ImTextureData* t : *dd->Textures)
        if (t->Status == ImTextureStatus_WantCreate || t->Status == ImTextureStatus_WantUpdates) { t->SetTexID((ImTextureID)(intptr_t)1); t->SetStatus(ImTextureStatus_OK); }
    for (int n = 0; n < dd->CmdListsCount; n++) {
        const ImDrawList* dl = dd->CmdLists[n];
        for (const ImDrawCmd& cmd : dl->CmdBuffer) {
            if (cmd.UserCallback) continue;
            ImTextureData* tex = cmd.TexRef._TexData;
            int cx0 = std::max(0, (int)cmd.ClipRect.x), cy0 = std::max(0, (int)cmd.ClipRect.y);
            int cx1 = std::min(fb.w, (int)ceilf(cmd.ClipRect.z)), cy1 = std::min(fb.h, (int)ceilf(cmd.ClipRect.w));
            for (unsigned i = 0; i + 2 < cmd.ElemCount; i += 3) {
                const ImDrawVert* v[3];
                for (int k = 0; k < 3; k++) v[k] = &dl->VtxBuffer[dl->IdxBuffer[cmd.IdxOffset + i + k] + cmd.VtxOffset];
                float minx = fminf(v[0]->pos.x, fminf(v[1]->pos.x, v[2]->pos.x)), maxx = fmaxf(v[0]->pos.x, fmaxf(v[1]->pos.x, v[2]->pos.x));
                float miny = fminf(v[0]->pos.y, fminf(v[1]->pos.y, v[2]->pos.y)), maxy = fmaxf(v[0]->pos.y, fmaxf(v[1]->pos.y, v[2]->pos.y));
                int x0 = std::max(cx0, (int)floorf(minx)), x1 = std::min(cx1, (int)ceilf(maxx)), y0 = std::max(cy0, (int)floorf(miny)), y1 = std::min(cy1, (int)ceilf(maxy));
                float d = (v[1]->pos.y - v[2]->pos.y) * (v[0]->pos.x - v[2]->pos.x) + (v[2]->pos.x - v[1]->pos.x) * (v[0]->pos.y - v[2]->pos.y);
                if (fabsf(d) < 1e-9f) continue;
                for (int y = y0; y < y1; y++) for (int x = x0; x < x1; x++) {
                    float px = x + 0.5f, py = y + 0.5f;
                    float l0 = ((v[1]->pos.y - v[2]->pos.y) * (px - v[2]->pos.x) + (v[2]->pos.x - v[1]->pos.x) * (py - v[2]->pos.y)) / d;
                    float l1 = ((v[2]->pos.y - v[0]->pos.y) * (px - v[2]->pos.x) + (v[0]->pos.x - v[2]->pos.x) * (py - v[2]->pos.y)) / d;
                    float l2 = 1 - l0 - l1;
                    if (l0 < -1e-4f || l1 < -1e-4f || l2 < -1e-4f) continue;
                    float col[4];
                    for (int c = 0; c < 4; c++) {
                        auto ch = [&](const ImDrawVert* q) { return ((q->col >> (8 * c)) & 255) / 255.f; };
                        col[c] = l0 * ch(v[0]) + l1 * ch(v[1]) + l2 * ch(v[2]);
                    }
                    if (tex) { float t4[4]; sample(tex, l0 * v[0]->uv.x + l1 * v[1]->uv.x + l2 * v[2]->uv.x, l0 * v[0]->uv.y + l1 * v[1]->uv.y + l2 * v[2]->uv.y, t4); for (int c = 0; c < 4; c++) col[c] *= t4[c]; }
                    float* dst = &fb.px[((size_t)y * fb.w + x) * 4]; float a = col[3];
                    for (int c = 0; c < 3; c++) dst[c] = col[c] * a + dst[c] * (1 - a);
                }
            }
        }
    }
}

int main(int argc, char** argv) {
    if (argc < 6) { fprintf(stderr, "usage: shot <tab> <frozen> <scrollY> <out.rgba> <font.ttf> [scale]\n"); return 2; }
    int tab = atoi(argv[1]), frozen = atoi(argv[2]); float scrollY = (float)atof(argv[3]);
    SC = argc > 6 ? (float)atof(argv[6]) : 1.5f;
    int W0 = (int)(1120 * SC), H0 = (int)(800 * SC);

    InitializeCriticalSection(&S.lock);
    S.versionOk = 1; S.hooked = 1; S.ui = 3; S.wave = 9; S.numWaves = 20; S.sun = 425; S.scene = 0;
    strncpy(S.msg, "Мод подключён. Меню — клавиша Tab.", sizeof S.msg - 1);
    // демонстрационные настройки
    ui = Cfg();
    ui.sunValOn = true; ui.sunVal = 1.5f; ui.skySunOn = true; ui.skySun = 2.f; ui.plantSunOn = true; ui.plantSun = 1.5f;
    ui.atkOn = true; ui.atkSpeed = 1.5f; ui.seedSpeedOn = true; ui.seedSpeed = 2.f; ui.autoCollect = true;
    ui.zCountOn = true; ui.zCount = 1.5f; ui.zHpOn = true; ui.zHpPct = 150; ui.zArmorPct = 200; ui.zSunOn = true; ui.zSunChance = 40; ui.zSunAmount = 25;
    ui.zWaveOn = true; ui.zWave = 1.5f; ui.convSpeedOn = true; ui.convSpeed = 2.f; ui.convSetOn = true;
    { unsigned long long m = 0; for (int t : {0, 5, 7, 10, 18, 26, 28, 29, 32, 34, 39, 40}) m |= 1ull << t; ui.convLo = (int)(unsigned)(m & 0xffffffffull); ui.convHi = (int)(unsigned)(m >> 32); }
    ui.coinValOn = true; ui.coinVal = 2.f;
    cfgSanitize(ui);
    RunState& r = S.runs[4]; r.active = 1; r.playerId = 7; r.pct = 34; r.secs = 5400; strncpy(r.player, "Игрок 1", sizeof r.player - 1);
    RunState& r2 = S.runs[1]; r2.active = 1; r2.done = 1; r2.playerId = 8; r2.pct = 100; r2.secs = 12600; strncpy(r2.player, "Игрок 2", sizeof r2.player - 1);
    S.frozenBuild = frozen;
    g_tab = tab; g_visible = true;

    IMGUI_CHECKVERSION(); ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); io.IniFilename = nullptr;
    io.DisplaySize = ImVec2((float)W0, (float)H0);
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;      // как у настоящих бэкендов: атлас шрифтов обновляет «рендерер»
    io.Fonts->AddFontFromFileTTF(argv[5], 17.f * SC, nullptr, io.Fonts->GetGlyphRangesCyrillic());
    applyStyle();
    // стиль масштабируется так же, как в игре: applyStyle() рассчитан на SC, добавьте при необходимости ScaleAllSizes
    Fb fb{W0, H0, std::vector<float>((size_t)W0 * H0 * 4)};
    for (int frame = 0; frame < 6; frame++) {
        for (size_t i = 0; i < fb.px.size(); i += 4) { fb.px[i] = 0.067f; fb.px[i + 1] = 0.086f; fb.px[i + 2] = 0.110f; fb.px[i + 3] = 1.f; }
        io.DeltaTime = 1.f / 60;
        for (ImTextureData* t : ImGui::GetPlatformIO().Textures) if (t->Status == ImTextureStatus_WantCreate || t->Status == ImTextureStatus_WantUpdates) { t->SetTexID((ImTextureID)(intptr_t)1); t->SetStatus(ImTextureStatus_OK); }
        ImGui::NewFrame();
        if (frame >= 2 && scrollY > 0) for (ImGuiWindow* w : ImGui::GetCurrentContext()->Windows) if (strstr(w->Name, "##content")) ImGui::SetScrollY(w, scrollY);
        drawUi();
        ImGui::Render();
        render(ImGui::GetDrawData(), fb);
    }
    FILE* f = fopen(argv[4], "wb");
    fwrite(&W0, 4, 1, f); fwrite(&H0, 4, 1, f);
    for (size_t i = 0; i < fb.px.size(); i++) { int v = (int)(fminf(fmaxf(fb.px[i], 0.f), 1.f) * 255.f + 0.5f); fputc(v, f); }
    fclose(f);
    return 0;
}
