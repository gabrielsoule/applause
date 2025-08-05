//
// Created by Gabriel Soule on 7/21/25.
//

#include "ParamsExtension.h"
#include "applause/core/PluginBase.h"
#include <cstring>
#include <cerrno>
#include <cctype>
#include <clap/events.h>
#include <sstream>
#include <iomanip>
#include <cmath>

#include "applause/util/DebugHelpers.h"

namespace applause
{
    // Default converter functions
    std::string ParamsExtension::defaultValueToText(float value, const ParamInfo& info)
    {
        std::ostringstream stream;

        if (info.stepped)
        {
            stream << static_cast<int>(value);
        }
        else
        {
            // Adaptive precision algorithm (from ParamValueTextBox)
            const int maxChars = 5;
            const int maxDecimals = 2;

            float absValue = std::abs(value);
            int integerDigits = (absValue >= 1.0f) ? static_cast<int>(std::log10(absValue)) + 1 : 1;
            int signChars = (value < 0) ? 1 : 0;
            int usedChars = integerDigits + signChars;

            if (usedChars >= maxChars)
            {
                stream << std::fixed << std::setprecision(0) << value;
            }
            else
            {
                int availableForDecimal = maxChars - usedChars;
                if (availableForDecimal >= 2)
                {
                    int decimalsToShow = std::min(maxDecimals, availableForDecimal - 1);
                    stream << std::fixed << std::setprecision(decimalsToShow) << value;
                }
                else
                {
                    stream << std::fixed << std::setprecision(0) << value;
                }
            }
        }

        // Append unit if present
        if (!info.unit.empty())
        {
            stream << " " << info.unit;
        }

        return stream.str();
    }

    std::optional<float> ParamsExtension::defaultTextToValue(const std::string& text, const ParamInfo& info)
    {
        if (text.empty())
        {
            return std::nullopt;
        }

        const char* str = text.c_str();

        // Skip leading non-numeric characters (except sign and decimal point)
        while (*str && !std::isdigit(*str) && *str != '-' && *str != '+' && *str != '.')
        {
            ++str;
        }

        if (!*str)
        {
            return std::nullopt;
        }

        // Use strtof for efficient parsing
        char* endptr;
        errno = 0;  // Clear errno before conversion
        float value = std::strtof(str, &endptr);

        // Check if any conversion happened
        if (endptr == str || errno == ERANGE)
        {
            return std::nullopt;
        }

        // Clamp to parameter range
        value = std::clamp(value, info.minValue, info.maxValue);

        // For stepped parameters, truncate to integer
        if (info.stepped)
        {
            value = static_cast<float>(static_cast<int>(value));
        }

        return value;
    }

    // ParamInfo implementation
    float ParamInfo::getValue() const noexcept
    {
        return handle_->getValue();
    }

    void ParamInfo::setValueNotifyingHost(float value) const noexcept
    {
        // Queue message to audio thread if message queue exists (GUI is present)
        if (registry_->message_queue_)
        {
            registry_->message_queue_->toAudio().enqueue({ParamMessageQueue::PARAM_VALUE, clapId, value});
        }

        handle_->value_->store(std::clamp(value, minValue, maxValue), std::memory_order_relaxed);

        // Immediately notify all UI listeners for instant synchronization
        on_value_changed(value);

        // Request flush from host if available
        if (registry_->host_params_ && registry_->host_params_->request_flush)
        {
            registry_->host_params_->request_flush(registry_->host_);
        }
    }

    void ParamInfo::setValueSilently(float value) const noexcept
    {
        handle_->value_->store(std::clamp(value, minValue, maxValue), std::memory_order_relaxed);
    }

    void ParamInfo::beginGesture() const noexcept
    {
        // Queue message to audio thread if message queue exists (GUI is present)
        if (registry_->message_queue_)
        {
            registry_->message_queue_->toAudio().enqueue({ParamMessageQueue::BEGIN_GESTURE, clapId, 0.0f});
        }

        // Request flush from host if available
        if (registry_->host_params_ && registry_->host_params_->request_flush)
        {
            registry_->host_params_->request_flush(registry_->host_);
        }
    }

    void ParamInfo::endGesture() const noexcept
    {
        // Queue message to audio thread if message queue exists (GUI is present)
        if (registry_->message_queue_)
        {
            registry_->message_queue_->toAudio().enqueue({ParamMessageQueue::END_GESTURE, clapId, 0.0f});
        }

        // Request flush from host if available
        if (registry_->host_params_ && registry_->host_params_->request_flush)
        {
            registry_->host_params_->request_flush(registry_->host_);
        }
    }

