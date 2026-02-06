#pragma once

#include <clap/events.h>
#include <clap/ext/params.h>
#include <clap/host.h>

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include "applause/core/Extension.h"
#include "applause/util/DebugHelpers.h"
#include "applause/util/Json.h"
#include "applause/util/ParamMessageQueue.h"
#include "applause/util/thirdparty/rocket.hpp"
#include "../util/ValueScaling.h"

namespace applause {
class ParamsExtension;
class ParamInfo;

/**
 * @brief Configuration structure for a parameter.
 */
struct ParamConfig {
    std::string string_id;       /// String identifier for the parameter (required)
    std::string name;            /// Display name (if empty, uses string_id)
    std::string module;          /// Module path for hierarchical grouping (e.g. "Filter/Envelope")
    std::string short_name;      /// Short display name (e.g., "Cutoff")
    std::string unit;            /// Unit string (e.g., "Hz", "dB")
    float min_value = 0.0f;      /// Minimum value
    float max_value = 1.0f;      /// Maximum value
    float default_value = 0.5f;  /// Default value
    bool is_stepped = false;     /// Whether parameter uses discrete integer values
    bool is_internal = false;    /// Whether parameter is internal (not exposed to DAW)
    bool is_hidden = false;      /// Whether parameter is hidden from host UI (still enumerated, but flagged CLAP_PARAM_IS_HIDDEN)
    bool is_polyphonic = false;  /// Whether parameter can/should be polyphonically modulated by a modulation system.
                                 ///   for example: osc tuning in a synth would be poly, but master out gain might not

    ValueScaling scaling = ValueScaling::linear();  /// Parameter scaling for normalization (default: linear)

    // Optional custom converters (default to nullptr)
    std::function<std::string(float value, const ParamInfo& info)> value_to_text;
    std::function<std::optional<float>(const std::string& text, const ParamInfo& info)> text_to_value;
};

/**
 * Provides a lightweight, efficient, and thread-safe handle
 * for interacting with a parameter's value.
 *
 * This class is designed for use in high-performance DSP code
 * where frequent read-only access to parameter values is required.
 */
struct ParamHandle {
    friend class ParamsExtension;
    friend class ParamInfo;

private:
    std::atomic<float>* value_ = nullptr;

public:
    [[nodiscard]] float getValue() const noexcept { return value_->load(std::memory_order_relaxed); }
};

/**
 * A heftier information struct that can be used in the UI thread
 * to display pertinent information to the user.
 *
 * This class should not be used from the audio thread or otherwise employed by
 * any performance-sensitive DSP code. The DSP side of your plugin should use
 * ParamHandles for efficient and cache-friendly read-only parameter access.
 */
class ParamInfo {
    friend class ParamsExtension;
    friend struct ParamHandle;

public:
    /**
     * The internal ID used by the CLAP host
     */
    uint32_t clapId;

    /**
     * A human-readable name, e.g. "Filter 1 Cutoff".
     * This will be given to the CLAP host and be visible within the hosted
     * environment.
     */
    std::string name;

    /**
     * Module path for hierarchical parameter grouping, e.g. "Filter/Envelope".
     * Uses forward slash '/' as separator to create nested groups.
     * This helps hosts organize parameters in their UI (tree views, sections,
     * etc.). Can be empty if no grouping is needed.
     */
    std::string module = "";

    /**
     * A short name that can be displayed on UI components, e.g. "Cutoff".
     * Useful for UI labels where text needs to be short and exact purpose
     * can be inferred from UI context.
     */
    std::string shortName = "";

    /**
     * The unit, if applicable, e.g. "Hz"
     */
    std::string unit = "";

    float minValue = 0.0f;
    float maxValue = 1.0f;
    float defaultValue = 0.0f;

    /**
     * If true, the parameter will be entirely "internal" and invisible to the
     * DAW. It will not be registered with the CLAP ABI. There will be no
     * automation, modulation, etc.
     */
    bool internal = false;

