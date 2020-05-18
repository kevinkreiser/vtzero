
#include <test.hpp>
#include <test_point.hpp>

#include <vtzero/builder.hpp>
#include <vtzero/builder_helper.hpp>
#include <vtzero/index.hpp>
#include <vtzero/output.hpp>

#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

template <typename T>
struct movable_not_copyable {
    constexpr static bool value = !std::is_copy_constructible<T>::value &&
                                  !std::is_copy_assignable<T>::value    &&
                                   std::is_nothrow_move_constructible<T>::value &&
                                   std::is_nothrow_move_assignable<T>::value;
};

static_assert(movable_not_copyable<vtzero::tile_builder>::value, "tile_builder should be nothrow movable, but not copyable");

static_assert(movable_not_copyable<vtzero::feature_builder<2>>::value, "feature_builder<2> should be nothrow movable, but not copyable");
static_assert(movable_not_copyable<vtzero::feature_builder<3>>::value, "feature_builder<3> should be nothrow movable, but not copyable");

static_assert(movable_not_copyable<vtzero::point_feature_builder<2>>::value, "point_feature_builder<2> should be nothrow movable, but not copyable");
static_assert(movable_not_copyable<vtzero::point_feature_builder<3>>::value, "point_feature_builder<3> should be nothrow movable, but not copyable");

static_assert(movable_not_copyable<vtzero::linestring_feature_builder<2>>::value, "linestring_feature_builder<2> should be nothrow movable, but not copyable");
static_assert(movable_not_copyable<vtzero::linestring_feature_builder<3>>::value, "linestring_feature_builder<3> should be nothrow movable, but not copyable");

static_assert(movable_not_copyable<vtzero::polygon_feature_builder<2>>::value, "polygon_feature_builder<2> should be nothrow movable, but not copyable");
static_assert(movable_not_copyable<vtzero::polygon_feature_builder<3>>::value, "polygon_feature_builder<3> should be nothrow movable, but not copyable");

TEST_CASE("Create tile from existing layers") {
    const std::string buffer{load_test_tile()};
    const vtzero::vector_tile tile{buffer};

    vtzero::tile_builder tbuilder;

    SECTION("add_existing_layer(layer)") {
        for (auto layer : tile) {
            tbuilder.add_existing_layer(layer);
        }
    }

    SECTION("add_existing_layer(data_view)") {
        for (auto layer : tile) {
            tbuilder.add_existing_layer(layer.data());
        }
    }

    const std::string data = tbuilder.serialize();

    REQUIRE(data == buffer);
}

TEST_CASE("Create layer based on existing layer") {
    const std::string buffer{load_test_tile()};
    const vtzero::vector_tile tile{buffer};
    const auto layer = tile.get_layer_by_name("place_label");
    REQUIRE(layer.extent() == 4096);

    vtzero::tile_builder tbuilder;
    vtzero::layer_builder lbuilder{tbuilder, layer};
    vtzero::point_feature_builder<2> fbuilder{lbuilder};
    fbuilder.set_integer_id(42);
    fbuilder.add_point(vtzero::point_2d{10, 20});
    fbuilder.commit();

    const std::string data = tbuilder.serialize();
    const vtzero::vector_tile new_tile{data};
    const auto new_layer = *new_tile.begin();
    REQUIRE(std::string(new_layer.name()) == "place_label");
    REQUIRE(new_layer.version() == 1);
    REQUIRE(new_layer.extent() == 4096);
}

TEST_CASE("Create layer and add keys/values") {
    vtzero::tile_builder tbuilder;
    vtzero::layer_builder lbuilder{tbuilder, "name"};

    const auto ki1 = lbuilder.add_key_without_dup_check("key1");
    const auto ki2 = lbuilder.add_key("key2");
    const auto ki3 = lbuilder.add_key("key1");

    REQUIRE(ki1 != ki2);
    REQUIRE(ki1 == ki3);

    const auto vi1 = lbuilder.add_value_without_dup_check(vtzero::encoded_property_value{"value1"});
    vtzero::encoded_property_value value2{"value2"};
    const auto vi2 = lbuilder.add_value_without_dup_check(vtzero::property_value{value2.data()});

    const auto vi3 = lbuilder.add_value(vtzero::encoded_property_value{"value1"});
    const auto vi4 = lbuilder.add_value(vtzero::encoded_property_value{19});
    const auto vi5 = lbuilder.add_value(vtzero::encoded_property_value{19.0});
    const auto vi6 = lbuilder.add_value(vtzero::encoded_property_value{22});
    vtzero::encoded_property_value nineteen{19};
    const auto vi7 = lbuilder.add_value(vtzero::property_value{nineteen.data()});

    REQUIRE(vi1 != vi2);
    REQUIRE(vi1 == vi3);
    REQUIRE(vi1 != vi4);
    REQUIRE(vi1 != vi5);
    REQUIRE(vi1 != vi6);
    REQUIRE(vi4 != vi5);
    REQUIRE(vi4 != vi6);
    REQUIRE(vi4 == vi7);
}

