#ifndef BOUNDINGBOX_H
#define BOUNDINGBOX_H

#include "glm_include.h"

#include <glm/gtx/type_trait.hpp>

#include <array>
#include <iosfwd>
#include <limits>

/*!
 * \brief The BoundingBox class maintains an AA 3D box
 *
 */
class BoundingBox {
    enum class State : uint8_t { INVALID, INFINITE, FINITE };

    glm::vec3 m_lower = glm::vec3(0);
    glm::vec3 m_upper = glm::vec3(0);

    State m_state = State::INVALID;

public:
    ///
    /// \brief Make a bounding box of infinite size
    ///
    static BoundingBox make_infinite();

    BoundingBox() = default;
    BoundingBox(glm::vec3 const& a, glm::vec3 const& b) noexcept { set(a, b); }

    BoundingBox(BoundingBox const&) = default;
    BoundingBox& operator=(BoundingBox const&) = default;

    ~BoundingBox() = default;

    ///
    /// \brief Get the center of the bounding box
    ///
    glm::vec3 center() const;

    auto state() const { return m_state; }

    bool is_invalid() const;
    bool is_finite() const;
    bool is_infinite() const;
    bool is_empty() const;

    void set_invalid();
    void set_infinite();

    bool        contains(glm::vec3 const& p) const;
    bool        contains(BoundingBox const& b) const;
    void        intersection(BoundingBox const& b);
    BoundingBox intersected(BoundingBox const& b) const;
    bool        intersects(BoundingBox const& b) const;

    glm::vec3 const& maximum() const;
    glm::vec3 const& minimum() const;

    std::array<BoundingBox, 2> split(float around, size_t axis) const;

    ///
    /// \brief Reset the bounds of the bounding box. This runs min and max to
    /// clamp the bounds just in case you flip them.
    void set(glm::vec3 const& a, glm::vec3 const& b);

    ///
    /// \brief Get the dimensions of the bounding box.
    ///
    glm::vec3 size() const;


    void        box_union(glm::vec3 const& p);
    void        box_union(BoundingBox const& b);
    BoundingBox united(glm::vec3 const& p) const;
    BoundingBox united(BoundingBox const& b) const;
};


#endif // BOUNDINGBOX_H
