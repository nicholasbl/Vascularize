#include "boundingbox.h"

#include <ostream>

template <class T>
struct IntervalHelper {
    T m_min;
    T m_max;

    IntervalHelper(T a, T b) : m_min(a), m_max(b) {}

    T distance(T p) {
        if (m_min > p) return m_min - p;
        if (m_max < p) return p - m_max;
        return 0.0f;
    }

    bool intersects(IntervalHelper const& other) {
        if (m_min > other.m_max) return false;
        if (m_max < other.m_min) return false;
        return true;
    }

    IntervalHelper intersection(IntervalHelper const& other) {
        // assuming they DO intersect! check beforehand!
        // its either wholly in or partially in

        // find the max min, and min max
        return IntervalHelper(std::max(m_min, other.m_min),
                              std::min(m_max, other.m_max));
    }
};

BoundingBox BoundingBox::make_infinite() {
    BoundingBox ret;
    ret.set_infinite();
    return ret;
}

glm::vec3 BoundingBox::center() const {
    if (!is_finite()) return glm::vec3(0);

    auto half = (m_upper - m_lower) / float(2.0);
    return m_lower + half;
}


bool BoundingBox::is_invalid() const { return m_state == State::INVALID; }


bool BoundingBox::is_finite() const { return m_state == State::FINITE; }


bool BoundingBox::is_infinite() const { return m_state == State::INFINITE; }


bool BoundingBox::is_empty() const {
    auto sz = size();

    for (size_t i = 0; i < 3; ++i) {
        if (sz[i] < std::numeric_limits<float>::epsilon()) return true;
    }

    return false;
}


void BoundingBox::set_invalid() { m_state = State::INVALID; }


void BoundingBox::set_infinite() { m_state = State::INFINITE; }


bool strict_less_than_eq(glm::vec3 const& a, glm::vec3 const& b) {
    auto r = b - a;

    // TODO: replace with glm::all( glm::lessThan(r, T(0)) );
    // after you have tested it

    for (size_t i = 0; i < 3; ++i) {
        // no, the below should not be <=
        if (r[i] < 0.0f) return false;
    }
    return true;
}


bool BoundingBox::contains(glm::vec3 const& p) const {
    switch (m_state) {
    case State::INVALID: return false;
    case State::INFINITE: return true;
    case State::FINITE:
        return strict_less_than_eq(m_lower, p) and
               strict_less_than_eq(p, m_upper);
    }

    __builtin_unreachable();
}


bool BoundingBox::contains(BoundingBox const& b) const {
    if (is_invalid()) return false;
    if (is_infinite()) {
        return !b.is_invalid();
    }

    // if we are here, we are finite
    if (b.is_infinite() or b.is_invalid()) return false;

    // and b is finite too. check.
    return strict_less_than_eq(m_lower, b.m_lower) and
           strict_less_than_eq(b.m_upper, m_upper);
}


void BoundingBox::intersection(BoundingBox const& b) {
    if (is_invalid() or b.is_invalid()) return;


    if (is_infinite()) {
        // if both infinite, nothing changes.
        if (b.is_infinite()) return;

        // b is finite, intersection is b
        set(b.minimum(), b.maximum());
        return;
    }

    // we are finite
    // if b is infinite, intersection is self, nothing changes.
    if (b.is_infinite()) return;

    // both finite. get the intersection.

    glm::vec3 new_min, new_max;

    for (size_t i = 0; i < 3; ++i) {
        IntervalHelper<float> our_interval(m_lower[i], m_upper[i]);
        IntervalHelper<float> their_interval(b.m_lower[i], b.m_upper[i]);

        if (not our_interval.intersects(their_interval)) {
            // its outside. the intersection doesnt exist.
            set_invalid();
            return;
        }

        auto isect = our_interval.intersection(their_interval);

        new_min[i] = isect.m_min;
        new_max[i] = isect.m_max;
    }

    set(new_min, new_max);
}


BoundingBox BoundingBox::intersected(BoundingBox const& b) const {
    if (is_invalid() or b.is_invalid()) return BoundingBox();


    if (is_infinite()) {
        // if both infinite, return copy of us, since we are infinite.
        if (b.is_infinite()) return *this;

        // b is finite, intersection is b
        return b;
    }

    // we are finite
    // if b is infinite, intersection is self
    if (b.is_infinite()) return *this;

    // both finite. get the intersection.

    glm::vec3 new_min, new_max;

    for (size_t i = 0; i < 3; ++i) {
        IntervalHelper<float> our_interval(m_lower[i], m_upper[i]);
        IntervalHelper<float> their_interval(b.m_lower[i], b.m_upper[i]);

        if (not our_interval.intersects(their_interval)) {
            // its outside. the intersection doesnt exist.
            return BoundingBox();
        }

        auto isect = our_interval.intersection(their_interval);

        new_min[i] = isect.m_min;
        new_max[i] = isect.m_max;
    }

    return { new_min, new_max };
}


