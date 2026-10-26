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
    handle_->value_->store(value, std::memory_order_relaxed);

    // Immediately notify all UI listeners for instant synchronization
    on_value_changed(value);

    notifyHost(ParamMessageQueue::PARAM_VALUE, value);
}

void ParamInfo::setValueSilently(float value) const noexcept {
    handle_->value_->store(ParamsExtension::coerceValue(value, *this), std::memory_order_relaxed);
}

void ParamInfo::beginGesture() const noexcept { notifyHost(ParamMessageQueue::BEGIN_GESTURE, 0.0f); }

void ParamInfo::endGesture() const noexcept { notifyHost(ParamMessageQueue::END_GESTURE, 0.0f); }

void ParamInfo::notifyHost(ParamMessageQueue::MessageType type, float value) const noexcept {
    // Only parameters the DAW can see have anything to tell it. Skipping the rest also means a queued message for a
    // dynamic parameter never outlives its slot binding, which is what keeps the audio thread off reused entries.
    if (!registry_->message_queue_ || !isAutomatable()) return;
    registry_->message_queue_->toAudio().enqueue({type, index_, value});

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

    return ext->external_param_count_ + ext->slot_count_;
}

bool ParamsExtension::clap_params_get_info(const clap_plugin_t* plugin, uint32_t param_index,
                                           clap_param_info_t* param_info) noexcept {
    auto* ext = PluginBase::findExtension<ParamsExtension>(plugin);
    if (!ext || !param_info) return false;

    std::string name;
    std::string module;
    if (param_index < ext->external_param_count_) {
        // Map external index to internal index
        uint32_t internal_index = ext->external_to_internal_index_[param_index];
        const ParamInfo& info = ext->infos_[internal_index];

        param_info->id = info.clapId;
        param_info->cookie = &ext->handles_[internal_index];
        name = info.name;
        module = info.module;

        param_info->min_value = info.minValue;
        param_info->max_value = info.maxValue;
        param_info->default_value = info.defaultValue;

        param_info->flags = 0;
        if (info.stepped) param_info->flags |= CLAP_PARAM_IS_STEPPED;
        if (info.isEnumerated()) param_info->flags |= CLAP_PARAM_IS_ENUM;
        if (info.hidden) param_info->flags |= CLAP_PARAM_IS_HIDDEN;

        // All parameters are automatable by default
        param_info->flags |= CLAP_PARAM_IS_AUTOMATABLE;
    } else if (param_index < ext->external_param_count_ + ext->slot_count_) {
        // Automation slots follow the static parameters. They stay continuous 0..1 and are never hidden: hosts can't
        // pick up range or flag changes without a restart, and a hidden VST3 parameter can't be automated.
        const uint32_t slot = param_index - ext->external_param_count_;
        const uint32_t param = ext->slot_param_[slot].load(std::memory_order_relaxed);

        param_info->id = ext->slot_ids_[slot];
        param_info->cookie = &ext->handles_[ext->capacity_ + slot];
        name = param == kNone ? "Unused " + std::to_string(slot + 1) : ext->infos_[param].name;
        if (param != kNone) module = ext->infos_[param].module;

        param_info->min_value = 0.0;
        param_info->max_value = 1.0;
        param_info->default_value = 0.0;
        param_info->flags = CLAP_PARAM_IS_AUTOMATABLE;
    } else {
        return false;
    }

    std::strncpy(param_info->name, name.c_str(), CLAP_NAME_SIZE - 1);
    param_info->name[CLAP_NAME_SIZE - 1] = '\0';

    std::strncpy(param_info->module, module.c_str(), CLAP_PATH_SIZE - 1);
    param_info->module[CLAP_PATH_SIZE - 1] = '\0';

    return true;
}

bool ParamsExtension::clap_params_get_value(const clap_plugin_t* plugin, clap_id param_id, double* out_value) noexcept {
    auto* ext = PluginBase::findExtension<ParamsExtension>(plugin);
    if (!ext || !out_value) return false;

    // Look up parameter by CLAP ID
    auto it = ext->clap_id_to_index_.find(param_id);
    if (it == ext->clap_id_to_index_.end()) return false;

    const uint32_t param = ext->hostParam(it->second);
    *out_value = param == kNone ? 0.0 : ext->toHost(param, ext->infos_[param].getValue());
    return true;
}

