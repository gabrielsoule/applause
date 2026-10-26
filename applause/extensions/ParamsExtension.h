#pragma once

#include <clap/events.h>
#include <clap/ext/params.h>
#include <clap/host.h>

#include <algorithm>
#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <applause/core/Extension.h>
#include <applause/util/DebugHelpers.h>
#include <applause/util/Json.h>
#include <applause/util/ParamMessageQueue.h>
#include <applause/util/thirdparty/rocket.hpp>
#include <applause/util/ValueScaling.h>

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
    bool is_mod_destination = true;  /// Whether a modulation system (e.g. ModMatrix) may target this parameter.
                                     ///   Turn off for parameters such as UI state that should never be modulated.

    ValueScaling scaling = ValueScaling::linear();  /// Parameter scaling for normalization (default: linear)

    // Optional custom converters (default to nullptr)
    std::function<std::string(float value, const ParamInfo& info)> value_to_text;
    std::function<std::optional<float>(const std::string& text, const ParamInfo& info)> text_to_value;

    /**
     * Labels for a contiguous choice parameter.
     *
     * When non-empty, max_value is derived as
     * min_value + choices.size() - 1, and the parameter is automatically marked
     * stepped and enumerated with linear scaling. Choice labels must
     * be unique UTF-8 strings without leading or trailing whitespace and are
     * copied into ParamInfo during registration. A custom value_to_text
     * overrides the displayed labels.
     */
    std::vector<std::string> choices;
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
    uint32_t clapId = CLAP_INVALID_ID;

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
     * If true, the parameter is invisible to the DAW: it isn't listed to the host, so the host can't automate it.
     * Modulation systems such as ModMatrix can still target it. Dynamic parameters are always internal and reach
     * the DAW only through an automation slot (see setAutomatable()).
     */
    bool internal = false;

    /**
     * If true, the parameter is enumerated to the host but flagged as hidden from the host's UI, e.g. a deprecated
     * parameter kept for old sessions. clap-wrapper turns this into VST3's kIsHidden, which also means "not
     * automatable", so don't use it on parameters that must stay automatable.
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
     * If false, modulation systems such as ModMatrix don't offer this parameter as a destination.
     */
    bool modDestination = true;

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
     * The value is clamped and, for stepped parameters, truncated before using
     * the custom converter or default formatting. Enumerated parameters use
     * their registered choice label by default.
     *
     * @param value The parameter value to format
     * @return Formatted text representation including unit if applicable
     */
    std::string valueToText(float value) const noexcept;

    /**
     * Parse user input text to extract a numeric value for this parameter.
     *
     * Uses a custom converter if provided. Otherwise, enumerated parameters
     * first match their displayed or canonical choice labels, then fall back to
     * extracting the first number found in the text. The converted value is
     * automatically clamped to [minValue, maxValue]. For stepped parameters,
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

    /** Whether this parameter represents an enumerated list of choices. */
    [[nodiscard]] bool isEnumerated() const noexcept { return !choice_names_.empty(); }

    /** Canonical choice labels in numeric value order. Empty for non-enumerated parameters. */
    [[nodiscard]] std::span<const std::string> choices() const noexcept { return choice_names_; }

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

    /**
     * Whether the DAW can automate this parameter: true for static parameters that aren't internal, and for dynamic
     * parameters while they hold an automation slot. Main thread only.
     */
    [[nodiscard]] bool isAutomatable() const noexcept;

    /**
     * Expose a dynamic parameter to the DAW through a free automation slot, or take it back (for example from a
     * right-click "Automate" menu). The slot takes the parameter's name, and the DAW is told on the next main-thread
     * callback. Main thread only.
     *
     * @return false if no slot is free or this isn't an active dynamic parameter
     */
    bool setAutomatable(bool automatable) const;

