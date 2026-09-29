/*
 * mod-era-events: Scourge Invasion (tier 6, after Ahn'Qiraj and before Naxxramas).
 *
 * mod-individual-progression already covers the zones around the necropolises with shards and
 * minions for this tier. This event adds what the original also had, taking turns:
 *
 *   City assault      A Pallid Horror or Patchwork Terror marches through Stormwind or Undercity
 *                     along the original paths, dragging Flameshockers with it. Kill it to win.
 *   Necropolis surge  The Herald of the Lich King raises three of the invasion's rare elites and
 *                     a stream of minions beside one of the necropolises. Kill the three to win.
 *
 * Released under the MIT License.
 */

#include "EraEventMobs.h"
#include "EraEvents.h"

#include "Creature.h"
#include "CreatureAI.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "MotionMaster.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "StringFormat.h"
#include "TemporarySummon.h"

namespace EraEvents
{
    namespace
    {
        // creature_template copies from data/sql/db-world.
        enum Creatures : uint32
        {
            NPC_SI_HERALD             = 9500800, // Herald of the Lich King, the event's anchor
            NPC_SI_PALLID_HORROR      = 9500801,
            NPC_SI_PATCHWORK_TERROR   = 9500802,
            NPC_SI_FLAMESHOCKER       = 9500803,
            NPC_SI_LUMBERING_HORROR   = 9500804,
            NPC_SI_SPIRIT_OF_DAMNED   = 9500805,
            NPC_SI_BONE_WITCH         = 9500806,
            NPC_SI_GHOUL_BERSERKER    = 9500807,
            NPC_SI_SPECTRAL_SOLDIER   = 9500808,
            NPC_SI_SKELETAL_SHOCKTROOPER = 9500809,
        };

        enum Spells : uint32
        {
            SPELL_MINION_SPAWN_IN       = 28234, // pink lightning
            SPELL_AURA_OF_FEAR          = 28313,
            SPELL_FLAMESHOCKERS_TOUCH   = 28314,
            SPELL_FLAMESHOCKERS_TOUCH2  = 28329,
            SPELL_FLAMESHOCKERS_REVENGE = 28323,
            SPELL_KNOCKDOWN             = 16790,
            SPELL_TRAMPLE               = 5568,
            SPELL_RIBBON_OF_SOULS       = 16243,
            SPELL_PSYCHIC_SCREAM        = 22884,
            SPELL_ARCANE_BOLT           = 13748,
            SPELL_INFECTED_BITE         = 7367,
            SPELL_ENRAGE                = 8599,
            SPELL_DEMORALIZING_SHOUT    = 16244,
            SPELL_SUNDER_ARMOR          = 21081,
            SPELL_BONE_SHARDS           = 17014,
        };

        // Herald lines (creature_text groups on NPC_SI_HERALD).
        enum HeraldTexts : uint8
        {
            SAY_HERALD_START   = 0,
            SAY_HERALD_DEFEAT  = 1,
            SAY_HERALD_TAUNT   = 2,
        };

        constexpr uint32 MAP_EASTERN_KINGDOMS = 0;
        constexpr uint32 MAP_KALIMDOR = 1;

        constexpr uint32 ZONE_STORMWIND = 1519;
        constexpr uint32 ZONE_UNDERCITY = 1497;

        constexpr uint32 DATA_PATH = 1;

        // The core's city attack spawn points and the paths that go with them
        // (WorldState::SummonPallid).
        struct CityRoute
        {
            char const* where;
            Position spawn;
            uint32 path;
        };

        struct City
        {
            char const* name;
            uint32 zoneId;
            std::array<CityRoute, 2> routes;
        };

