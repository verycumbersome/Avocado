#include "gui.h"
#include <imgui.h>
#include <cstdint>
#include <filesystem>
#include "utils/string.h"
#include "gte.h"
#include "platform/windows/config.h"

using namespace std::experimental::filesystem::v1;

const char *mapIo(uint32_t address) {
    address -= 0x1f801000;

#define IO(begin, end, periph)                   \
    if (address >= (begin) && address < (end)) { \
        return periph;                           \
    }

    IO(0x00, 0x24, "memoryControl");
    IO(0x40, 0x50, "controller");
    IO(0x50, 0x60, "serial");
    IO(0x60, 0x64, "memoryControl");
    IO(0x70, 0x78, "interrupt");
    IO(0x80, 0x100, "dma");
    IO(0x100, 0x110, "timer0");
    IO(0x110, 0x120, "timer1");
    IO(0x120, 0x130, "timer2");
    IO(0x800, 0x804, "cdrom");
    IO(0x810, 0x818, "gpu");
    IO(0x820, 0x828, "mdec");
    IO(0xC00, 0x1000, "spu");
    IO(0x1000, 0x1043, "exp2");
    return "";
}
int vramTextureId = 0;
bool gteRegistersEnabled = false;
bool ioLogEnabled = false;
bool gteLogEnabled = false;
bool gpuLogEnabled = false;
bool showVRAM = false;
bool singleFrame = false;
bool skipRender = false;
bool showIo = false;
bool exitProgram = false;
bool doHardReset = false;

bool showVramWindow = false;
bool showBiosWindow = false;

void replayCommands(mips::gpu::GPU *gpu, int to) {
    auto commands = gpu->gpuLogList;
    gpu->vram = gpu->prevVram;

    gpu->gpuLogEnabled = false;
    for (int i = 0; i <= to; i++) {
        auto cmd = commands.at(i);

        if (cmd.args.size() == 0) printf("Panic! no args");
        int mask = cmd.command << 24;

        for (auto arg : cmd.args) {
            gpu->write(0, mask | arg);
            mask = 0;
        }
    }
    gpu->gpuLogEnabled = true;
}

void dumpRegister(const char *name, uint32_t *reg) {
    ImGui::BulletText(name);
    ImGui::NextColumn();
    ImGui::InputInt("", (int *)reg, 1, 100, ImGuiInputTextFlags_CharsHexadecimal);
    ImGui::NextColumn();
}