private:
    void notifyHost(ParamMessageQueue::MessageType type, float value) const noexcept;

    ParamHandle* handle_ = nullptr;
    uint32_t index_ = 0;  // Position in the registry's arrays; identifies this parameter in UI/audio messages

    // A pointer to the parent registry, so that the host can be notified about
    // parameter changes. In the future, we can and should use an event system
    // for this.
    ParamsExtension* registry_ = nullptr;

    // Custom converters (following member naming convention)
    std::function<std::string(float value, const ParamInfo& info)> value_to_text_;
    std::function<std::optional<float>(const std::string& text, const ParamInfo& info)> text_to_value_;

    std::vector<std::string> choice_names_;

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
 * the audio thread. ApplauseEditor owns one and connects it to the
 * ParamsExtension it is given.
 *
 * Dynamic parameters (e.g. the controls of an effect the user adds at runtime)
 * come from a pool sized in the constructor: activateDynamicParam() and
 * deactivateDynamicParam(). They are internal, and the user can expose one at a
 * time to the DAW through a fixed set of automation slots, because DAWs freeze a
 * plugin's parameter list once it is loaded (ParamInfo::setAutomatable()).
 * Plugins using them must keep this order:
 *   1. Publish DSP or UI that reads a dynamic parameter only after
 *      activateDynamicParam() returns.
 *   2. Stop all DSP and UI use of a dynamic parameter before calling
 *      deactivateDynamicParam().
 *   3. In process(), call processEvents() first, then pick up the plugin's own
 *      DSP changes, then run modulation (e.g. ModMatrix::loadParamBaseValues()).
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

    static float coerceValue(float value, const ParamInfo& info) noexcept;

    void flush(const clap_input_events_t* in, const clap_output_events_t* out) noexcept;

    ParamInfo makeInfo(const ParamConfig& config);
    void storeParam(uint32_t index, ParamInfo info);
    clap_id uniqueClapId(std::string_view text) const;

    bool setAutomatable(uint32_t index, bool automatable);
    void bindSlot(uint32_t index, uint32_t slot);
    void unbindSlot(uint32_t index);
    void requestRescan() noexcept;

    // The parameter behind an index from clap_id_to_index_: itself, or what an automation slot is bound to (or kNone)
    uint32_t hostParam(uint32_t index) const noexcept {
        return index < capacity_ ? index : slot_param_[index - capacity_].load(std::memory_order_acquire);
    }
    // The host reaches dynamic parameters only through 0..1 automation slots; these map host values to and from
    // a parameter's own range
    float fromHost(uint32_t param, double value) const noexcept;
    double toHost(uint32_t param, float plain) const noexcept;

    void onActivate() noexcept override { active_ = true; }
    void onDeactivate() noexcept override { active_ = false; }
    void onMainThread() noexcept override;

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
    uint32_t capacity_ = 0;       // Dynamic pool plus room for static parameters
    uint32_t dynamic_count_ = 0;  // The dynamic pool occupies indices [0, dynamic_count_)
    uint32_t slot_count_ = 0;     // DAW automation slots; slot s uses handles_[capacity_ + s] as its cookie

    static constexpr uint32_t kNone = UINT32_MAX;

    // Read on the audio thread. A dynamic parameter's info is only rewritten while no slot points at it.
    std::unique_ptr<std::atomic<uint32_t>[]> slot_param_;  // slot -> dynamic parameter
    std::unique_ptr<std::atomic<uint32_t>[]> param_slot_;  // dynamic parameter -> slot
    std::vector<clap_id> slot_ids_;                         // fixed after construction
    std::atomic<uint32_t> generation_{0};       // bumped whenever a parameter leaves its slot
    std::atomic<uint32_t> seen_generation_{0};  // the generation the last finished processEvents() call started with

    // Main thread only
    std::vector<uint32_t> free_params_;    // unused dynamic entries, oldest released first
    std::vector<uint32_t> free_slots_;     // never-used slots first, then oldest released
    std::vector<std::string> slot_owner_;  // string id last bound to each slot
    std::vector<uint32_t> unbound_at_;     // generation at which each dynamic parameter last left a slot
    bool active_ = false;
    bool pending_rescan_ = false;