bool ParamsExtension::clap_params_value_to_text(const clap_plugin_t* plugin, clap_id param_id, double value,
                                                char* out_buffer, uint32_t out_buffer_capacity) noexcept {
    auto* ext = PluginBase::findExtension<ParamsExtension>(plugin);
    if (!ext || !out_buffer || out_buffer_capacity == 0) return false;

    auto it = ext->clap_id_to_index_.find(param_id);
    if (it == ext->clap_id_to_index_.end()) return false;

    const uint32_t param = ext->hostParam(it->second);
    const std::string text = param == kNone ? "-" : ext->infos_[param].valueToText(ext->fromHost(param, value));
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

    const uint32_t param = ext->hostParam(it->second);
    if (param == kNone) return false;

    auto parsed = ext->infos_[param].textToValue(param_value_text);
    if (!parsed.has_value()) return false;

    *out_value = ext->toHost(param, parsed.value());
    return true;
}

void ParamsExtension::clap_params_flush(const clap_plugin_t* plugin, const clap_input_events_t* in,
                                        const clap_output_events_t* out) noexcept {
    auto* ext = PluginBase::findExtension<ParamsExtension>(plugin);
    if (!ext) return;

    ext->flush(in, out);
}

ParamsExtension::ParamsExtension(uint32_t max_params, uint32_t max_dynamic_params, uint32_t host_slots) :
    message_queue_(nullptr) {
    clap_struct_ = {};
    clap_struct_.count = clap_params_count;
    clap_struct_.get_info = clap_params_get_info;
    clap_struct_.get_value = clap_params_get_value;
    clap_struct_.value_to_text = clap_params_value_to_text;
    clap_struct_.text_to_value = clap_params_text_to_value;
    clap_struct_.flush = clap_params_flush;

    dynamic_count_ = max_dynamic_params;
    capacity_ = max_dynamic_params + max_params;
    slot_count_ = host_slots;
    values_ = std::make_unique<std::atomic<float>[]>(capacity_);
    handles_ = std::make_unique<ParamHandle[]>(capacity_ + slot_count_);  // Slot cookies follow the parameters
    infos_ = std::make_unique<ParamInfo[]>(capacity_);
    scale_info_ = std::make_unique<ValueScaleInfo[]>(capacity_);
    for (uint32_t i = 0; i < capacity_; ++i) handles_[i].value_ = &values_[i];

    // The dynamic pool comes first. Its entries are internal, and an empty string id marks one as unused.
    param_slot_ = std::make_unique<std::atomic<uint32_t>[]>(dynamic_count_);
    unbound_at_.assign(dynamic_count_, 0);
    for (uint32_t i = 0; i < dynamic_count_; ++i) {
        ParamInfo info;
        info.internal = true;
        info.value_to_text_ = defaultValueToText;
        info.text_to_value_ = defaultTextToValue;
        storeParam(i, std::move(info));
        param_slot_[i].store(kNone, std::memory_order_relaxed);
        free_params_.push_back(i);
    }
    param_count_ = dynamic_count_;

    slot_param_ = std::make_unique<std::atomic<uint32_t>[]>(slot_count_);
    slot_owner_.resize(slot_count_);
    for (uint32_t slot = 0; slot < slot_count_; ++slot) {
        slot_param_[slot].store(kNone, std::memory_order_relaxed);
        slot_ids_.push_back(uniqueClapId("applause/automation_slot/" + std::to_string(slot)));
        clap_id_to_index_[slot_ids_.back()] = capacity_ + slot;
        free_slots_.push_back(slot);
    }
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

ParamInfo ParamsExtension::makeInfo(const ParamConfig& config) {
    ASSERT(!config.string_id.empty(), "Parameters need a string id");

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
    info.modDestination = config.is_mod_destination;
    info.stringId = config.string_id;
    info.choice_names_ = config.choices;
    info.value_to_text_ = config.value_to_text ? config.value_to_text : defaultValueToText;
    info.text_to_value_ = config.text_to_value ? config.text_to_value : defaultTextToValue;

    // Rendering choice labels can call back into the parameter, so the converters get a provisional handle.
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

    return info;
}

void ParamsExtension::storeParam(uint32_t index, ParamInfo info) {
    info.index_ = index;
    info.handle_ = &handles_[index];
    info.registry_ = this;
    values_[index].store(info.defaultValue, std::memory_order_relaxed);
    scale_info_[index] = ValueScaleInfo{info.minValue, info.maxValue, info.scaling_};
    infos_[index] = std::move(info);
}

clap_id ParamsExtension::uniqueClapId(std::string_view text) const {
    // Stable FNV-1a hash, with linear probing past ids that are already taken
    uint32_t hash = 2166136261u;
    for (const char c : text) {
        hash ^= static_cast<uint32_t>(c);  // Keeps the original id for non-ASCII bytes, so saved automation still finds it
        hash *= 16777619u;
    }
    while (clap_id_to_index_.contains(hash)) ++hash;
    return hash;
}

void ParamsExtension::registerParam(const ParamConfig& config) {
    ASSERT(param_count_ < capacity_,
           "Too many parameters registered! Allocate more through the ParamsExtension constructor.");

    ParamInfo info = makeInfo(config);
    info.clapId = uniqueClapId(config.module.empty() ? config.string_id : config.module + "/" + config.string_id);

    const uint32_t index = param_count_++;
    clap_id_to_index_[info.clapId] = index;
    string_id_to_index_[config.string_id] = index;
    if (!info.internal) {
        external_to_internal_index_.push_back(index);
        external_param_count_++;
    }
    storeParam(index, std::move(info));
}

bool ParamsExtension::activateDynamicParam(const ParamConfig& config) {
    if (string_id_to_index_.contains(config.string_id)) return false;

    // An entry that held a slot is only reused once a processEvents() call that started after it left the slot has
    // finished, so the audio thread can't still be converting a slot value with its old range.
    const uint32_t seen = seen_generation_.load(std::memory_order_acquire);
    const auto entry =
        std::ranges::find_if(free_params_, [&](uint32_t index) { return !active_ || unbound_at_[index] <= seen; });
    if (entry == free_params_.end()) return false;

    ParamInfo info = makeInfo(config);
    info.internal = true;

    const uint32_t index = *entry;
    free_params_.erase(entry);
    string_id_to_index_[config.string_id] = index;
    storeParam(index, std::move(info));
    on_dynamic_param_changed(index, true);
    return true;
}

void ParamsExtension::deactivateDynamicParam(std::string_view string_id) {
    const auto it = string_id_to_index_.find(std::string(string_id));
    if (it == string_id_to_index_.end()) return;
    const uint32_t index = it->second;
    ASSERT(index < dynamic_count_, "'{}' is a static parameter and can't be deactivated", string_id);
    if (index >= dynamic_count_) return;

    unbindSlot(index);
    on_dynamic_param_changed(index, false);
    // Erase by key: a signal handler may have activated parameters and invalidated the iterator
    string_id_to_index_.erase(infos_[index].stringId);
    infos_[index].stringId.clear();
    free_params_.push_back(index);
}

bool ParamInfo::isAutomatable() const noexcept {
    if (index_ >= registry_->dynamic_count_) return !internal;
    return registry_->param_slot_[index_].load(std::memory_order_relaxed) != ParamsExtension::kNone;
}

bool ParamInfo::setAutomatable(bool automatable) const { return registry_->setAutomatable(index_, automatable); }

bool ParamsExtension::setAutomatable(uint32_t index, bool automatable) {
    if (index >= dynamic_count_ || infos_[index].stringId.empty()) return false;
    if (!automatable) {
        unbindSlot(index);
        return true;
    }
    if (infos_[index].isAutomatable()) return true;

    // Prefer the slot this parameter had last, so the DAW automation recorded on it drives the parameter again
    const auto& string_id = infos_[index].stringId;
    auto slot = std::ranges::find_if(free_slots_, [&](uint32_t candidate) { return slot_owner_[candidate] == string_id; });
    if (slot == free_slots_.end()) slot = free_slots_.begin();
    if (slot == free_slots_.end()) return false;
    bindSlot(index, *slot);
    return true;
}

void ParamsExtension::bindSlot(uint32_t index, uint32_t slot) {
    std::erase(free_slots_, slot);
    slot_owner_[slot] = infos_[index].stringId;
    // The release stores publish the parameter's info to the audio thread along with the binding
    param_slot_[index].store(slot, std::memory_order_release);
    slot_param_[slot].store(index, std::memory_order_release);
    requestRescan();
}

void ParamsExtension::unbindSlot(uint32_t index) {
    const uint32_t slot = param_slot_[index].load(std::memory_order_relaxed);
    if (slot == kNone) return;

    slot_param_[slot].store(kNone, std::memory_order_relaxed);
    param_slot_[index].store(kNone, std::memory_order_relaxed);
    unbound_at_[index] = generation_.fetch_add(1, std::memory_order_release) + 1;  // publishes the unbinding above
    free_slots_.push_back(slot);
    requestRescan();

    // A sleeping plugin may not process for a while, and a flush lets the audio side confirm the unbinding
    if (active_ && host_params_ && host_params_->request_flush) host_params_->request_flush(host_);
}

void ParamsExtension::requestRescan() noexcept {
    // Sent from onMainThread(), never inline: hosts mishandle renames sent from inside setState() or activate()
    if (std::exchange(pending_rescan_, true)) return;
    if (host_ && host_->request_callback) host_->request_callback(host_);
}

void ParamsExtension::onMainThread() noexcept {
    if (std::exchange(pending_rescan_, false))
        rescan(CLAP_PARAM_RESCAN_INFO | CLAP_PARAM_RESCAN_VALUES | CLAP_PARAM_RESCAN_TEXT);
}

float ParamsExtension::fromHost(uint32_t param, double value) const noexcept {
    const ParamInfo& info = infos_[param];
    if (param >= dynamic_count_) return coerceValue(static_cast<float>(value), info);

    const float n = std::fmin(std::fmax(static_cast<float>(value), 0.0f), 1.0f);  // NaN becomes 0
    if (!info.stepped) return coerceValue(info.fromNormalized(n), info);

    // Equal bins, so every step gets the same share of an automation lane
    const float low = std::trunc(info.minValue);
    const float steps = std::trunc(info.maxValue) - low + 1.0f;
    return coerceValue(low + std::fmin(std::floor(n * steps), steps - 1.0f), info);
}

double ParamsExtension::toHost(uint32_t param, float plain) const noexcept {
    const ParamInfo& info = infos_[param];
    if (param >= dynamic_count_) return plain;

    float n;
    if (info.stepped) {
        const float low = std::trunc(info.minValue);
        const float span = std::trunc(info.maxValue) - low;
        n = span > 0.0f ? (std::trunc(plain) - low) / span : 0.0f;
    } else {
        n = info.toNormalized(plain);
    }
    return std::fmin(std::fmax(n, 0.0f), 1.0f);
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
    ASSERT(it != clap_id_to_index_.end() && it->second < capacity_, "Parameter with CLAP ID {} not found", paramId);
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
    // Slot bindings as of this call; reported back at the end so the main thread knows when it can reuse entries
    const uint32_t generation = generation_.load(std::memory_order_acquire);

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
                    ASSERT((index < param_count_ && infos_[index].clapId == param_id) ||
                               (index >= capacity_ && slot_ids_[index - capacity_] == param_id),
                           "Cookie for parameter ID {} does not match registry", param_id);
                } else {
                    auto it = clap_id_to_index_.find(param_id);
                    ASSERT(it != clap_id_to_index_.end(), "Parameter ID {} not found in registry", param_id);
                    index = it->second;
                }

                // An automation slot forwards to the dynamic parameter bound to it, if any
                if (index >= capacity_) {
                    index = slot_param_[index - capacity_].load(std::memory_order_acquire);
                    if (index == kNone) continue;
                } else {
                    ASSERT(!infos_[index].internal, "Received parameter event for internal parameter '{}' (ID {})",
                           infos_[index].name, param_id);
                }

                const float new_value = fromHost(index, param_event->value);
                values_[index].store(new_value, std::memory_order_relaxed);

                // Notify UI of parameter change from host
                if (message_queue_)
                    message_queue_->toUi().enqueue({ParamMessageQueue::PARAM_VALUE, index, new_value});
            }
        }
    }

    // Process any queued parameter changes from UI to host
    if (message_queue_ && out) {
        ParamMessageQueue::Message message{};

        while (message_queue_->toAudio().try_dequeue(message)) {
            // Static parameters reach the host directly; dynamic ones only through the slot they're bound to, and
            // their info may only be read while that binding holds
            clap_id host_id;
            if (message.index < dynamic_count_) {
                const uint32_t slot = param_slot_[message.index].load(std::memory_order_acquire);
                if (slot == kNone) continue;
                host_id = slot_ids_[slot];
            } else {
                const ParamInfo& info = infos_[message.index];
                if (info.internal) continue;  // The host doesn't know this parameter
                host_id = info.clapId;
            }

            switch (message.type) {
                case ParamMessageQueue::PARAM_VALUE: {
                    clap_event_param_value_t event = {};
                    event.header.size = sizeof(clap_event_param_value_t);
                    event.header.time = 0;  // Process at start of buffer
                    event.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
                    event.header.type = CLAP_EVENT_PARAM_VALUE;
                    event.header.flags = 0;

                    event.param_id = host_id;
                    event.cookie = nullptr;  // hosts route plugin output by param_id, not cookie
                    event.note_id = -1;     // Wildcard - not note-specific
                    event.port_index = -1;  // Wildcard
                    event.channel = -1;     // Wildcard
                    event.key = -1;         // Wildcard
                    event.value = toHost(message.index, message.value);

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

                    event.param_id = host_id;

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

                    event.param_id = host_id;

                    out->try_push(out, &event.header);
                    break;
                }
            }
        }
    }

    seen_generation_.store(generation, std::memory_order_release);
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
