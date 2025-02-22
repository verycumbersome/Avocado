#include "system.h"
#include <fmt/core.h>
#include <cstdlib>
#include <cstring>
#include "bios/functions.h"
#include "config.h"
#include "sound/sound.h"
#include "utils/address.h"
#include "utils/gpu_draw_list.h"
#include "utils/file.h"
#include "utils/psx_exe.h"

// Translation includes
#include <map>
#include <iostream>
#include <nlohmann/json.hpp>
#include <unistd.h>
#include <fstream>

void System::fillTranslationTable(){
    std::ifstream ifs("/Users/matthewjordan/Code/TokimekiMemorialTranslated/pointer_table.json", std::ios::in);

    nlohmann::json pointer_tbl;
    ifs >> pointer_tbl;

    for (auto iter: pointer_tbl.items()) {
            auto key = iter.key();
            auto val = iter.value();

            std::vector<uint8_t> tmp;
            for (int i = 0; i < val.size(); i++){
                tmp.push_back(val[i]);
            }
            tmp.push_back(0); // add NULL pointer to sentence

            //std::cout << std::stoi(key) << std::endl;
            std::cout << "KEY: " << key << std::endl;
            translation[std::stoi(key)] = tmp;
    }
}

//void System::fillTranslationTable(){
    //translation[0] = {124, 142, 142, 142};
//}

System::System() {
    bios.fill(0);
    ram.resize(!config.options.system.ram8mb ? RAM_SIZE_2MB : RAM_SIZE_8MB, 0);
    scratchpad.fill(0);
    expansion.fill(0);

    fillTranslationTable();
    int pointer_tmp = 1638400;

    translation[pointer_tmp] = {130, 128, 0x20, 0x81, 0x42};

    cpu = std::make_unique<mips::CPU>(this);
    gpu = std::make_unique<gpu::GPU>(this);
    spu = std::make_unique<spu::SPU>(this);
    mdec = std::make_unique<mdec::MDEC>();

    cdrom = std::make_unique<device::cdrom::CDROM>(this);
    controller = std::make_unique<device::controller::Controller>(this);
    dma = std::make_unique<device::dma::DMA>(this);
    expansion2 = std::make_unique<Expansion2>();
    interrupt = std::make_unique<Interrupt>(this);
    memoryControl = std::make_unique<MemoryControl>();
    ramControl = std::make_unique<RamControl>();
    cacheControl = std::make_unique<CacheControl>(this);
    serial = std::make_unique<Serial>();
    for (int t : {0, 1, 2}) {
        timer[t] = std::make_unique<device::timer::Timer>(this, t);
    }

    debugOutput = config.debug.log.system;
    biosLog = config.debug.log.bios;

    cycles = 0;
}

// Note: stupid static_casts and asserts are only to suppress MSVC warnings

// Warning: This function does not check array boundaries. Make sure that address is aligned!
template <typename T>
constexpr T read_fast(uint8_t* device, uint32_t addr) {
    static_assert(std::is_same<T, uint8_t>() || std::is_same<T, uint16_t>() || std::is_same<T, uint32_t>(), "Invalid type used");

    if (sizeof(T) == 1) return static_cast<T>(((uint8_t*)device)[addr]);
    if (sizeof(T) == 2) return static_cast<T>(((uint16_t*)device)[addr / 2]);
    if (sizeof(T) == 4) return static_cast<T>(((uint32_t*)device)[addr / 4]);
    return 0;
}

// Warning: This function does not check array boundaries. Make sure that address is aligned!
template <typename T>
constexpr void write_fast(uint8_t* device, uint32_t addr, T value) {
    static_assert(std::is_same<T, uint8_t>() || std::is_same<T, uint16_t>() || std::is_same<T, uint32_t>(), "Invalid type used");

    if (sizeof(T) == 1)
        ((uint8_t*)device)[addr] = static_cast<uint8_t>(value);
    else if (sizeof(T) == 2)
        ((uint16_t*)device)[addr / 2] = static_cast<uint16_t>(value);
    else if (sizeof(T) == 4)
        ((uint32_t*)device)[addr / 4] = static_cast<uint32_t>(value);
}