    /**
     * If true, the parameter is enumerated to the host but flagged as hidden
     * from the host's UI. Hosts running the VST3/AU clap-wrapper may not
     */
    bool hidden = false;

    /**
     * If true, the parameter represents discrete integer values only.
     * Values are cast to integer using truncation (2.8 → 2).
     * Common for mode selection, boolean parameters, enumerated choices.
     */
    bool stepped = false;

    /**
     * If true, the parameter supports polyphonic modulation (per-voice values).
     * For example, oscillator tuning would be polyphonic, but master volume might not.
     */
    bool polyphonic = false;

    /**
     * The original string identifier used during registration.
     * Used for modulation destination registration and state serialization.
     */
    std::string stringId;

    /**
     * Signal emitted when the parameter value changes from the host side.
     * UI components can connect to this to update their visual state.
     */
    mutable rocket::signal<void(float)> on_value_changed;

    float getValue() const noexcept;
    /**
     * Atomically set the value of the parameter, and notify both the plugin
     * host and the (audio-thread components of) the plugin.
     *
     * This function can ONLY be called from the UI thread.
     */
    void setValueNotifyingHost(float value) const noexcept;

    /**
     * You should only use this function if you know what you're doing!
     *
     * Atomically set the value of the parameter without notifying the host.
     * The only reason you should use this function is that you're overriding
     * any default CLAP event handling, and therefore you need to respond
     * to a parameter change from the host's side.
     */
    void setValueSilently(float value) const noexcept;

    /**
     * Notify the host that parameter gesture has begun (e.g., user started
     * dragging slider). This should be called when the user starts interacting
     * with a parameter control.
     */
    void beginGesture() const noexcept;

    /**
     * Notify the host that parameter gesture has ended (e.g., user released
     * slider). This should be called when the user finishes interacting with a
     * parameter control.
     */
    void endGesture() const noexcept;

    /**
     * Convert a parameter value to display text.
     * Uses custom converter if provided, otherwise uses default formatting.
     *
     * @param value The parameter value to format
     * @return Formatted text representation including unit if applicable
     */
    std::string valueToText(float value) const noexcept;

    /**
     * Parse user input text to extract a numeric value for this parameter.
     *
     * Uses custom converter if provided, otherwise extracts the first number
     * found in the text, ignoring non-numeric characters. The extracted value
     * is automatically clamped to [minValue, maxValue]. For stepped parameters,
     * the value is truncated to an integer.
     *
     * @param text User input text to parse (e.g., "123.4", "50Hz", "100 ms")
     * @return The parsed and clamped value, or std::nullopt if no number found
     *
     * Examples:
     *   "123.4" → 123.4
     *   "50Hz" → 50.0
     *   "100 ms" → 100.0
     *   "-45.6dB" → -45.6 (clamped if outside range)
     *   "abc" → std::nullopt
     *   "" → std::nullopt
     */
    std::optional<float> textToValue(const std::string& text) const noexcept;

    /**
     * Convert a plain value to normalized [0,1] using configured scaling.
     */
    [[nodiscard]] float toNormalized(float plainValue) const noexcept {
        return scaling_.toNormalized(plainValue, minValue, maxValue);
    }

    /**
     * Convert a normalized [0,1] value to plain using configured scaling.
     */
    [[nodiscard]] float fromNormalized(float normalized) const noexcept {
        return scaling_.fromNormalized(normalized, minValue, maxValue);
    }

    /**
     * Get current value as normalized [0,1].
     */
    [[nodiscard]] float getNormalized() const noexcept {
        return toNormalized(getValue());
    }

private:
    ParamHandle* handle_ = nullptr;

    // A pointer to the parent registry, so that the host can be notified about
    // parameter changes. In the future, we can and should use an event system
    // for this.
    ParamsExtension* registry_ = nullptr;