TEST_CASE("Create layer and add scalings") {
    vtzero::scaling scaling_elev{11, 2.2, 3.3};

    vtzero::scaling scaling0{0, 1.0, 0.0};
    vtzero::scaling scaling1{1, 2.0, 1.0};
    vtzero::scaling scaling2{2, 3.0, 2.0};

    vtzero::tile_builder tbuilder;
    vtzero::layer_builder lbuilder{tbuilder, "name", 3};

    { // we need to add a feature, otherwise the layer will not be serialized
        vtzero::point_feature_builder<3> fbuilder{lbuilder};
        fbuilder.add_point(vtzero::point_3d{});
        fbuilder.commit();
    }

    REQUIRE(lbuilder.elevation_scaling() == vtzero::scaling{});
    lbuilder.set_elevation_scaling(scaling_elev);
    REQUIRE(lbuilder.elevation_scaling() == scaling_elev);

    const auto index0 = lbuilder.add_attribute_scaling(scaling0);
    REQUIRE(index0.value() == 0);
    const auto index1 = lbuilder.add_attribute_scaling(scaling1);
    REQUIRE(index1.value() == 1);
    const auto index2 = lbuilder.add_attribute_scaling(scaling2);
    REQUIRE(index2.value() == 2);

    const std::string data = tbuilder.serialize();

    const vtzero::vector_tile tile{data};

    auto layer = *tile.begin();
    REQUIRE(layer);
    REQUIRE(layer.name() == "name");
    REQUIRE(layer.version() == 3);
    REQUIRE(layer.num_features() == 1);

    REQUIRE(layer.elevation_scaling() == scaling_elev);
    REQUIRE(layer.num_attribute_scalings() == 3);
    REQUIRE(layer.attribute_scaling(vtzero::index_value{0}) == scaling0);
    REQUIRE(layer.attribute_scaling(vtzero::index_value{1}) == scaling1);
    REQUIRE(layer.attribute_scaling(vtzero::index_value{2}) == scaling2);
    REQUIRE_THROWS_AS(layer.attribute_scaling(vtzero::index_value{3}), const std::out_of_range&);
}

TEST_CASE("Committing a feature succeeds after a geometry was added") {
    vtzero::tile_builder tbuilder;
    vtzero::layer_builder lbuilder{tbuilder, "test"};

    { // explicit commit after geometry
        vtzero::point_feature_builder<2> fbuilder{lbuilder};
        fbuilder.set_integer_id(1);
        fbuilder.add_point(vtzero::point_2d{10, 10});
        fbuilder.commit();
    }

    { // explicit commit after attributes
        vtzero::point_feature_builder<2> fbuilder{lbuilder};
        fbuilder.set_integer_id(2);
        fbuilder.add_point(vtzero::point_2d{10, 10});
        fbuilder.add_property("foo", vtzero::encoded_property_value{"bar"});
        fbuilder.commit();
    }

    { // extra commits or rollbacks are okay but no other calls
        vtzero::point_feature_builder<2> fbuilder{lbuilder};
        fbuilder.set_integer_id(3);
        fbuilder.add_point(vtzero::point_2d{10, 10});
        fbuilder.add_property("foo", vtzero::encoded_property_value{"bar"});
        fbuilder.commit();

        SECTION("superfluous commit()") {
            fbuilder.commit();
        }
        SECTION("superfluous rollback()") {
            fbuilder.rollback();
        }

        REQUIRE_THROWS_AS(fbuilder.set_integer_id(10), const assert_error&);
        REQUIRE_THROWS_AS(fbuilder.add_point(vtzero::point_2d{20, 20}), const assert_error&);
        REQUIRE_THROWS_AS(fbuilder.add_property("x", "y"), const assert_error&);
    }

    const std::string data = tbuilder.serialize();

    const vtzero::vector_tile tile{data};
    const auto layer = *tile.begin();

    uint64_t n = 1;
    for (const auto feature : layer) {
        REQUIRE(feature.integer_id() == n++);
    }

    REQUIRE(n == 4);
}

