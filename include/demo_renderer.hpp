#pragma once

#include <string>
#include <functional>
#include <memory>
#include <map>

namespace ascii {

// Interface for procedural demos
class DemoRenderer {
public:
    virtual ~DemoRenderer() = default;

    // Render a single frame
    // frame_index: current frame number
    // width, height: output dimensions in characters
    // ramp: character ramp to use
    virtual std::string render_frame(
        int frame_index,
        int width,
        int height,
        const std::string& ramp
    ) = 0;

    // Get demo name
    virtual std::string name() const = 0;

    // Get demo description
    virtual std::string description() const = 0;
};

// Demo registry
class DemoRegistry {
public:
    using DemoFactory = std::function<std::unique_ptr<DemoRenderer>()>;

    static DemoRegistry& instance();

    void register_demo(const std::string& name, DemoFactory factory);
    std::unique_ptr<DemoRenderer> create(const std::string& name);
    void list_demos() const;

private:
    DemoRegistry() = default;
    std::map<std::string, DemoFactory> demos_;
};

// Helper macro for registering demos
#define REGISTER_DEMO(ClassName) \
    static bool ClassName##_registered = []() { \
        DemoRegistry::instance().register_demo( \
            ClassName().name(), \
            []() -> std::unique_ptr<DemoRenderer> { return std::make_unique<ClassName>(); } \
        ); \
        return true; \
    }();

} // namespace ascii