        std::array<City, 2> const CITIES = {{
            { "Stormwind", ZONE_STORMWIND, {{
                { "Stormwind Keep",     { -8578.15f, 886.382f, 87.3148f, 0.586275f }, 163941 },
                { "the Trade District", { -8578.15f, 886.382f, 87.3148f, 0.586275f }, 163942 },
            }}},
            { "Undercity", ZONE_UNDERCITY, {{
                { "the Royal Quarter",  { 1595.87f, 440.539f, -46.3349f, 2.28207f }, 163944 },
                { "the Trade Quarter",  { 1659.2f, 265.988f, -62.1788f, 3.64283f }, 163943 },
            }}},
        }};

        // One of mod-individual-progression's necrotic shards in each invasion zone: open ground
        // next to a necropolis.
        struct SurgeSite
        {
            char const* zone;
            uint32 mapId;
            uint32 zoneId;
            Position center;
        };

        std::array<SurgeSite, 6> const SURGE_SITES = {{
            { "Winterspring",         MAP_KALIMDOR,         618, { 6279.14f, -4775.33f, 756.56f, 6.18776f } },
            { "Tanaris",              MAP_KALIMDOR,         440, { -8166.66f, -3809.78f, 15.0426f, 0.149923f } },
            { "Azshara",              MAP_KALIMDOR,          16, { 3515.43f, -4149.34f, 106.829f, 0.0640161f } },
            { "the Blasted Lands",    MAP_EASTERN_KINGDOMS,   4, { -11183.3f, -2985.49f, 8.22008f, 0.0105759f } },
            { "the Eastern Plaguelands", MAP_EASTERN_KINGDOMS, 139, { 1933.74f, -3099.59f, 87.1927f, 0.112467f } },
            { "the Burning Steppes",  MAP_EASTERN_KINGDOMS,  46, { -7722.25f, -2234.68f, 136.647f, 6.20495f } },
        }};

        std::array<uint32, 3> const SURGE_RARES = { NPC_SI_LUMBERING_HORROR, NPC_SI_SPIRIT_OF_DAMNED, NPC_SI_BONE_WITCH };
        std::array<uint32, 3> const SURGE_MINIONS = { NPC_SI_GHOUL_BERSERKER, NPC_SI_SPECTRAL_SOLDIER, NPC_SI_SKELETAL_SHOCKTROOPER };

        constexpr uint32 SURGE_WAVE_MS = 45 * IN_MILLISECONDS;
        constexpr uint32 SURGE_WAVE_SIZE = 4;
        constexpr uint32 SURGE_MAX_MINIONS = 16;
        constexpr uint32 TAUNT_MS = 3 * MINUTE * IN_MILLISECONDS;

        enum class Variant : uint8
        {
            CityAssault,
            NecropolisSurge,
        };

        class ScourgeInvasionEvent : public EraEvent
        {
        public:
            ScourgeInvasionEvent() : EraEvent(EVENT_SCOURGE_INVASION),
                _next(urand(0, 1) ? Variant::CityAssault : Variant::NecropolisSurge) { }

            char const* GetKey() const override { return "si"; }
            char const* GetName() const override { return "Scourge Invasion"; }
            char const* GetConfigName() const override { return "ScourgeInvasion"; }
            uint8 GetTier() const override { return 6; }

            bool Prepare(std::vector<Player*> const& cohort) override
            {
                _variant = _next;
                _next = _variant == Variant::CityAssault ? Variant::NecropolisSurge : Variant::CityAssault;

                if (_variant == Variant::CityAssault)
                {
                    // The city of whichever side has more players of this era online.
                    uint32 alliance = 0;
                    uint32 horde = 0;
                    for (Player* player : cohort)
                        (player->GetTeamId() == TEAM_ALLIANCE ? alliance : horde)++;

                    _city = alliance == horde ? urand(0, 1) : (alliance > horde ? 0 : 1);
                    _route = urand(0, 1);
                }
                else
                    _site = urand(0, SURGE_SITES.size() - 1);

                return true;
            }

