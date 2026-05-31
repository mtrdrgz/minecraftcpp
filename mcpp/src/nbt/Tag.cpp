#include "Tag.h"
#include <stdexcept>

namespace mc::nbt {

bool NbtCompound::has(std::string_view key) const {
    return tags.find(std::string(key)) != tags.end();
}
NbtTag* NbtCompound::get(std::string_view key) {
    auto it = tags.find(std::string(key));
    return it == tags.end() ? nullptr : &it->second;
}
const NbtTag* NbtCompound::get(std::string_view key) const {
    auto it = tags.find(std::string(key));
    return it == tags.end() ? nullptr : &it->second;
}
void NbtCompound::put(std::string key, NbtTag tag) {
    tags[std::move(key)] = std::move(tag);
}

#define GET_TYPED(name, cpptype, tagtype, field) \
    cpptype NbtCompound::name(std::string_view key, cpptype def) const { \
        auto* t = get(key); \
        if (!t) return def; \
        if (auto* p = t->as<tagtype>()) return (cpptype)*p; \
        return def; \
    }

GET_TYPED(getByte,   int8_t,  int8_t,  )
GET_TYPED(getShort,  int16_t, int16_t, )
GET_TYPED(getInt,    int32_t, int32_t, )
GET_TYPED(getLong,   int64_t, int64_t, )
GET_TYPED(getFloat,  float,   float,   )
GET_TYPED(getDouble, double,  double,  )

std::string NbtCompound::getString(std::string_view key, std::string_view def) const {
    auto* t = get(key);
    if (!t) return std::string(def);
    if (auto* p = t->as<std::string>()) return *p;
    return std::string(def);
}

const NbtCompound* NbtCompound::getCompound(std::string_view key) const {
    auto* t = get(key);
    if (!t) return nullptr;
    if (auto* p = t->as<std::shared_ptr<NbtCompound>>()) return p->get();
    return nullptr;
}
NbtCompound* NbtCompound::getCompound(std::string_view key) {
    auto* t = get(key);
    if (!t) return nullptr;
    if (auto* p = t->as<std::shared_ptr<NbtCompound>>()) return p->get();
    return nullptr;
}
const NbtList* NbtCompound::getList(std::string_view key) const {
    auto* t = get(key);
    if (!t) return nullptr;
    if (auto* p = t->as<std::shared_ptr<NbtList>>()) return p->get();
    return nullptr;
}
const NbtIntArray* NbtCompound::getIntArray(std::string_view key) const {
    auto* t = get(key);
    if (!t) return nullptr;
    return t->as<NbtIntArray>();
}
const NbtByteArray* NbtCompound::getByteArray(std::string_view key) const {
    auto* t = get(key);
    if (!t) return nullptr;
    return t->as<NbtByteArray>();
}
const NbtLongArray* NbtCompound::getLongArray(std::string_view key) const {
    auto* t = get(key);
    if (!t) return nullptr;
    return t->as<NbtLongArray>();
}

} // namespace mc::nbt
