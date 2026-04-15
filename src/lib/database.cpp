#include "jsmn_helpers.h"

#include "ptcgp_sim/common.h"
#include "ptcgp_sim/database.h"

#include <cassert>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
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
    // Find the space separator between expansion code and number
    auto pos = full_id.find(' ');
    assert(pos != std::string::npos && pos > 0 && pos + 1 < full_id.size()
        && "Database::parse_id: invalid id format, expected \"<expansion> <number>\"");

    CardId result;
    result.expansion = full_id.substr(0, pos);
    result.number    = std::stoi(full_id.substr(pos + 1));
    return result;
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
            bool is_pokemon = jsmn_tok_eq(js, tokens[i], "Pokemon");
            bool is_trainer = jsmn_tok_eq(js, tokens[i], "Trainer");
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

                if (jsmn_tok_eq(js, key, "id")) 
                {
                    // Parse "A1 002" -> CardId{"A1", 2}
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
                    {
                        card.weakness = std::nullopt;
                    } 
                    else 
                    {
                        card.weakness = energy_from_string(jsmn_tok_str(js, val));
                    }
                    ++i;
                } 
                else if (jsmn_tok_eq(js, key, "retreat_cost"))
                {
                    if (val.type == JSMN_ARRAY) 
                    {
                        int arr_size = val.size;
                        ++i; // move past the array token into elements
                        for (int a = 0; a < arr_size; ++a) 
                        {
                            if (i >= ntok) break;
                            card.retreat_cost.push_back(energy_from_string(jsmn_tok_str(js, tokens[i])));
                            ++i;
                        }
                    } 
                    else 
                    {
                        ++i;
                    }
                } 
                else if (jsmn_tok_eq(js, key, "stage"))
                {
                    card.stage = std::stoi(jsmn_tok_str(js, val));
                    ++i;
                }
                else if (jsmn_tok_eq(js, key, "evolves_from"))
                {
                    if (val.type == JSMN_PRIMITIVE && js[val.start] == 'n')
                    {
                        card.evolves_from = std::nullopt;
                    }
                    else
                    {
                        card.evolves_from = jsmn_tok_str(js, val);
                    }
                    ++i;
                }
                else if (jsmn_tok_eq(js, key, "attacks"))
                {
                    if (val.type == JSMN_ARRAY)
                    {
                        int atk_arr_size = val.size;
                        ++i; // move past the array token
                        for (int a = 0; a < atk_arr_size; ++a)
                        {
                            if (i >= ntok) break;
                            // Each attack is an object
                            if (tokens[i].type != JSMN_OBJECT) { ++i; continue; }
                            int atk_obj_size = tokens[i].size;
                            ++i;
                            Attack atk;
                            for (int af = 0; af < atk_obj_size; ++af)
                            {
                                if (i >= ntok) break;
                                const jsmntok_t& akey = tokens[i]; ++i;
                                if (i >= ntok) break;
                                const jsmntok_t& aval = tokens[i];
                                if (jsmn_tok_eq(js, akey, "name"))
                                {
                                    atk.name = jsmn_tok_str(js, aval);
                                    ++i;
                                }
                                else if (jsmn_tok_eq(js, akey, "damage"))
                                {
                                    atk.damage = std::stoi(jsmn_tok_str(js, aval));
                                    ++i;
                                }
                                else if (jsmn_tok_eq(js, akey, "energy_required"))
                                {
                                    if (aval.type == JSMN_ARRAY)
                                    {
                                        int ecnt = aval.size;
                                        ++i;
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
                                    // Skip unknown attack field
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
                    else
                    {
                        ++i;
                    }
                }
                else
                {
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

const Card* Database::find_by_id(const CardId& id) const 
{
    for (const Card& c : Cards)
        if (c.id == id) return &c;
    return nullptr;
}

} // namespace ptcgp_sim