bool BoundingBox::intersects(BoundingBox const& b) const {
    for (size_t i = 0; i < 3; ++i) {
        IntervalHelper<float> our_interval(m_lower[i], m_upper[i]);
        IntervalHelper<float> their_interval(b.m_lower[i], b.m_upper[i]);

        if (!our_interval.intersects(their_interval)) {
            return false;
        }
    }
    return true;
}


float BoundingBox::nearest(glm::vec3 const& p) const {
    switch (m_state) {
    case State::INVALID: return 0.0f;
    case State::INFINITE: return 0.0f;
    case State::FINITE: {
        float dist(0);
        for (size_t i = 0; i < 3; ++i) {
            IntervalHelper<float> our_interval(m_lower[i], m_upper[i]);
            dist += glm::pow(our_interval.distance(p[i]), 2);
        }
        return glm::sqrt(dist);
    }
    }

    __builtin_unreachable();
}


float BoundingBox::farthest(glm::vec3 const& p) const {
    switch (m_state) {
    case State::INVALID: return std::numeric_limits<float>::max();
    case State::INFINITE: return std::numeric_limits<float>::max();
    case State::FINITE: {
        float dist(0);
        for (size_t i = 0; i < 3; ++i) {
            auto center = float(0.5) * (m_lower[i] + m_upper[i]);
            if (p[i] < center) {
                dist += glm::pow(m_upper[i] - p[i], 2);
            } else {
                dist += glm::pow(p[i] - m_lower[i], 2);
            }
        }
        return glm::sqrt(dist);
    }
    }

    __builtin_unreachable();
}


glm::vec3 const& BoundingBox::maximum() const { return m_upper; }

glm::vec3 const& BoundingBox::minimum() const { return m_lower; }

std::array<BoundingBox, 2> BoundingBox::split(float around, size_t axis) const {
    auto lower_max  = maximum();
    lower_max[axis] = std::clamp(around, m_lower[axis], m_upper[axis]);

    auto upper_min  = minimum();
    upper_min[axis] = std::clamp(around, m_lower[axis], m_upper[axis]);

    // these should auto, clamp
    return { BoundingBox(m_lower, lower_max), BoundingBox(upper_min, m_upper) };
}

void BoundingBox::set(glm::vec3 const& a, glm::vec3 const& b) {
    m_state = State::FINITE;
    m_lower = glm::min(a, b);
    m_upper = glm::max(a, b);
}


glm::vec3 BoundingBox::size() const {
    if (!is_finite()) return glm::vec3(0);
    return m_upper - m_lower;
}

void BoundingBox::box_union(glm::vec3 const& p) {
    if (is_infinite()) return;

    if (is_invalid()) {
        set(p, p); // this doesnt seem right
        return;
    }

    // we are finite
    m_lower = glm::min(m_lower, p);
    m_upper = glm::max(m_upper, p);
}


void BoundingBox::box_union(BoundingBox const& b) {
    if (is_infinite()) return;

    // if we are nothing, then we are b.
    // - if b is invalid, then nothing changes
    // - if b is infinite, then the union is infinite
    // - if b is finite, then the union is b.
    if (is_invalid()) {
        *this = b;
        return;
    }

    // we are finite
    if (b.is_invalid()) return;
    if (b.is_infinite()) {
        set_infinite();
        return;
    }

    // we are finite, and b is finite
    m_lower = glm::min(m_lower, b.m_lower);
    m_upper = glm::max(m_upper, b.m_upper);
}


BoundingBox BoundingBox::united(glm::vec3 const& p) const {
    if (is_invalid()) return BoundingBox(p, p);
    if (is_infinite()) return *this;

    return BoundingBox(glm::min(m_lower, p), glm::max(m_upper, p));
}


BoundingBox BoundingBox::united(BoundingBox const& b) const {
    if (is_infinite()) return *this;

    // if we are nothing, then we return b.
    // - if b is invalid, then nothing changes
    // - if b is infinite, then the union is infinite
    // - if b is finite, then the union is b.
    if (is_invalid()) {
        return b;
    }

    // we are finite
    if (b.is_invalid()) return *this;
    if (b.is_infinite()) {
        BoundingBox r;
        r.set_infinite();
        return r;
    }

    // we are finite, and b is finite
    return BoundingBox(glm::min(m_lower, b.m_lower),
                       glm::max(m_upper, b.m_upper));
}
