#include "demo_renderer.hpp"
#include "charset.hpp"
#include <cmath>
#include <vector>
#include <sstream>
#include <algorithm>

namespace ascii {

constexpr float PI = 3.14159265358979323846f;

class PlanetDemo : public DemoRenderer {
public:
    std::string name() const override { return "planet"; }
    std::string description() const override {
        return "Textured rotating planet with latitude/longitude grid";
    }

    std::string render_frame(int frame_index, int width, int height, const std::string& ramp) override {
        const float t = frame_index * 0.03f;

        // Fixed light direction
        float lx = 0.7f, ly = 0.5f, lz = -0.5f;
        float l_len = std::sqrt(lx * lx + ly * ly + lz * lz);
        lx /= l_len; ly /= l_len; lz /= l_len;

        std::ostringstream oss;
        const float radius = 0.85f;
        const float aspect = static_cast<float>(width) / height * 0.5f;

        for (int y = 0; y < height; ++y) {
            float ny = (static_cast<float>(y) / height) * 2.0f - 1.0f;

            for (int x = 0; x < width; ++x) {
                float nx = ((static_cast<float>(x) / width) * 2.0f - 1.0f) * aspect;

                float dist_sq = nx * nx + ny * ny;

                if (dist_sq <= radius * radius) {
                    float nz = -std::sqrt(radius * radius - dist_sq);

                    // Sphere surface normal
                    float norm_x = nx / radius;
                    float norm_y = ny / radius;
                    float norm_z = nz / radius;

                    // Convert to spherical coordinates for texturing
                    float theta = std::atan2(norm_y, norm_x) + t; // Rotation around Y axis
                    float phi = std::acos(-norm_z);

                    // Normalize to [0, 1]
                    float u = (theta + PI) / (2.0f * PI);
                    float v = phi / PI;

                    // Procedural texture: latitude/longitude grid + continents
                    float grid_lat = std::abs(std::fmod(v * 18.0f, 1.0f) - 0.5f); // 18 lat lines
                    float grid_lon = std::abs(std::fmod(u * 36.0f, 1.0f) - 0.5f); // 36 lon lines
                    bool is_grid = (grid_lat < 0.05f || grid_lon < 0.05f);

                    // Procedural "continents" using sine waves
                    float land = std::sin(u * 7.0f) * std::cos(v * 5.0f) +
                                 std::sin(u * 13.0f + v * 11.0f) * 0.3f;
                    bool is_land = land > 0.2f;

                    // Base albedo
                    float albedo = is_land ? 0.6f : 0.3f;
                    if (is_grid) albedo = 0.9f; // Grid lines are bright

                    // Diffuse lighting
                    float diffuse = std::max(0.0f, -(norm_x * lx + norm_y * ly + norm_z * lz));
                    float brightness = std::clamp(0.05f + albedo * diffuse, 0.0f, 1.0f);

                    int idx = static_cast<int>(brightness * (ramp.size() - 1));
                    oss << ramp[idx];
                } else {
                    oss << ' ';
                }
            }
            oss << '\n';
        }

        return oss.str();
    }
};

REGISTER_DEMO(PlanetDemo)

} // namespace ascii