template <typename T, typename Device>
constexpr T read_io(Device& periph, uint32_t addr) {
    static_assert(std::is_same<T, uint8_t>() || std::is_same<T, uint16_t>() || std::is_same<T, uint32_t>(), "Invalid type used");

    if (sizeof(T) == 1) return periph->read(addr);
    if (sizeof(T) == 2) return periph->read(addr) | periph->read(addr + 1) << 8;
    if (sizeof(T) == 4)
        return periph->read(addr) | periph->read(addr + 1) << 8 | periph->read(addr + 2) << 16 | periph->read(addr + 3) << 24;
    return 0;
}

template <typename T, typename Device>
constexpr void write_io(Device& periph, uint32_t addr, T data) {
    static_assert(std::is_same<T, uint8_t>() || std::is_same<T, uint16_t>() || std::is_same<T, uint32_t>(), "Invalid type used");

    if (sizeof(T) == 1) {
        periph->write(addr, (static_cast<uint8_t>(data)) & 0xff);
    } else if (sizeof(T) == 2) {
        periph->write(addr, (static_cast<uint16_t>(data)) & 0xff);
        periph->write(addr + 1, (static_cast<uint16_t>(data) >> 8) & 0xff);
    } else if (sizeof(T) == 4) {
        periph->write(addr, (static_cast<uint32_t>(data)) & 0xff);
        periph->write(addr + 1, (static_cast<uint32_t>(data) >> 8) & 0xff);
        periph->write(addr + 2, (static_cast<uint32_t>(data) >> 16) & 0xff);
        periph->write(addr + 3, (static_cast<uint32_t>(data) >> 24) & 0xff);
    }
}

#ifdef ENABLE_IO_LOG
#define LOG_IO(mode, size, addr, data, pc) ioLogList.push_back({(mode), (size), (addr), (data), (pc)})
#else
#define LOG_IO(mode, size, addr, data, pc)
#endif

#define READ_IO(begin, end, periph)                                              \
    if (addr >= (begin) && addr < (end)) {                                       \
        auto data = read_io<T>((periph), addr - (begin));                        \
                                                                                 \
        LOG_IO(IO_LOG_ENTRY::MODE::READ, sizeof(T) * 8, address, data, cpu->PC); \
        return data;                                                             \
    }

