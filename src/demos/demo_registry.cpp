#include "demo_renderer.hpp"
#include <iostream>
#include <iomanip>

namespace ascii {

DemoRegistry& DemoRegistry::instance() {
    static DemoRegistry instance;
    return instance;
}

void DemoRegistry::register_demo(const std::string& name, DemoFactory factory) {
    demos_[name] = std::move(factory);
}

std::unique_ptr<DemoRenderer> DemoRegistry::create(const std::string& name) {
    auto it = demos_.find(name);
    if (it != demos_.end()) {
        return it->second();
    }
    return nullptr;
}

void DemoRegistry::list_demos() const {
    std::cout << "Available procedural demos:\n" << std::endl;
    for (const auto& [name, factory] : demos_) {
        auto demo = factory();
        std::cout << "  " << std::left << std::setw(12) << name
                  << " - " << demo->description() << std::endl;
    }
    std::cout << "\nRun a demo with: ascii-art demo <name>" << std::endl;
}

} // namespace ascii
