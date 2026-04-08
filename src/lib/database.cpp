#define JSMN_STATIC
#include "jsmn.h"

#include "ptcgp_sim/database.h"

#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace ptcgp_sim 
{

// ---------------------------------------------------------------------------
// jsmn helpers
// ---------------------------------------------------------------------------

static std::string tok_str(const char* json, const jsmntok_t& t) 
{
    return std::string(json + t.start, t.end - t.start);
}

static bool tok_eq(const char* json, const jsmntok_t& t, const char* s) 
{
    int len = t.end - t.start;

    return (int)strlen(s) == len && strncmp(json + t.start, s, len) == 0;
}

// ---------------------------------------------------------------------------
// EnergyType parsing
// ---------------------------------------------------------------------------

static EnergyType parse_energy_type(const std::string& s) 
{
    if (s == "Grass")     return EnergyType::Grass;
    if (s == "Fire")      return EnergyType::Fire;
    if (s == "Water")     return EnergyType::Water;
    if (s == "Lightning") return EnergyType::Lightning;
    if (s == "Psychic")   return EnergyType::Psychic;
    if (s == "Fighting")  return EnergyType::Fighting;
    if (s == "Darkness")  return EnergyType::Darkness;
    if (s == "Metal")     return EnergyType::Metal;
    if (s == "Dragon")    return EnergyType::Dragon;
    if (s == "Colorless") return EnergyType::Colorless;
    return EnergyType::Unknown;
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
    // Read entire file into memory
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open())
        throw std::runtime_error("Database::load: cannot open " + path);

    std::ostringstream ss;
    ss << file.rdbuf();
    const std::string json = ss.str();
    const char* js = json.c_str();
    const int   js_len = (int)json.size();

    // First pass: count tokens
    jsmn_parser parser;
    jsmn_init(&parser);
    int ntok = jsmn_parse(&parser, js, js_len, nullptr, 0);
    if (ntok < 0)
        throw std::runtime_error("Database::load: jsmn failed to parse JSON");

    std::vector<jsmntok_t> tokens((size_t)ntok);
    jsmn_init(&parser);
    jsmn_parse(&parser, js, js_len, tokens.data(), (unsigned int)ntok);

    Database db;

    // tokens[0] is the top-level array
    // Each element is an object { "Pokemon": {...} } or { "Trainer": {...} }
    int i = 1; // current token index
    while (i < ntok) 
    {
        // Expect an object with one key
        if (tokens[i].type != JSMN_OBJECT) { ++i; continue; }

        int obj_size = tokens[i].size; // number of key-value pairs
        ++i;

        for (int kv = 0; kv < obj_size; ++kv) 
        {
            // Key: "Pokemon" or "Trainer"
            bool is_pokemon = tok_eq(js, tokens[i], "Pokemon");
            bool is_trainer = tok_eq(js, tokens[i], "Trainer");
            ++i; // move to value (inner object)

            if (!is_pokemon && !is_trainer) 
            {
                // Skip unknown top-level key and its value subtree
                if (tokens[i].type == JSMN_OBJECT || tokens[i].type == JSMN_ARRAY) 
                {
                    int end = tokens[i].end;
                    ++i;
                    while (i < ntok && tokens[i].start < end) ++i;
                } 
                else 
                {
                    ++i;
                }
                continue;
            }

            // tokens[i] is the inner object { "id": ..., "hp": ..., ... }
            if (tokens[i].type != JSMN_OBJECT) { ++i; continue; }

            Card card;
            card.type = is_pokemon ? CardType::Pokemon : CardType::Trainer;

            int inner_size = tokens[i].size;
            ++i; // move into inner object fields

            for (int f = 0; f < inner_size; ++f) 
            {
                if (i >= ntok) break;
                const jsmntok_t& key = tokens[i];
                ++i;
                if (i >= ntok) break;
                const jsmntok_t& val = tokens[i];

                if (tok_eq(js, key, "id")) 
                {
                    card.id = tok_str(js, val);
                    ++i;
                } 
                else if (tok_eq(js, key, "name")) 
                {
                    card.name = tok_str(js, val);
                    ++i;
                } 
                else if (tok_eq(js, key, "hp")) 
                {
                    card.hp = std::stoi(tok_str(js, val));
                    ++i;
                } 
                else if (tok_eq(js, key, "energy_type")) 
                {
                    card.energy_type = parse_energy_type(tok_str(js, val));
                    ++i;
                } 
                else if (tok_eq(js, key, "weakness")) 
                {
                    if (val.type == JSMN_PRIMITIVE && js[val.start] == 'n') 
                    {
                        card.weakness = std::nullopt;
                    } 
                    else 
                    {
                        card.weakness = parse_energy_type(tok_str(js, val));
                    }
                    ++i;
                } 
                else if (tok_eq(js, key, "retreat_cost")) 
                {
                    if (val.type == JSMN_ARRAY) 
                    {
                        int arr_size = val.size;
                        ++i; // move past the array token into elements
                        for (int a = 0; a < arr_size; ++a) 
                        {
                            if (i >= ntok) break;
                            card.retreat_cost.push_back(
                                parse_energy_type(tok_str(js, tokens[i])));
                            ++i;
                        }
                    } 
                    else 
                    {
                        ++i;
                    }
                } 
                else 
                {
                    // Skip unknown field value (may be object/array subtree)
                    if (val.type == JSMN_OBJECT || val.type == JSMN_ARRAY) 
                    {
                        int end = val.end;
                        ++i;
                        while (i < ntok && tokens[i].start < end) ++i;
                    } 
                    else 
                    {
                        ++i;
                    }
                }
            }

            db.Cards.push_back(std::move(card));
        }
    }

    return db;
}

// ---------------------------------------------------------------------------
// Database::find_by_id
// ---------------------------------------------------------------------------

const Card* Database::find_by_id(const std::string& id) const 
{
    for (const Card& c : Cards)
        if (c.id == id) return &c;
    return nullptr;
}

} // namespace ptcgp_sim