public:
    static constexpr const char* ID = CLAP_EXT_PARAMS;

    void onHostReady() noexcept override;

    /**
     * @brief Construct the parameters extension with space for up to @p max_params static parameters,
     * @p max_dynamic_params dynamic ones (see activateDynamicParam()), and @p host_slots DAW automation slots that
     * dynamic parameters can borrow (see ParamInfo::setAutomatable()).
     *
     * The DAW lists the slots after the static parameters, named "Unused N" while free. Their number and order
     * become part of saved DAW automation, so don't change them after shipping.
     */
    ParamsExtension(uint32_t max_params = 128, uint32_t max_dynamic_params = 0, uint32_t host_slots = 0);

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
     * @throws std::invalid_argument if enum-param choice metadata is invalid
     * @note Thread-safe: Call only from main thread during plugin
     * initialization
     * @note Generates a stable CLAP ID from the string ID using FNV-1a hash
     */
    void registerParam(const ParamConfig& config);

    /**
     * @brief Create a parameter at runtime, for example when the user adds an effect. Main thread only.
     *
     * The parameter takes an entry from the dynamic pool. It is internal (the DAW doesn't see it), stores real
     * values like any other parameter, and can be looked up by string id with getHandle() and getInfo(). Use string
     * ids that name the effect instance (e.g. "fx/7c1e/rate") so they stay stable when effects move.
     *
     * Only hand the parameter's handles to the audio thread after this returns.
     *
     * While the plugin is active, an entry freed by deactivating an automated parameter becomes reusable only after
     * the next processEvents() call. Size the pool for the most dynamic parameters alive at once plus host_slots, so
     * that replacing a whole set of parameters (e.g. loading a preset) always finds free entries.
     *
     * @return false if no entry is free or the string id is already in use
     * @throws std::invalid_argument if enum-param choice metadata is invalid
     */
    bool activateDynamicParam(const ParamConfig& config);

    /**
     * @brief Remove a parameter created with activateDynamicParam(). Main thread only.
     *
     * Stop all audio-thread and UI use of the parameter (handles, ParamInfo references) before calling this: its
     * entry is reused by later activations.
     */
    void deactivateDynamicParam(std::string_view string_id);

    /**
     * Fired on the main thread when a dynamic parameter is activated (true) or deactivated (false), with its index
     * into getAllParameters(). The parameter's info is still readable when this fires for a deactivation.
     */
    mutable rocket::signal<void(uint32_t index, bool active)> on_dynamic_param_changed;

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

        // Save all static parameter values (including internal ones)
        for (uint32_t i = dynamic_count_; i < param_count_; ++i) {
            const auto& param_info = infos_[i];

            // Save each parameter as an object for better readability and
            // debugging
            applause::json param_obj;
            param_obj["id"] = param_info.clapId;
            param_obj["value"] = param_info.getValue();
            param_obj["name"] = param_info.name;  // Include name for readability and debugging

            params.push_back(param_obj);
        }

        // Dynamic parameters are saved by string id. Loaders that predate them skip these entries (no "id").
        for (uint32_t i = 0; i < dynamic_count_; ++i) {
            const auto& param_info = infos_[i];
            if (param_info.stringId.empty()) continue;
            applause::json param_obj;
            param_obj["string_id"] = param_info.stringId;
            param_obj["value"] = param_info.getValue();
            if (const uint32_t slot = param_slot_[i].load(std::memory_order_relaxed); slot != kNone)
                param_obj["slot"] = slot;
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

        const auto apply = [this](uint32_t index, float value) {
            const auto& info = infos_[index];
            info.setValueSilently(value);
            info.on_value_changed(info.getValue());  // Loading runs on the main thread, so listeners are notified directly
        };
        std::vector<std::pair<uint32_t, uint32_t>> slot_bindings;  // dynamic parameter, slot

        // Load each parameter (expects object entries: {"id", "value", ...} or {"string_id", "value"})
        for (const auto& param_obj : json) {
            if (!param_obj.is_object()) {
                LOG_WARN("Skipping invalid parameter entry (expected object)");
                continue;
            }

            const float value = param_obj.value("value", 0.0f);

            // Dynamic parameters: the plugin activates them before loading
            if (param_obj.contains("string_id")) {
                const auto it = string_id_to_index_.find(param_obj.value("string_id", std::string{}));
                if (it != string_id_to_index_.end() && it->second < dynamic_count_) {
                    apply(it->second, value);
                    if (const uint32_t slot = param_obj.value("slot", kNone); slot != kNone)
                        slot_bindings.emplace_back(it->second, slot);
                    loaded_count++;
                } else {
                    missing_count++;
                }
                continue;
            }

            // Expected format: {"id": 123, "value": 0.5, "name": "..."}
            const clap_id param_id = param_obj.value("id", CLAP_INVALID_ID);

            // Apply the value if we have this parameter (slot ids map past capacity_ and carry no value)
            auto it = clap_id_to_index_.find(param_id);
            if (it != clap_id_to_index_.end() && it->second < capacity_) {
                apply(it->second, value);
                loaded_count++;
            } else {
                missing_count++;
                LOG_DBG("Parameter with ID {} not found in current plugin", param_id);
            }
        }

        // Loading replaces every slot binding, so the DAW's automation lanes find the same parameters again
        for (uint32_t i = 0; i < dynamic_count_; ++i) unbindSlot(i);
        for (const auto& [index, slot] : slot_bindings) {
            if (slot < slot_count_ && slot_param_[slot].load(std::memory_order_relaxed) == kNone &&
                !infos_[index].isAutomatable())
                bindSlot(index, slot);
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
