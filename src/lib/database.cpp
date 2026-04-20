#include "jsmn_helpers.h"

#include "ptcgp_sim/common.h"
#include "ptcgp_sim/database.h"
#include "ptcgp_sim/attack_mechanic_dictionary.h"
#include "ptcgp_sim/ability_mechanic_dictionary.h"



#include <cassert>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace ptcgp_sim 
{

// ---------------------------------------------------------------------------
// CardId helpers
// ---------------------------------------------------------------------------

std::string CardId::to_string() const
{
    std::ostringstream oss;
    oss << expansion << " " << std::setw(3) << std::setfill('0') << number;
    return oss.str();
}

// Parse "A1 002" or "B2b 015" -> CardId{"A1", 2} / CardId{"B2b", 15}
CardId Database::parse_id(const std::string& full_id)
{
    auto pos = full_id.find(' ');
    assert(pos != std::string::npos && pos > 0 && pos + 1 < full_id.size()
        && "Database::parse_id: invalid id format, expected \"<expansion> <number>\"");

    CardId result;
    result.expansion = full_id.substr(0, pos);
    result.number    = std::stoi(full_id.substr(pos + 1));
    return result;
}

// ---------------------------------------------------------------------------
// Mechanic serialisation helpers (for intermediate JSON files)
// ---------------------------------------------------------------------------

// Escape a string for JSON output (handles quotes and backslashes).
static std::string json_escape(const std::string& s)
{
    std::string out;
    out.reserve(s.size() + 4);
    for (char c : s)
    {
        if (c == '"')  { out += "\\\""; }
        else if (c == '\\') { out += "\\\\"; }
        else if (c == '\n') { out += "\\n"; }
        else if (c == '\r') { out += "\\r"; }
        else if (c == '\t') { out += "\\t"; }
        else                { out += c; }
    }
    return out;
}

// ---------------------------------------------------------------------------
// Mechanic deserialisation helpers (for reading intermediate JSON files)
// ---------------------------------------------------------------------------

// Minimal JSON value extractor: find the string value for a key in a flat
// JSON object string like {"coins":2,"extra_damage":30,"include_fixed_damage":false}
static std::string extract_json_value(const std::string& obj, const std::string& key)
{
    // Search for "key": in the object string
    std::string search = "\"" + key + "\":";
    auto pos = obj.find(search);
    if (pos == std::string::npos) return "";
    pos += search.size();
    // Skip whitespace
    while (pos < obj.size() && obj[pos] == ' ') ++pos;
    if (pos >= obj.size()) return "";
    // Read until comma, } or end
    std::string val;
    bool in_string = (obj[pos] == '"');
    if (in_string) ++pos;
    while (pos < obj.size())
    {
        char c = obj[pos];
        if (in_string)
        {
            if (c == '"') break;
            val += c;
        }
        else
        {
            if (c == ',' || c == '}') break;
            val += c;
        }
        ++pos;
    }
    return val;
}

// ---------------------------------------------------------------------------
// Mechanic factory registry
//
// Maps type_name string -> factory that default-constructs a Mechanic.
// mechanic_from_json() uses this to avoid a growing if-else chain.
// To add a new mechanic: register it in the factory map below.
// ---------------------------------------------------------------------------

using MechanicFactory = std::unique_ptr<AttackMechanic>(*)();

static const std::unordered_map<std::string, MechanicFactory>& mechanic_factory_map()
{
    static const std::unordered_map<std::string, MechanicFactory> MAP = {
        { "BasicDamage",         []() -> std::unique_ptr<AttackMechanic> { return std::make_unique<BasicDamage>(); } },
        { "FlipNCoinDamage",     []() -> std::unique_ptr<AttackMechanic> { return std::make_unique<FlipNCoinDamage>(); } },
        { "FlipNCoinExtraDamage",[]() -> std::unique_ptr<AttackMechanic> { return std::make_unique<FlipNCoinExtraDamage>(); } },
        { "SelfHeal",            []() -> std::unique_ptr<AttackMechanic> { return std::make_unique<SelfHeal>(); } },
        { "FlipUntilTailsDamage",[]() -> std::unique_ptr<AttackMechanic> { return std::make_unique<FlipUntilTailsDamage>(); } },
    };
    return MAP;
}

static std::unique_ptr<AttackMechanic> mechanic_from_json(const std::string& type, const std::string& params_json)
{
    const auto& registry = mechanic_factory_map();
    auto it = registry.find(type);
    if (it == registry.end()) return nullptr;
    auto mech = it->second();
    mech->from_params_json(params_json);
    return mech;
}

// ---------------------------------------------------------------------------
// Ability mechanic factory registry
// ---------------------------------------------------------------------------

using AbilityMechanicFactory = std::unique_ptr<AbilityMechanic>(*)();

static const std::unordered_map<std::string, AbilityMechanicFactory>& ability_mechanic_factory_map()
{
    static const std::unordered_map<std::string, AbilityMechanicFactory> MAP = {
        { "HealAllYourPokemon",    []() -> std::unique_ptr<AbilityMechanic> { return std::make_unique<HealAllYourPokemon>(); } },
        { "HealOneYourPokemon",    []() -> std::unique_ptr<AbilityMechanic> { return std::make_unique<HealOneYourPokemon>(); } },
        { "HealActiveYourPokemon", []() -> std::unique_ptr<AbilityMechanic> { return std::make_unique<HealActiveYourPokemon>(); } },
        { "ReduceDamageFromAttacks", []() -> std::unique_ptr<AbilityMechanic> { return std::make_unique<ReduceDamageFromAttacks>(); } },
        { "UnknownAbility",        []() -> std::unique_ptr<AbilityMechanic> { return std::make_unique<UnknownAbilityMechanic>(); } },
    };
    return MAP;
}

// Returns nullptr only if type is empty; always returns UnknownAbilityMechanic for unrecognised types.
static std::unique_ptr<AbilityMechanic> ability_mechanic_from_json(const std::string& type, const std::string& params_json)
{
    if (type.empty()) return nullptr;
    const auto& registry = ability_mechanic_factory_map();
    auto it = registry.find(type);
    std::unique_ptr<AbilityMechanic> mech;
    if (it != registry.end())
        mech = it->second();
    else
        mech = std::make_unique<UnknownAbilityMechanic>();
    mech->from_params_json(params_json);
    return mech;
}

// ---------------------------------------------------------------------------
// Database::parse_json  (internal — parses database.json, no mechanic resolution)
// ---------------------------------------------------------------------------

Database Database::parse_json(const std::string& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open())
        throw std::runtime_error("Database::load: cannot open " + path);

    std::ostringstream ss;
    ss << file.rdbuf();
    const std::string json = ss.str();
    const char* js = json.c_str();
    const int   js_len = (int)json.size();

    jsmn_parser parser;
    jsmn_init(&parser);
    int ntok = jsmn_parse(&parser, js, js_len, nullptr, 0);
    if (ntok < 0)
        throw std::runtime_error("Database::load: jsmn failed to parse JSON");

    std::vector<jsmntok_t> tokens((size_t)ntok);
    jsmn_init(&parser);
    jsmn_parse(&parser, js, js_len, tokens.data(), (unsigned int)ntok);

    Database db;

    int i = 1;
    while (i < ntok)
    {
        if (tokens[i].type != JSMN_OBJECT) { ++i; continue; }

        int obj_size = tokens[i].size;
        ++i;

        for (int kv = 0; kv < obj_size; ++kv)
        {
            bool is_pokemon = jsmn_tok_eq(js, tokens[i], "Pokemon");
            bool is_trainer = jsmn_tok_eq(js, tokens[i], "Trainer");
            ++i;

            if (!is_pokemon && !is_trainer)
            {
                if (tokens[i].type == JSMN_OBJECT || tokens[i].type == JSMN_ARRAY)
                {
                    int end = tokens[i].end; ++i;
                    while (i < ntok && tokens[i].start < end) ++i;
                }
                else { ++i; }
                continue;
            }

            if (tokens[i].type != JSMN_OBJECT) { ++i; continue; }

            Card card;
            card.type = is_pokemon ? CardType::Pokemon : CardType::Trainer;

            int inner_size = tokens[i].size;
            ++i;

            for (int f = 0; f < inner_size; ++f)
            {
                if (i >= ntok) break;
                const jsmntok_t& key = tokens[i]; ++i;
                if (i >= ntok) break;
                const jsmntok_t& val = tokens[i];

                if (jsmn_tok_eq(js, key, "id"))
                {
                    card.id = Database::parse_id(jsmn_tok_str(js, val));
                    ++i;
                }
                else if (jsmn_tok_eq(js, key, "name"))
                {
                    card.name = jsmn_tok_str(js, val);
                    ++i;
                }
                else if (jsmn_tok_eq(js, key, "hp"))
                {
                    card.hp = std::stoi(jsmn_tok_str(js, val));
                    ++i;
                }
                else if (jsmn_tok_eq(js, key, "energy_type"))
                {
                    card.energy_type = energy_from_string(jsmn_tok_str(js, val));
                    ++i;
                }
                else if (jsmn_tok_eq(js, key, "weakness"))
                {
                    if (val.type == JSMN_PRIMITIVE && js[val.start] == 'n')
                        card.weakness = std::nullopt;
                    else
                        card.weakness = energy_from_string(jsmn_tok_str(js, val));
                    ++i;
                }
                else if (jsmn_tok_eq(js, key, "retreat_cost"))
                {
                    if (val.type == JSMN_ARRAY)
                    {
                        int arr_size = val.size; ++i;
                        for (int a = 0; a < arr_size; ++a)
                        {
                            if (i >= ntok) break;
                            card.retreat_cost.push_back(
                                energy_from_string(jsmn_tok_str(js, tokens[i])));
                            ++i;
                        }
                    }
                    else { ++i; }
                }
                else if (jsmn_tok_eq(js, key, "stage"))
                {
                    card.stage = std::stoi(jsmn_tok_str(js, val));
                    ++i;
                }
                else if (jsmn_tok_eq(js, key, "evolves_from"))
                {
                    if (val.type == JSMN_PRIMITIVE && js[val.start] == 'n')
                        card.evolves_from = std::nullopt;
                    else
                        card.evolves_from = jsmn_tok_str(js, val);
                    ++i;
                }
                else if (jsmn_tok_eq(js, key, "attacks"))
                {
                    if (val.type == JSMN_ARRAY)
                    {
                        int atk_arr_size = val.size; ++i;
                        for (int a = 0; a < atk_arr_size; ++a)
                        {
                            if (i >= ntok) break;
                            if (tokens[i].type != JSMN_OBJECT) { ++i; continue; }
                            int atk_obj_size = tokens[i].size; ++i;
                            Attack atk;
                            for (int af = 0; af < atk_obj_size; ++af)
                            {
                                if (i >= ntok) break;
                                const jsmntok_t& akey = tokens[i]; ++i;
                                if (i >= ntok) break;
                                const jsmntok_t& aval = tokens[i];

                                // database.json uses "title" for attack name
                                if (jsmn_tok_eq(js, akey, "title"))
                                {
                                    atk.name = jsmn_tok_str(js, aval);
                                    ++i;
                                }
                                // database.json uses "fixed_damage" for base damage
                                else if (jsmn_tok_eq(js, akey, "fixed_damage"))
                                {
                                    atk.damage = std::stoi(jsmn_tok_str(js, aval));
                                    ++i;
                                }
                                else if (jsmn_tok_eq(js, akey, "effect"))
                                {
                                    if (aval.type == JSMN_PRIMITIVE && js[aval.start] == 'n')
                                        atk.effect = std::nullopt;
                                    else
                                        atk.effect = jsmn_tok_str(js, aval);
                                    ++i;
                                }
                                else if (jsmn_tok_eq(js, akey, "energy_required"))
                                {
                                    if (aval.type == JSMN_ARRAY)
                                    {
                                        int ecnt = aval.size; ++i;
                                        for (int e = 0; e < ecnt; ++e)
                                        {
                                            if (i >= ntok) break;
                                            atk.energy_required.push_back(
                                                energy_from_string(jsmn_tok_str(js, tokens[i])));
                                            ++i;
                                        }
                                    }
                                    else { ++i; }
                                }
                                else
                                {
                                    if (aval.type == JSMN_OBJECT || aval.type == JSMN_ARRAY)
                                    {
                                        int end = aval.end; ++i;
                                        while (i < ntok && tokens[i].start < end) ++i;
                                    }
                                    else { ++i; }
                                }
                            }
                            card.attacks.push_back(std::move(atk));
                        }
                    }
                    else { ++i; }
                }
                else if (jsmn_tok_eq(js, key, "ability"))
                {
                    // ability is an object: {"name": "...", "effect": "..."}
                    // or null if the Pokemon has no ability
                    if (val.type == JSMN_PRIMITIVE && js[val.start] == 'n')
                    {
                        card.ability = std::nullopt;
                        ++i;
                    }
                    else if (val.type == JSMN_OBJECT)
                    {
                        Ability ab;
                        int ab_size = val.size; ++i;
                        for (int af = 0; af < ab_size; ++af)
                        {
                            if (i >= ntok) break;
                            const jsmntok_t& akey = tokens[i]; ++i;
                            if (i >= ntok) break;
                            const jsmntok_t& aval = tokens[i];
                            if (jsmn_tok_eq(js, akey, "name"))
                            {
                                ab.name = jsmn_tok_str(js, aval);
                                ++i;
                            }
                            else if (jsmn_tok_eq(js, akey, "effect"))
                            {
                                ab.effect = jsmn_tok_str(js, aval);
                                ++i;
                            }
                            else
                            {
                                if (aval.type == JSMN_OBJECT || aval.type == JSMN_ARRAY)
                                {
                                    int end = aval.end; ++i;
                                    while (i < ntok && tokens[i].start < end) ++i;
                                }
                                else { ++i; }
                            }
                        }
                        card.ability = ab;
                    }
                    else { ++i; }
                }
                else
                {
                    if (val.type == JSMN_OBJECT || val.type == JSMN_ARRAY)
                    {
                        int end = val.end; ++i;
                        while (i < ntok && tokens[i].start < end) ++i;
                    }
                    else { ++i; }
                }
            }

            db.Cards.push_back(std::move(card));
        }
    }

    return db;
}

