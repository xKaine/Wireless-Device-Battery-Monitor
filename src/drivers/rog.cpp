// ASUS ROG wireless keyboards, mice and the ROG Pelta / Delta II headsets (VID 0B05), including the ROG Omni Receiver (0B05:1ACE).
// Query: [report id][12][sub]   sub 01 = keyboard battery, sub 07 = mouse battery.
// Reply: [report id][12][sub]...  keyboard: [6] % (Falchion: [11]), [9] == 1 charging
//                                 mouse:    [5] level (x25 on older 0-4 gauges), [10] charging
// [1..2] == FF AA means the device behind the receiver did not answer (asleep / off).
// Offsets count the report id byte. Only query commands are sent.
// Protocol and model table from github.com/SteveWang92/peripheral-battery-tray (rog-battery,
// MIT, itself based on G-Helper) and github.com/1gcat/BatteryTool.
#include "drivers.h"

#include <set>

#include <cwctype>

namespace {

enum class Kind { Keyboard, Mouse };

struct Model {
    USHORT pid;
    const wchar_t* name;
    Kind kind;
    BYTE reportId;       // 0 = unnumbered
    int iface;           // USB interface (-1 = any vendor-page collection)
    const wchar_t* col;  // required collection tag in the path ("col02"), or nullptr
    int offset;          // battery byte
    int scale;           // battery multiplier
};

constexpr Kind KB = Kind::Keyboard, MS = Kind::Mouse;

const Model kModels[] = {
    // Keyboards (Omni = col02 report 02, receiver/wired = interface 1 unnumbered)
    { 0x1A85, L"ROG Azoth", KB, 2, 2, L"col02", 6, 1 },
    { 0x1A85, L"ROG Azoth", KB, 0, 1, nullptr, 6, 1 },
    { 0x1A83, L"ROG Azoth", KB, 0, 1, nullptr, 6, 1 },
    { 0x1B42, L"ROG Azoth Extreme", KB, 2, 2, L"col02", 6, 1 },
    { 0x1B3F, L"ROG Azoth Extreme", KB, 0, 1, nullptr, 6, 1 },
    { 0x1CF1, L"ROG Azoth Extreme SE", KB, 2, 2, L"col02", 6, 1 },
    { 0x1CEF, L"ROG Azoth Extreme SE", KB, 0, 1, nullptr, 6, 1 },
    { 0x1C25, L"ROG Azoth X", KB, 0, 1, nullptr, 6, 1 },
    { 0x1C24, L"ROG Azoth X", KB, 0, 1, nullptr, 6, 1 },
    { 0x193E, L"ROG Falchion", KB, 0, 1, nullptr, 11, 1 },
    { 0x193C, L"ROG Falchion", KB, 0, 1, nullptr, 11, 1 },
    { 0x1B04, L"ROG Falchion RX", KB, 0, -1, nullptr, 11, 1 },
    { 0x1B06, L"ROG Falchion RX Low Profile", KB, 2, 2, L"col02", 11, 1 },
    { 0x1AB0, L"ROG Strix Scope II 96", KB, 2, 2, L"col02", 6, 1 },
    { 0x1AAE, L"ROG Strix Scope II 96", KB, 0, 1, nullptr, 6, 1 },
    { 0x1B7A, L"ROG Strix Scope II 96 RX", KB, 2, 2, L"col02", 6, 1 },
    { 0x1B78, L"ROG Strix Scope II 96 RX", KB, 0, 1, nullptr, 6, 1 },
    { 0x1A07, L"ROG Strix Scope RX TKL", KB, 0, -1, nullptr, 6, 1 },
    // Mice (Omni = col03 report 03, receiver/wired = interface 0 unnumbered)
    { 0x18E5, L"ROG Chakram", MS, 0, 0, nullptr, 5, 25 },
    { 0x18E3, L"ROG Chakram", MS, 0, 0, nullptr, 5, 25 },
    { 0x1A1A, L"ROG Chakram X", MS, 0, 0, nullptr, 5, 1 },
    { 0x1A18, L"ROG Chakram X", MS, 0, 0, nullptr, 5, 1 },
    { 0x18A0, L"ROG Gladius II Wireless", MS, 0, 2, nullptr, 5, 25 },
    { 0x197F, L"ROG Gladius III Wireless", MS, 0, 0, nullptr, 5, 1 },
    { 0x197D, L"ROG Gladius III Wireless", MS, 0, 0, nullptr, 5, 1 },
    { 0x1A72, L"ROG Gladius III Aimpoint", MS, 3, 2, L"col03", 5, 1 },
    { 0x1A72, L"ROG Gladius III Aimpoint", MS, 0, 0, nullptr, 5, 1 },
    { 0x1A70, L"ROG Gladius III Aimpoint", MS, 0, 0, nullptr, 5, 1 },
    { 0x1B0C, L"ROG Gladius III Aimpoint EVA-02", MS, 0, 0, nullptr, 5, 1 },
    { 0x1B0A, L"ROG Gladius III Aimpoint EVA-02", MS, 0, 0, nullptr, 5, 1 },
    { 0x1A94, L"ROG Harpe Ace Aim Lab Edition", MS, 3, 2, L"col03", 5, 1 },
    { 0x1A94, L"ROG Harpe Ace Aim Lab Edition", MS, 0, 0, nullptr, 5, 1 },
    { 0x1A92, L"ROG Harpe Ace Aim Lab Edition", MS, 0, 0, nullptr, 5, 1 },
    { 0x1B68, L"ROG Harpe Ace Extreme", MS, 3, 2, L"col03", 5, 1 },
    { 0x1B69, L"ROG Harpe Ace Extreme", MS, 3, 2, L"col03", 5, 1 },
    { 0x1B67, L"ROG Harpe Ace Extreme", MS, 0, 0, nullptr, 5, 1 },
    { 0x1B65, L"ROG Harpe Ace Mini", MS, 3, 2, L"col03", 5, 1 },
    { 0x1B63, L"ROG Harpe Ace Mini", MS, 0, 0, nullptr, 5, 1 },
    { 0x1AD0, L"ROG Harpe II Ace", MS, 3, 2, L"col03", 5, 1 },
    { 0x1C69, L"ROG Harpe II Ace", MS, 0, 0, nullptr, 5, 1 },
    { 0x1960, L"ROG Keris Wireless", MS, 0, 0, nullptr, 5, 25 },
    { 0x195E, L"ROG Keris Wireless", MS, 0, 0, nullptr, 5, 25 },
    { 0x1A59, L"ROG Keris Wireless EVA Edition", MS, 0, 0, nullptr, 5, 25 },
    { 0x1A57, L"ROG Keris Wireless EVA Edition", MS, 0, 0, nullptr, 5, 25 },
    { 0x1A68, L"ROG Keris Wireless Aimpoint", MS, 3, 2, L"col03", 5, 1 },
    { 0x1A6A, L"ROG Keris Wireless Aimpoint", MS, 3, 2, L"col03", 5, 1 },
    { 0x1A68, L"ROG Keris Wireless Aimpoint", MS, 0, 0, nullptr, 5, 1 },
    { 0x1A66, L"ROG Keris Wireless Aimpoint", MS, 0, 0, nullptr, 5, 1 },
    { 0x1B1A, L"ROG Keris II Ace", MS, 3, 2, L"col03", 5, 1 },
    { 0x1B18, L"ROG Keris II Ace", MS, 3, 2, L"col03", 5, 1 },
    { 0x1B16, L"ROG Keris II Ace", MS, 0, 0, nullptr, 5, 1 },
    { 0x1C0E, L"ROG Keris II Origin", MS, 3, 2, L"col03", 5, 1 },
    { 0x1C0C, L"ROG Keris II Origin", MS, 0, 0, nullptr, 5, 1 },
    { 0x1D4E, L"ROG Keris II Origin KJP", MS, 3, 2, L"col03", 5, 1 },
    { 0x1D4C, L"ROG Keris II Origin KJP", MS, 0, 0, nullptr, 5, 1 },
    { 0x1908, L"ROG Pugio II", MS, 0, 0, nullptr, 5, 25 },
    { 0x1906, L"ROG Pugio II", MS, 0, 0, nullptr, 5, 25 },
    { 0x1979, L"ROG Spatha X", MS, 0, 0, nullptr, 5, 1 },
    { 0x1977, L"ROG Spatha X", MS, 0, 0, nullptr, 5, 1 },
    { 0x18B4, L"ROG Strix Carry", MS, 0, 1, nullptr, 7, 25 },
    { 0x1949, L"ROG Strix Impact II Wireless", MS, 0, 0, nullptr, 5, 25 },
    { 0x1947, L"ROG Strix Impact II Wireless", MS, 0, 0, nullptr, 5, 25 },
    { 0x1AD7, L"ROG Strix Impact III Wireless", MS, 3, 2, L"col03", 5, 1 },
};

bool PathHas(const std::wstring& path, const wchar_t* tag) {
    std::wstring p = path;
    for (auto& ch : p) ch = towlower(ch);
    return p.find(tag) != std::wstring::npos;
}

enum class Reply { Ok, Asleep, None };

Reply Query(HidDev& d, BYTE rid, BYTE cmd, BYTE sub, std::vector<BYTE>& reply) {
    d.Flush();
    if (!d.Write({ rid, cmd, sub })) return Reply::None;
    ULONGLONG end = GetTickCount64() + 500;
    std::vector<BYTE> r;
    while ((LONGLONG)end - (LONGLONG)GetTickCount64() > 0) {
        int left = (int)((LONGLONG)end - (LONGLONG)GetTickCount64());
        if (d.Read(r, left) < 11) return Reply::None;
        if (rid == 0) r.insert(r.begin(), 0x00);  // restore the report id slot so offsets match
        if (r.size() < 12) continue;
        if (r[1] == 0xFF && r[2] == 0xAA) return Reply::Asleep;
        if (r[1] == cmd && r[2] == sub) { reply = r; return Reply::Ok; }
    }
    return Reply::None;
}

// Reads one model's battery through an open collection. Returns false if it did not answer.
bool ReadModel(HidDev& d, const Model& m, DeviceStatus& s) {
    std::vector<BYTE> r;
    Reply res = Query(d, m.reportId, 0x12, m.kind == KB ? 0x01 : 0x07, r);
    if (res == Reply::None) return false;
    s.key = L"rog:" + Hex4(m.pid) + (m.kind == KB ? L":kb" : L":ms");
    s.name = m.name;
    s.kind = m.kind == KB ? DeviceKind::Keyboard : DeviceKind::Mouse;
    s.via = m.reportId ? L"ROG Omni Receiver" : L"2.4 GHz receiver / USB";
    if (res == Reply::Asleep) {
        s.asleep = true;
    } else if (m.kind == KB) {
        if (r[m.offset] <= 100) s.percent = r[m.offset];
        s.charging = r[9] == 1;
    } else {
        int v = r[m.offset] * m.scale;
        if (v > 0 && v <= 100) s.percent = v;
        else s.asleep = true;  // 0 means unavailable, not empty
        s.charging = r[10] != 0;
    }
    return true;
}

void AddOnce(PollContext& ctx, std::vector<std::wstring>& seen, const DeviceStatus& s) {
    for (auto& n : seen)
        if (n == s.name) return;
    seen.push_back(s.name);
    ctx.out.push_back(s);
}

// ROG Omni Receiver (0B05:1ACE): ask it which devices are paired ([01 A0 00] on col01 lists their
// product ids in 4-byte slots from byte 5), then read each one on its own channel of the receiver.
void PollOmni(PollContext& ctx, std::vector<std::wstring>& seen) {
    if (ctx.IsClaimed(0x0B05, 0x1ACE)) return;
    auto list = ctx.Find(0x0B05, 0x1ACE, [](const HidColl& c) { return c.iface == 2 && c.outLen() >= 12 && PathHas(c.path, L"col01"); });
    if (list.empty()) return;
    HidDev d;
    std::vector<BYTE> r;
    if (!d.Open(*list[0]) || Query(d, 0x01, 0xA0, 0x00, r) != Reply::Ok) return;
    ctx.Claim(0x0B05, 0x1ACE);
    for (size_t at = 5; at + 1 < r.size(); at += 4) {
        USHORT pid = (USHORT)(r[at] | (r[at + 1] << 8));
        if (!pid) break;
        for (const Model& m : kModels) {
            if (m.pid != pid || !m.reportId) continue;  // only the Omni-channel entry of that model
            for (const HidColl* c : ctx.Find(0x0B05, 0x1ACE, [&](const HidColl& c) { return c.iface == 2 && c.outLen() >= 12 && PathHas(c.path, m.col); })) {
                HidDev dev;
                DeviceStatus s;
                if (dev.Open(*c) && ReadModel(dev, m, s)) { AddOnce(ctx, seen, s); break; }
            }
            break;
        }
    }
}

// ROG Pelta (0B05:1B84) and ROG Delta II (0B05:1AFA) headsets. Vendor collection FF00:0001 on
// interface 3, report id CC, 64 bytes each way.
//   CC 12 07 -> CC 12 07 00 00 [5] sleep min [6] battery % (FE = unknown) [7] low-battery alert %
//   CC 12 08 -> CC 12 08 00 00 [5] 1 = charging
// A sleeping or switched-off headset answers with FF AA in [5..6] or [1..2].
struct HeadsetModel {
    USHORT pid;
    const wchar_t* name;
};

const HeadsetModel kHeadsets[] = {
    { 0x1B84, L"ROG Pelta" },
    { 0x1AFA, L"ROG Delta II" },
};

Reply PeltaQuery(HidDev& d, BYTE cmd, std::vector<BYTE>& reply) {
    d.Flush();
    // The first answer after the headset reconnects can take ~750 ms.
    ULONGLONG end = GetTickCount64() + 1200;
    std::vector<BYTE> r;
    int n = d.SetOutputAndRead({ 0xCC, 0x12, cmd }, r, 1200);
    if (n < 0 && d.Write({ 0xCC, 0x12, cmd })) {
        int left = (int)((LONGLONG)end - (LONGLONG)GetTickCount64());
        if (left > 0) n = d.Read(r, left);
    }
    while (n >= 8) {
        if (r[0] == 0xCC && r[1] == 0xFF && r[2] == 0xAA) { reply = r; return Reply::Asleep; }
        if (r[0] == 0xCC && r[1] == 0x12) {
            if (r[5] == 0xFF && r[6] == 0xAA) { reply = r; return Reply::Asleep; }
            if (r[2] == cmd) { reply = r; return Reply::Ok; }
        }
        int left = (int)((LONGLONG)end - (LONGLONG)GetTickCount64());
        if (left <= 0) break;
        n = d.Read(r, left);
    }
    return Reply::None;
}

void PollPelta(PollContext& ctx) {
    for (const HeadsetModel& hs : kHeadsets) {
        if (ctx.IsClaimed(0x0B05, hs.pid)) continue;
        auto colls = ctx.Find(0x0B05, hs.pid, [](const HidColl& c) {
            return c.iface == 3 && c.page() == 0xFF00 && c.usage() == 0x0001 && c.inLen() >= 8 && c.outLen() >= 8;
        });
        for (const HidColl* c : colls) {
            HidDev d;
            std::vector<BYTE> r;
            if (!d.Open(*c)) continue;
            Reply res = PeltaQuery(d, 0x07, r);
            if (res == Reply::None) continue;
            DeviceStatus s;
            s.key = L"rog:" + Hex4(hs.pid) + L":hs";
            s.name = hs.name;
            s.kind = DeviceKind::Headset;
            s.via = L"2.4 GHz receiver";
            if (res == Reply::Asleep) {
                s.asleep = true;
            } else {
                if (r[6] <= 100) s.percent = r[6];
                else s.asleep = true;  // FE = level not known yet
                std::vector<BYTE> ch;
                if (PeltaQuery(d, 0x08, ch) == Reply::Ok) s.charging = ch[5] == 1;
            }
            ctx.out.push_back(s);
            ctx.Claim(0x0B05, hs.pid);
            break;
        }
    }
}

// ROG Cetra Open Wireless (0B05:1CAC) earbuds on their SpeedNova USB-C dongle. Same vendor
// collection and report as the Pelta (FF00:0001, interface 3, report id CC, 64 bytes), but the
// battery is behind sub 12 (sub 07 answers with unrelated data on this dongle):
//   CC 12 12 -> CC 12 12 00 00 [5] left % [6] right % [7] case %
// Verified on hardware against Armoury Crate (L 99 / R 99 / case 86); [5] rose 99 -> 100 with only
// the left earbud in the case. The case level is the last one the case reported (Armoury Crate
// notes it is not live). A level of 0 is treated as not reported. With both earbuds in the closed
// case the dongle answers FF AA, shown as asleep.
//
// ROG Cetra True Wireless SpeedNova (0B05:1AD3, same collection) answers differently: sub 12 gives
// FF AA even while the earbuds are connected, and the levels are behind sub 07:
//   CC 12 07 -> CC 12 07 00 00 [5] sleep min [6] left % [7] right % [8] case %
//   CC 12 08 -> CC 12 08 00 00 [5] 1 = charging
// Seen on a connected pair: [5..8] = 05 54 53 53 (84 / 83 / 83). Which byte is which earbud is
// read from that one sample and still has to be confirmed against the vendor software.
struct EarbudModel {
    USHORT pid;
    const wchar_t* name;
    BYTE sub;       // battery query sub command
    int left;       // byte offsets (report id counted) of left / right / case level
    int right;
    int box;
    bool chargeQuery;  // charging state is a separate sub 08 query
};

const EarbudModel kEarbuds[] = {
    { 0x1CAC, L"ROG Cetra Open Wireless", 0x12, 5, 6, 7, false },
    { 0x1AD3, L"ROG Cetra True Wireless SpeedNova", 0x07, 6, 7, 8, true },
};

void PollCetraEarbuds(PollContext& ctx, const EarbudModel& model) {
    const USHORT pid = model.pid;
    if (ctx.IsClaimed(0x0B05, pid)) return;
    auto colls = ctx.Find(0x0B05, pid, [](const HidColl& c) {
        return c.iface == 3 && c.page() == 0xFF00 && c.usage() == 0x0001 && c.inLen() >= 8 && c.outLen() >= 8;
    });
    for (const HidColl* c : colls) {
        HidDev d;
        std::vector<BYTE> r;
        if (!d.Open(*c)) continue;
        Reply res = PeltaQuery(d, model.sub, r);
        if (res == Reply::None) continue;
        DeviceStatus s;
        s.key = L"rog:" + Hex4(pid) + L":hs";
        s.name = model.name;
        s.kind = DeviceKind::Headset;
        s.via = L"2.4 GHz receiver";
        auto level = [&](int at) { return r.size() > (size_t)at && r[at] > 0 && r[at] <= 100 ? (int)r[at] : -1; };
        int left = res == Reply::Ok ? level(model.left) : -1;
        int right = res == Reply::Ok ? level(model.right) : -1;
        int box = res == Reply::Ok ? level(model.box) : -1;
        if (left < 0 && right < 0) {
            s.asleep = true;
        } else {
            s.percent = left < 0 ? right : right < 0 ? left : (left < right ? left : right);
            std::wstring detail;
            auto part = [&](const wchar_t* label, int v) {
                if (v < 0) return;
                if (!detail.empty()) detail += L", ";
                detail += label + std::to_wstring(v) + L"%";
            };
            part(L"L ", left);
            part(L"R ", right);
            part(L"case ", box);
            s.detail = detail;
            if (model.chargeQuery) {
                std::vector<BYTE> ch;
                if (PeltaQuery(d, 0x08, ch) == Reply::Ok && ch.size() > 5) s.charging = ch[5] == 1;
            }
        }
        ctx.out.push_back(s);
        ctx.Claim(0x0B05, pid);
        break;
    }
}

}  // namespace