void gteRegistersWindow(mips::gte::GTE gte) {
    if (!gteRegistersEnabled) {
        return;
    }
    ImGui::Begin("GTE registers", &gteRegistersEnabled);

    ImGui::Columns(3, 0, false);
    ImGui::Text("IR1:  %04hX", gte.ir[1]);
    ImGui::NextColumn();
    ImGui::Text("IR2:  %04hX", gte.ir[2]);
    ImGui::NextColumn();
    ImGui::Text("IR3:  %04hX", gte.ir[3]);
    ImGui::NextColumn();

    ImGui::Separator();
    ImGui::Text("MAC0: %08X", gte.mac[0]);

    ImGui::Separator();
    ImGui::Text("MAC1: %08X", gte.mac[1]);
    ImGui::NextColumn();
    ImGui::Text("MAC2: %08X", gte.mac[2]);
    ImGui::NextColumn();
    ImGui::Text("MAC3: %08X", gte.mac[3]);
    ImGui::NextColumn();

    ImGui::Separator();
    ImGui::Text("TRX:  %08X", gte.tr.x);
    ImGui::NextColumn();
    ImGui::Text("TRY:  %08X", gte.tr.y);
    ImGui::NextColumn();
    ImGui::Text("TRZ:  %08X", gte.tr.z);
    ImGui::NextColumn();

    ImGui::Separator();

    for (int i = 0; i < 4; i++) {
        ImGui::Text("S%dX:  %04hX", i, gte.s[i].x);
        ImGui::NextColumn();
        ImGui::Text("S%dY:  %04hX", i, gte.s[i].y);
        ImGui::NextColumn();
        ImGui::Text("S%dZ:  %04hX", i, gte.s[i].z);
        ImGui::NextColumn();
    }

    ImGui::Columns(3, 0, false);
    ImGui::Separator();

    ImGui::Text("RT11:  %04hX", gte.rt.v11);
    ImGui::NextColumn();
    ImGui::Text("RT12:  %04hX", gte.rt.v12);
    ImGui::NextColumn();
    ImGui::Text("RT13:  %04hX", gte.rt.v13);
    ImGui::NextColumn();

    ImGui::Text("RT21:  %04hX", gte.rt.v21);
    ImGui::NextColumn();
    ImGui::Text("RT22:  %04hX", gte.rt.v22);
    ImGui::NextColumn();
    ImGui::Text("RT23:  %04hX", gte.rt.v23);
    ImGui::NextColumn();

    ImGui::Text("RT31:  %04hX", gte.rt.v31);
    ImGui::NextColumn();
    ImGui::Text("RT32:  %04hX", gte.rt.v32);
    ImGui::NextColumn();
    ImGui::Text("RT33:  %04hX", gte.rt.v33);
    ImGui::NextColumn();

    ImGui::Separator();
    ImGui::Text("VX0:  %04hX", gte.v[0].x);
    ImGui::NextColumn();
    ImGui::Text("VY0:  %04hX", gte.v[0].y);
    ImGui::NextColumn();
    ImGui::Text("VZ0:  %04hX", gte.v[0].z);
    ImGui::NextColumn();

    ImGui::Separator();
    ImGui::Text("OFX:  %08X", gte.of[0]);
    ImGui::NextColumn();
    ImGui::Text("OFY:  %08X", gte.of[1]);
    ImGui::NextColumn();
    ImGui::Text("H:   %04hX", gte.h);
    ImGui::NextColumn();

    ImGui::Separator();
    ImGui::Text("DQA:  %04hX", gte.dqa);
    ImGui::NextColumn();
    ImGui::Text("DQB:  %08X", gte.dqb);
    ImGui::NextColumn();

    ImGui::End();
}

void gteLogWindow(mips::CPU *cpu) {
    if (!gteLogEnabled) {
        return;
    }
    static char filterBuffer[16];
    static bool searchActive = false;
    bool filterActive = strlen(filterBuffer) > 0;
    ImGui::Begin("GTE Log", &gteLogEnabled);

    ImGui::BeginChild("GTE Log", ImVec2(0, -ImGui::GetItemsLineHeightWithSpacing()), false, ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

    for (int i = 0; i < cpu->gte.log.size(); i++) {
        auto ioEntry = cpu->gte.log[i];
        std::string t;
        if (ioEntry.mode == mips::gte::GTE::GTE_ENTRY::MODE::func) {
            t = string_format("%5d %c 0x%02x", i, 'F', ioEntry.n);
        } else {
            t = string_format("%5d %c %2d: 0x%08x", i, ioEntry.mode == mips::gte::GTE::GTE_ENTRY::MODE::read ? 'R' : 'W', ioEntry.n,
                              ioEntry.data);
        }
        if (filterActive && t.find(filterBuffer) != std::string::npos)  // if found
        {
            // if search enabled - continue
            ImGui::TextColored(ImVec4(1.f, 1.f, 0.f, 1.f), t.c_str());
            continue;
        }
        if (searchActive) continue;

        ImGui::Text(t.c_str());
    }
    ImGui::PopStyleVar();
    ImGui::EndChild();

    ImGui::Text("Filter");
    ImGui::SameLine();
    if (ImGui::InputText("", filterBuffer, sizeof(filterBuffer) / sizeof(char), ImGuiInputTextFlags_EnterReturnsTrue)) {
        searchActive = !searchActive;
    }

    ImGui::End();
}

void gpuLogWindow(mips::CPU *cpu) {
    if (!gpuLogEnabled) {
        return;
    }
    ImGui::Begin("GPU Log", &gpuLogEnabled);

    ImGui::BeginChild("GPU Log", ImVec2(0, -ImGui::GetItemsLineHeightWithSpacing()), false);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

    int renderTo = -1;
    ImGuiListClipper clipper(cpu->getGPU()->gpuLogList.size());
    while (clipper.Step()) {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
            auto entry = cpu->getGPU()->gpuLogList[i];

            bool nodeOpen = ImGui::TreeNode((void *)(intptr_t)i, "cmd: 0x%02x  %s", entry.command, device::gpu::CommandStr[(int)entry.cmd]);

            if (ImGui::IsItemHovered()) {
                printf("Render to cmd %d\n", i);
                renderTo = i;
            }

            if (nodeOpen) {
                // Render arguments
                for (auto arg : entry.args) {
                    ImGui::Text("- 0x%08x", arg);
                }
                ImGui::TreePop();
            }
        }
    }
    ImGui::PopStyleVar();
    ImGui::EndChild();

    ImGui::End();

    if (renderTo >= 0) {
        replayCommands(cpu->getGPU(), renderTo);
    }
}

