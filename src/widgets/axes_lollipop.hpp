#pragma once
#include <gtkmm.h>
#include <glm/gtx/quaternion.hpp>

namespace dune3d {
class AxesLollipop : public Gtk::DrawingArea {
public:
    AxesLollipop();
    void set_quat(const glm::quat &q);

private:
    glm::quat m_quat;
    Glib::RefPtr<Pango::Layout> m_layout;
    float m_size = 5;
    int m_hovered_face = -1;
    double m_last_x = 0;
    double m_last_y = 0;
    int m_width = 0;
    int m_height = 0;

    void render(const Cairo::RefPtr<Cairo::Context> &cr, int w, int h);
    void create_layout();
    void setup_controllers();
    int get_face_at_position(double x, double y) const;
};
} // namespace dune3d