TEST_CASE("Committing a feature fails with assert if no geometry was added") {
    vtzero::tile_builder tbuilder;
    vtzero::layer_builder lbuilder{tbuilder, "test"};

    SECTION("explicit immediate commit") {
        vtzero::point_feature_builder<2> fbuilder{lbuilder};
        REQUIRE_THROWS_AS(fbuilder.commit(), const assert_error&);
    }

    SECTION("explicit commit after setting id") {
        vtzero::point_feature_builder<2> fbuilder{lbuilder};
        fbuilder.set_integer_id(2);
        REQUIRE_THROWS_AS(fbuilder.commit(), const assert_error&);
    }
}

TEST_CASE("String ids are not allowed in version 2 tiles") {
    vtzero::tile_builder tbuilder;
    vtzero::layer_builder lbuilder{tbuilder, "test"};
    vtzero::point_feature_builder<2> fbuilder{lbuilder};
    REQUIRE_THROWS_AS(fbuilder.set_string_id("foo"), const assert_error&);
}

TEST_CASE("String ids are okay in version 3 tiles") {
    vtzero::tile_builder tbuilder;
    vtzero::layer_builder lbuilder{tbuilder, "test", 3};
    vtzero::point_feature_builder<2> fbuilder{lbuilder};
    fbuilder.set_string_id("foo");
    fbuilder.add_point(vtzero::point_2d{10, 10});
    fbuilder.commit();

    const std::string data = tbuilder.serialize();

    const vtzero::vector_tile tile{data};
    const auto layer = *tile.begin();

    const auto feature = *layer.begin();
    REQUIRE_FALSE(feature.has_integer_id());
    REQUIRE(feature.has_string_id());
    REQUIRE(feature.string_id() == "foo");
}

TEST_CASE("Create layer with x/y/zoom/extent") {
    vtzero::tile_builder tbuilder;
    vtzero::layer_builder lbuilder{tbuilder, "test", 3, vtzero::tile{5, 3, 12, 8192}};
    vtzero::point_feature_builder<2> fbuilder{lbuilder};
    fbuilder.set_string_id("foo");
    fbuilder.add_point(vtzero::point_2d{10, 10});
    fbuilder.commit();

    const std::string data = tbuilder.serialize();

    const vtzero::vector_tile tile{data};
    auto layer = *tile.begin();

    REQUIRE(layer.get_tile().x() == 5);
    REQUIRE(layer.get_tile().y() == 3);
    REQUIRE(layer.get_tile().zoom() == 12);
    REQUIRE(layer.extent() == 8192);
}

TEST_CASE("Rollback feature") {
    vtzero::tile_builder tbuilder;
    vtzero::layer_builder lbuilder{tbuilder, "test"};

    {
        vtzero::point_feature_builder<2> fbuilder{lbuilder};
        fbuilder.set_integer_id(1);
        fbuilder.add_point(vtzero::point_2d{10, 10});
        fbuilder.commit();
    }

    { // immediate rollback
        vtzero::point_feature_builder<2> fbuilder{lbuilder};
        fbuilder.set_integer_id(2);
        fbuilder.rollback();
    }

    { // rollback after setting id
        vtzero::point_feature_builder<2> fbuilder{lbuilder};
        fbuilder.set_integer_id(3);
        fbuilder.rollback();
    }

    { // rollback after geometry
        vtzero::point_feature_builder<2> fbuilder{lbuilder};
        fbuilder.set_integer_id(4);
        fbuilder.add_point(vtzero::point_2d{20, 20});
        fbuilder.rollback();
    }

    { // rollback after attributes
        vtzero::point_feature_builder<2> fbuilder{lbuilder};
        fbuilder.set_integer_id(5);
        fbuilder.add_point(vtzero::point_2d{20, 20});
        fbuilder.add_property("foo", vtzero::encoded_property_value{"bar"});
        fbuilder.rollback();
    }

    { // implicit rollback after geometry
        vtzero::point_feature_builder<2> fbuilder{lbuilder};
        fbuilder.set_integer_id(6);
        fbuilder.add_point(vtzero::point_2d{10, 10});
    }

    { // implicit rollback after attributes
        vtzero::point_feature_builder<2> fbuilder{lbuilder};
        fbuilder.set_integer_id(7);
        fbuilder.add_point(vtzero::point_2d{10, 10});
        fbuilder.add_property("foo", vtzero::encoded_property_value{"bar"});
    }

    {
        vtzero::point_feature_builder<2> fbuilder{lbuilder};
        fbuilder.set_integer_id(8);
        fbuilder.add_point(vtzero::point_2d{30, 30});
        fbuilder.commit();
    }

    const std::string data = tbuilder.serialize();

    const vtzero::vector_tile tile{data};
    const auto layer = *tile.begin();

    auto it = layer.begin();
    auto feature = *it++;
    REQUIRE(feature.has_integer_id());
    REQUIRE_FALSE(feature.has_string_id());
    REQUIRE(feature.integer_id() == 1);
    feature = *it++;
    REQUIRE(feature.has_integer_id());
    REQUIRE_FALSE(feature.has_string_id());
    REQUIRE(feature.integer_id() == 8);

    REQUIRE(it == layer.end());
}