void ioWindow(mips::CPU *cpu) {
    if (!showIo) {
        return;
    }
    ImGui::Begin("IO", &showIo);

    ImGui::Columns(1, 0, false);
    ImGui::Text("Timer 0");

    ImGui::Columns(2, 0, false);
    dumpRegister("current", (uint32_t *)&cpu->timer0->current);
    dumpRegister("target", (uint32_t *)&cpu->timer0->target);
    dumpRegister("mode", (uint32_t *)&cpu->timer0->mode);

    ImGui::Columns(1, 0, false);
    ImGui::Text("Timer 1");

    ImGui::Columns(2, 0, false);
    dumpRegister("current", (uint32_t *)&cpu->timer1->current);
    dumpRegister("target", (uint32_t *)&cpu->timer1->target);
    dumpRegister("mode", (uint32_t *)&cpu->timer1->mode);

    ImGui::Columns(1, 0, false);
    ImGui::Text("Timer 2");

    ImGui::Columns(2, 0, false);
    dumpRegister("current", (uint32_t *)&cpu->timer2->current);
    dumpRegister("target", (uint32_t *)&cpu->timer2->target);
    dumpRegister("mode", (uint32_t *)&cpu->timer2->mode);

    ImGui::End();
}

void ioLogWindow(mips::CPU *cpu) {
#ifdef ENABLE_IO_LOG
    if (!ioLogEnabled) {
        return;
    }
    ImGui::Begin("IO Log", &ioLogEnabled);

    ImGui::BeginChild("IO Log", ImVec2(0, -ImGui::GetItemsLineHeightWithSpacing()), false, ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

    ImGuiListClipper clipper(cpu->ioLogList.size());
    while (clipper.Step()) {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
            auto ioEntry = cpu->ioLogList[i];
            ImGui::Text("%c %2d 0x%08x: 0x%0*x %*s %s", ioEntry.mode == mips::CPU::IO_LOG_ENTRY::MODE::READ ? 'R' : 'W', ioEntry.size,
                        ioEntry.addr, ioEntry.size / 4, ioEntry.data,
                        // padding
                        8 - ioEntry.size / 4, "", mapIo(ioEntry.addr));
        }
    }
    ImGui::PopStyleVar();
    ImGui::EndChild();

    ImGui::End();
#endif
}