    // Custom converters (following member naming convention)
    std::function<std::string(float value, const ParamInfo& info)> value_to_text_;
    std::function<std::optional<float>(const std::string& text, const ParamInfo& info)> text_to_value_;

    ValueScaling scaling_;  // Parameter scaling for normalization

    friend class ParamsExtension;  // Allow ParamsExtension to access converters
};

/**
 * Implements the CLAP parameter extension, enabling efficient parameter
 * management between a plugin and the host.
 *
 * The ParamsExtension handles registering parameters, providing real-time
 * read-only access to parameter values, and facilitating communication of
 * parameter changes through the CLAP API. It is designed to be cache-friendly
 * and blazing fast.
 *
 * To use the extension, instantiate it in your plugin constructor (you'll have
 * to pass in the host pointer from your PluginBase), register it with your
 * plugin, and register some parameters!
 *
 * Make sure to call ParamsExtension::processEvents(...) in your plugin's
 * process function, otherwise the ParamExtension won't receive parameter
 * updates from the host during processing.
 *
 * The ParamsExtension has an optional pointer to a ParamMessageQueue which
 * facilitates safe, cross-thread communication between the main UI thread and
 * the audio thread. If your plugin has a GUI, you'll need to make sure that
 * both the ParamsExtension and the GUI share a pointer to a ParamMessageQueue.
 * The applause GUIExtension comes with a ParamMessageQueue that you can plug
 * directly into your ParamsExtension during construction.
 */
class ParamsExtension : public IExtension {
    friend struct ParamHandle;
    friend class ParamInfo;

private:
    mutable clap_plugin_params_t clap_struct_{};  ///< C struct for CLAP host

    static uint32_t clap_params_count(const clap_plugin_t* plugin) noexcept;
    static bool clap_params_get_info(const clap_plugin_t* plugin, uint32_t param_index,
                                     clap_param_info_t* param_info) noexcept;
    static bool clap_params_get_value(const clap_plugin_t* plugin, clap_id param_id, double* out_value) noexcept;
    static bool clap_params_value_to_text(const clap_plugin_t* plugin, clap_id param_id, double value, char* out_buffer,
                                          uint32_t out_buffer_capacity) noexcept;
    static bool clap_params_text_to_value(const clap_plugin_t* plugin, clap_id param_id, const char* param_value_text,
                                          double* out_value) noexcept;
    static void clap_params_flush(const clap_plugin_t* plugin, const clap_input_events_t* in,
                                  const clap_output_events_t* out) noexcept;

    void flush(const clap_input_events_t* in, const clap_output_events_t* out) noexcept;

    ParamMessageQueue* message_queue_;
    const clap_host_params_t* host_params_ = nullptr;
    std::unique_ptr<std::atomic<float>[]> values_;
    std::unique_ptr<ParamHandle[]> handles_;
    std::unique_ptr<ParamInfo[]> infos_;
    std::unique_ptr<ValueScaleInfo[]> scale_info_;  // DSP-safe scaling info (parallel to values_)

    // Lookup structures for O(1) access
    std::unordered_map<clap_id, uint32_t> clap_id_to_index_;
    std::unordered_map<std::string, uint32_t> string_id_to_index_;
    std::vector<uint32_t> external_to_internal_index_;

    uint32_t param_count_ = 0;
    uint32_t external_param_count_ = 0;
    uint32_t max_params_ = 0;

public:
    static constexpr const char* ID = CLAP_EXT_PARAMS;

    void onHostReady() noexcept override;

    /**
     * @brief Construct the parameters extension with space for up to
     * @p max_params parameters.
     */
    ParamsExtension(uint32_t max_params = 128);

    // Default converter functions (static members)
    static std::string defaultValueToText(float value, const ParamInfo& info);
    static std::optional<float> defaultTextToValue(const std::string& text, const ParamInfo& info);

    const char* id() const override { return ID; }

    const void* getClapExtensionStruct() const override { return &clap_struct_; }