            std::string GetWarningText(uint32 minutes) const override
            {
                if (_variant == Variant::CityAssault)
                    return Acore::StringFormat("Scourge abominations are gathering outside {}. They will break into {} in {} minutes!",
                        City().name, Route().where, minutes);

                return Acore::StringFormat("The necropolis over {} is stirring. The Herald of the Lich King arrives in {} minutes!",
                    Site().zone, minutes);
            }

            std::string GetStartText() const override
            {
                if (_variant == Variant::CityAssault)
                    return Acore::StringFormat("The Scourge is attacking {}! An abomination is loose in {}.", City().name, Route().where);

                return Acore::StringFormat("The Herald of the Lich King has raised the dead in {}! Destroy his three champions.", Site().zone);
            }

            std::string GetEndText(bool won) const override
            {
                if (_variant == Variant::CityAssault)
                    return won
                        ? Acore::StringFormat("The abomination has fallen. {} stands!", City().name)
                        : Acore::StringFormat("The Scourge withdraws from {}, leaving its dead behind.", City().name);

                return won
                    ? Acore::StringFormat("The Herald's champions are destroyed. {} is safe, for now.", Site().zone)
                    : Acore::StringFormat("The dead of {} sink back into the earth. The Herald will return.", Site().zone);
            }

            bool IsEventArea(uint32 mapId, uint32 zoneId, uint32 /*areaId*/) const override
            {
                if (_variant == Variant::CityAssault)
                    return mapId == MAP_EASTERN_KINGDOMS && zoneId == City().zoneId;

                return mapId == Site().mapId && zoneId == Site().zoneId;
            }

            bool Start() override
            {
                _tauntTimer = TAUNT_MS;
                _waveTimer = 0;
                _raresLeft = 0;

                if (_variant == Variant::CityAssault)
                {
                    CityRoute const& route = Route();
                    if (!SpawnAnchor(MAP_EASTERN_KINGDOMS, route.spawn, NPC_SI_HERALD))
                        return false;

                    uint32 entry = urand(0, 1) ? NPC_SI_PALLID_HORROR : NPC_SI_PATCHWORK_TERROR;
                    Creature* horror = Summon(entry, route.spawn);
                    if (!horror)
                        return false;

                    horror->AI()->SetData(DATA_PATH, route.path);
                    return true;
                }

                SurgeSite const& site = Site();
                Creature* herald = SpawnAnchor(site.mapId, site.center, NPC_SI_HERALD);
                if (!herald)
                    return false;

                herald->AI()->Talk(SAY_HERALD_START);

                for (uint32 entry : SURGE_RARES)
                {
                    if (Creature* rare = Summon(entry, RandomPointAround(site.center, 12.0f, 25.0f)))
                    {
                        rare->CastSpell(rare, SPELL_MINION_SPAWN_IN, true);
                        ++_raresLeft;
                    }
                }

                return _raresLeft > 0;
            }

            void Update(uint32 diff) override
            {
                Creature* anchor = GetAnchor();
                if (!anchor)
                    return;

                if (_tauntTimer <= diff)
                {
                    _tauntTimer = TAUNT_MS;
                    if (_variant == Variant::NecropolisSurge)
                        anchor->AI()->Talk(SAY_HERALD_TAUNT);
                }
                else
                    _tauntTimer -= diff;

                if (_variant != Variant::NecropolisSurge)
                    return;

                if (_waveTimer > diff)
                {
                    _waveTimer -= diff;
                    return;
                }

                _waveTimer = SURGE_WAVE_MS;

                uint32 alive = 0;
                for (uint32 entry : SURGE_MINIONS)
                    alive += CountAlive(entry);

                Position const& center = Site().center;
                for (uint32 i = 0; i < SURGE_WAVE_SIZE && alive < SURGE_MAX_MINIONS; ++i, ++alive)
                {
                    uint32 entry = SURGE_MINIONS[urand(0, SURGE_MINIONS.size() - 1)];
                    if (Creature* minion = Summon(entry, RandomPointAround(center, 25.0f, 40.0f)))
                        SendTo(minion, RandomPointAround(center, 5.0f, 15.0f), false);
                }
            }