    std::string ParamInfo::valueToText(float value) const noexcept
    {
        return value_to_text_(value, *this);
    }

    std::optional<float> ParamInfo::textToValue(const std::string& text) const noexcept
    {
        return text_to_value_(text, *this);
    }

    uint32_t ParamsExtension::clap_params_count(const clap_plugin_t* plugin) noexcept
    {
        auto* ext = PluginBase::findExtension<ParamsExtension>(plugin);
        if (!ext) return 0;

        return ext->external_param_count_;
    }

    bool ParamsExtension::clap_params_get_info(const clap_plugin_t* plugin,
                                               uint32_t param_index,
                                               clap_param_info_t* param_info) noexcept
    {
        auto* ext = PluginBase::findExtension<ParamsExtension>(plugin);
        if (!ext || !param_info) return false;

        // Check if index is valid
        if (param_index >= ext->external_to_internal_index_.size())
            return false;

        // Map external index to internal index
        uint32_t internal_index = ext->external_to_internal_index_[param_index];
        const ParamInfo& info = ext->infos_[internal_index];

        // Fill clap_param_info_t structure
        param_info->id = info.clapId;
        param_info->cookie = nullptr; // We don't use cookies

        // Copy name (ensure null termination)
        std::strncpy(param_info->name, info.name.c_str(), CLAP_NAME_SIZE - 1);
        param_info->name[CLAP_NAME_SIZE - 1] = '\0';

        // Module path - we don't use modules yet
        param_info->module[0] = '\0';

        // Set value ranges
        param_info->min_value = info.minValue;
        param_info->max_value = info.maxValue;
        param_info->default_value = info.defaultValue;

        // Set flags
        param_info->flags = 0;
        if (info.stepped)
            param_info->flags |= CLAP_PARAM_IS_STEPPED;

        // All parameters are automatable by default
        param_info->flags |= CLAP_PARAM_IS_AUTOMATABLE;

        return true;
    }

    bool ParamsExtension::clap_params_get_value(const clap_plugin_t* plugin,
                                                clap_id param_id,
                                                double* out_value) noexcept
    {
        auto* ext = PluginBase::findExtension<ParamsExtension>(plugin);
        if (!ext || !out_value) return false;

        // Look up parameter by CLAP ID
        auto it = ext->clap_id_to_index_.find(param_id);
        if (it == ext->clap_id_to_index_.end())
            return false;

        uint32_t index = it->second;
        *out_value = static_cast<double>(ext->values_[index].load(std::memory_order_relaxed));

        return true;
    }

    bool ParamsExtension::clap_params_value_to_text(const clap_plugin_t* plugin,
                                                    clap_id param_id,
                                                    double value,
                                                    char* out_buffer,
                                                    uint32_t out_buffer_capacity) noexcept
    {
        auto* ext = PluginBase::findExtension<ParamsExtension>(plugin);
        if (!ext || !out_buffer || out_buffer_capacity == 0) return false;

        // Look up parameter info
        auto it = ext->clap_id_to_index_.find(param_id);
        if (it == ext->clap_id_to_index_.end())
            return false;

        const ParamInfo& info = ext->infos_[it->second];

        // Use the converter to format the value
        std::string text = info.valueToText(static_cast<float>(value));

        // Copy to output buffer
        std::strncpy(out_buffer, text.c_str(), out_buffer_capacity - 1);
        out_buffer[out_buffer_capacity - 1] = '\0';

        return true;
    }

    bool ParamsExtension::clap_params_text_to_value(const clap_plugin_t* plugin,
                                                    clap_id param_id,
                                                    const char* param_value_text,
                                                    double* out_value) noexcept
    {
        auto* ext = PluginBase::findExtension<ParamsExtension>(plugin);
        if (!ext || !param_value_text || !out_value) return false;

        // Look up parameter info
        auto it = ext->clap_id_to_index_.find(param_id);
        if (it == ext->clap_id_to_index_.end())
            return false;

        const ParamInfo& info = ext->infos_[it->second];

        // Use the converter to parse the text
        auto parsed = info.textToValue(param_value_text);
        if (!parsed.has_value())
            return false;

        *out_value = static_cast<double>(parsed.value());
        return true;
    }

