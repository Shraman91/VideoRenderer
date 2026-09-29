#include "demo_renderer.hpp"
#include "charset.hpp"
#include <cmath>
#include <vector>
#include <sstream>
#include <algorithm>

namespace ascii {

constexpr float PI = 3.14159265358979323846f;

class DonutDemo : public DemoRenderer {
public:
    std::string name() const override { return "donut"; }
    std::string description() const override {
        return "Classic spinning donut (parametric torus)";
    }

    std::string render_frame(int frame_index, int width, int height, const std::string& ramp) override {
        const float A = frame_index * 0.07f;  // Rotation around X axis
        const float B = frame_index * 0.03f;  // Rotation around Z axis

        // Pre-compute trig values
        const float cosA = std::cos(A), sinA = std::sin(A);
        const float cosB = std::cos(B), sinB = std::sin(B);

        // Initialize buffers
        std::vector<std::vector<float>> zbuffer(height, std::vector<float>(width, 0.0f));
        std::vector<std::string> canvas(height, std::string(width, ' '));

        // Donut parameters
        const float R1 = 1.0f;    // Minor radius (tube)
        const float R2 = 2.0f;    // Major radius (donut center to tube center)
        const float K2 = 5.0f;    // Distance from viewer
        const float K1 = std::min(width, height) * K2 * 3.0f / (8.0f * (R1 + R2));

        // Render donut by sampling its surface
        const int theta_steps = 90;  // Steps around tube
        const int phi_steps = 180;   // Steps around donut

        for (int i = 0; i < theta_steps; ++i) {
            float theta = 2.0f * PI * i / theta_steps;
            float costheta = std::cos(theta);
            float sintheta = std::sin(theta);

            for (int j = 0; j < phi_steps; ++j) {
                float phi = 2.0f * PI * j / phi_steps;
                float cosphi = std::cos(phi);
                float sinphi = std::sin(phi);

                // 3D coordinates on the torus surface
                float circlex = R2 + R1 * costheta;
                float circley = R1 * sintheta;

                float x = circlex * (cosB * cosphi + sinA * sinB * sinphi) - circley * cosA * sinB;
                float y = circlex * (sinB * cosphi - sinA * cosB * sinphi) + circley * cosA * cosB;
                float z = K2 + cosA * circlex * sinphi + circley * sinA;
                float ooz = 1.0f / z;  // One over z (for projection)

                // Project to 2D screen coordinates
                int xp = static_cast<int>(width / 2.0f + K1 * ooz * x);
                int yp = static_cast<int>(height / 2.0f - K1 * ooz * y);

                // Calculate surface normal for lighting
                float nx = costheta * (cosB * cosphi + sinA * sinB * sinphi) - cosA * sintheta * sinB;
                float ny = costheta * (sinB * cosphi - sinA * cosB * sinphi) + cosA * sintheta * cosB;
                float nz = cosA * costheta * sinphi + sintheta * sinA;

                // Lighting: dot product with light direction (0, 1, -1) normalized
                float light_x = 0.0f, light_y = 0.577f, light_z = -0.577f;
                float luminance = nx * light_x + ny * light_y + nz * light_z;

                // Only draw if visible (luminance > 0 and in bounds)
                if (luminance > 0 && xp >= 0 && xp < width && yp >= 0 && yp < height) {
                    if (ooz > zbuffer[yp][xp]) {
                        zbuffer[yp][xp] = ooz;
                        int lum_idx = static_cast<int>(luminance * (ramp.size() - 1));
                        lum_idx = std::clamp(lum_idx, 0, static_cast<int>(ramp.size()) - 1);
                        canvas[yp][xp] = ramp[lum_idx];
                    }
                }
            }
        }

        std::ostringstream oss;
        for (const auto& line : canvas) {
            oss << line << '\n';
        }
        return oss.str();
    }
};

REGISTER_DEMO(DonutDemo)

} // namespace ascii