static vtzero::layer next_nonempty_layer(vtzero::layer_iterator& it, const vtzero::layer_iterator end) {
    while (it != end) {
        auto layer = *it++;
        if (!layer.empty()) {
            return layer;
        }
    }
    return vtzero::layer{};
}

static bool vector_tile_equal(const std::string& t1, const std::string& t2) {
    const vtzero::vector_tile vt1{t1};
    const vtzero::vector_tile vt2{t2};

    auto it1 = vt1.begin();
    auto it2 = vt2.begin();

    const auto end1 = vt1.end();
    const auto end2 = vt2.end();

    for (auto l1 = next_nonempty_layer(it1, end1), l2 = next_nonempty_layer(it2, end2);
         l1 || l2;
         l1 = next_nonempty_layer(it1, end1), l2 = next_nonempty_layer(it2, end2)) {

        if (!l1 ||
            !l2 ||
            l1.version() != l2.version() ||
            l1.get_tile() != l2.get_tile() ||
            l1.num_features() != l2.num_features() ||
            l1.name() != l2.name()) {
            return false;
        }

        for (auto it1 = l1.begin(), it2 = l2.begin(); it1 != l1.end() || it2 != l2.end(); ++it1, ++it2) {
            if (it1 == l1.end() || it2 == l2.end()) {
                return false;
            }
            const auto f1 = *it1;
            const auto f2 = *it2;
            if (!f1 ||
                !f2 ||
                f1.integer_id() != f2.integer_id() ||
                f1.string_id() != f2.string_id() ||
                f1.geometry_type() != f2.geometry_type() ||
                f1.geometry_data() != f2.geometry_data() ||
                f1.elevations_data() != f2.elevations_data() ||
                f1.tags_data() != f2.tags_data() ||
                f1.attributes_data() != f2.attributes_data() ||
                f1.geometric_attributes_data() != f2.geometric_attributes_data()) {
                return false;
            }
        }
    }

    return true;
}

TEST_CASE("vector_tile_equal") {
    REQUIRE(vector_tile_equal("", ""));

    const std::string buffer{load_test_tile()};
    REQUIRE(buffer.size() == 269388);
    REQUIRE(vector_tile_equal(buffer, buffer));

    REQUIRE_FALSE(vector_tile_equal(buffer, ""));
}

TEST_CASE("Copy tile") {
    const std::string buffer{load_test_tile()};
    const vtzero::vector_tile tile{buffer};

    vtzero::tile_builder tbuilder;

    for (const auto layer : tile) {
        vtzero::layer_builder lbuilder{tbuilder, layer};
        for (const auto feature : layer) {
            vtzero::copy_feature(feature, lbuilder);
        }
    }

    const std::string data = tbuilder.serialize();
    REQUIRE(vector_tile_equal(buffer, data));
}