            void OnSummonDied(ObjectGuid /*guid*/, uint32 entry) override
            {
                if (_variant == Variant::CityAssault)
                {
                    if (entry == NPC_SI_PALLID_HORROR || entry == NPC_SI_PATCHWORK_TERROR)
                        MarkWon();
                    return;
                }

                if (std::find(SURGE_RARES.begin(), SURGE_RARES.end(), entry) == SURGE_RARES.end())
                    return;

                if (_raresLeft && --_raresLeft == 0)
                {
                    if (Creature* herald = GetAnchor())
                        herald->AI()->Talk(SAY_HERALD_DEFEAT);
                    MarkWon();
                }
            }

        private:
            struct City const& City() const { return CITIES[_city]; }
            CityRoute const& Route() const { return CITIES[_city].routes[_route]; }
            SurgeSite const& Site() const { return SURGE_SITES[_site]; }

            Variant _variant = Variant::CityAssault;
            Variant _next;
            uint32 _city = 0;
            uint32 _route = 0;
            uint32 _site = 0;
            uint32 _raresLeft = 0;
            uint32 _waveTimer = 0;
            uint32 _tauntTimer = 0;
        };
    }

    std::unique_ptr<EraEvent> CreateScourgeInvasionEvent()
    {
        return std::make_unique<ScourgeInvasionEvent>();
    }
}

using namespace EraEvents;

// The core's npc_pallid_horror writes the global Scourge Invasion state when it dies, so the event
// uses its own: march the path, drag Flameshockers along, and keep sending more at whoever is
// fighting.
struct npc_era_pallid_horror : public ScriptedAI
{
    npc_era_pallid_horror(Creature* creature) : ScriptedAI(creature), _summons(me) { }

    void InitializeAI() override
    {
        me->AddAura(SPELL_AURA_OF_FEAR, me);
        me->SetWalk(false);
        ScheduleTasks();
    }

    void SetData(uint32 type, uint32 value) override
    {
        if (type != DATA_PATH)
            return;

        me->GetMotionMaster()->Clear(false);
        me->GetMotionMaster()->MoveWaypoint(value, false);
    }

    void ScheduleTasks()
    {
        scheduler.Schedule(0s, [this](TaskContext /*context*/)
        {
            SummonEscort();
        }).Schedule(1s, [this](TaskContext context)
        {
            Talk(0);
            context.Repeat(65s, 300s);
        }).Schedule(20s, [this](TaskContext context)
        {
            SummonAtAttacker();
            context.Repeat(20s, 30s);
        });
    }

    // 5 to 9 Flameshockers around it, as sniffed for the original.
    void SummonEscort()
    {
        uint32 const amount = urand(5, 9);
        for (uint32 i = 0; i < amount; ++i)
        {
            if (Creature* summon = me->SummonCreature(NPC_SI_FLAMESHOCKER, me->GetPosition(), TEMPSUMMON_CORPSE_TIMED_DESPAWN, 30 * IN_MILLISECONDS))
            {
                float angle = float(i) * (float(M_PI) / (float(amount) / 2.0f)) + me->GetOrientation();
                summon->GetMotionMaster()->Clear(true);
                summon->GetMotionMaster()->MoveFollow(me, 2.5f, angle);
            }
        }
    }

