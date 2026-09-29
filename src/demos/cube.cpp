#include "demo_renderer.hpp"
#include "charset.hpp"
#include <cmath>
#include <vector>
#include <algorithm>
#include <sstream>

namespace ascii {

class CubeDemo : public DemoRenderer {
public:
    std::string name() const override { return "cube"; }
    std::string description() const override {
        return "Rotating 3D wireframe/filled cube with perspective";
    }

    std::string render_frame(int frame_index, int width, int height, const std::string& ramp) override {
        const float t = frame_index * 0.05f;

        // Rotation matrices
        const float cos_x = std::cos(t * 0.7f);
        const float sin_x = std::sin(t * 0.7f);
        const float cos_y = std::cos(t);
        const float sin_y = std::sin(t);

        // Define cube vertices in 3D
        const float cube_verts[][3] = {
            {-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1},
            {-1, -1,  1}, {1, -1,  1}, {1, 1,  1}, {-1, 1,  1}
        };

        // Transform and project vertices
        std::vector<std::pair<int, int>> projected;
        std::vector<float> depths;

        for (const auto& v : cube_verts) {
            // Rotate around X axis
            float y1 = v[1] * cos_x - v[2] * sin_x;
            float z1 = v[1] * sin_x + v[2] * cos_x;

            // Rotate around Y axis
            float x2 = v[0] * cos_y + z1 * sin_y;
            float z2 = -v[0] * sin_y + z1 * cos_y;

            // Perspective projection
            const float fov = 5.0f;
            const float z_offset = 5.0f;
            float depth = z2 + z_offset;
            float scale = fov / depth;

            int sx = static_cast<int>(x2 * scale * std::min(width, height) / 4.0f + width / 2.0f);
            int sy = static_cast<int>(y1 * scale * std::min(width, height) / 4.0f + height / 2.0f);

            projected.emplace_back(sx, sy);
            depths.push_back(depth);
        }

        // Define edges
        const int edges[][2] = {
            {0, 1}, {1, 2}, {2, 3}, {3, 0}, // back face
            {4, 5}, {5, 6}, {6, 7}, {7, 4}, // front face
            {0, 4}, {1, 5}, {2, 6}, {3, 7}  // connecting edges
        };

        // Draw on ASCII canvas
        std::vector<std::vector<float>> zbuffer(height, std::vector<float>(width, 1e9f));
        std::vector<std::string> canvas(height, std::string(width, ' '));

        // Draw edges
        for (const auto& edge : edges) {
            int i0 = edge[0], i1 = edge[1];
            auto [x0, y0] = projected[i0];
            auto [x1, y1] = projected[i1];
            float z0 = depths[i0];
            float z1 = depths[i1];

            draw_line(canvas, zbuffer, x0, y0, z0, x1, y1, z1, width, height, ramp);
        }

        // Draw vertices as highlights
        for (size_t i = 0; i < projected.size(); ++i) {
            auto [x, y] = projected[i];
            float z = depths[i];
            if (x >= 0 && x < width && y >= 0 && y < height && z < zbuffer[y][x]) {
                canvas[y][x] = ramp.back(); // brightest character
                zbuffer[y][x] = z;
            }
        }

        std::ostringstream oss;
        for (const auto& line : canvas) {
            oss << line << '\n';
        }
        return oss.str();
    }

private:
    void draw_line(std::vector<std::string>& canvas, std::vector<std::vector<float>>& zbuffer,
                   int x0, int y0, float z0, int x1, int y1, float z1,
                   int width, int height, const std::string& ramp) {
        int dx = std::abs(x1 - x0);
        int dy = std::abs(y1 - y0);
        int sx = (x0 < x1) ? 1 : -1;
        int sy = (y0 < y1) ? 1 : -1;
        int err = dx - dy;

        int steps = std::max(dx, dy);
        if (steps == 0) steps = 1;

        int x = x0, y = y0;
        for (int step = 0; step <= steps; ++step) { 
            float t = static_cast<float>(step) / steps;
            float z = z0 + t * (z1 - z0);

            if (x >= 0 && x < width && y >= 0 && y < height && z < zbuffer[y][x]) {
                // Depth-based brightness
                float brightness = std::clamp(1.0f - (z - 3.0f) / 4.0f, 0.0f, 1.0f);
                int idx = static_cast<int>(brightness * (ramp.size() - 1));
                canvas[y][x] = ramp[idx];
                zbuffer[y][x] = z;
            }

            if (x == x1 && y == y1) break;

            int e2 = 2 * err;
            if (e2 > -dy) { err -= dy; x += sx; }
            if (e2 < dx) { err += dx; y += sy; }
        }
    }
};

REGISTER_DEMO(CubeDemo)

} // namespace ascii