TEST_CASE("Copy tile using geometry_feature_builder<2>") {
    const std::string buffer{load_test_tile()};
    const vtzero::vector_tile tile{buffer};

    vtzero::tile_builder tbuilder;

    for (const auto layer : tile) {
        vtzero::layer_builder lbuilder{tbuilder, layer};
        for (const auto feature : layer) {
            vtzero::feature_builder<2> fbuilder{lbuilder};
            fbuilder.copy_id(feature);
            fbuilder.copy_geometry(feature);
            fbuilder.copy_attributes(feature);
            fbuilder.commit();
        }
    }

    const std::string data = tbuilder.serialize();
    REQUIRE(vector_tile_equal(buffer, data));
}

TEST_CASE("Copy only point geometries using geometry_feature_builder<2>") {
    const std::string buffer{load_test_tile()};
    const vtzero::vector_tile tile{buffer};

    vtzero::tile_builder tbuilder;

    std::size_t n = 0;
    for (const auto layer : tile) {
        vtzero::layer_builder lbuilder{tbuilder, layer};
        for (const auto feature : layer) {
            vtzero::feature_builder<2> fbuilder{lbuilder};
            fbuilder.set_integer_id(feature.integer_id());
            if (feature.geometry_type() == vtzero::GeomType::POINT) {
                fbuilder.copy_geometry(feature);
                fbuilder.copy_attributes(feature);
                fbuilder.commit();
                ++n;
            } else {
                fbuilder.rollback();
            }
        }
    }
    REQUIRE(n == 17);

    const std::string data = tbuilder.serialize();

    n = 0;
    const vtzero::vector_tile result_tile{data};
    for (const auto layer : result_tile) {
        n += layer.num_features();
    }

    REQUIRE(n == 17);
}

struct points_to_vector {

    constexpr static const int dimensions = 2;
    constexpr static const unsigned int max_geometric_attributes = 0;

    std::vector<vtzero::point_2d> m_points{};

    static vtzero::point_2d convert(const vtzero::point_2d& p) noexcept {
        return p;
    }

    void points_begin(const uint32_t count) {
        m_points.reserve(count);
    }

    void points_point(const vtzero::point_2d point) {
        m_points.push_back(point);
    }

    void points_end() const {
    }

    const std::vector<vtzero::point_2d>& result() const {
        return m_points;
    }

}; // struct points_to_vector

TEST_CASE("Copy only point geometries using point_feature_builder<2>") {
    const std::string buffer{load_test_tile()};
    const vtzero::vector_tile tile{buffer};

    vtzero::tile_builder tbuilder;

    std::size_t n = 0;
    for (const auto layer : tile) {
        vtzero::layer_builder lbuilder{tbuilder, layer};
        for (const auto feature : layer) {
            vtzero::point_feature_builder<2> fbuilder{lbuilder};
            fbuilder.copy_id(feature);
            if (feature.geometry_type() == vtzero::GeomType::POINT) {
                const auto points = feature.decode_point_geometry(points_to_vector{});
                vtzero::add_points_from_container(points, fbuilder);
                fbuilder.copy_attributes(feature);
                fbuilder.commit();
                ++n;
            } else {
                fbuilder.rollback();
            }
        }
    }
    REQUIRE(n == 17);

    const std::string data = tbuilder.serialize();

    n = 0;
    const vtzero::vector_tile result_tile{data};
    for (const auto layer : result_tile) {
        n += layer.num_features();
    }

    REQUIRE(n == 17);
}

TEST_CASE("Build point feature from container with too many points") {

    // fake container pretending to contain too many points
    struct test_container {

        static std::size_t size() noexcept {
            return 1UL << 29U;
        }

        static vtzero::point_2d* begin() noexcept {
            return nullptr;
        }

        static vtzero::point_2d* end() noexcept {
            return nullptr;
        }

    };

    vtzero::tile_builder tbuilder;
    vtzero::layer_builder lbuilder{tbuilder, "test"};
    vtzero::point_feature_builder<2> fbuilder{lbuilder};

    fbuilder.set_integer_id(1);

    test_container tc;
    REQUIRE_THROWS_AS(vtzero::add_points_from_container(tc, fbuilder), const vtzero::geometry_exception&);
}

TEST_CASE("Moving a feature builder is allowed") {
    vtzero::tile_builder tbuilder;
    vtzero::layer_builder lbuilder{tbuilder, "test"};
    vtzero::point_feature_builder<2> fbuilder{lbuilder};

    auto fbuilder2 = std::move(fbuilder);
    vtzero::point_feature_builder<2> fbuilder3{std::move(fbuilder2)};
}

