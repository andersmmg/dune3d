#include "axes_lollipop.hpp"
#include "util/color.hpp"
#include <glm/gtx/rotate_vector.hpp>
#include <iostream>

namespace dune3d {

static const std::array<std::string, 3> s_xyz = {"X", "Y", "Z"};

static const Color &get_color(unsigned int ax, float z)
{
    static const std::array<Color, 3> ax_colors_pos = {
            Color::new_from_int(255, 54, 83),
            Color::new_from_int(138, 219, 0),
            Color::new_from_int(44, 142, 254),
    };
    static const std::array<Color, 3> ax_colors_neg = {
            Color::new_from_int(155, 57, 7),
            Color::new_from_int(98, 137, 34),
            Color::new_from_int(51, 100, 155),
    };
    if (z >= -.001)
        return ax_colors_pos.at(ax);
    else
        return ax_colors_neg.at(ax);
}

AxesLollipop::AxesLollipop()
{
    create_layout();
    for (unsigned int ax = 0; ax < 3; ax++) {
        m_layout->set_text(s_xyz.at(ax));
        auto ext = m_layout->get_pixel_logical_extents();
        m_size = std::max(m_size, (float)ext.get_width());
        m_size = std::max(m_size, (float)ext.get_height());
    }
    set_content_height(100);
    set_content_width(100);
    set_draw_func(sigc::mem_fun(*this, &AxesLollipop::render));
    setup_controllers();
}

void AxesLollipop::create_layout()
{
    m_layout = create_pango_layout("");
    Pango::AttrList attrs;
    auto attr = Pango::Attribute::create_attr_weight(Pango::Weight::BOLD);
    attrs.insert(attr);
    m_layout->set_attributes(attrs);
}

void AxesLollipop::set_quat(const glm::quat &q)
{
    m_quat = q;
    queue_draw();
}

struct Face {
    std::array<int, 4> vertices;
    unsigned int axis;
    std::string label;
    bool positive;
    int face_id;
};

static bool is_face_visible(const glm::vec3 &v0, const glm::vec3 &v1, const glm::vec3 &v2)
{
    glm::vec3 edge1 = v1 - v0;
    glm::vec3 edge2 = v2 - v0;
    glm::vec3 normal = glm::cross(edge1, edge2);
    return normal.z < 0;
}

void AxesLollipop::setup_controllers()
{
    auto motion_controller = Gtk::EventControllerMotion::create();
    motion_controller->signal_motion().connect([this](double x, double y) {
        m_last_x = x;
        m_last_y = y;
        int old_hovered = m_hovered_face;
        m_hovered_face = get_face_at_position(x, y);
        if (old_hovered != m_hovered_face) {
            queue_draw();
        }
    });
    motion_controller->signal_leave().connect([this] {
        if (m_hovered_face != -1) {
            m_hovered_face = -1;
            queue_draw();
        }
    });
    add_controller(motion_controller);

    auto click_controller = Gtk::GestureClick::create();
    click_controller->set_button(1);
    click_controller->signal_pressed().connect([this](int n_press, double x, double y) {
        int face_id = get_face_at_position(x, y);
        if (face_id >= 0) {
            const char* face_names[] = {"+X", "-X", "+Y", "-Y", "+Z", "-Z"};
            std::cout << "Clicked face: " << face_names[face_id] << std::endl;
        }
    });
    add_controller(click_controller);
}

int AxesLollipop::get_face_at_position(double x, double y) const
{
    if (m_width == 0 || m_height == 0)
        return -1;

    const float sc = (std::min(m_width, m_height) / 2) - m_size;
    const double size = 0.9;
    const glm::vec3 vertices[8] = {
        {-size, -size, -size},
        { size, -size, -size},
        { size,  size, -size},
        {-size,  size, -size},
        {-size, -size,  size},
        { size, -size,  size},
        { size,  size,  size},
        {-size,  size,  size}
    };

    const Face faces[6] = {
        {{1, 5, 6, 2}, 0, "X", true, 0},    // +X
        {{4, 0, 3, 7}, 0, "-X", false, 1},  // -X
        {{3, 2, 6, 7}, 1, "Y", true, 2},    // +Y
        {{4, 5, 1, 0}, 1, "-Y", false, 3},  // -Y
        {{5, 4, 7, 6}, 2, "Z", true, 4},    // +Z
        {{0, 1, 2, 3}, 2, "-Z", false, 5}   // -Z
    };

    glm::quat corrected_view_quat = glm::angleAxis(glm::pi<float>(), glm::vec3(0.0f, 1.0f, 0.0f)) * glm::inverse(m_quat);

    glm::vec3 transformed[8];
    for (int i = 0; i < 8; i++) {
        transformed[i] = glm::rotate(corrected_view_quat, vertices[i]) * sc;
    }

    float mouse_x = x - m_width / 2.0f;
    float mouse_y = y - m_height / 2.0f;
    for (const auto &face : faces) {
        const auto &v0 = transformed[face.vertices[0]];
        const auto &v1 = transformed[face.vertices[1]];
        const auto &v2 = transformed[face.vertices[2]];

        if (!is_face_visible(v0, v1, v2))
            continue;
        const auto &v3 = transformed[face.vertices[3]];
        glm::vec2 p(mouse_x, mouse_y);
        glm::vec2 p0(v0.x, v0.y);
        glm::vec2 p1(v1.x, v1.y);
        glm::vec2 p2(v2.x, v2.y);
        glm::vec2 p3(v3.x, v3.y);

        auto sign = [](const glm::vec2 &p1, const glm::vec2 &p2, const glm::vec2 &p3) {
            return (p1.x - p3.x) * (p2.y - p3.y) - (p2.x - p3.x) * (p1.y - p3.y);
        };

        float d1 = sign(p, p0, p1);
        float d2 = sign(p, p1, p2);
        float d3 = sign(p, p2, p0);
        bool in_tri1 = ((d1 <= 0) && (d2 <= 0) && (d3 <= 0)) || ((d1 >= 0) && (d2 >= 0) && (d3 >= 0));

        d1 = sign(p, p0, p2);
        d2 = sign(p, p2, p3);
        d3 = sign(p, p3, p0);
        bool in_tri2 = ((d1 <= 0) && (d2 <= 0) && (d3 <= 0)) || ((d1 >= 0) && (d2 >= 0) && (d3 >= 0));

        if (in_tri1 || in_tri2) {
            return face.face_id;
        }
    }

    return -1;
}

void AxesLollipop::render(const Cairo::RefPtr<Cairo::Context> &cr, int w, int h)
{
    m_width = w;
    m_height = h;
    const float sc = (std::min(w, h) / 2) - m_size;
    cr->translate(w / 2, h / 2);
    cr->set_line_width(1.5);
    
    const double size = 0.9;
    const glm::vec3 vertices[8] = {
        {-size, -size, -size},
        { size, -size, -size},
        { size,  size, -size},
        {-size,  size, -size},
        {-size, -size,  size},
        { size, -size,  size},
        { size,  size,  size},
        {-size,  size,  size}
    };
    

    const Face faces[6] = {
        {{1, 5, 6, 2}, 0, "X", true, 0},    // Right face (+X)
        {{4, 0, 3, 7}, 0, "-X", false, 1},  // Left face (-X)
        {{3, 2, 6, 7}, 1, "Y", true, 2},    // Top face (+Y)
        {{4, 5, 1, 0}, 1, "-Y", false, 3},  // Bottom face (-Y)
        {{5, 4, 7, 6}, 2, "Z", true, 4},    // Front face (+Z)
        {{0, 1, 2, 3}, 2, "-Z", false, 5}   // Back face (-Z)
    };
    
    glm::quat corrected_view_quat = glm::angleAxis(glm::pi<float>(), glm::vec3(0.0f, 1.0f, 0.0f)) * glm::inverse(m_quat);
    
    // Transform all vertices
    glm::vec3 transformed[8];
    for (int i = 0; i < 8; i++) {
        transformed[i] = glm::rotate(corrected_view_quat, vertices[i]) * sc;
    }
    
    struct VisibleFace {
        const Face *face;
        float depth;
    };
    std::vector<VisibleFace> visible_faces;
    
    for (const auto &face : faces) {
        const auto &v0 = transformed[face.vertices[0]];
        const auto &v1 = transformed[face.vertices[1]];
        const auto &v2 = transformed[face.vertices[2]];
        
        if (is_face_visible(v0, v1, v2)) {
            float avg_z = 0;
            for (int idx : face.vertices) {
                avg_z += transformed[idx].z;
            }
            avg_z /= 4;
            visible_faces.push_back({&face, avg_z});
        }
    }
    
    std::sort(visible_faces.begin(), visible_faces.end(),
              [](const VisibleFace &a, const VisibleFace &b) { return a.depth < b.depth; });
    
    for (const auto &vf : visible_faces) {
        const Face &face = *vf.face;
        const auto &v0 = transformed[face.vertices[0]];
        const auto &v1 = transformed[face.vertices[1]];
        const auto &v2 = transformed[face.vertices[2]];
        const auto &v3 = transformed[face.vertices[3]];
        
        auto color = dune3d::get_color(face.axis, face.positive ? 1.0f : -1.0f);
        
        if (m_hovered_face == face.face_id) {
            color.r = std::min(1.0f, color.r * 1.4f);
            color.g = std::min(1.0f, color.g * 1.4f);
            color.b = std::min(1.0f, color.b * 1.4f);
        }
        
        cr->move_to(v0.x, v0.y);
        cr->line_to(v1.x, v1.y);
        cr->line_to(v2.x, v2.y);
        cr->line_to(v3.x, v3.y);
        cr->close_path();
        cr->set_source_rgba(color.r, color.g, color.b, 0.7);
        cr->fill_preserve();
        
        cr->set_source_rgb(0, 0, 0);
        cr->stroke();
        
        if (!face.label.empty()) {
            m_layout->set_text(face.label);
            auto ext = m_layout->get_pixel_logical_extents();
            
            float center_x = (v0.x + v1.x + v2.x + v3.x) / 4.0f;
            float center_y = (v0.y + v1.y + v2.y + v3.y) / 4.0f;
            
            cr->set_source_rgb(0, 0, 0);
            cr->move_to(center_x - ext.get_width() / 2, center_y - ext.get_height() / 2);
            m_layout->show_in_cairo_context(cr);
        }
    }
}

} // namespace dune3d