void PollRog(PollContext& ctx) {
    std::vector<std::wstring> seen;  // one entry per device name (wired + receiver both present while charging)
    PollPelta(ctx);
    for (const EarbudModel& e : kEarbuds) PollCetraEarbuds(ctx, e);
    PollOmni(ctx, seen);
    for (const Model& m : kModels) {
        if (ctx.IsClaimed(0x0B05, m.pid)) continue;
        auto colls = ctx.Find(0x0B05, m.pid, [&](const HidColl& c) {
            if (c.outLen() < 12 || c.page() < 0xFF00) return false;
            if (m.iface >= 0 && c.iface != m.iface) return false;
            return !m.col || PathHas(c.path, m.col);
        });
        for (const HidColl* c : colls) {
            HidDev d;
            DeviceStatus s;
            if (!d.Open(*c) || !ReadModel(d, m, s)) continue;
            // A receiver channel (e.g. the SpeedNova 8K receiver 1AD0) answers "asleep" when nothing is
            // paired to it too, so a model is only shown there once it has really answered (issue #140).
            static std::set<USHORT> awake;
            if (m.reportId && s.asleep && !awake.count(m.pid)) continue;
            if (m.reportId && !s.asleep) awake.insert(m.pid);
            AddOnce(ctx, seen, s);
            ctx.Claim(0x0B05, m.pid);
            break;
        }
    }
}
