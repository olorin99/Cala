#include "Cala/ui/ProfileWindow.h"
#include <imgui.h>
#include <Ende/profile/ProfileManager.h>

cala::ui::ProfileWindow::ProfileWindow(Engine* engine, Renderer *renderer)
    : _engine(engine),
    _renderer(renderer)
{}

void cala::ui::ProfileWindow::render() {
    ImGui::Begin("Profiling");

    ImGui::Text("FPS: %f", _engine->driver().fps());

    if (ImGui::BeginTable("Timings", 2)) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);

        ImGui::Text("Milliseconds: %f", _engine->driver().milliseconds());

        ImGui::TableNextColumn();

        std::rotate(_globalTime.begin(), _globalTime.begin() + 1, _globalTime.end());
        _globalTime.back() = _engine->driver().milliseconds();
        ImGui::PlotLines("Milliseconds", &_globalTime[0], _globalTime.size());

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
        ImGui::EndTable();

        ImGui::Text("Total GPU: %f", totalGPUTime / 1e6);
        ImGui::Text("CPU/GPU: %f", _engine->driver().milliseconds() / (totalGPUTime / 1e6));
    }

    ImGui::End();
}