#define READ_IO32(begin, end, periph)                                                                                    \
    if (addr >= (begin) && addr < (end)) {                                                                               \
        T data = 0;                                                                                                      \
        if (sizeof(T) == 4) {                                                                                            \
            data = (periph)->read(addr - (begin));                                                                       \
        } else {                                                                                                         \
            fmt::print("[SYS] R Unsupported access to " #periph " with bit size {}\n", static_cast<int>(sizeof(T) * 8)); \
        }                                                                                                                \
                                                                                                                         \
        LOG_IO(IO_LOG_ENTRY::MODE::READ, sizeof(T) * 8, address, data, cpu->PC);                                         \
        return data;                                                                                                     \
    }

#define WRITE_IO(begin, end, periph)                                              \
    if (addr >= (begin) && addr < (end)) {                                        \
        write_io<T>((periph), addr - (begin), data);                              \
                                                                                  \
        LOG_IO(IO_LOG_ENTRY::MODE::WRITE, sizeof(T) * 8, address, data, cpu->PC); \
        return;                                                                   \
    }

#define WRITE_IO32(begin, end, periph)                                                                                   \
    if (addr >= (begin) && addr < (end)) {                                                                               \
        if (sizeof(T) == 4) {                                                                                            \
            (periph)->write(addr - (begin), data);                                                                       \
        } else {                                                                                                         \
            fmt::print("[SYS] W Unsupported access to " #periph " with bit size {}\n", static_cast<int>(sizeof(T) * 8)); \
        }                                                                                                                \
                                                                                                                         \
        LOG_IO(IO_LOG_ENTRY::MODE::WRITE, sizeof(T) * 8, address, data, cpu->PC);                                        \
        return;                                                                                                          \
    }

template <typename T>
INLINE T System::readMemory(uint32_t address) {
    static_assert(std::is_same<T, uint8_t>() || std::is_same<T, uint16_t>() || std::is_same<T, uint32_t>(), "Invalid type used");

    uint32_t addr = align_mips<T>(address);

    if (in_range<RAM_BASE, RAM_SIZE_8MB>(addr)) {
        ram_tmp = read_fast<T>(ram.data(), (addr - RAM_BASE) & (ram.size() - 1));

        if (print_dialog){
            // Get the name of the person speaking
            if ((addr > 0x0CA032) && (addr < 0x0CA068)) {
                if ((sizeof(T) == 1)) {
                    if (ram_tmp == 0) {
                        std::cout << "[NAME]" << std::endl;
                    } else {
                        delimeter = true;
                        printf("%X ", ram_tmp);
                    }
                }
            }

            // Get the dialog of the person speaking
            if ((addr > 0x1FEBEE) && (addr < 0x1FEC90)) {
                if (sizeof(T) == 1) {
                    if (ram_tmp == 0) {
                        std::cout << std::endl;
                    } else {
                        delimeter = true;
                        printf("%X ", ram_tmp);
                    }

                } else if (sizeof(T) == 2 && delimeter) {
                    delimeter = false;
                    std::cout << std::endl;
                }
            }
        }


        if (debug_read_trace){
            trace.push(addr);
            trace.push(ram_tmp);
            if (trace.size() > trace_len) {
                trace.pop();
                trace.pop();
            }

            //if (((addr >= breakpoint) && (addr <= breakpoint)) || breakpoint_reached){
            if (((ram_tmp >= breakpoint) && (ram_tmp <= breakpoint)) || breakpoint_reached){
                breakpoint_reached = true;
                trace_counter--;

                //for (uint32_t i = 0; i < sizeof(cpu->reg); i++){
                    //printf("REG %d: %X\n", i, cpu->reg[i]);
                //}

                if (trace_counter <= 0){
                    printf("\nSTART TRACE:\n");
                    printf(" ----------------------\n");
                    for (uint32_t i = 1; i < trace.size(); i++){
                        printf("%d. \033[34m ADDR:\033[0m 0x%-10X", i, trace.front());
                        trace.pop();
                        printf("\033[32mDATA:\033[0m 0x%-10X", trace.front());
                        trace.pop();

                        printf("\n");
                    }
                    printf("\033[31mBYTE\n");
                    printf("\033[34m ADDR:\033[0m 0x%-10X", addr);
                    printf("\033[32mDATA:\033[0m 0x%-10X\n", ram_tmp);
                    printf(" ----------------------\n");
                    printf("END TRACE:\n\n");
                }
            }
        }

        return read_fast<T>(ram.data(), (addr - RAM_BASE) & (ram.size() - 1));
    }
    if (in_range<EXPANSION_BASE, EXPANSION_SIZE>(addr)) {
        return read_fast<T>(expansion.data(), addr - EXPANSION_BASE);
    }
    if (in_range<SCRATCHPAD_BASE, SCRATCHPAD_SIZE>(addr)) {
        return read_fast<T>(scratchpad.data(), addr - SCRATCHPAD_BASE);
    }
    if (in_range<BIOS_BASE, BIOS_SIZE>(addr)) {
        return read_fast<T>(bios.data(), addr - BIOS_BASE);
    }

    READ_IO(0x1f801000, 0x1f801024, memoryControl);
    READ_IO(0x1f801040, 0x1f801050, controller);
    READ_IO(0x1f801050, 0x1f801060, serial);
    READ_IO(0x1f801060, 0x1f801064, ramControl);
    READ_IO(0x1f801070, 0x1f801078, interrupt);
    READ_IO(0x1f801080, 0x1f801100, dma);
    READ_IO(0x1f801100, 0x1f801110, timer[0]);
    READ_IO(0x1f801110, 0x1f801120, timer[1]);
    READ_IO(0x1f801120, 0x1f801130, timer[2]);
    READ_IO(0x1f801800, 0x1f801804, cdrom);
    READ_IO32(0x1f801810, 0x1f801818, gpu);
    READ_IO32(0x1f801820, 0x1f801828, mdec);
    READ_IO(0x1f801C00, 0x1f802000, spu);
    READ_IO(0x1f802000, 0x1f804000, expansion2);


    if (in_range<0xfffe0130, 4>(address) && sizeof(T) == 4) {
        auto data = cacheControl->read(0);
        LOG_IO(IO_LOG_ENTRY::MODE::READ, sizeof(T) * 8, address, data, cpu->PC);
        return data;
    }

    printf("Translation addr: %X\n", addr);
    if (in_range<TRANSLATION_BASE, TRANSLATION_SIZE>(addr)) {
        // ADDR: The key for the JSON given by the remapped pointer in ROM
        // PTR: The pointer in the array at the JSON address

        if (ptr == 0) {
            ptr = addr - (TRANSLATION_BASE + 1);
        }

        addr = addr - ((TRANSLATION_BASE + 1) + ptr);
        printf("Translation addr: %X\n", addr);

        // If addr is in translation bank
        if (translation.find(ptr) != translation.end()) {
            printf("PTR %d\n", ptr);
            printf("ADDR %d\n", addr);
            for (int i = 0; i < translation[ptr].size(); i++){
                printf("%d\n", translation[ptr][i]);
            }

            if (translation[ptr][addr] == 0){
                ptr = 0;
                return 0;
            }

            return read_fast<T>(translation[ptr].data(), addr);

        } else {
            return 0;
        }
    }

    fmt::print("[SYS] R Unhandled address at 0x{:08x}\n", address);
    cpu->busError();

    return 0;
}
template <typename T>
INLINE void System::writeMemory(uint32_t address, T data) {
    static_assert(std::is_same<T, uint8_t>() || std::is_same<T, uint16_t>() || std::is_same<T, uint32_t>(), "Invalid type used");

    if (unlikely(cpu->cop0.status.isolateCache)) {
        uint32_t tag = (address & 0xfffff000) >> 12;
        uint16_t index = (address & 0xffc) >> 2;
        cpu->icache[index] = mips::CacheLine{tag, data};
        return;
    }

    uint32_t addr = align_mips<T>(address);

    if (in_range<RAM_BASE, RAM_SIZE_8MB>(addr)) {
        return write_fast<T>(ram.data(), (addr - RAM_BASE) & (ram.size() - 1), data);
    }
    if (in_range<EXPANSION_BASE, EXPANSION_SIZE>(addr)) {
        return write_fast<T>(expansion.data(), addr - EXPANSION_BASE, data);
    }
    if (in_range<SCRATCHPAD_BASE, SCRATCHPAD_SIZE>(addr)) {
        return write_fast<T>(scratchpad.data(), addr - SCRATCHPAD_BASE, data);
    }

    WRITE_IO(0x1f801000, 0x1f801024, memoryControl);
    WRITE_IO(0x1f801040, 0x1f801050, controller);
    WRITE_IO(0x1f801050, 0x1f801060, serial);
    WRITE_IO(0x1f801060, 0x1f801064, ramControl);
    WRITE_IO(0x1f801070, 0x1f801078, interrupt);
    WRITE_IO(0x1f801080, 0x1f801100, dma);
    WRITE_IO(0x1f801100, 0x1f801110, timer[0]);
    WRITE_IO(0x1f801110, 0x1f801120, timer[1]);
    WRITE_IO(0x1f801120, 0x1f801130, timer[2]);
    WRITE_IO(0x1f801800, 0x1f801804, cdrom);
    WRITE_IO32(0x1f801810, 0x1f801818, gpu);
    WRITE_IO32(0x1f801820, 0x1f801828, mdec);
    WRITE_IO(0x1f801C00, 0x1f802000, spu);
    WRITE_IO(0x1f802000, 0x1f804000, expansion2);

    if (in_range<0xfffe0130, 4>(address) && sizeof(T) == 4) {
        cacheControl->write(0, data);
        LOG_IO(IO_LOG_ENTRY::MODE::WRITE, sizeof(T) * 8, address, data, cpu->PC);
        return;
    }


    fmt::print("[SYS] W Unhandled address at 0x{:08x}: 0x{:02x}\n", address, data);
    cpu->busError();
}

uint8_t System::readMemory8(uint32_t address) {
    return readMemory<uint8_t>(address);
}

uint16_t System::readMemory16(uint32_t address) {
    return readMemory<uint16_t>(address);
}

uint32_t System::readMemory32(uint32_t address) {
    return readMemory<uint32_t>(address);
}

void System::writeMemory8(uint32_t address, uint8_t data) {
    writeMemory<uint8_t>(address, data);
}

void System::writeMemory16(uint32_t address, uint16_t data) {
    writeMemory<uint16_t>(address, data);
}

void System::writeMemory32(uint32_t address, uint32_t data) {
    writeMemory<uint32_t>(address, data);
}

void System::printFunctionInfo(const char* functionNum, const bios::Function& f) {
    fmt::print("  {}: {}(", functionNum, f.name);
    unsigned int a = 0;
    for (auto arg : f.args) {
        uint32_t param = cpu->reg[4 + a];
        if (true) {
            fmt::print("{} = ", arg.name);
        }
        switch (arg.type) {
            case bios::Type::INT:
            case bios::Type::POINTER: fmt::print("0x{:x}", param); break;
            case bios::Type::CHAR: fmt::print("'{:c}'", (char)param); break;
            case bios::Type::STRING: {
                fmt::print("\"");
                for (int i = 0; i < 32; i++) {
                    uint8_t c = readMemory8(param + i);

                    if (c == 0) {
                        break;
                    } else if (c != 0 && i == 32 - 1) {
                        fmt::print("...");
                    } else {
                        fmt::print("{:c}", isprint(c) ? (char)c : '_');
                    }
                }
                fmt::print("\"");
                break;
            }
        }
        if (a < (f.args.size() - 1)) {
            fmt::print(", ");
        }
        a++;
        if (a > 4) break;
    }
    fmt::print(")\n");
}

void System::handleBiosFunction() {
    uint32_t maskedPC = cpu->PC & 0x1FFFFF;
    uint8_t functionNumber = cpu->reg[9];
    bool log = biosLog;

    int tableNum = (maskedPC - 0xA0) / 0x10;
    if (tableNum > 2) return;

    const auto& table = bios::tables[tableNum];
    const auto& function = table.find(functionNumber);

    if (function == table.end()) {
        fmt::print("  BIOS {:1X}(0x{:02X}): Unknown function!\n", 0xA + tableNum, functionNumber);
        return;
    }
    if (function->second.callback != nullptr) {
        log = function->second.callback(this);
    }

    if (log) {
        std::string type = fmt::format("BIOS {:1X}({:02X})", 0xA + tableNum, functionNumber);
        printFunctionInfo(type.c_str(), function->second);
    }
}

void System::handleSyscallFunction() {
    uint8_t functionNumber = cpu->reg[4];
    bool log = biosLog;

    const auto& function = bios::SYSCALL.find(functionNumber);
    if (function == bios::SYSCALL.end()) return;

    if (function->second.callback != nullptr) {
        log = function->second.callback(this);
    }

    if (log) {
        std::string type = fmt::format("SYSCALL({:X})", functionNumber);
        cpu->sys->printFunctionInfo(type.c_str(), function->second);
    }
}

void System::singleStep() {
    state = State::run;
    cpu->executeInstructions(1);
    state = State::pause;

    dma->step();
    cdrom->step(3);
    timer[0]->step(3);
    timer[1]->step(3);
    timer[2]->step(3);
    controller->step();
    spu->step(cdrom.get());

    if (gpu->emulateGpuCycles(3)) {
        interrupt->trigger(interrupt::VBLANK);
    }
}

void System::emulateFrame() {
#ifdef ENABLE_IO_LOG
    ioLogList.clear();
#endif
    cpu->gte.log.clear();

    if (GpuDrawList::currentFrame == 0) {
        gpu->prevVram = gpu->vram;

        // Save initial state
        if (gpu->gpuLogEnabled) {
            gpu->gpuLogList.clear();
            GpuDrawList::dumpInitialState(gpu.get());
        }
    }

    if (++GpuDrawList::currentFrame >= GpuDrawList::framesToCapture) {
        GpuDrawList::currentFrame = 0;
        if (GpuDrawList::framesToCapture != 0) {
            toast(fmt::format("{} frames capture complete", GpuDrawList::framesToCapture));
            GpuDrawList::framesToCapture = 0;
            state = State::pause;
            return;
        }
    }

    int systemCycles = 300;
    for (;;) {
        if (!cpu->executeInstructions(systemCycles / 3)) {
            return;
        }

        dma->step();
        cdrom->step(systemCycles / 1.5f);
        timer[0]->step(systemCycles);
        timer[1]->step(systemCycles);
        timer[2]->step(systemCycles);

        static float spuCounter = 0;

        float magicNumber = 1.575f;
        if (!gpu->isNtsc()) {
            // Hack to prevent crackling audio on PAL games
            // Note - this overclocks SPU clock, bugs might appear.
            magicNumber *= 50.f / 60.f;
        }
        spuCounter += (float)systemCycles / magicNumber / (float)0x300;
        if (spuCounter >= 1.f) {
            spu->step(cdrom.get());
            spuCounter -= 1.0f;
        }

        if (spu->bufferReady) {
            spu->bufferReady = false;
            Sound::appendBuffer(spu->audioBuffer.begin(), spu->audioBuffer.end());
        }

        controller->step();

        if (gpu->emulateGpuCycles(systemCycles)) {
            interrupt->trigger(interrupt::VBLANK);
            return;  // frame emulated
        }

        // TODO: Move this code to Timer class
        if (gpu->gpuLine > gpu->linesPerFrame() - 20) {
            auto& t = *timer[1];
            if (t.mode.syncEnabled) {
                using modes = device::timer::CounterMode::SyncMode1;
                auto mode1 = static_cast<modes>(timer[1]->mode.syncMode);
                if (mode1 == modes::resetAtVblank || mode1 == modes::resetAtVblankAndPauseOutside) {
                    timer[1]->current._reg = 0;
                } else if (mode1 == modes::pauseUntilVblankAndFreerun) {
                    timer[1]->paused = false;
                    timer[1]->mode.syncEnabled = false;
                }
            }
        }
        // Handle Timer1 - Reset on VBlank
    }
}

void System::softReset() {
    //    cdrom->reset();
    //    controller->reset();
    dma->reset();
    expansion2->reset();
    gpu->reset();
    interrupt->reset();
    mdec->reset();
    memoryControl->reset();
    ramControl->reset();
    cacheControl->reset();
    //    spu->reset();
    serial->reset();
    for (int t : {0, 1, 2}) {
        //        timer[t]->reset();
    }

    //    cpu->reset();
    cpu->setPC(0xBFC00000);
    cpu->inBranchDelay = false;
    state = State::run;
}

bool System::isSystemReady() { return biosLoaded; }

bool System::loadExeFile(const std::vector<uint8_t>& _exe) {
    if (_exe.empty()) return false;
    assert(_exe.size() >= 0x800);

    PsxExe exe;
    memcpy(&exe, _exe.data(), sizeof(exe));

    if (exe.t_size > _exe.size() - 0x800) {
        fmt::print("Invalid exe t_size: 0x{:08x}\n", exe.t_size);
        exe.t_size = _exe.size() - 0x800;
    }

    for (uint32_t i = 0; i < exe.t_size; i++) {
        writeMemory8(exe.t_addr + i, _exe[0x800 + i]);
    }

    cpu->setPC(exe.pc0);
    cpu->setReg(28, exe.gp0);

    if (exe.s_addr != 0) {
        cpu->setReg(29, exe.s_addr + exe.s_size);
        cpu->setReg(30, exe.s_addr + exe.s_size);
    }

    cpu->inBranchDelay = false;

    return true;
}

bool System::loadBios(const std::string& path) {
    const char* licenseString = "Sony Computer Entertainment Inc";

    auto _bios = getFileContents(path);
    if (_bios.empty()) {
        fmt::print("[SYS] Cannot open BIOS {}", path);
        return false;
    }
    assert(_bios.size() <= 512 * 1024);

    if (memcmp(_bios.data() + 0x108, licenseString, strlen(licenseString)) != 0) {
        fmt::print("[WARNING]: Loaded bios ({}) have invalid header, are you using correct file?\n", getFilenameExt(path));
    }

    std::copy(_bios.begin(), _bios.end(), bios.begin());
    this->biosPath = path;
    state = State::run;
    biosLoaded = true;

    auto patch = [&](uint32_t address, uint32_t opcode) {
        address &= bios.size() - 1;
        for (int i = 0; i < 4; i++) {
            bios[address + i] = (opcode >> (i * 8)) & 0xff;
        }
    };

    if (config.debug.log.system) {
        fmt::print("[INFO] Patching BIOS for system log\n");
        patch(0x6F0C, 0x24010001);
        patch(0x6F14, 0xAF81A9C0);
    }

    return true;
}

bool System::loadExpansion(const std::vector<uint8_t>& _exp) {
    const char* licenseString = "Licensed by Sony Computer Entertainment Inc";

    if (_exp.empty()) {
        return false;
    }

    assert(_exp.size() <= EXPANSION_SIZE);

    if (memcmp(_exp.data() + 4, licenseString, strlen(licenseString)) != 0) {
        fmt::print("[WARN]: Loaded expansion have invalid header, are you using correct file?\n");
    }

    std::copy(_exp.begin(), _exp.end(), expansion.begin());
    return true;
}

// Pretty print CPU registers at each instruction
void System::print_reg(){
    for (uint32_t i = 0; i < cpu->REGISTER_COUNT / 4; i++){
        for (int j = 0; j < cpu->REGISTER_COUNT / 8; j++){
            printf("    REG %d: %-10X", i * 4 + j, cpu->reg[i * 4 + j]);
        }
        printf("\n");
    }
}

void System::dumpRam() {
    std::vector<uint8_t> ram(this->ram.begin(), this->ram.end());
    putFileContents("ram.bin", ram);
}