    void ParamsExtension::clap_params_flush(const clap_plugin_t* plugin,
                                            const clap_input_events_t* in,
                                            const clap_output_events_t* out) noexcept
    {
        auto* ext = PluginBase::findExtension<ParamsExtension>(plugin);
        if (!ext) return;

        ext->flush(in, out);
    }

    ParamsExtension::ParamsExtension(const clap_host_t* host, uint32_t max_params)
        : host_(host)
    {
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

        // Initialize other members
        message_queue_ = nullptr;

        // Get the params extension from the host
        if (host_)
        {
            host_params_ = static_cast<const clap_host_params_t*>(
                host_->get_extension(host_, CLAP_EXT_PARAMS)
            );

            if (host_params_)
            {
                LOG_INFO("Successfully obtained host params extension");
            }
            else
            {
                LOG_WARN("Host does not provide params extension");
            }
        }
        else
        {
            host_params_ = nullptr;
        }
    }

    void ParamsExtension::registerParam(const ParamConfig& config)
    {
        ASSERT(param_count_ < max_params_,
               "Too many parameters registered! Allocate more through the ParamRegistry constructor.");

        ASSERT(config.default_value >= config.min_value && config.default_value <= config.max_value,
               "Default value not between min and max value!");

        // Create ParamInfo from ParamConfig
        ParamInfo info;
        info.name = config.name.empty() ? config.string_id : config.name;
        info.shortName = config.short_name;
        info.unit = config.unit;
        info.minValue = config.min_value;
        info.maxValue = config.max_value;
        info.defaultValue = config.default_value;
        info.stepped = config.is_stepped;
        info.internal = config.is_internal;

        // Generate stable CLAP ID using FNV-1a hash
        {
            const char* str = config.string_id.c_str();
            uint32_t hash = 2166136261u;
            while (*str)
            {
                hash ^= static_cast<uint32_t>(*str++);
                hash *= 16777619u;
            }
            info.clapId = hash;
        }

        // Check for hash collisions
        if (clap_id_to_index_.count(info.clapId) > 0)
        {
            // Simple collision resolution: linear probing
            uint32_t originalId = info.clapId;
            uint32_t probe = 1;
            while (clap_id_to_index_.count(info.clapId) > 0)
            {
                info.clapId = originalId + probe;
                probe++;
            }
        }

        // Store in dense arrays using current count as index
        uint32_t index = param_count_;
        values_[index].store(info.defaultValue);
        handles_[index].value_ = &values_[index];
        info.handle_ = &handles_[index];
        infos_[index] = info;
        infos_[index].registry_ = this;

        // Use custom converters if provided, otherwise use defaults
        infos_[index].value_to_text_ = config.value_to_text ? config.value_to_text : defaultValueToText;
        infos_[index].text_to_value_ = config.text_to_value ? config.text_to_value : defaultTextToValue;

        // Update lookup structures
        clap_id_to_index_[info.clapId] = index;
        string_id_to_index_[config.string_id] = index;

        // Track external parameters for host enumeration
        if (!info.internal)
        {
            external_to_internal_index_.push_back(index);
            external_param_count_++;
        }

        param_count_++;


        LOG_INFO("Registered parameter {} with CLAP ID {} at index {}",
                 config.string_id, info.clapId, index);
    }

    ParamHandle& ParamsExtension::getHandle(clap_id paramId)
    {
        auto it = clap_id_to_index_.find(paramId);
        ASSERT(it != clap_id_to_index_.end(), "Parameter with CLAP ID " + std::to_string(paramId) + " not found");
        uint32_t index = it->second;
        ASSERT(handles_[index].value_ != nullptr,
               "Parameter handle not initialized for ID: " + std::to_string(paramId));
        return handles_[index];
    }

    ParamHandle& ParamsExtension::getHandle(std::string_view stringId)
    {
        // Note: std::unordered_map doesn't directly support string_view lookup in C++17
        // We need to convert to std::string for the lookup
        auto it = string_id_to_index_.find(std::string(stringId));
        ASSERT(it != string_id_to_index_.end(), "Parameter with string ID '" + std::string(stringId) + "' not found");
        uint32_t index = it->second;
        ASSERT(handles_[index].value_ != nullptr,
               "Parameter handle not initialized for string ID: " + std::string(stringId));
        return handles_[index];
    }

    ParamInfo& ParamsExtension::getInfo(clap_id paramId)
    {
        auto it = clap_id_to_index_.find(paramId);
        ASSERT(it != clap_id_to_index_.end(), "Parameter with CLAP ID " + std::to_string(paramId) + " not found");
        return infos_[it->second];
    }

