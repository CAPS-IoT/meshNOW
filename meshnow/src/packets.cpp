#include "packets.hpp"

#include <bitsery/adapter/buffer.h>
#include <bitsery/bitsery.h>
#include <bitsery/deserializer.h>
#include <bitsery/ext/std_optional.h>
#include <bitsery/ext/std_variant.h>
#include <bitsery/serializer.h>
#include <bitsery/traits/array.h>
#include <bitsery/traits/vector.h>
#include <esp_now.h>

#include <optional>
#include <variant>
#include <vector>

#include "constants.hpp"

namespace {

using OutputAdapter = bitsery::OutputBufferAdapter<meshnow::util::Buffer>;
using InputAdapter = bitsery::InputBufferAdapter<meshnow::util::Buffer>;

}  // namespace

// SPECIAL SERIALIZATION EXTENSIONS //

namespace bitsery {

namespace ext {

// Efficiently serialize data as we already encode the size of the data using other attributes
class DataFragmentExtension {
   public:
    DataFragmentExtension(uint16_t frag_num, uint16_t total_size) : frag_num(frag_num), total_size(total_size) {}

    template <typename Ser, typename Func>
    void serialize(Ser& ser, const meshnow::util::Buffer& data, Func&&) const {
        validateWrite(data);

        for (const auto& byte : data) {
            ser.value1b(byte);
        }
    }

    inline void validateWrite(const meshnow::util::Buffer& data) const {
        assert(data.size() <= meshnow::MAX_FRAG_PAYLOAD_SIZE && "Data too large");
        //        assert(frag_num <= 6 && "Fragment number too large");
        //        assert(total_size <= 1500 && "Total size too large");
        assert(frag_num < (total_size + meshnow::MAX_FRAG_PAYLOAD_SIZE - 1) / meshnow::MAX_FRAG_PAYLOAD_SIZE &&
               "Fragment number and total size mismatch");
    }

    template <typename Des, typename Func>
    void deserialize(Des& des, meshnow::util::Buffer& data, Func&&) const {
        validateRead(des.adapter());

        // if last fragment, only read the remaining size, otherwise read MAX_FRAG_PAYLOAD_SIZE
        uint16_t to_read = total_size - (frag_num * meshnow::MAX_FRAG_PAYLOAD_SIZE);
        if (to_read > meshnow::MAX_FRAG_PAYLOAD_SIZE) {
            to_read = meshnow::MAX_FRAG_PAYLOAD_SIZE;
        }

        data.resize(to_read);
        for (auto& byte : data) {
            des.value1b(byte);
        }
    }

    template <typename Reader>
    inline void validateRead(Reader& r) const {
        if (frag_num <= 6) return;
        if (total_size <= 1500) return;
        if ((frag_num + 1) * meshnow::MAX_FRAG_PAYLOAD_SIZE <= total_size) return;

        r.error(bitsery::ReaderError::InvalidData);
    }

   private:
    uint16_t frag_num;
    uint16_t total_size;
};

}  // namespace ext

namespace traits {

template <>
struct ExtensionTraits<ext::DataFragmentExtension, meshnow::util::Buffer> {
    using TValue = meshnow::util::Buffer::value_type;
    static constexpr bool SupportValueOverload = false;
    static constexpr bool SupportObjectOverload = false;
    static constexpr bool SupportLambdaOverload = true;
};

}  // namespace traits

}  // namespace bitsery

// PAYLOAD SERIALIZERS //

namespace meshnow::packets {

template <typename S>
static void serialize(S& s, Status& p) {
    s.value1b(p.state);
    // TODO optimize with custom extension
    s.ext(p.root, bitsery::ext::StdOptional{});
}

template <typename S>
static void serialize(S&, SearchProbe&) {
    // no data
}

template <typename S>
static void serialize(S&, SearchReply&) {
    // no data
}

template <typename S>
static void serialize(S&, ConnectRequest&) {
    // no data
}

template <typename S>
static void serialize(S& s, ConnectOk& p) {
    s.object(p.root);
}

template <typename S>
static void serialize(S& s, RoutingTableAdd& p) {
    s.object(p.entry);
}

template <typename S>
static void serialize(S& s, RoutingTableRemove& p) {
    s.object(p.entry);
}

template <typename S>
static void serialize(S&, RootUnreachable&) {
    // no data
}

template <typename S>
static void serialize(S& s, RootReachable& p) {
    s.object(p.root);
}

template <typename S>
static void serialize(S& s, DataFragment& p) {
    s.value4b(p.frag_id);
    s.value2b(p.options.packed);
    s.ext(p.data, bitsery::ext::DataFragmentExtension{p.options.unpacked.frag_num, p.options.unpacked.total_size},
          [] {});
}

template <typename S>
static void serialize(S& s, CustomData& p) {
    s.container1b(p.data, MAX_CUSTOM_PAYLOAD_SIZE);
}

}  // namespace meshnow::packets

// HELPER SERIALIZERS //

namespace {

// full packet also includes magic bytes but is not exposed publicly
struct FullPacket {
    std::array<uint8_t, 3> magic;
    meshnow::packets::Packet packet;
};

template <typename S>
void serialize(S& s, FullPacket& fp) {
    s.container1b(fp.magic);
    s.object(fp.packet);
}

}  // namespace

namespace meshnow::util {

template <typename S>
static void serialize(S& s, MacAddr& m) {
    s.container1b(m.addr);
}

}  // namespace meshnow::util

namespace meshnow::packets {

template <typename S>
static void serialize(S& s, Packet& p) {
    s.value4b(p.id);
    s.object(p.from);
    s.object(p.to);
    s.ext(p.payload, bitsery::ext::StdVariant{[](S& s, auto& p) { s.object(p); }});
}

}  // namespace meshnow::packets

// PACKET SERIALIZATION //

namespace meshnow::packets {

util::Buffer serialize(const Packet& packet) {
    util::Buffer buffer;

    FullPacket fp{MAGIC, packet};

    // write
    auto written_size = bitsery::quickSerialization(OutputAdapter{buffer}, fp);

    // shrink and return
    buffer.resize(written_size);
    buffer.shrink_to_fit();
    return buffer;
}

std::optional<Packet> deserialize(const util::Buffer& buffer) {
    FullPacket fp;

    // read
    auto [error, red_everything] = bitsery::quickDeserialization(InputAdapter{buffer.begin(), buffer.size()}, fp);

    // check for errors
    if (error == bitsery::ReaderError::NoError && red_everything && fp.magic == MAGIC) {
        return fp.packet;
    } else {
        return std::nullopt;
    }
}

}  // namespace meshnow::packets