    // A pair of Flameshockers rises next to someone fighting the assault.
    void SummonAtAttacker()
    {
        if (_summons.size() >= 20)
            return;

        std::list<Player*> players;
        Acore::AnyPlayerInObjectRangeCheck check(me, 60.0f);
        Acore::PlayerListSearcher<Acore::AnyPlayerInObjectRangeCheck> searcher(me, players, check);
        Cell::VisitObjects(me, searcher, 60.0f);

        players.remove_if([](Player* player) { return player->IsGameMaster() || !player->IsAlive(); });
        if (players.empty())
            return;

        Player* target = Acore::Containers::SelectRandomContainerElement(players);
        for (uint8 i = 0; i < 2; ++i)
        {
            float x, y, z;
            target->GetNearPoint(target, x, y, z, 1.0f, 5.0f, frand(0.0f, 2.0f * float(M_PI)));
            if (Creature* summon = me->SummonCreature(NPC_SI_FLAMESHOCKER, x, y, z, 0.0f, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 30 * IN_MILLISECONDS))
                summon->AI()->AttackStart(target);
        }
    }

    void JustSummoned(Creature* summon) override
    {
        summon->CastSpell(summon, SPELL_MINION_SPAWN_IN, true);
        summon->SetWalk(false);
        _summons.Summon(summon);
        NotifySummoned(me, summon);
    }

    void SummonedCreatureDies(Creature* summon, Unit* /*killer*/) override
    {
        _summons.Despawn(summon);
    }

    void JustDied(Unit* /*killer*/) override
    {
        _summons.DoForAllSummons([](WorldObject* summon)
        {
            if (Creature* creature = summon->ToCreature())
                if (creature->IsAlive())
                    creature->KillSelf();
        });

        NotifyCreatureDied(me);
    }

    void UpdateAI(uint32 diff) override
    {
        scheduler.Update(diff);
        if (!UpdateVictim())
            return;

        DoMeleeAttackIfReady();
    }

private:
    SummonList _summons;
};

void AddEraEventScourgeScripts()
{
    RegisterMob(NPC_SI_FLAMESHOCKER, {
        .spells = {
            { SPELL_FLAMESHOCKERS_TOUCH,  30000, 45000, SpellTarget::Victim },
            { SPELL_FLAMESHOCKERS_TOUCH2, 30000, 45000, SpellTarget::Victim },
        },
        .deathSpell = SPELL_FLAMESHOCKERS_REVENGE,
    });

    RegisterMob(NPC_SI_LUMBERING_HORROR, {
        .spells = {
            { SPELL_KNOCKDOWN, 10000, 15000, SpellTarget::Victim },
            { SPELL_TRAMPLE,    8000, 12000, SpellTarget::Self },
        },
        .aura = SPELL_AURA_OF_FEAR,
    });

    RegisterMob(NPC_SI_SPIRIT_OF_DAMNED, {
        .spells = {
            { SPELL_RIBBON_OF_SOULS,  5000,  8000, SpellTarget::RandomEnemy },
            { SPELL_PSYCHIC_SCREAM,  20000, 30000, SpellTarget::Self },
        },
    });

    RegisterMob(NPC_SI_BONE_WITCH, {
        .spells = {
            { SPELL_ARCANE_BOLT, 4000, 6000, SpellTarget::Victim },
        },
    });

    RegisterMob(NPC_SI_GHOUL_BERSERKER, {
        .spells = {
            { SPELL_INFECTED_BITE, 13000, 18000, SpellTarget::Victim },
        },
        .spawnVisual = SPELL_MINION_SPAWN_IN,
        .enrageSpell = SPELL_ENRAGE,
        .enragePct = 30,
    });

    RegisterMob(NPC_SI_SPECTRAL_SOLDIER, {
        .spells = {
            { SPELL_DEMORALIZING_SHOUT, 20000, 25000, SpellTarget::Self },
            { SPELL_SUNDER_ARMOR,        6000, 10000, SpellTarget::Victim },
        },
        .spawnVisual = SPELL_MINION_SPAWN_IN,
    });

    RegisterMob(NPC_SI_SKELETAL_SHOCKTROOPER, {
        .spells = {
            { SPELL_BONE_SHARDS, 16000, 20000, SpellTarget::Victim },
        },
        .spawnVisual = SPELL_MINION_SPAWN_IN,
    });

    RegisterCreatureAI(npc_era_pallid_horror);
}
