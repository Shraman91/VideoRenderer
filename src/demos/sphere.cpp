#include "demo_renderer.hpp"
#include "charset.hpp"
#include <cmath>
#include <vector>
#include <sstream>
#include <algorithm>

namespace ascii {

constexpr float PI = 3.14159265358979323846f;

class SphereDemo : public DemoRenderer {
public:
    std::string name() const override { return "sphere"; }
    std::string description() const override {
        return "Rotating shaded 3D sphere with directional lighting";
    }

    std::string render_frame(int frame_index, int width, int height, const std::string& ramp) override {
        const float t = frame_index * 0.05f;

        // Light direction rotating in 3D
        float lx = std::cos(t);
        float ly = std::sin(t * 0.7f);
        float lz = -std::sin(t);
        float l_len = std::sqrt(lx * lx + ly * ly + lz * lz);
        lx /= l_len; ly /= l_len; lz /= l_len;

        std::ostringstream oss;
        const float radius = 0.85f;
        const float aspect = static_cast<float>(width) / height * 0.5f; // Char cell aspect correction

        for (int y = 0; y < height; ++y) {
            float ny = (static_cast<float>(y) / height) * 2.0f - 1.0f;

            for (int x = 0; x < width; ++x) {
                float nx = ((static_cast<float>(x) / width) * 2.0f - 1.0f) * aspect;

                float dist_sq = nx * nx + ny * ny;

                if (dist_sq <= radius * radius) {
                    // Sphere surface point Z
                    float nz = -std::sqrt(radius * radius - dist_sq);

                    // Normal vector (normalized since it's a sphere at origin)
                    float norm_x = nx / radius;
                    float norm_y = ny / radius;
                    float norm_z = nz / radius;

                    // Diffuse lighting (Lambertian)
                    float diffuse = std::max(0.0f, -(norm_x * lx + norm_y * ly + norm_z * lz));

                    // Specular highlight (Blinn-Phong)
                    float hx = lx, hy = ly, hz = lz - 1.0f;
                    float h_len = std::sqrt(hx * hx + hy * hy + hz * hz);
                    if (h_len > 0) { hx /= h_len; hy /= h_len; hz /= h_len; }
                    float spec = std::pow(std::max(0.0f, -(norm_x * hx + norm_y * hy + norm_z * hz)), 16.0f);

                    float brightness = std::clamp(0.1f + 0.7f * diffuse + 0.3f * spec, 0.0f, 1.0f);
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

REGISTER_DEMO(SphereDemo)

} // namespace ascii