    /**
     * @brief Set the message queue for thread-safe UI<->audio communication.
     * This should be called by the GUI extension when connecting the params to
     * the UI.
     * @param queue Pointer to the message queue (typically owned by the Editor)
     */
    void setMessageQueue(ParamMessageQueue* queue) { message_queue_ = queue; }

    /**
     * @brief Register a new parameter with the extension.
     * @param config ParamConfig containing the parameter configuration
     * @note Thread-safe: Call only from main thread during plugin
     * initialization
     * @note Generates a stable CLAP ID from the string ID using FNV-1a hash
     */
    void registerParam(const ParamConfig& config);

    /**
     * @brief Get a lightweight handle for real-time (audio thread) parameter
     * read-only access.
     * @param paramId The CLAP ID of the parameter
     * @return Reference to the parameter handle
     * @throws Assertion failure if parameter not found
     * @note The result of this function should always be cached; do not call it
     * every process()!
     */
    ParamHandle& getHandle(clap_id paramId);

    /**
     * @brief Get a lightweight handle for real-time parameter access.
     * @param stringId The string identifier used when registering the parameter
     * @return Reference to the parameter handle
     * @throws Assertion failure if parameter not found
     * @note The result of this function should always be cached; do not call it
     * every process()!
     */
    ParamHandle& getHandle(std::string_view stringId);

    /**
     * @brief Get full parameter information for UI and host interaction.
     * @param paramId The CLAP ID of the parameter
     * @return Reference to the parameter info object
     * @throws Assertion failure if parameter not found
     * @note Result of this function should always be cached.
     * You should not call this function from the audio thread, or use ParamInfo
     * in any real-time DSP code.
     */
    ParamInfo& getInfo(clap_id paramId);

    /**
     * @brief Get full parameter information for UI and host interaction.
     * @param stringId The string identifier used when registering the parameter
     * @return Reference to the parameter info object
     * @throws Assertion failure if parameter not found
     * @note Result of this function should always be cached.
     * You should not call this function from the audio thread, or use ParamInfo
     * in any real-time DSP code.
     */
    ParamInfo& getInfo(std::string_view stringId);

    /**
     * @brief Get a span of all registered parameters.
     * @return std::span containing all ParamInfo objects
     * @note The span size equals the number of registered parameters (both
     * internal and external)
     */
    std::span<ParamInfo> getAllParameters() const noexcept;

    /**
     * Get normalized value for parameter at index (DSP-safe).
     */
    [[nodiscard]] float getNormalizedAt(uint32_t index) const noexcept {
        float plain = values_[index].load(std::memory_order_relaxed);
        const auto& s = scale_info_[index];
        return s.scaling.toNormalized(plain, s.min, s.max);
    }

    /**
     * Convert normalized to plain value for parameter at index (DSP-safe).
     */
    [[nodiscard]] float fromNormalizedAt(uint32_t index, float norm) const noexcept {
        const auto& s = scale_info_[index];
        return s.scaling.fromNormalized(norm, s.min, s.max);
    }

    /**
     * Get raw values array pointer (DSP-safe bulk access).
     */
    [[nodiscard]] const std::atomic<float>* getValuesArray() const noexcept {
        return values_.get();
    }

    /**
     * Get scale info array pointer (DSP-safe bulk access).
     */
    [[nodiscard]] const ValueScaleInfo* getScaleInfoArray() const noexcept {
        return scale_info_.get();
    }

    /**
     * Get total parameter count.
     */
    [[nodiscard]] uint32_t getParamCount() const noexcept {
        return param_count_;
    }

    /**
     * @brief Process the CLAP events generated during process()
     * If you use the ParamsExtension, you MUST call this in your process()
     * function so that the extension can respond to parameter events and send
     * outgoing parameter changes to the host.
     * @param in The CLAP input event struct from process()
     * @param out the CLAP output event struct from process()
     */
    void processEvents(const clap_input_events_t* in, const clap_output_events_t* out);