    ParamInfo& ParamsExtension::getInfo(std::string_view stringId)
    {
        // Note: std::unordered_map doesn't directly support string_view lookup in C++17
        // We need to convert to std::string for the lookup
        auto it = string_id_to_index_.find(std::string(stringId));
        ASSERT(it != string_id_to_index_.end(), "Parameter with string ID '" + std::string(stringId) + "' not found");
        return infos_[it->second];
    }

    std::span<ParamInfo> ParamsExtension::getAllParameters() const noexcept
    {
        return std::span<ParamInfo>(infos_.get(), param_count_);
    }

    void ParamsExtension::processEvents(const clap_input_events_t* in, const clap_output_events_t* out)
    {
        if (in)
        {
            uint32_t event_count = in->size(in);

            for (uint32_t i = 0; i < event_count; ++i)
            {
                const clap_event_header_t* header = in->get(in, i);
                if (!header) continue;

                // Check if this is a parameter value event
                if (header->type == CLAP_EVENT_PARAM_VALUE)
                {
                    const clap_event_param_value_t* param_event =
                        reinterpret_cast<const clap_event_param_value_t*>(header);
                    const clap_id param_id = param_event->param_id;
                    // Look up the parameter
                    auto it = clap_id_to_index_.find(param_id);
                    ASSERT(it != clap_id_to_index_.end(),
                           "Parameter ID " + std::to_string(param_id) + " not found in registry");

                    uint32_t index = it->second;
                    const auto& param_info = infos_[index];
                    ASSERT(!param_info.internal,
                           "Received parameter event for internal parameter '" + param_info.name + "' (ID " + std::
                           to_string(param_id) +
                           ")");

                    float new_value = static_cast<float>(param_event->value);
                    ASSERT(new_value >= param_info.minValue && new_value <= param_info.maxValue,
                           "Parameter value " + std::to_string(new_value) + " out of range [" +
                           std::to_string(param_info.minValue) + ", " + std::to_string(param_info.maxValue) +
                           "] for parameter '" +
                           param_info.name + "'");

                    if (param_info.stepped)
                    {
                        new_value = static_cast<float>(static_cast<int>(new_value));
                    }

                    // Update the atomic value
                    values_[index].store(new_value, std::memory_order_relaxed);

                    // Notify UI of parameter change from host
                    if (message_queue_) message_queue_->toUi().enqueue({
                        ParamMessageQueue::PARAM_VALUE, param_id, new_value
                    });
                }
            }
        }

        // Process any queued parameter changes from UI to host
        if (message_queue_ && out)
        {
            ParamMessageQueue::Message message{};

            while (message_queue_->toAudio().try_dequeue(message))
            {
                switch (message.type)
                {
                case ParamMessageQueue::PARAM_VALUE:
                    {
                        clap_event_param_value_t event = {};
                        event.header.size = sizeof(clap_event_param_value_t);
                        event.header.time = 0; // Process at start of buffer
                        event.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
                        event.header.type = CLAP_EVENT_PARAM_VALUE;
                        event.header.flags = 0;

                        event.param_id = message.paramId;
                        event.cookie = nullptr;
                        event.note_id = -1; // Wildcard - not note-specific
                        event.port_index = -1; // Wildcard
                        event.channel = -1; // Wildcard
                        event.key = -1; // Wildcard
                        event.value = message.value;

                        out->try_push(out, &event.header);
                        break;
                    }

                case ParamMessageQueue::BEGIN_GESTURE:
                    {
                        clap_event_param_gesture_t event = {};
                        event.header.size = sizeof(clap_event_param_gesture_t);
                        event.header.time = 0; // Process at start of buffer
                        event.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
                        event.header.type = CLAP_EVENT_PARAM_GESTURE_BEGIN;
                        event.header.flags = 0;

                        event.param_id = message.paramId;

                        out->try_push(out, &event.header);
                        break;
                    }

                case ParamMessageQueue::END_GESTURE:
                    {
                        clap_event_param_gesture_t event = {};
                        event.header.size = sizeof(clap_event_param_gesture_t);
                        event.header.time = 0; // Process at start of buffer
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

    void ParamsExtension::flush(const clap_input_events_t* in,
                                const clap_output_events_t* out) noexcept
    {
        processEvents(in, out);
    }
} // namespace applause
