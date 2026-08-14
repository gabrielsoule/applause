#include "ParamsExtension.h"

#include <clap/events.h>

#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

#include <applause/core/PluginBase.h>
#include <applause/util/DebugHelpers.h>

namespace applause {
float ParamsExtension::coerceValue(float value, const ParamInfo& info) noexcept {
    if (info.isEnumerated() && std::isnan(value)) value = info.defaultValue;
    value = std::clamp(value, info.minValue, info.maxValue);
    return info.stepped ? std::trunc(value) : value;
}

// This default converter function tries to fit the number into five digits,
// using no more than two digits of decimal precision (1/100ths).
std::string ParamsExtension::defaultValueToText(float value, const ParamInfo& info) {
    value = coerceValue(value, info);

    if (info.isEnumerated()) {
        const float offset = value - info.minValue;
        if (std::isfinite(offset) && offset >= 0.0f && std::trunc(offset) == offset &&
            static_cast<double>(offset) < static_cast<double>(info.choices().size()))
            return info.choices()[static_cast<std::size_t>(offset)];
        return std::to_string(value);
    }

    std::ostringstream stream;

    if (info.stepped) {
        stream << static_cast<int>(value);
    } else {
        const int max_chars = 5;
        const int max_decimals = 2;

        float abs_value = std::abs(value);
        int integer_digits = (abs_value >= 1.0f) ? static_cast<int>(std::log10(abs_value)) + 1 : 1;
        int sign_chars = (value < 0) ? 1 : 0;
        int used_chars = integer_digits + sign_chars;

        if (used_chars >= max_chars) {
            stream << std::fixed << std::setprecision(0) << value;
        } else {
            int available_for_decimal = max_chars - used_chars;
            if (available_for_decimal >= 2) {
                int decimals_to_show = std::min(max_decimals, available_for_decimal - 1);
                stream << std::fixed << std::setprecision(decimals_to_show) << value;
            } else {
                stream << std::fixed << std::setprecision(0) << value;
            }
        }
    }

    // Append unit if present
    if (!info.unit.empty()) {
        stream << "" << info.unit;
    }

    return stream.str();
}

std::optional<float> ParamsExtension::defaultTextToValue(const std::string& text, const ParamInfo& info) {
    if (info.isEnumerated()) {
        const auto find_choice = [&info](std::string_view candidate) -> std::optional<float> {
            const auto choices = info.choices();
            for (std::size_t index = 0; index < choices.size(); ++index) {
                const float value = info.minValue + static_cast<float>(index);
                if (info.valueToText(value) == candidate) return value;
            }

            const auto canonical_choice = std::find(choices.begin(), choices.end(), candidate);
            if (canonical_choice != choices.end())
                return info.minValue + static_cast<float>(canonical_choice - choices.begin());
            return std::nullopt;
        };

        if (auto value = find_choice(text)) return value;

        std::string_view choice_text = text;
        while (!choice_text.empty() && std::isspace(static_cast<unsigned char>(choice_text.front())))
            choice_text.remove_prefix(1);
        while (!choice_text.empty() && std::isspace(static_cast<unsigned char>(choice_text.back())))
            choice_text.remove_suffix(1);
        if (choice_text.size() != text.size()) {
            if (auto value = find_choice(choice_text)) return value;
        }
    }

    if (text.empty()) {
        return std::nullopt;
    }

    const char* str = text.c_str();

    // Skip leading non-numeric characters (except sign and decimal point)
    while (*str && !std::isdigit(*str) && *str != '-' && *str != '+' && *str != '.') {
        ++str;
    }

    if (!*str) {
        return std::nullopt;
    }

    char* endptr;
    errno = 0;
    float value = std::strtof(str, &endptr);

    if (endptr == str || errno == ERANGE) {
        return std::nullopt;
    }

    return coerceValue(value, info);
}

float ParamInfo::getValue() const noexcept { return handle_->getValue(); }

void ParamInfo::setValueNotifyingHost(float value) const noexcept {
    value = ParamsExtension::coerceValue(value, *this);

    // Queue message to audio thread if message queue exists (GUI is present)
    if (registry_->message_queue_) {
        registry_->message_queue_->toAudio().enqueue({ParamMessageQueue::PARAM_VALUE, clapId, value});
    }

    handle_->value_->store(value, std::memory_order_relaxed);

    // Immediately notify all UI listeners for instant synchronization
    on_value_changed(value);

    // Request flush from host if available
    if (registry_->host_params_ && registry_->host_params_->request_flush) {
        registry_->host_params_->request_flush(registry_->host_);
    }
}

void ParamInfo::setValueSilently(float value) const noexcept {
    handle_->value_->store(ParamsExtension::coerceValue(value, *this), std::memory_order_relaxed);
}

void ParamInfo::beginGesture() const noexcept {
    // Queue message to audio thread if message queue exists (GUI is present)
    if (registry_->message_queue_) {
        registry_->message_queue_->toAudio().enqueue({ParamMessageQueue::BEGIN_GESTURE, clapId, 0.0f});
    }

    // Request flush from host if available
    if (registry_->host_params_ && registry_->host_params_->request_flush) {
        registry_->host_params_->request_flush(registry_->host_);
    }
}

void ParamInfo::endGesture() const noexcept {
    // Queue message to audio thread if message queue exists (GUI is present)
    if (registry_->message_queue_) {
        registry_->message_queue_->toAudio().enqueue({ParamMessageQueue::END_GESTURE, clapId, 0.0f});
    }

    // Request flush from host if available
    if (registry_->host_params_ && registry_->host_params_->request_flush) {
        registry_->host_params_->request_flush(registry_->host_);
    }
}

std::string ParamInfo::valueToText(float value) const noexcept {
    return value_to_text_(ParamsExtension::coerceValue(value, *this), *this);
}

std::optional<float> ParamInfo::textToValue(const std::string& text) const noexcept {
    auto value = text_to_value_(text, *this);
    if (value) value = ParamsExtension::coerceValue(*value, *this);
    return value;
}

uint32_t ParamsExtension::clap_params_count(const clap_plugin_t* plugin) noexcept {
    auto* ext = PluginBase::findExtension<ParamsExtension>(plugin);
    if (!ext) return 0;

    return ext->external_param_count_;
}

bool ParamsExtension::clap_params_get_info(const clap_plugin_t* plugin, uint32_t param_index,
                                           clap_param_info_t* param_info) noexcept {
    auto* ext = PluginBase::findExtension<ParamsExtension>(plugin);
    if (!ext || !param_info) return false;

    if (param_index >= ext->external_to_internal_index_.size()) return false;

    // Map external index to internal index
    uint32_t internal_index = ext->external_to_internal_index_[param_index];
    const ParamInfo& info = ext->infos_[internal_index];

    param_info->id = info.clapId;
    param_info->cookie = &ext->handles_[internal_index];

    std::strncpy(param_info->name, info.name.c_str(), CLAP_NAME_SIZE - 1);
    param_info->name[CLAP_NAME_SIZE - 1] = '\0';

    std::strncpy(param_info->module, info.module.c_str(), CLAP_PATH_SIZE - 1);
    param_info->module[CLAP_PATH_SIZE - 1] = '\0';

    param_info->min_value = info.minValue;
    param_info->max_value = info.maxValue;
    param_info->default_value = info.defaultValue;

    param_info->flags = 0;
    if (info.stepped) param_info->flags |= CLAP_PARAM_IS_STEPPED;
    if (info.isEnumerated()) param_info->flags |= CLAP_PARAM_IS_ENUM;
    if (info.hidden) param_info->flags |= CLAP_PARAM_IS_HIDDEN;

    // All parameters are automatable by default
    param_info->flags |= CLAP_PARAM_IS_AUTOMATABLE;

    return true;
}

bool ParamsExtension::clap_params_get_value(const clap_plugin_t* plugin, clap_id param_id, double* out_value) noexcept {
    auto* ext = PluginBase::findExtension<ParamsExtension>(plugin);
    if (!ext || !out_value) return false;

    // Look up parameter by CLAP ID
    auto it = ext->clap_id_to_index_.find(param_id);
    if (it == ext->clap_id_to_index_.end()) return false;

    uint32_t index = it->second;
    *out_value = static_cast<double>(ext->values_[index].load(std::memory_order_relaxed));

    return true;
}

bool ParamsExtension::clap_params_value_to_text(const clap_plugin_t* plugin, clap_id param_id, double value,
                                                char* out_buffer, uint32_t out_buffer_capacity) noexcept {
    auto* ext = PluginBase::findExtension<ParamsExtension>(plugin);
    if (!ext || !out_buffer || out_buffer_capacity == 0) return false;

    auto it = ext->clap_id_to_index_.find(param_id);
    if (it == ext->clap_id_to_index_.end()) return false;

    const ParamInfo& info = ext->infos_[it->second];
    std::string text = info.valueToText(static_cast<float>(value));
    std::strncpy(out_buffer, text.c_str(), out_buffer_capacity - 1);
    out_buffer[out_buffer_capacity - 1] = '\0';

    return true;
}

bool ParamsExtension::clap_params_text_to_value(const clap_plugin_t* plugin, clap_id param_id,
                                                const char* param_value_text, double* out_value) noexcept {
    auto* ext = PluginBase::findExtension<ParamsExtension>(plugin);
    if (!ext || !param_value_text || !out_value) return false;

    auto it = ext->clap_id_to_index_.find(param_id);
    if (it == ext->clap_id_to_index_.end()) return false;

    const ParamInfo& info = ext->infos_[it->second];
    auto parsed = info.textToValue(param_value_text);
    if (!parsed.has_value()) return false;

    *out_value = static_cast<double>(parsed.value());
    return true;
}

void ParamsExtension::clap_params_flush(const clap_plugin_t* plugin, const clap_input_events_t* in,
                                        const clap_output_events_t* out) noexcept {
    auto* ext = PluginBase::findExtension<ParamsExtension>(plugin);
    if (!ext) return;

    ext->flush(in, out);
}

ParamsExtension::ParamsExtension(uint32_t max_params) : message_queue_(nullptr) {
    clap_struct_ = {};
    clap_struct_.count = clap_params_count;
    clap_struct_.get_info = clap_params_get_info;
    clap_struct_.get_value = clap_params_get_value;
    clap_struct_.value_to_text = clap_params_value_to_text;
    clap_struct_.text_to_value = clap_params_text_to_value;
    clap_struct_.flush = clap_params_flush;

    max_params_ = max_params;
    values_ = std::make_unique<std::atomic<float>[]>(max_params_);
    handles_ = std::make_unique<ParamHandle[]>(max_params_);
    infos_ = std::make_unique<ParamInfo[]>(max_params_);
    scale_info_ = std::make_unique<ValueScaleInfo[]>(max_params_);
}

void ParamsExtension::onHostReady() noexcept {
    host_params_ = nullptr;
    if (host_) {
        host_params_ = static_cast<const clap_host_params_t*>(host_->get_extension(host_, CLAP_EXT_PARAMS));

        if (host_params_) {
            LOG_INFO("Successfully obtained host params extension");
        } else {
            LOG_WARN("Host does not provide params extension");
        }
    }
}

void ParamsExtension::registerParam(const ParamConfig& config) {
    ASSERT(param_count_ < max_params_,
           "Too many parameters registered! Allocate more through the "
           "ParamRegistry constructor.");

    if (!config.choices.empty()) {
        if (config.choices.size() < 2)
            throw std::invalid_argument("Choice parameter '" + config.string_id + "' must have at least two choices");

        constexpr double max_exact_float_integer = 1u << std::numeric_limits<float>::digits;
        const double max_choice = static_cast<double>(config.min_value) +
                                  static_cast<double>(config.choices.size() - 1);
        if (!std::isfinite(config.min_value) || std::trunc(config.min_value) != config.min_value ||
            static_cast<double>(config.choices.size() - 1) > max_exact_float_integer ||
            std::abs(static_cast<double>(config.min_value)) > max_exact_float_integer ||
            std::abs(max_choice) > max_exact_float_integer)
            throw std::invalid_argument("Choice parameter '" + config.string_id + "' has an invalid range");

        for (auto choice = config.choices.begin(); choice != config.choices.end(); ++choice) {
            const bool invalid_whitespace =
                choice->empty() || std::isspace(static_cast<unsigned char>(choice->front())) ||
                std::isspace(static_cast<unsigned char>(choice->back()));
            if (invalid_whitespace || choice->find('\0') != std::string::npos)
                throw std::invalid_argument("Choice parameter '" + config.string_id + "' has an invalid label");
            if (std::find(config.choices.begin(), choice, *choice) != choice)
                throw std::invalid_argument("Choice parameter '" + config.string_id + "' has duplicate labels");
        }

        if (!std::isfinite(config.default_value) || config.default_value < config.min_value ||
            static_cast<double>(config.default_value) > max_choice)
            throw std::invalid_argument("Choice parameter '" + config.string_id + "' has an invalid default value");
    }

    const float min_value = config.min_value;
    const float max_value = config.choices.empty()
                                ? config.max_value
                                : static_cast<float>(static_cast<double>(config.min_value) +
                                                     static_cast<double>(config.choices.size() - 1));

    ASSERT(config.default_value >= min_value && config.default_value <= max_value,
           "Default value not between min and max value!");

    // Create ParamInfo from ParamConfig
    ParamInfo info;
    info.name = config.name.empty() ? config.string_id : config.name;
    info.module = config.module;
    info.shortName = config.short_name;
    info.unit = config.unit;
    info.minValue = min_value;
    info.maxValue = max_value;
    info.stepped = config.is_stepped || !config.choices.empty();
    info.defaultValue = coerceValue(config.default_value, info);
    info.internal = config.is_internal;
    info.hidden = config.is_hidden;
    info.scaling_ = config.choices.empty() ? config.scaling : ValueScaling::linear();
    info.polyphonic = config.is_polyphonic;
    info.stringId = config.string_id;
    info.choice_names_ = config.choices;
    info.value_to_text_ = config.value_to_text ? config.value_to_text : defaultValueToText;
    info.text_to_value_ = config.text_to_value ? config.text_to_value : defaultTextToValue;

    std::string id;

    if (config.module.empty()) {
        id = config.string_id;
    } else {
        id = config.module + "/" + config.string_id;
    }

    // Generate stable CLAP ID using FNV-1a hash
    {
        const char* str = id.c_str();
        uint32_t hash = 2166136261u;
        while (*str) {
            hash ^= static_cast<uint32_t>(*str++);
            hash *= 16777619u;
        }
        info.clapId = hash;
    }

    // Check for hash collisions
    if (clap_id_to_index_.count(info.clapId) > 0) {
        // Simple collision resolution: linear probing
        uint32_t originalId = info.clapId;
        uint32_t probe = 1;
        while (clap_id_to_index_.count(info.clapId) > 0) {
            info.clapId = originalId + probe;
            probe++;
        }
    }

    uint32_t index = param_count_;
    std::atomic<float> provisional_value{info.defaultValue};
    ParamHandle provisional_handle;
    provisional_handle.value_ = &provisional_value;
    info.handle_ = &provisional_handle;
    info.registry_ = this;

    if (info.isEnumerated()) {
        std::vector<std::string> rendered_choices;
        rendered_choices.reserve(info.choices().size());
        for (std::size_t choice_index = 0; choice_index < info.choices().size(); ++choice_index) {
            const float value = info.minValue + static_cast<float>(choice_index);
            auto label = info.value_to_text_(coerceValue(value, info), info);
            const bool invalid_whitespace =
                label.empty() || std::isspace(static_cast<unsigned char>(label.front())) ||
                std::isspace(static_cast<unsigned char>(label.back()));
            if (invalid_whitespace || label.find('\0') != std::string::npos)
                throw std::invalid_argument("Choice parameter '" + config.string_id +
                                            "' has a converter that produces an invalid label");
            if (std::find(rendered_choices.begin(), rendered_choices.end(), label) != rendered_choices.end())
                throw std::invalid_argument("Choice parameter '" + config.string_id +
                                            "' has a converter that produces duplicate labels");
            const auto canonical = std::find(info.choices().begin(), info.choices().end(), label);
            if (canonical != info.choices().end() &&
                static_cast<std::size_t>(canonical - info.choices().begin()) != choice_index)
                throw std::invalid_argument("Choice parameter '" + config.string_id +
                                            "' has ambiguous canonical and displayed labels");
            rendered_choices.push_back(std::move(label));
        }
    }

    // Store in dense arrays using current count as index
    values_[index].store(info.defaultValue);
    handles_[index].value_ = &values_[index];
    info.handle_ = &handles_[index];
    infos_[index] = info;
    infos_[index].registry_ = this;

    // Populate DSP-safe scale info array
    scale_info_[index] = ValueScaleInfo{
        info.minValue,
        info.maxValue,
        info.scaling_
    };

    // Update lookup structures
    clap_id_to_index_[info.clapId] = index;
    string_id_to_index_[config.string_id] = index;

    // Track external parameters for host enumeration
    if (!info.internal) {
        external_to_internal_index_.push_back(index);
        external_param_count_++;
    }

    param_count_++;
}

ParamHandle& ParamsExtension::getHandle(clap_id paramId) {
    auto it = clap_id_to_index_.find(paramId);
    ASSERT(it != clap_id_to_index_.end(), "Parameter with CLAP ID {} not found", paramId);
    uint32_t index = it->second;
    ASSERT(handles_[index].value_ != nullptr, "Parameter handle not initialized for ID: {}", paramId);
    return handles_[index];
}

ParamHandle& ParamsExtension::getHandle(std::string_view stringId) {
    auto it = string_id_to_index_.find(std::string(stringId));
    ASSERT(it != string_id_to_index_.end(), "Parameter with string ID '{}' not found", stringId);
    uint32_t index = it->second;
    ASSERT(handles_[index].value_ != nullptr, "Parameter handle not initialized for string ID: {}", stringId);
    return handles_[index];
}

ParamInfo& ParamsExtension::getInfo(clap_id paramId) {
    auto it = clap_id_to_index_.find(paramId);
    ASSERT(it != clap_id_to_index_.end(), "Parameter with CLAP ID {} not found", paramId);
    return infos_[it->second];
}

ParamInfo& ParamsExtension::getInfo(std::string_view stringId) {
    auto it = string_id_to_index_.find(std::string(stringId));
    ASSERT(it != string_id_to_index_.end(), "Parameter with string ID '{}' not found", stringId);
    return infos_[it->second];
}

std::span<ParamInfo> ParamsExtension::getAllParameters() const noexcept {
    return std::span<ParamInfo>(infos_.get(), param_count_);
}

void ParamsExtension::processEvents(const clap_input_events_t* in, const clap_output_events_t* out) {
    if (in) {
        uint32_t event_count = in->size(in);

        for (uint32_t i = 0; i < event_count; ++i) {
            const clap_event_header_t* header = in->get(in, i);
            if (!header) continue;

            // Check if this is a parameter value event
            if (header->type == CLAP_EVENT_PARAM_VALUE) {
                const clap_event_param_value_t* param_event = reinterpret_cast<const clap_event_param_value_t*>(header);
                const clap_id param_id = param_event->param_id;

                // Fast path: the host echoes back the handle pointer we issued as the
                // cookie in get_info(); hosts that don't support cookies send nullptr.
                uint32_t index;
                if (param_event->cookie) [[likely]] {
                    index = static_cast<uint32_t>(static_cast<ParamHandle*>(param_event->cookie) - handles_.get());
                    ASSERT(index < param_count_ && infos_[index].clapId == param_id,
                           "Cookie for parameter ID {} does not match registry", param_id);
                } else {
                    auto it = clap_id_to_index_.find(param_id);
                    ASSERT(it != clap_id_to_index_.end(), "Parameter ID {} not found in registry", param_id);
                    index = it->second;
                }
                const auto& param_info = infos_[index];
                ASSERT(!param_info.internal,
                       "Received parameter event for internal parameter '{}' "
                       "(ID {})",
                       param_info.name, param_id);

                const float new_value = coerceValue(static_cast<float>(param_event->value), param_info);

                values_[index].store(new_value, std::memory_order_relaxed);

                // Notify UI of parameter change from host
                if (message_queue_)
                    message_queue_->toUi().enqueue({ParamMessageQueue::PARAM_VALUE, param_id, new_value});
            }
        }
    }

    // Process any queued parameter changes from UI to host
    if (message_queue_ && out) {
        ParamMessageQueue::Message message{};

        while (message_queue_->toAudio().try_dequeue(message)) {
            switch (message.type) {
                case ParamMessageQueue::PARAM_VALUE: {
                    clap_event_param_value_t event = {};
                    event.header.size = sizeof(clap_event_param_value_t);
                    event.header.time = 0;  // Process at start of buffer
                    event.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
                    event.header.type = CLAP_EVENT_PARAM_VALUE;
                    event.header.flags = 0;

                    event.param_id = message.paramId;
                    event.cookie = nullptr;  // hosts route plugin output by param_id, not cookie
                    event.note_id = -1;     // Wildcard - not note-specific
                    event.port_index = -1;  // Wildcard
                    event.channel = -1;     // Wildcard
                    event.key = -1;         // Wildcard
                    event.value = message.value;

                    out->try_push(out, &event.header);
                    break;
                }

                case ParamMessageQueue::BEGIN_GESTURE: {
                    clap_event_param_gesture_t event = {};
                    event.header.size = sizeof(clap_event_param_gesture_t);
                    event.header.time = 0;  // Process at start of buffer
                    event.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
                    event.header.type = CLAP_EVENT_PARAM_GESTURE_BEGIN;
                    event.header.flags = 0;

                    event.param_id = message.paramId;

                    out->try_push(out, &event.header);
                    break;
                }

                case ParamMessageQueue::END_GESTURE: {
                    clap_event_param_gesture_t event = {};
                    event.header.size = sizeof(clap_event_param_gesture_t);
                    event.header.time = 0;  // Process at start of buffer
                    event.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
                    event.header.type = CLAP_EVENT_PARAM_GESTURE_END;
                    event.header.flags = 0;

                    event.param_id = message.paramId;

                    out->try_push(out, &event.header);
                    break;
                }
            }
        }
    }
}

void ParamsExtension::flush(const clap_input_events_t* in, const clap_output_events_t* out) noexcept {
    processEvents(in, out);
}
void ParamsExtension::rescan(clap_param_rescan_flags flags) noexcept {
    if (host_params_ && host_params_->rescan) {
        host_params_->rescan(host_, flags);
    }
}

}  // namespace applause