    /**
     * @brief Request the host to rescan parameter info. This allows the host to know that parameter metadata,
     * e.g. display name, has changed.
     *
     * @param flags Bitmask of clap_param_rescan_flags indicating what changed
     */
    void rescan(clap_param_rescan_flags flags) noexcept;

    /**
     * @brief Save all parameter values to a JSON object.
     *
     * Saves parameters in a format that supports versioning and
     * forward/backward compatibility. Each parameter is saved with its ID,
     * value, and optionally its name for debugging.
     *
     * @param json The JSON object to save parameters into (typically
     * json["params"])
     * @return true on success, false on error
     * @note Call this from your StateExtension save callback
     */
    bool saveToJson(applause::json& json) noexcept;

    /**
     * @brief Load parameter values from a JSON object.
     *
     * Loads parameters from JSON, gracefully handling missing parameters
     * (useful when loading older states or states from different plugin
     * versions).
     *
     * @param json The JSON object containing parameter data
     * @return true on success, false on error
     * @note Call this from your StateExtension load callback
     */
    bool loadFromJson(const applause::json& json) noexcept;
};

// Inline implementations for JSON serialization
inline bool ParamsExtension::saveToJson(applause::json& json) noexcept {
    try {
        auto params = applause::json::array();

        // Save all parameter values (including internal ones)
        for (uint32_t i = 0; i < param_count_; ++i) {
            const auto& param_info = infos_[i];

            // Save each parameter as an object for better readability and
            // debugging
            applause::json param_obj;
            param_obj["id"] = param_info.clapId;
            param_obj["value"] = param_info.getValue();
            param_obj["name"] = param_info.name;  // Include name for readability and debugging

            params.push_back(param_obj);
        }

        json = params;

        LOG_DBG("Saved {} parameter values to JSON state", params.size());
        return true;
    } catch (const std::exception& e) {
        LOG_ERR("Failed to save parameters to JSON: {}", e.what());
        return false;
    } catch (...) {
        LOG_ERR("Failed to save parameters to JSON: unknown exception");
        return false;
    }
}

inline bool ParamsExtension::loadFromJson(const applause::json& json) noexcept {
    try {
        if (!json.is_array()) {
            LOG_WARN("Parameters JSON is not an array; skipping parameter load");
            return true;  // Not a fatal error — ignore unexpected shapes
        }

        [[maybe_unused]] size_t loaded_count = 0;
        [[maybe_unused]] size_t missing_count = 0;

        // Load each parameter (expects object entries: {"id", "value", ...})
        for (const auto& param_obj : json) {
            if (!param_obj.is_object()) {
                LOG_WARN("Skipping invalid parameter entry (expected object)");
                continue;
            }

            // Expected format: {"id": 123, "value": 0.5, "name": "..."}
            const clap_id param_id = param_obj.value("id", CLAP_INVALID_ID);
            const float value = param_obj.value("value", 0.0f);

            // Apply the value if we have this parameter
            auto it = clap_id_to_index_.find(param_id);
            if (it != clap_id_to_index_.end()) {
                uint32_t index = it->second;
                values_[index].store(value, std::memory_order_relaxed);

                // Notify UI of parameter change if message queue exists
                if (message_queue_) {
                    message_queue_->toUi().enqueue({ParamMessageQueue::PARAM_VALUE, param_id, value});
                }

                loaded_count++;
            } else {
                missing_count++;
                LOG_DBG("Parameter with ID {} not found in current plugin", param_id);
            }
        }

        LOG_DBG("Loaded {} parameters from JSON state ({} missing/removed)", loaded_count, missing_count);
        return true;
    } catch (const std::exception& e) {
        LOG_ERR("Failed to load parameters from JSON: {}", e.what());
        return false;
    } catch (...) {
        LOG_ERR("Failed to load parameters from JSON: unknown exception");
        return false;
    }
}
}  // namespace applause
