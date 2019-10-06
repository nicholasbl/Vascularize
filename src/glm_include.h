#ifndef GLM_INTERFACE_H
#define GLM_INTERFACE_H

#define GLM_FORCE_SIZE_FUNC
#define GLM_FORCE_SIZE_T_LENGTH
#define GLM_SIMD_ENABLE_XYZW_UNION
#define GLM_ENABLE_EXPERIMENTAL
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <glm/glm.hpp>
#include <glm/gtc/packing.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/norm.hpp>

#include <fmt/core.h>
#include <fmt/format.h>

#include <array>

// Custom formatting for glm types
namespace fmt {
template <>
struct formatter<glm::vec3> {
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx) {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(glm::vec3 const& p, FormatContext& ctx) {
        return format_to(ctx.out(), "({}, {}, {})", p.x, p.y, p.z);
    }
};

template <>
struct formatter<glm::i64vec3> {
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx) {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(glm::i64vec3 const& p, FormatContext& ctx) {
        return format_to(ctx.out(), "({}, {}, {})", p.x, p.y, p.z);
    }
};

} // namespace fmt

/*!
 * \brief Select elements from one vector or another.
 *
 * Selection vector should be (convertable to) boolean; 0 selects an element
 * from the first vector, 1 from the second.
 *
 */
template <class Selector>
glm::vec3 select(Selector const& s, glm::vec3 const& a, glm::vec3 const& b) {
    glm::vec3 ret;
    for (int i = 0; i < 3; ++i) {
        // ret[i] = s[i] ? b[i] : a[i];
        ret[i] = (1 - s[i]) * a[i] + s[i] * b[i]; // no branch, seems faster
    }
    return ret;
}

/*!
 * \brief Build a vector from the array in order X Y Z.
 */
inline glm::dvec3 vector_from_array(std::array<double, 3> const& arr) {
    return glm::dvec3(arr[0], arr[1], arr[2]);
}


/*!
 * \brief Build a normalized quaternion, in the order of X Y Z W, from the given
 * array.
 */
inline glm::dquat quat_from_array(std::array<double, 4> const& arr) {
    glm::dquat quat(arr[3], arr[0], arr[1], arr[2]);
    return glm::normalize(quat);
}

/*!
 * \brief Convert a quat to a vector. Useful for shader variables.
 */
inline glm::vec4 quat_to_vector(glm::quat const& q) {
    return { q.x, q.y, q.z, q.w };
}

/*!
 * \brief Rotate a given vector by the quaternion to produce a new vector.
 *
 * Check if this can be removed with a glm function
 */
inline glm::dvec3 rotated_vector(glm::dquat const& q, glm::dvec3 const& v) {
    auto p = q * glm::dquat(0, v) * glm::conjugate(q);
    return glm::dvec3(p.x, p.y, p.z);
}


/*!
 * \brief Take a homogeneous coordinate and convert it to 3D.
 *
 * Just divides the vector by the w coordinate. If w is zero, it's your fault.
 */
inline glm::vec3 de_homogeneous(glm::vec4 const& v) {
    // if this explodes its your fault
    return glm::vec3(v) / v.w;
}

/*!
 * \brief Take a homogeneous coordinate and convert it to 3D.
 *
 * Just divides the vector by the w coordinate. If w is zero, it's your fault.
 */
inline glm::dvec3 de_homogeneous(glm::dvec4 const& v) {
    // if this explodes its your fault
    return glm::dvec3(v) / v.w;
}

// remember that if you need the squared length of a vector or the
// squared distance, use the length2 and distance2 functions

#endif // GLM_INTERFACE_H
