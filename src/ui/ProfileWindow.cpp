#include "Cala/ui/ProfileWindow.h"
#include <imgui.h>
#include <Ende/profile/ProfileManager.h>

cala::ui::ProfileWindow::ProfileWindow(Engine* engine, Renderer *renderer)
    : _engine(engine),
    _renderer(renderer),
    _cpuAvg(0),
    _gpuAvg(0)
{}

void cala::ui::ProfileWindow::render() {
    ImGui::Begin("Profiling");

    ImGui::Text("FPS: %f", _engine->device().fps());

    if (ImGui::BeginTable("Timings", 2)) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);

        ImGui::Text("Milliseconds: %f", _engine->device().milliseconds());

        ImGui::TableNextColumn();

        std::rotate(_globalTime.begin(), _globalTime.begin() + 1, _globalTime.end());
        _globalTime.back() = std::make_pair(_engine->device().milliseconds(), 0);
        ImGui::PlotLines("Milliseconds", &_globalTime[0].first, _globalTime.size());

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);

        ImGui::Text("CPU Times:");
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);

#ifdef ENDE_PROFILE
        {
            PROFILE_NAMED("show_profile");
            u32 currentProfileFrame = ende::profile::ProfileManager::getCurrentFrame();
            auto f = ende::profile::ProfileManager::getFrameData(currentProfileFrame);
            tsl::robin_map<const char*, std::pair<f64, u32>> data;
            for (auto& profileData : f) {
                f64 diff = (profileData.end.nanoseconds() - profileData.start.nanoseconds()) / 1e6;
                auto it = data.find(profileData.label);
                if (it == data.end())
                    data.emplace(std::make_pair(profileData.label, std::make_pair(diff, 1)));
                else {
                    it.value().first += diff;
                    it.value().second++;
                }
            }

            ende::Vector<std::pair<const char*, std::pair<f64, u32>>> dataVec;

            for (auto& func : data) {
                dataVec.push(std::make_pair(func.first, std::make_pair(func.second.first, func.second.second)));
            }
            std::sort(dataVec.begin(), dataVec.end(), [](std::pair<const char*, std::pair<f64, u32>> lhs, std::pair<const char*, std::pair<f64, u32>> rhs) -> bool {
                return lhs.first > rhs.first;
            });
            for (auto& func : dataVec) {
                ImGui::Text("\t%s ms", func.first);
                auto it = _times.find(func.first);
                if (it == _times.end()) {
                    _times.insert(std::make_pair(func.first, std::array<f32, 60>{}));
                    it = _times.find(func.first);
                }
                std::rotate(it.value().begin(), it.value().begin() + 1, it.value().end());
                it.value().back() = func.second.first;
                ImGui::TableNextColumn();
                ImGui::PlotLines(std::to_string(func.second.first).c_str(), &it.value()[0], it.value().size());
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
            }
        }
#endif

        ImGui::Text("GPU Times:");
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        auto passTimers = _renderer->timers();
        u64 totalGPUTime = 0;
        for (auto& timer : passTimers) {
            u64 time = timer.second.result();
            totalGPUTime += time;
            ImGui::Text("\t%s ms", timer.first);
            auto it = _times.find(timer.first);
            if (it == _times.end()) {
                _times.insert(std::make_pair(timer.first, std::array<f32, 60>{}));
                it = _times.find(timer.first);
            }
            std::rotate(it.value().begin(), it.value().begin() + 1, it.value().end());
            it.value().back() = time / 1e6;
            ImGui::TableNextColumn();
            ImGui::PlotLines(std::to_string(time / 1e6).c_str(), &it.value()[0], it.value().size());
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
        }
        _globalTime.back().second = totalGPUTime / 1e6;
        ImGui::EndTable();

        f32 cpuSum = 0;
        f32 gpuSum = 0;

        for (auto& frame : _globalTime) {
            cpuSum += frame.first;
            gpuSum += frame.second;
        }
        _cpuAvg = cpuSum / 60;
        _gpuAvg = gpuSum / 60;

        cpuSum = 0;
        gpuSum = 0;

        f32 cpuMin = 100000;
        f32 cpuMax = 0;
        f32 gpuMin = 100000;
        f32 gpuMax = 0;

        for (auto& frame : _globalTime) {
            f32 cpuDiff = std::abs(frame.first - _cpuAvg);
            f32 gpuDiff = std::abs(frame.second - _gpuAvg);
            cpuSum += cpuDiff * cpuDiff;
            gpuSum += gpuDiff * gpuDiff;
            cpuMin = std::min(cpuMin, cpuDiff);
            gpuMin = std::min(gpuMin, gpuDiff);
            cpuMax = std::max(cpuMax, cpuDiff);
            gpuMax = std::max(gpuMax, gpuDiff);
        }
        f32 cpuStdDev = std::sqrt(cpuSum / 59);
        f32 gpuStdDev = std::sqrt(gpuSum / 59);


        ImGui::Text("Total GPU: %f", totalGPUTime / 1e6);
        ImGui::Text("CPU/GPU: %f", _engine->device().milliseconds() / (totalGPUTime / 1e6));
        ImGui::Text("CPU Avg: %f, StdDev: %f, MaxDev: %f, MinDev: %f", _cpuAvg, cpuStdDev, cpuMax, cpuMin);
        ImGui::Text("GPU Avg: %f, StdDev: %f, MaxDev: %f, MinDev: %f", _gpuAvg, gpuStdDev, gpuMax, gpuMin);
    }

    ImGui::End();
}