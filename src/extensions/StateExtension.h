#pragma once

#include "core/Extension.h"
#include <clap/ext/state.h>
#include <clap/stream.h>
#include <functional>
#include <yas/binary_iarchive.hpp>
#include <yas/binary_oarchive.hpp>
#include <yas/std_types.hpp>

namespace applause
{
    /**
     * @brief CLAP state extension for plugin state serialization/deserialization.
     *
     * This extension allows plugins to save and restore their state to/from a binary stream.
     * To use this extension, supply it with save and load callbacks as seen below. The YAS serialization
     * library supports primitive types as well as standard library structures like maps and vectors.
     *
     * The order that you save data in must match the order that you load it in: it's just a simple stream.
     *
     * Example usage:
     * @code
     * stateExt.setSaveCallback([this](auto& ar) {
     *     ar & myValue1 & myValue2;
     *     myParamsExtension.saveToStream(ar)
     * });
     *
     * stateExt.setLoadCallback([this](auto& ar) {
     *     ar & myValue1 & myValue2;
     *     myParamsExtension.loadFromStream(ar)
     * });
     * @endcode
     */
    class StateExtension : public IExtension
    {
    public:
        /**
         * @brief Save callback type - called with a YAS binary output archive.
         * The lambda should serialize all state data using the archive.
         * @tparam Archive YAS binary output archive type
         * @return true on success, false on error
         */
        template <typename Archive>
        using SaveCallback = std::function<bool(Archive&)>;

        /**
         * @brief Load callback type - called with a YAS binary input archive.
         * The lambda should deserialize all state data from the archive.
         * @tparam Archive YAS binary input archive type
         * @return true on success, false on error
         */
        template <typename Archive>
        using LoadCallback = std::function<bool(Archive&)>;

    private:
        mutable clap_plugin_state_t clap_struct_{};

        // Type-erased callbacks to avoid template complications
        std::function<bool(const clap_ostream_t*)> save_impl_;
        std::function<bool(const clap_istream_t*)> load_impl_;

        /**
         * @brief CLAP C callback: Save plugin state to stream.
         * @param plugin Plugin instance
         * @param stream Output stream to write state to
         * @return true on success, false on error
         */
        static bool clap_state_save(const clap_plugin_t* plugin, const clap_ostream_t* stream) noexcept;

        /**
         * @brief CLAP C callback: Load plugin state from stream.
         * @param plugin Plugin instance
         * @param stream Input stream to read state from
         * @return true on success, false on error
         */
        static bool clap_state_load(const clap_plugin_t* plugin, const clap_istream_t* stream) noexcept;

    public:
        static constexpr const char* ID = CLAP_EXT_STATE;

        StateExtension();

        const char* id() const override { return ID; }
        const void* getClapExtensionStruct() const override { return &clap_struct_; }

        /**
         * @brief Set the save state callback.
         *
         * The callback will be invoked with a YAS binary output archive when the host
         * requests the plugin state to be saved. Use the archive to serialize your data.
         *
         * @param callback Lambda that performs state serialization
         */
        template<typename SaveFunc>
    void setSaveCallback(SaveFunc callback);

        /**
         * @brief Set the load state callback.
         *
         * The callback will be invoked with a YAS binary input archive when the host
         * requests the plugin state to be loaded. Use the archive to deserialize your data.
         *
         * @param callback Lambda that performs state deserialization
         */
        template<typename LoadFunc>
    void setLoadCallback(LoadFunc callback);

        /**
         * @brief Check if both save and load callbacks have been set.
         * @return true if the extension is ready to handle state operations
         */
        bool isConfigured() const { return save_impl_ && load_impl_; }
    };

    // Stream wrapper implementations
    namespace detail {
        // Wrapper class to make clap_ostream compatible with YAS
        class ClapOStream {
        public:
            explicit ClapOStream(const clap_ostream_t* os) : os_(os) {}
            
            auto write(const void* s, uint64_t n) noexcept {
                return os_->write(os_, s, n);
            }
            
        private:
            const clap_ostream_t* const os_;
        };

        // Wrapper class to make clap_istream compatible with YAS
        class ClapIStream {
        public:
            explicit ClapIStream(const clap_istream_t* is) : is_(is) {}
            
            auto read(void* s, uint64_t n) noexcept {
                return is_->read(is_, s, n);
            }
            
        private:
            const clap_istream_t* const is_;
        };
    } // namespace detail

    // Template implementations
    template<typename SaveFunc>
    void StateExtension::setSaveCallback(SaveFunc callback) {
        save_impl_ = [callback](const clap_ostream_t* stream) -> bool {
            try {
                detail::ClapOStream os(stream);
                yas::binary_oarchive<detail::ClapOStream> ar(os);
                return callback(ar);
            } catch (...) {
                return false;
            }
        };
    }

    template<typename LoadFunc>
    void StateExtension::setLoadCallback(LoadFunc callback) {
        load_impl_ = [callback](const clap_istream_t* stream) -> bool {
            try {
                detail::ClapIStream is(stream);
                yas::binary_iarchive<detail::ClapIStream> ar(is);
                return callback(ar);
            } catch (...) {
                return false;
            }
        };
    }

} // namespace applause
