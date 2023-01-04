#include "Cala/Material.h"


cala::Material::Material(cala::Engine* engine)
    : _engine(engine),
      _uniformBuffer(engine->driver(), 256, backend::BufferUsage::UNIFORM, backend::MemoryProperties::HOST_VISIBLE | backend::MemoryProperties::HOST_COHERENT)
{}

cala::MaterialInstance cala::Material::instance() {
    auto mat = MaterialInstance(*this, _uniformData.size());
    _uniformData.resize(_uniformData.size() + _setSize);
    return std::move(mat);
}

void cala::Material::setProgram(cala::Material::Variants variant, cala::ProgramHandle program) {
    _programs.resize((u32)variant + 1);
    _programs[(u32)variant] = program;
//    _programs.emplace(std::make_pair(variant, program));
}

cala::ProgramHandle cala::Material::getProgram(cala::Material::Variants variant) {
    u32 index = (u32)variant;
    if (index >= _programs.size())
        return _programs.front();
    return _programs[index];
//    auto it = _programs.find(variant); //TODO: error handling
//    if (it != _programs.end())
//        return it->second;
//    return _programs.begin()->second;
}