// ---------------------------------------------------------------------------
// Database::resolve_mechanics
//
// Reads pair_mechanic.json and in one pass:
//   1. Sets atk.mechanic on each Card's attacks
//   2. Populates pair_attack_mechanic() and pair_ability_mechanic() runtime maps
//
// pair_mechanic.json is the single source of truth — generated once by
// `ptcgp_cli util --build_dictionary`.  If the file is absent, a warning is
// printed and the function returns without crashing.
// ---------------------------------------------------------------------------
void Database::resolve_mechanics(const std::string& pair_mechanic_path)
{
    std::ifstream dict_file(pair_mechanic_path);
    if (!dict_file.is_open())
    {
        std::cerr << "[warn] pair_mechanic.json not found at: " << pair_mechanic_path
                  << "\n       Run: ptcgp_cli util --build_dictionary\n";
        return;
    }

    // Read the whole file
    std::ostringstream ss;
    ss << dict_file.rdbuf();
    const std::string dict_json = ss.str();

    // Build a lookup: card id string -> Card*
    std::unordered_map<std::string, Card*> id_map;
    for (auto& card : Cards)
        id_map[card.id.to_string()] = &card;

    // Runtime map to populate (attacks only — ability mechanic lives on card.ability->mechanic)
    auto& rt_attack = const_cast<std::unordered_map<std::string, const AttackMechanic*>&>(
        pair_attack_mechanic());

    // Walk through each top-level key (pokemon id)
    // Format: { "<id>": { "ability": {...}|null, "attacks": [ {...}, ... ] }, ... }
    std::size_t pos = 0;
    while (pos < dict_json.size())
    {
        // Find next quoted key (pokemon id)
        auto key_start = dict_json.find('"', pos);
        if (key_start == std::string::npos) break;
        auto key_end = dict_json.find('"', key_start + 1);
        if (key_end == std::string::npos) break;
        const std::string pokemon_id = dict_json.substr(key_start + 1, key_end - key_start - 1);
        pos = key_end + 1;

        // Locate the opening brace of this pokemon's value object
        auto obj_open = dict_json.find('{', pos);
        if (obj_open == std::string::npos) break;

        // Find the matching closing brace for this pokemon object
        // (handles nested braces from ability/params objects)
        int depth = 0;
        std::size_t obj_close = obj_open;
        for (std::size_t ci = obj_open; ci < dict_json.size(); ++ci)
        {
            if (dict_json[ci] == '{') ++depth;
            else if (dict_json[ci] == '}') { --depth; if (depth == 0) { obj_close = ci; break; } }
        }
        const std::string pokemon_obj = dict_json.substr(obj_open, obj_close - obj_open + 1);
        pos = obj_close + 1;

        Card* card = nullptr;
        auto card_it = id_map.find(pokemon_id);
        if (card_it != id_map.end())
            card = card_it->second;

        // ---- ability field ----
        auto ability_pos = pokemon_obj.find("\"ability\":");
        if (ability_pos != std::string::npos)
        {
            auto colon = pokemon_obj.find(':', ability_pos);
            std::size_t val_start = colon + 1;
            while (val_start < pokemon_obj.size() && std::isspace((unsigned char)pokemon_obj[val_start]))
                ++val_start;

            if (pokemon_obj.substr(val_start, 4) != "null" && card && card->ability.has_value())
            {
                // Find the ability object { ... }
                auto ab_open  = pokemon_obj.find('{', colon);
                auto ab_close = pokemon_obj.find('}', ab_open);
                if (ab_open != std::string::npos && ab_close != std::string::npos)
                {
                    const std::string ab_entry = pokemon_obj.substr(ab_open, ab_close - ab_open + 1);
                    const std::string mtype    = extract_json_value(ab_entry, "mechanic_type");

                    auto params_pos = ab_entry.find("\"params\":");
                    std::string params_json = "{}";
                    if (params_pos != std::string::npos)
                    {
                        auto pb = ab_entry.find('{', params_pos);
                        auto pe = ab_entry.find('}', pb);
                        if (pb != std::string::npos && pe != std::string::npos)
                            params_json = ab_entry.substr(pb, pe - pb + 1);
                    }

                    // Build mechanic directly onto card->ability->mechanic
                    // Use UnknownAbilityMechanic as fallback for unrecognised types
                    auto mech = ability_mechanic_from_json(mtype, params_json);
                    card->ability->mechanic = mech
                        ? std::move(mech)
                        : std::make_unique<UnknownAbilityMechanic>();
                }
            }
        }

        // ---- attacks field ----
        auto attacks_pos = pokemon_obj.find("\"attacks\":");
        if (attacks_pos == std::string::npos) continue;
        auto arr_start = pokemon_obj.find('[', attacks_pos);
        if (arr_start == std::string::npos) continue;
        auto arr_end = pokemon_obj.find(']', arr_start);
        if (arr_end == std::string::npos) continue;

        const std::string attacks_section = pokemon_obj.substr(arr_start, arr_end - arr_start + 1);

        if (!card) continue;

        // Parse each attack entry { ... } in the array
        std::size_t apos = 0;
        while (apos < attacks_section.size())
        {
            auto obj_start = attacks_section.find('{', apos);
            if (obj_start == std::string::npos) break;
            auto obj_end = attacks_section.find('}', obj_start);
            if (obj_end == std::string::npos) break;
            const std::string entry = attacks_section.substr(obj_start, obj_end - obj_start + 1);
            apos = obj_end + 1;

            const std::string idx_str = extract_json_value(entry, "attack_index");
            if (idx_str.empty()) continue;
            const int atk_idx = std::stoi(idx_str);
            if (atk_idx < 0 || atk_idx >= (int)card->attacks.size()) continue;

            const std::string mtype = extract_json_value(entry, "mechanic_type");
            if (mtype.empty() || mtype == "BasicDamage" || mtype == "Unknown") continue;

            auto params_pos = entry.find("\"params\":");
            std::string params_json = "{}";
            if (params_pos != std::string::npos)
            {
                auto pb = entry.find('{', params_pos);
                auto pe = entry.find('}', pb);
                if (pb != std::string::npos && pe != std::string::npos)
                    params_json = entry.substr(pb, pe - pb + 1);
            }

            // Construct mechanic via factory and assign to attack
            auto mech = mechanic_from_json(mtype, params_json);
            if (mech)
            {
                const std::string atk_key = pokemon_id + ":" + std::to_string(atk_idx);
                // Populate rt_attack with a stable pointer from the in-memory dictionary
                const auto& ammap = attack_mechanic_dictionary();
                for (const auto& kv : ammap)
                {
                    if (kv.second->type_name() == mtype)
                    {
                        rt_attack[atk_key] = kv.second.get();
                        break;
                    }
                }
                card->attacks[atk_idx].mechanic = std::move(mech);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Database::build_attack_mechanic_dictionary
// ---------------------------------------------------------------------------

bool Database::build_attack_mechanic_dictionary(
    const std::string& db_path,
    const std::string& attack_mechanic_path)
{
    std::cout << "[info] Building attack mechanic dictionary from database.json...\n";

    Database db = Database::parse_json(db_path);
    const auto& mmap = attack_mechanic_dictionary();

    // Build attack_mechanic_dictionary.json
    // effect text -> AttackMechanic type + params
    std::unordered_map<std::string, const AttackMechanic*> resolved_effects;
    std::vector<std::string>                               unresolved_effects;

    for (const auto& card : db.Cards)
    {
        for (const auto& atk : card.attacks)
        {
            if (!atk.effect.has_value()) continue;
            const std::string& eff = *atk.effect;
            if (resolved_effects.count(eff) ||
                std::find(unresolved_effects.begin(), unresolved_effects.end(), eff) != unresolved_effects.end())
                continue;

            auto it = mmap.find(eff);
            if (it != mmap.end())
                resolved_effects[eff] = it->second.get();
            else
                unresolved_effects.push_back(eff);
        }
    }

    {
        std::ofstream out(attack_mechanic_path);
        if (!out.is_open())
        {
            std::cerr << "[warn] Could not write attack mechanic dictionary: " << attack_mechanic_path << "\n";
            return false;
        }

        out << "{\n";
        bool first = true;
        for (const auto& kv : resolved_effects)
        {
            if (!first) out << ",\n";
            first = false;
            out << "  \"" << json_escape(kv.first) << "\": {"
                << "\"mechanic_type\": \"" << kv.second->type_name() << "\", "
                << "\"params\": " << kv.second->params_json()
                << "}";
        }
        for (const auto& eff : unresolved_effects)
        {
            if (!first) out << ",\n";
            first = false;
            out << "  \"" << json_escape(eff) << "\": {"
                << "\"mechanic_type\": \"Unknown\", "
                << "\"params\": {}"
                << "}";
        }
        out << "\n}\n";
        out.close();
    }

    std::cout << "[info] attack_mechanic_dictionary.json: "
              << resolved_effects.size() << " recognised, "
              << unresolved_effects.size() << " unrecognised\n";
    std::cout << "[info] Output: " << attack_mechanic_path << "\n";

    return true;
}

// ---------------------------------------------------------------------------
// Database::build_ability_mechanic_dictionary
// ---------------------------------------------------------------------------

bool Database::build_ability_mechanic_dictionary(
    const std::string& db_path,
    const std::string& ability_mechanic_path)
{
    std::cout << "[info] Building ability mechanic dictionary from database.json...\n";

    Database db = Database::parse_json(db_path);
    const auto& amap = ability_mechanic_dictionary();

    // Build ability_mechanic_dictionary.json
    // ability effect text -> AbilityMechanic type + params
    std::unordered_map<std::string, const AbilityMechanic*> resolved_abilities;
    std::vector<std::string>                                unresolved_abilities;

    for (const auto& card : db.Cards)
    {
        if (card.type != CardType::Pokemon) continue;
        if (!card.ability.has_value()) continue;

        const std::string& eff = card.ability->effect;
        if (resolved_abilities.count(eff) ||
            std::find(unresolved_abilities.begin(), unresolved_abilities.end(), eff) != unresolved_abilities.end())
            continue;

        auto it = amap.find(eff);
        if (it != amap.end())
            resolved_abilities[eff] = it->second.get();
        else
            unresolved_abilities.push_back(eff);
    }

    {
        std::ofstream out(ability_mechanic_path);
        if (!out.is_open())
        {
            std::cerr << "[warn] Could not write ability mechanic dictionary: " << ability_mechanic_path << "\n";
            return false;
        }

        out << "{\n";
        bool first = true;
        for (const auto& kv : resolved_abilities)
        {
            if (!first) out << ",\n";
            first = false;
            out << "  \"" << json_escape(kv.first) << "\": {"
                << "\"mechanic_type\": \"" << kv.second->type_name() << "\", "
                << "\"params\": " << kv.second->params_json()
                << "}";
        }
        for (const auto& eff : unresolved_abilities)
        {
            if (!first) out << ",\n";
            first = false;
            out << "  \"" << json_escape(eff) << "\": {"
                << "\"mechanic_type\": \"UnknownAbility\", "
                << "\"params\": {}"
                << "}";
        }
        out << "\n}\n";
        out.close();
    }

    std::cout << "[info] ability_mechanic_dictionary.json: "
              << resolved_abilities.size() << " recognised, "
              << unresolved_abilities.size() << " unrecognised\n";
    std::cout << "[info] Output: " << ability_mechanic_path << "\n";

    return true;
}

// ---------------------------------------------------------------------------
// Database::build_pair_mechanic
//
// Writes pair_mechanic.json — one entry per Pokemon with both ability and
// attacks fields combined:
//
//   "A1 001": {
//     "ability": { "mechanic_type": "HealAllYourPokemon", "params": {...} },
//     "attacks": [
//       { "attack_index": 0, "mechanic_type": "BasicDamage", "params": {} },
//       { "attack_index": 1, "mechanic_type": "FlipNCoinDamage", "params": {...} }
//     ]
//   }
//
// Also populates the pair_attack_mechanic() and pair_ability_mechanic()
// runtime maps from the loaded cards.
// ---------------------------------------------------------------------------
bool Database::build_pair_mechanic(const std::string& pair_mechanic_path)
{
    const auto& ammap  = attack_mechanic_dictionary();
    const auto& abmap  = ability_mechanic_dictionary();

    // Populate runtime maps
    auto& rt_attack = const_cast<std::unordered_map<std::string, const AttackMechanic*>&>(
        pair_attack_mechanic());
    auto& rt_ability = const_cast<std::unordered_map<std::string, const AbilityMechanic*>&>(
        pair_ability_mechanic());

    int total_pokemon      = 0;
    int total_attacks      = 0;
    int count_atk_basic    = 0;
    int count_atk_resolved = 0;
    int count_atk_unknown  = 0;
    int count_ab_resolved  = 0;
    int count_ab_unknown   = 0;

    std::ofstream out(pair_mechanic_path);
    if (!out.is_open())
    {
        std::cerr << "[warn] Could not write pair mechanic: " << pair_mechanic_path << "\n";
        return false;
    }

    out << "{\n";
    bool first_card = true;

    for (const auto& card : Cards)
    {
        if (card.type != CardType::Pokemon) continue;
        if (card.attacks.empty() && !card.ability.has_value()) continue;

        ++total_pokemon;
        if (!first_card) out << ",\n";
        first_card = false;

        const std::string card_key = card.id.to_string();
        out << "  \"" << json_escape(card_key) << "\": {\n";

        // ---- ability field ----
        if (card.ability.has_value())
        {
            const std::string& eff = card.ability->effect;
            auto it = abmap.find(eff);
            std::string mtype   = "UnknownAbility";
            std::string mparams = "{}";
            if (it != abmap.end())
            {
                mtype   = it->second->type_name();
                mparams = it->second->params_json();
                rt_ability[card_key] = it->second.get();
                ++count_ab_resolved;
            }
            else
            {
                ++count_ab_unknown;
            }
            out << "    \"ability\": {"
                << "\"mechanic_type\": \"" << mtype << "\", "
                << "\"params\": " << mparams
                << "}";
        }
        else
        {
            out << "    \"ability\": null";
        }

        // ---- attacks field ----
        out << ",\n    \"attacks\": [\n";
        for (int ai = 0; ai < (int)card.attacks.size(); ++ai)
        {
            ++total_attacks;
            const Attack& atk = card.attacks[ai];

            std::string mtype   = "BasicDamage";
            std::string mparams = "{}";

            if (atk.effect.has_value())
            {
                auto it = ammap.find(*atk.effect);
                if (it != ammap.end())
                {
                    mtype   = it->second->type_name();
                    mparams = it->second->params_json();
                    if (mtype == "BasicDamage") ++count_atk_basic;
                    else
                    {
                        ++count_atk_resolved;
                        std::string atk_key = card_key + ":" + std::to_string(ai);
                        rt_attack[atk_key] = it->second.get();
                    }
                }
                else
                {
                    mtype = "Unknown";
                    ++count_atk_unknown;
                    std::cerr << "[warn] unknown attack effect: \""
                              << *atk.effect << "\"\n";
                }
            }
            else
            {
                ++count_atk_basic;
            }

            if (ai > 0) out << ",\n";
            out << "      {"
                << "\"attack_index\": " << ai << ", "
                << "\"mechanic_type\": \"" << mtype << "\", "
                << "\"params\": " << mparams
                << "}";
        }
        out << "\n    ]\n";
        out << "  }";
    }

    out << "\n}\n";
    out.close();

    std::cout << "[info] pair_mechanic.json: "
              << total_pokemon << " Pokemon, "
              << total_attacks << " attacks ("
              << count_atk_basic << " BasicDamage, "
              << count_atk_resolved << " resolved, "
              << count_atk_unknown << " unknown), "
              << "ability: " << count_ab_resolved << " resolved, "
              << count_ab_unknown << " unknown\n";
    std::cout << "[info] Output: " << pair_mechanic_path << "\n";

    return true;
}

// ---------------------------------------------------------------------------
// Database::build_dictionaries  (convenience wrapper)
// ---------------------------------------------------------------------------

bool Database::build_dictionaries(
    const std::string& db_path,
    const std::string& attack_mechanic_path,
    const std::string& ability_mechanic_path,
    const std::string& pair_mechanic_path)
{
    bool ok = true;
    ok &= build_attack_mechanic_dictionary(db_path, attack_mechanic_path);
    ok &= build_ability_mechanic_dictionary(db_path, ability_mechanic_path);
    // build_pair_mechanic is a non-static member — parse a fresh db instance
    Database db = Database::parse_json(db_path);
    ok &= db.build_pair_mechanic(pair_mechanic_path);
    return ok;
}

// ---------------------------------------------------------------------------
// Database::load
// ---------------------------------------------------------------------------

Database Database::load()
{
    return Database::load(PTCGP_DATABASE_PATH);
}

Database Database::load(const std::string& path)
{
    // Determine sibling path for pair_mechanic.json
    std::string dir;
    auto slash = path.find_last_of("/\\");
    if (slash != std::string::npos)
        dir = path.substr(0, slash + 1);

    const std::string pair_mechanic_path = dir + "pair_mechanic.json";

    // 1. Parse database.json — card names, HP, energy, attack names/costs.
    //    Effect text strings are read but mechanic resolution is deferred.
    Database db = Database::parse_json(path);

    // 2. Read pair_mechanic.json and in one pass:
    //      - set atk.mechanic on each attack
    //      - populate pair_attack_mechanic() and pair_ability_mechanic() runtime maps
    //    If pair_mechanic.json is absent, resolve_mechanics() prints a warning.
    db.resolve_mechanics(pair_mechanic_path);

    return db;
}

// ---------------------------------------------------------------------------
// Database::find_by_id
// ---------------------------------------------------------------------------

const Card* Database::find_by_id(const CardId& id) const
{
    for (const Card& c : Cards)
        if (c.id == id) return &c;
    return nullptr;
}

} // namespace ptcgp_sim
