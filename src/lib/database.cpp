#include "jsmn_helpers.h"

#include "ptcgp_sim/common.h"
#include "ptcgp_sim/database.h"
#include "ptcgp_sim/mechanic_map.h"



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

using MechanicFactory = std::unique_ptr<Mechanic>(*)();

static const std::unordered_map<std::string, MechanicFactory>& mechanic_factory_map()
{
    static const std::unordered_map<std::string, MechanicFactory> MAP = {
        { "BasicDamage",         []() -> std::unique_ptr<Mechanic> { return std::make_unique<BasicDamage>(); } },
        { "FlipNCoinDamage",     []() -> std::unique_ptr<Mechanic> { return std::make_unique<FlipNCoinDamage>(); } },
        { "FlipNCoinExtraDamage",[]() -> std::unique_ptr<Mechanic> { return std::make_unique<FlipNCoinExtraDamage>(); } },
        { "SelfHeal",            []() -> std::unique_ptr<Mechanic> { return std::make_unique<SelfHeal>(); } },
        { "FlipUntilTailsDamage",[]() -> std::unique_ptr<Mechanic> { return std::make_unique<FlipUntilTailsDamage>(); } },
    };
    return MAP;
}

static std::unique_ptr<Mechanic> mechanic_from_json(const std::string& type, const std::string& params_json)
{
    const auto& registry = mechanic_factory_map();
    auto it = registry.find(type);
    if (it == registry.end()) return nullptr; // Unknown — leave mechanic as nullptr
    auto mech = it->second();
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
// Reads attack_mechanic_dictionary.json (if it exists) and applies the stored
// Mechanic to each Attack on the loaded cards.  Falls back to inline
// effect_mechanic_map() resolution if the file is absent.
// ---------------------------------------------------------------------------
void Database::resolve_mechanics(const std::string& attack_dict_path)
{
    // Try to load the attack mechanic dictionary
    std::ifstream dict_file(attack_dict_path);
    if (dict_file.is_open())
    {
        // Read the whole file
        std::ostringstream ss;
        ss << dict_file.rdbuf();
        const std::string dict_json = ss.str();

        // Build a map: pokemon_id_string -> vector of (attack_index, Mechanic)
        // We parse the JSON manually using simple string search (no extra deps).
        // Format:
        // { "<id>": { "attacks": [ { "attack_index": N, "mechanic_type": "...", "params": {...} }, ... ] }, ... }

        // Build a lookup: card id string -> Card*
        std::unordered_map<std::string, Card*> id_map;
        for (auto& card : Cards)
            id_map[card.id.to_string()] = &card;

        // Walk through each top-level key (pokemon id)
        std::size_t pos = 0;
        while (pos < dict_json.size())
        {
            // Find next quoted key
            auto key_start = dict_json.find('"', pos);
            if (key_start == std::string::npos) break;
            auto key_end = dict_json.find('"', key_start + 1);
            if (key_end == std::string::npos) break;
            std::string pokemon_id = dict_json.substr(key_start + 1, key_end - key_start - 1);
            pos = key_end + 1;

            // Skip to the "attacks" array
            auto attacks_pos = dict_json.find("\"attacks\"", pos);
            if (attacks_pos == std::string::npos) break;
            auto arr_start = dict_json.find('[', attacks_pos);
            if (arr_start == std::string::npos) break;
            auto arr_end = dict_json.find(']', arr_start);
            if (arr_end == std::string::npos) break;

            std::string attacks_section = dict_json.substr(arr_start, arr_end - arr_start + 1);
            pos = arr_end + 1;

            // Find the card
            auto card_it = id_map.find(pokemon_id);
            if (card_it == id_map.end()) continue;
            Card* card = card_it->second;

            // Parse each attack entry in the array
            std::size_t apos = 0;
            while (apos < attacks_section.size())
            {
                auto obj_start = attacks_section.find('{', apos);
                if (obj_start == std::string::npos) break;
                auto obj_end = attacks_section.find('}', obj_start);
                if (obj_end == std::string::npos) break;
                std::string entry = attacks_section.substr(obj_start, obj_end - obj_start + 1);
                apos = obj_end + 1;

                // Extract attack_index
                std::string idx_str = extract_json_value(entry, "attack_index");
                if (idx_str.empty()) continue;
                int atk_idx = std::stoi(idx_str);

                // Extract mechanic_type
                std::string mtype = extract_json_value(entry, "mechanic_type");

                // Extract params object
                auto params_start = entry.find("\"params\":");
                std::string params_json = "{}";
                if (params_start != std::string::npos)
                {
                    auto pb = entry.find('{', params_start);
                    auto pe = entry.find('}', pb);
                    if (pb != std::string::npos && pe != std::string::npos)
                        params_json = entry.substr(pb, pe - pb + 1);
                }

                if (atk_idx >= 0 && atk_idx < (int)card->attacks.size())
                {
                    auto mech = mechanic_from_json(mtype, params_json);
                    // BasicDamage and Unknown leave mechanic as nullptr
                    if (mech && mech->type_name() != "BasicDamage" && mech->type_name() != "Unknown")
                        card->attacks[atk_idx].mechanic = std::move(mech);
                }
            }
        }
        return;
    }

    // Fallback: resolve inline using effect_mechanic_map()
    const auto& emap = effect_mechanic_map();
    for (auto& card : Cards)
    {
        for (auto& atk : card.attacks)
        {
            if (!atk.effect.has_value()) continue;
            auto it = emap.find(*atk.effect);
            if (it != emap.end())
                atk.mechanic = it->second->clone();
            else
                atk.mechanic = std::make_unique<UnknownMechanic>();
        }
    }
}

// ---------------------------------------------------------------------------
// Database::build_dictionaries
// ---------------------------------------------------------------------------

bool Database::build_dictionaries(
    const std::string& db_path,
    const std::string& mechanic_path,
    const std::string& attack_dict_path)
{
    std::cout << "[info] Building mechanic dictionary from database.json...\n";

    // Parse the raw database
    Database db = Database::parse_json(db_path);

    const auto& mmap = effect_mechanic_map();

    // -----------------------------------------------------------------------
    // Step 1: Build mechanic_dictionary.json
    // Collect all unique effect texts and resolve them.
    // -----------------------------------------------------------------------
    // resolved_effects: effect text -> prototype pointer (non-owning, from mmap)
    std::unordered_map<std::string, const Mechanic*> resolved_effects;
    std::vector<std::string>                         unresolved_effects;

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

    // Write mechanic_dictionary.json
    {
        std::ofstream out(mechanic_path);
        if (!out.is_open())
        {
            std::cerr << "[warn] Could not write mechanic dictionary: " << mechanic_path << "\n";
            // Continue to build attack dict in memory even if file write fails
        }
        else
        {
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
    }

    // -----------------------------------------------------------------------
    // Step 2: Build attack_mechanic_dictionary.json
    // -----------------------------------------------------------------------
    int total_pokemon   = 0;
    int total_attacks   = 0;
    int count_basic     = 0;
    int count_resolved  = 0;
    int count_unknown   = 0;

    {
        std::ofstream out(attack_dict_path);
        if (!out.is_open())
        {
            std::cerr << "[warn] Could not write attack mechanic dictionary: " << attack_dict_path << "\n";
            return false;
        }

        out << "{\n";
        bool first_card = true;

        for (const auto& card : db.Cards)
        {
            if (card.type != CardType::Pokemon) continue;
            if (card.attacks.empty()) continue;

            ++total_pokemon;
            if (!first_card) out << ",\n";
            first_card = false;

            out << "  \"" << json_escape(card.id.to_string()) << "\": {\n";
            out << "    \"attacks\": [\n";

            for (int ai = 0; ai < (int)card.attacks.size(); ++ai)
            {
                ++total_attacks;
                const Attack& atk = card.attacks[ai];

                std::string mtype = "BasicDamage";
                std::string mparams = "{}";

                if (atk.effect.has_value())
                {
                    auto it = mmap.find(*atk.effect);
                    if (it != mmap.end())
                    {
                        mtype   = it->second->type_name();
                        mparams = it->second->params_json();
                        if (mtype == "BasicDamage") ++count_basic;
                        else                        ++count_resolved;
                    }
                    else
                    {
                        mtype = "Unknown";
                        ++count_unknown;
                        std::cerr << "[warn] unknown attack effect: \""
                                  << *atk.effect << "\"\n";
                    }
                }
                else
                {
                    ++count_basic;
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
    }

    // Report stats
    std::cout << "[info] Mechanic dictionary: "
              << resolved_effects.size() << " recognised, "
              << unresolved_effects.size() << " unrecognised\n";
    std::cout << "[info] Attack mechanic dictionary: "
              << total_pokemon << " Pokemon processed, "
              << total_attacks << " attacks mapped ("
              << count_basic << " BasicDamage, "
              << count_resolved << " resolved, "
              << count_unknown << " unknown)\n";
    std::cout << "[info] Output: " << mechanic_path << "\n";
    std::cout << "[info] Output: " << attack_dict_path << "\n";

    return true;
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
    // Determine sibling paths for the intermediate files
    // (same directory as the database file)
    std::string dir;
    auto slash = path.find_last_of("/\\");
    if (slash != std::string::npos)
        dir = path.substr(0, slash + 1);

    const std::string mechanic_path    = dir + "mechanic_dictionary.json";
    const std::string attack_dict_path = dir + "attack_mechanic_dictionary.json";

    // Auto-build intermediate files if either is absent
    {
        std::ifstream m(mechanic_path);
        std::ifstream a(attack_dict_path);
        if (!m.is_open() || !a.is_open())
        {
            build_dictionaries(path, mechanic_path, attack_dict_path);
        }
    }

    // Parse the raw JSON
    Database db = Database::parse_json(path);

    // Apply mechanic resolution from the attack dictionary
    db.resolve_mechanics(attack_dict_path);

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
