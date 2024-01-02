#include "Cala/ui/ProfileWindow.h"
#include <imgui.h>
#include <Ende/profile/ProfileManager.h>
#include <implot/implot.h>

cala::ui::ProfileWindow::ProfileWindow(ImGuiContext* context, Engine* engine, Renderer *renderer)
    : Window(context),
    _engine(engine),
    _renderer(renderer),
    _cpuAvg(0),
    _gpuAvg(0),
    _frameOffset(0)
{}

//TODO: split with simple plot and complex plot where complex plot can plot multiple datasets e.g. all cpu timings
void plotGraph(const char* label, std::span<f32> times, u32 offset, f32 height = 60, f32 width = -1) {
    auto [min, max] = std::minmax_element(times.begin(), times.end());
    if (ImPlot::BeginPlot(label, ImVec2(width, height))) {
        ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoTickLabels, ImPlotAxisFlags_AutoFit);
        ImPlot::SetupAxisLimits(ImAxis_Y1, 0, *max, ImPlotCond_Always);
        ImPlot::PlotLine("", times.data(), times.size(), 1, 0, 0, offset);
        ImPlot::EndPlot();
    }
};

void simplePlotGraph(const char* label, std::span<f32> times, u32 offset, f32 height = 60, f32 width = -1) {
    auto [min, max] = std::minmax_element(times.begin(), times.end());
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 0, 0, 255));
    ImGui::PlotLines("", times.data(), times.size(), offset, label, 0, *max, ImVec2{ width, height });
    ImGui::PopStyleColor();
};

void cala::ui::ProfileWindow::render() {
    // update times
#ifdef ENDE_PROFILE
    _cpuTimes[_frameOffset] = _engine->device().milliseconds();

    u32 currentProfilerFrame = ende::profile::ProfileManager::getCurrentFrame();
    auto frameData = ende::profile::ProfileManager::getFrameData(currentProfilerFrame);
    tsl::robin_map<const char*, std::pair<f64, u32>> data;


    for (auto& profileData : frameData) {
        auto duration = profileData.end - profileData.start;
        f64 diff = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
//        f64 diff = (profileData.end.nanoseconds() - profileData.start.nanoseconds()) / 1e6;
        auto it = data.find(profileData.label);
        if (it == data.end())
            data.emplace(std::make_pair(profileData.label, std::make_pair(diff, 1)));
        else {
            it.value().first += diff;
            it.value().second++;
        }
    }

    std::vector<std::pair<const char*, std::pair<f32, u32>>> dataVec;

    for (auto& func : data) {
        dataVec.push_back(std::make_pair(func.first, std::make_pair(func.second.first, func.second.second)));
    }
    std::sort(dataVec.begin(), dataVec.end(), [](std::pair<const char*, std::pair<f32, u32>> lhs, std::pair<const char*, std::pair<f32, u32>> rhs) -> bool {
        return lhs.first > rhs.first;
    });

    for (auto& func : dataVec) {
        auto it = _times.find(func.first);
        if (it == _times.end()) {
            _times.insert(std::make_pair(func.first, std::array<f32, MAX_FRAME_COUNT>{}));
            it = _times.find(func.first);
        }
        auto& dataArray = it.value();
        dataArray[_frameOffset] = func.second.first;
    }
#endif
    auto passTimers = _renderer->timers();
    u64 totalGPUTime = 0;
    for (auto& timer : passTimers) {
        u64 time = timer.second.result();
        totalGPUTime += time;
        auto it = _times.find(timer.first);
        if (it == _times.end()) {
            _times.insert(std::make_pair(timer.first, std::array<f32, MAX_FRAME_COUNT>{}));
            it = _times.find(timer.first);
        }
        it.value()[_frameOffset] = time / 1e6;
    }
    _gpuTimes[_frameOffset] = totalGPUTime / 1e6;

    // cpu average
    f32 cpuTotal = 0;
    for (auto& frameTime : _cpuTimes) {
        cpuTotal += frameTime;
    }
    _cpuAvg = cpuTotal / _cpuTimes.size();
    // gpu average
    f32 gpuTotal = 0;
    for (auto& frameTime : _gpuTimes) {
        gpuTotal += frameTime;
    }
    _gpuAvg = gpuTotal / _gpuTimes.size();

    // deviations
    cpuTotal = 0;
    gpuTotal = 0;
    f32 cpuMinDev = 1000000;
    f32 cpuMaxDev = 0;
    f32 gpuMinDev = 1000000;
    f32 gpuMaxDev = 0;
    for (auto& frameTime : _cpuTimes) {
        f32 diff = std::abs(frameTime - _cpuAvg);
        cpuTotal += diff * diff;
        cpuMinDev = std::min(cpuMinDev, diff);
        cpuMaxDev = std::max(cpuMaxDev, diff);
    }
    for (auto& frameTime : _gpuTimes) {
        f32 diff = std::abs(frameTime - _gpuAvg);
        gpuTotal += diff * diff;
        gpuMinDev = std::min(gpuMinDev, diff);
        gpuMaxDev = std::max(gpuMaxDev, diff);
    }
    f32 cpuStdDev = std::sqrt(cpuTotal / (MAX_FRAME_COUNT - 1));
    f32 gpuStdDev = std::sqrt(gpuTotal / (MAX_FRAME_COUNT - 1));


    // display times
    if (ImGui::Begin("Profiling")) {
        ImGui::Text("FPS: %f", _engine->device().fps());

        if (ImGui::BeginTable("Timings", 2)) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);

            ImGui::Text("Milliseconds: %f", _engine->device().milliseconds());

            ImGui::TableNextColumn();

            plotGraph("Milliseconds", _cpuTimes, _frameOffset, 240);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);

            ImGui::Text("CPU Times:");
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);

#ifdef ENDE_PROFILE
            {
                for (auto& func : dataVec) {
                    ImGui::Text("\t%s ms", func.first);
                    auto it = _times.find(func.first);
                    if (it == _times.end())
                        continue;

                    auto& dataArray = it.value();
                    ImGui::TableNextColumn();
                    std::string label = std::format("{} ms", func.second.first);
                    simplePlotGraph(label.c_str(), dataArray, _frameOffset, 30.f);
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                }
            }
#endif

            ImGui::Text("GPU Times:");
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            for (auto& timer : passTimers) {
                u64 time = timer.second.result();
                ImGui::Text("\t%s ms", timer.first);
                auto it = _times.find(timer.first);
                if (it == _times.end())
                    continue;

                auto& dataArray = it.value();
                ImGui::TableNextColumn();
                std::string label = std::format("{} ms", time / 1e6);
                simplePlotGraph(label.c_str(), dataArray, _frameOffset, 30.f);
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
            }
            ImGui::EndTable();


            ImGui::Text("Total GPU: %f", totalGPUTime / 1e6);
            ImGui::Text("CPU/GPU: %f", _engine->device().milliseconds() / (totalGPUTime / 1e6));
            ImGui::Text("CPU Avg: %f, StdDev: %f, MaxDev: %f, MinDev: %f", _cpuAvg, cpuStdDev, cpuMaxDev, cpuMinDev);
            ImGui::Text("GPU Avg: %f, StdDev: %f, MaxDev: %f, MinDev: %f", _gpuAvg, gpuStdDev, gpuMaxDev, gpuMinDev);
        }

    }
    ImGui::End();


    _frameOffset = (_frameOffset + 1) % MAX_FRAME_COUNT;
}