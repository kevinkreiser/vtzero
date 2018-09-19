#ifndef VTZERO_BUILDER_IMPL_HPP
#define VTZERO_BUILDER_IMPL_HPP

/*****************************************************************************

vtzero - Tiny and fast vector tile decoder and encoder in C++.

This file is from https://github.com/mapbox/vtzero where you can find more
documentation.

*****************************************************************************/

/**
 * @file builder_impl.hpp
 *
 * @brief Contains classes internal to the builder.
 */

#include "encoded_property_value.hpp"
#include "property_value.hpp"
#include "types.hpp"

#include <protozero/pbf_builder.hpp>
#include <protozero/pbf_message.hpp>

#include <cstdlib>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace vtzero {

    namespace detail {

        class layer_builder_base {

        public:

            layer_builder_base() noexcept = default;

            virtual ~layer_builder_base() noexcept = default;

            layer_builder_base(const layer_builder_base&) noexcept = default;
            layer_builder_base& operator=(const layer_builder_base&) noexcept = default;

            layer_builder_base(layer_builder_base&&) noexcept = default;
            layer_builder_base& operator=(layer_builder_base&&) noexcept = default;

            virtual std::size_t estimated_size() const = 0;

            virtual void build(protozero::pbf_builder<detail::pbf_tile>& pbf_tile_builder) const = 0;

        }; // class layer_builder_base

        class string_table {

            // Below this value, no index will be used to find entries in the
            // key/value tables. This number is based on some initial
            // benchmarking but probably needs some tuning.
            // See also https://github.com/mapbox/vtzero/issues/30
            static constexpr const uint32_t max_entries_flat = 20;

            // Buffer containing the encoded table
            std::string m_data{};

            // The index into the table
            using map_type = std::unordered_map<std::string, index_value>;
            map_type m_index{};

            // The pbf message builder for our table
            protozero::pbf_builder<detail::pbf_layer> m_pbf_message;

            // The number of entries in the table
            uint32_t m_num = 0;

            // PBF message type
            detail::pbf_layer m_pbf_type;

            static index_value find_in_table(const data_view text, const std::string& data) {
                uint32_t index = 0;
                protozero::pbf_message<detail::pbf_layer> pbf_table{data};

                while (pbf_table.next()) {
                    const auto v = pbf_table.get_view();
                    if (v == text) {
                        return index_value{index};
                    }
                    ++index;
                }

                return index_value{};
            }

            // Read the key or value table and populate an index from its
            // entries. This is done once the table becomes too large to do
            // linear search in it.
            static void populate_index(const std::string& data, map_type& map) {
                uint32_t index = 0;
                protozero::pbf_message<detail::pbf_layer> pbf_table{data};

                while (pbf_table.next()) {
                    map[pbf_table.get_string()] = index++;
                }
            }

        public:

            explicit string_table(detail::pbf_layer pbf_type) :
                m_pbf_message(m_data),
                m_pbf_type(pbf_type) {
            }

            const std::string& data() const noexcept {
                return m_data;
            }

            index_value add_without_dup_check(const data_view text) {
                m_pbf_message.add_string(m_pbf_type, text);
                return m_num++;
            }

            index_value find(const data_view text) {
                if (m_num < max_entries_flat) {
                    return find_in_table(text, m_data);
                }

                if (m_index.empty()) {
                    populate_index(m_data, m_index);
                }

                auto& v = m_index[std::string(text)];
                if (!v.valid()) {
                    v = add_without_dup_check(text);
                }
                return v;
            }

            index_value add(const data_view text) {
                const auto index = find(text);
                if (index.valid()) {
                    return index;
                }
                return add_without_dup_check(text);
            }

        }; // class string_table

        class layer_builder_impl : public layer_builder_base {

            // Buffer containing the encoded layer metadata and features
            std::string m_data;

            string_table m_keys_table{detail::pbf_layer::keys};
            string_table m_values_table{detail::pbf_layer::values};
            string_table m_string_values_table{detail::pbf_layer::string_values};

            // The double_values index table
            std::vector<double> m_double_values;

            // The float_values index table
            std::vector<float> m_float_values;

            // The int_values index table
            std::vector<uint64_t> m_int_values;

            protozero::pbf_builder<detail::pbf_layer> m_pbf_message_layer;

            // The number of features in the layer
            std::size_t m_num_features = 0;

            // Vector tile spec version
            uint32_t m_version = 0;

        public:

            template <typename TString>
            layer_builder_impl(TString&& name, uint32_t version, uint32_t extent) :
                m_pbf_message_layer(m_data),
                m_version(version) {
                m_pbf_message_layer.add_uint32(detail::pbf_layer::version, version);
                m_pbf_message_layer.add_string(detail::pbf_layer::name, std::forward<TString>(name));
                m_pbf_message_layer.add_uint32(detail::pbf_layer::extent, extent);
            }

            ~layer_builder_impl() noexcept override = default;

            layer_builder_impl(const layer_builder_impl&) = delete;
            layer_builder_impl& operator=(const layer_builder_impl&) = delete;

            layer_builder_impl(layer_builder_impl&&) = default;
            layer_builder_impl& operator=(layer_builder_impl&&) = default;

            uint32_t version() const noexcept {
                return m_version;
            }

            index_value add_key_without_dup_check(const data_view text) {
                return m_keys_table.add_without_dup_check(text);
            }

            index_value add_key(const data_view text) {
                return m_keys_table.add(text);
            }

            index_value add_value_without_dup_check(const property_value value) {
                vtzero_assert(m_version < 3);
                return m_values_table.add_without_dup_check(value.data());
            }

            index_value add_value_without_dup_check(const encoded_property_value& value) {
                vtzero_assert(m_version < 3);
                return m_values_table.add_without_dup_check(value.data());
            }

            index_value add_value(const property_value value) {
                vtzero_assert(m_version < 3);
                return m_values_table.add(value.data());
            }

            index_value add_value(const encoded_property_value& value) {
                vtzero_assert(m_version < 3);
                return m_values_table.add(value.data());
            }

            index_value add_string_value_without_dup_check(data_view value) {
                vtzero_assert(m_version == 3);
                return m_string_values_table.add_without_dup_check(value);
            }

            index_value add_string_value(data_view value) {
                vtzero_assert(m_version == 3);
                return m_string_values_table.add(value);
            }

            index_value add_double_value_without_dup_check(double value) {
                vtzero_assert(m_version == 3);
                m_double_values.push_back(value);
                return index_value{static_cast<uint32_t>(m_double_values.size() - 1)};
            }

            index_value add_double_value(double value) {
                vtzero_assert(m_version == 3);
                const auto it = std::find(m_double_values.cbegin(), m_double_values.cend(), value);
                if (it != m_double_values.cend()) {
                    return index_value{static_cast<uint32_t>(std::distance(m_double_values.cbegin(), it))};
                }
                return add_double_value_without_dup_check(value);
            }

            index_value add_float_value_without_dup_check(float value) {
                vtzero_assert(m_version == 3);
                m_float_values.push_back(value);
                return index_value{static_cast<uint32_t>(m_float_values.size() - 1)};
            }

            index_value add_float_value(float value) {
                vtzero_assert(m_version == 3);
                const auto it = std::find(m_float_values.cbegin(), m_float_values.cend(), value);
                if (it != m_float_values.cend()) {
                    return index_value{static_cast<uint32_t>(std::distance(m_float_values.cbegin(), it))};
                }
                return add_float_value_without_dup_check(value);
            }

            index_value add_int_value_without_dup_check(uint64_t value) {
                vtzero_assert(m_version == 3);
                m_int_values.push_back(value);
                return index_value{static_cast<uint32_t>(m_int_values.size() - 1)};
            }

            index_value add_int_value(uint64_t value) {
                vtzero_assert(m_version == 3);
                const auto it = std::find(m_int_values.cbegin(), m_int_values.cend(), value);
                if (it != m_int_values.cend()) {
                    return index_value{static_cast<uint32_t>(std::distance(m_int_values.cbegin(), it))};
                }
                return add_int_value_without_dup_check(value);
            }

            protozero::pbf_builder<detail::pbf_layer>& message() noexcept {
                return m_pbf_message_layer;
            }

            void increment_feature_count() noexcept {
                ++m_num_features;
            }

            std::size_t estimated_size() const override {
                constexpr const std::size_t estimated_overhead_for_pbf_encoding = 14;
                return m_data.size() +
                       m_keys_table.data().size() +
                       m_values_table.data().size() +
                       m_double_values.size() +
                       m_float_values.size() +
                       m_int_values.size() +
                       estimated_overhead_for_pbf_encoding;
            }

            void build(protozero::pbf_builder<detail::pbf_tile>& pbf_tile_builder) const override {
                if (m_num_features > 0) {
                    if (m_version < 3) {
                        pbf_tile_builder.add_bytes_vectored(detail::pbf_tile::layers,
                                                            m_data,
                                                            m_keys_table.data(),
                                                            m_values_table.data());
                    } else {
                        constexpr const std::size_t estimated_overhead_for_pbf_encoding = 2 * (1 + 2);
                        std::string values_tables_data;
                        values_tables_data.reserve(m_double_values.size() + m_float_values.size() + m_int_values.size() + estimated_overhead_for_pbf_encoding);
                        if (!m_double_values.empty()) {
                            protozero::pbf_builder<detail::pbf_layer> pbf_table{values_tables_data};
                            pbf_table.add_packed_double(detail::pbf_layer::double_values, m_double_values.begin(), m_double_values.end());
                        }
                        if (!m_float_values.empty()) {
                            protozero::pbf_builder<detail::pbf_layer> pbf_table{values_tables_data};
                            pbf_table.add_packed_float(detail::pbf_layer::float_values, m_float_values.begin(), m_float_values.end());
                        }
                        if (!m_int_values.empty()) {
                            protozero::pbf_builder<detail::pbf_layer> pbf_table{values_tables_data};
                            pbf_table.add_packed_uint64(detail::pbf_layer::int_values, m_int_values.begin(), m_int_values.end());
                        }

                        pbf_tile_builder.add_bytes_vectored(detail::pbf_tile::layers,
                                                            m_data,
                                                            m_keys_table.data(),
                                                            m_string_values_table.data(),
                                                            values_tables_data);
                    }
                }
            }

        }; // class layer_builder_impl

        class layer_builder_existing : public layer_builder_base {

            data_view m_data;

        public:

            explicit layer_builder_existing(const data_view data) :
                m_data(data) {
            }

            std::size_t estimated_size() const override {
                constexpr const std::size_t estimated_overhead_for_pbf_encoding = 8;
                return m_data.size() + estimated_overhead_for_pbf_encoding;
            }

            void build(protozero::pbf_builder<detail::pbf_tile>& pbf_tile_builder) const override {
                pbf_tile_builder.add_bytes(detail::pbf_tile::layers, m_data);
            }

        }; // class layer_builder_existing

    } // namespace detail

} // namespace vtzero

#endif // VTZERO_BUILDER_IMPL_HPP