void biosSelectionWindow() {
    static bool biosesFound = false;
    static std::vector<std::string> bioses;
    static int currentBiosIndex = 0;

    if (!biosesFound) {
        bioses.clear();
        auto dir = directory_iterator("data/bios");
        for (auto &e : dir) {
            if (!is_regular_file(e)) continue;

            auto path = e.path().string();
            auto ext = getExtension(path);
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

            if (ext == "bin" || ext == "rom") {
                bioses.push_back(path);
            }
        }
        biosesFound = true;
    }

    ImGui::Begin("BIOS", &showBiosWindow);
    if (bioses.empty()) {
        ImGui::Text(
            "BIOS directory is empty.\n"
            "You need one of BIOS files (eg. SCPH1001.bin) placed in data/bios directory.\n"
            ".bin and .rom extensions are recognised.");
    } else {
        ImGui::PushItemWidth(-1);
        ImGui::ListBox("", &currentBiosIndex, [](void *data, int idx, const char **out_text) {
            const std::vector<std::string> *v = (std::vector<std::string> *)data;
            *out_text = v->at(idx).c_str();
            return true;
        }, (void *)&bioses, (int)bioses.size());

        if (ImGui::Button("Select") && currentBiosIndex < bioses.size()) {
            config["bios"] = bioses[currentBiosIndex];
            config["initialized"] = true;

            biosesFound = false;
            //        showBiosWindow = false;
            doHardReset = true;
        }
        ImGui::PopItemWidth();
    }
    ImGui::End();
}

void vramWindow() {
    auto defaultSize = ImVec2(1024, 512);
    ImGui::Begin("VRAM", &showVramWindow, defaultSize, -1, ImGuiWindowFlags_NoScrollbar);
    ImGui::Image(ImTextureID(vramTextureId), ImGui::GetWindowSize());
    if (ImGui::Button("Reset size")) ImGui::SetWindowSize(defaultSize);
    ImGui::End();
}

void renderImgui(mips::CPU *cpu) {
    auto gte = cpu->gte;

    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Exit")) {
                exitProgram = false;
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Emulation")) {
            if (ImGui::MenuItem("Soft reset", "F2")) cpu->softReset();
            if (ImGui::MenuItem("Hard reset")) doHardReset = true;

            const char *shellStatus = cpu->cdrom->getShell() ? "Shell opened" : "Shell closed";
            if (ImGui::MenuItem(shellStatus, "F3")) {
                cpu->cdrom->toggleShell();
            }

            if (ImGui::MenuItem("Single frame", "F7")) {
                singleFrame = true;
                cpu->state = mips::CPU::State::run;
            }

            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Debug")) {
            ImGui::MenuItem("IO", NULL, &showIo);
            ImGui::MenuItem("GTE registers", NULL, &gteRegistersEnabled);
            ImGui::MenuItem("BIOS log", "F5", &cpu->biosLog);
#ifdef ENABLE_IO_LOG
            ImGui::MenuItem("IO log", NULL, &ioLogEnabled);
#endif
            ImGui::MenuItem("GTE log", NULL, &gteLogEnabled);
            ImGui::MenuItem("GPU log", NULL, &gpuLogEnabled);
            ImGui::MenuItem("VRAM window", NULL, &showVramWindow);
            ImGui::MenuItem("Disassembly (slow)", "F6", &cpu->disassemblyEnabled);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Options")) {
            if (ImGui::MenuItem("BIOS", NULL)) showBiosWindow = true;
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    gteRegistersWindow(gte);
    ioLogWindow(cpu);
    gteLogWindow(cpu);
    gpuLogWindow(cpu);
    ioWindow(cpu);

    if (showBiosWindow) biosSelectionWindow();
    if (showVramWindow) vramWindow();

    if (!config["initialized"]) {
        ImGui::Begin("Avocado");
        ImGui::Text("Avocado needs to be set up before running.");
        ImGui::Text("You need one of BIOS files placed in data/bios directory.");

        if (ImGui::Button("Select BIOS file")) {
            showBiosWindow = true;
            config["initialized"] = true;
        }

        ImGui::End();
    }

    ImGui::Render();
}
