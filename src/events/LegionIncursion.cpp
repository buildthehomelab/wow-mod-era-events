/*
 * mod-era-events: Legion Incursion (tier 7, after Naxxramas and before the Dark Portal opens).
 *
 * The run-up to the Burning Crusade. mod-individual-progression already stages the Legion's
 * assault on the Dark Portal for this tier; this event carries it across Azeroth: a fel portal tears
 * open outside a town and demons march on it in waves, a Dreadlord leads the middle wave and a Pit
 * Commander comes through last. Kill the Pit Commander to close the portal.
 *
 * Released under the MIT License.
 */

#include "EraEventMobs.h"
#include "EraEvents.h"

#include "Creature.h"
#include "CreatureAI.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "StringFormat.h"
#include "TemporarySummon.h"

namespace EraEvents
{
    namespace
    {
        // creature_template copies from data/sql/db-world.
        enum Creatures : uint32
        {
            NPC_LEGION_ANCHOR      = 9500820, // Legion Transporter, invisible
            NPC_LEGION_FELGUARD    = 9500821,
            NPC_LEGION_INFERNAL    = 9500822,
            NPC_LEGION_FEL_STALKER = 9500823,
            NPC_LEGION_VOIDWALKER  = 9500824,
            NPC_LEGION_ANGUISHER   = 9500825,
            NPC_LEGION_DREADLORD   = 9500826,
            NPC_LEGION_PIT_COMMANDER = 9500827,
        };

        enum Spells : uint32
        {
            SPELL_CLEAVE        = 15284,
            SPELL_WAR_STOMP     = 16727,
            SPELL_KNOCK_AWAY    = 10101,
            SPELL_IMMOLATION    = 19483,
            SPELL_MANA_BURN     = 11981,
            SPELL_SHADOW_BOLT   = 20825,
            SPELL_SHADOW_VOLLEY = 17228,
            SPELL_SLEEP         = 12098,
        };

        // The green fel portal of the Hellfire invasion ("Infernaling Summoner Visual").
        constexpr uint32 GO_FEL_PORTAL = 183283;

        constexpr uint32 MAP_EASTERN_KINGDOMS = 0;
        constexpr uint32 MAP_KALIMDOR = 1;

        // Where the portal opens (open ground at the edge of town) and where the demons head for.
        struct Site
        {
            char const* town;
            char const* zone;
            uint32 mapId;
            uint32 zoneId;
            Position portal;
            Position target;
        };

        std::array<Site, 5> const SITES = {{
            { "Gadgetzan",           "Tanaris",                MAP_KALIMDOR,         440,
                { -7108.6f, -3829.62f, 9.62f, 3.6f },   { -7157.02f, -3755.97f, 8.47f, 0.13f } },
            { "Everlook",            "Winterspring",           MAP_KALIMDOR,         618,
                { 6749.09f, -4713.45f, 721.3f, 4.33f },  { 6726.46f, -4654.4f, 720.99f, 3.56f } },
            { "Cenarion Hold",       "Silithus",               MAP_KALIMDOR,        1377,
                { -6874.96f, 722.57f, 45.75f, 0.54f },  { -6817.11f, 776.11f, 47.89f, 3.89f } },
            { "Nethergarde Keep",    "the Blasted Lands",      MAP_EASTERN_KINGDOMS,   4,
                { -11003.2f, -3403.04f, 62.07f, 3.85f }, { -11002.2f, -3333.93f, 65.03f, 2.06f } },
            { "Light's Hope Chapel", "the Eastern Plaguelands", MAP_EASTERN_KINGDOMS, 139,
                { 2275.17f, -5394.57f, 86.57f, 0.45f },  { 2288.8f, -5319.14f, 89.0f, 2.2f } },
        }};

        std::array<uint32, 5> const DEMONS = {
            NPC_LEGION_FELGUARD, NPC_LEGION_INFERNAL, NPC_LEGION_FEL_STALKER, NPC_LEGION_VOIDWALKER, NPC_LEGION_ANGUISHER
        };

        constexpr uint32 WAVE_COUNT = 6;
        constexpr uint32 DREADLORD_WAVE = 3;
        constexpr uint32 WAVE_MS = 50 * IN_MILLISECONDS;
        constexpr uint32 FIRST_WAVE_MS = 10 * IN_MILLISECONDS;
        constexpr uint32 MAX_DEMONS = 24;

        class LegionIncursionEvent : public EraEvent
        {
        public:
            LegionIncursionEvent() : EraEvent(EVENT_LEGION_INCURSION) { }

            char const* GetKey() const override { return "legion"; }
            char const* GetName() const override { return "Legion Incursion"; }
            char const* GetConfigName() const override { return "LegionIncursion"; }
            uint8 GetTier() const override { return 7; }

            bool Prepare(std::vector<Player*> const& /*cohort*/) override
            {
                _site = urand(0, SITES.size() - 1);
                return true;
            }

            std::string GetWarningText(uint32 minutes) const override
            {
                return Acore::StringFormat("Fel energy is gathering outside {} in {}. The Burning Legion will break through in {} minutes!",
                    SiteRef().town, SiteRef().zone, minutes);
            }

            std::string GetStartText() const override
            {
                return Acore::StringFormat("A fel portal has torn open outside {} in {}! Demons are marching on the town.",
                    SiteRef().town, SiteRef().zone);
            }

            std::string GetEndText(bool won) const override
            {
                return won
                    ? Acore::StringFormat("The Pit Commander is slain and the portal outside {} collapses. The Dark Portal still waits...", SiteRef().town)
                    : Acore::StringFormat("The portal outside {} fades. The Legion has taken what it came for.", SiteRef().town);
            }

            bool IsEventArea(uint32 mapId, uint32 zoneId, uint32 /*areaId*/) const override
            {
                return mapId == SiteRef().mapId && zoneId == SiteRef().zoneId;
            }

            bool Start() override
            {
                Site const& site = SiteRef();
                if (!SpawnAnchor(site.mapId, site.portal, NPC_LEGION_ANCHOR))
                    return false;

                SummonObject(GO_FEL_PORTAL, site.portal);
                _wave = 0;
                _waveTimer = FIRST_WAVE_MS;
                _bossUp = false;
                return true;
            }

            void Update(uint32 diff) override
            {
                if (_bossUp || !GetAnchor())
                    return;

                if (_waveTimer > diff)
                {
                    _waveTimer -= diff;
                    return;
                }

                _waveTimer = WAVE_MS;
                ++_wave;

                if (_wave > WAVE_COUNT)
                {
                    SpawnBoss();
                    return;
                }

                SpawnWave(_wave);
            }

            void OnSummonDied(ObjectGuid /*guid*/, uint32 entry) override
            {
                if (entry == NPC_LEGION_PIT_COMMANDER)
                    MarkWon();
            }

        private:
            Site const& SiteRef() const { return SITES[_site]; }

            void SpawnWave(uint32 wave)
            {
                Site const& site = SiteRef();

                uint32 alive = 0;
                for (uint32 entry : DEMONS)
                    alive += CountAlive(entry);

                for (uint32 i = 0; i < 4 + wave && alive < MAX_DEMONS; ++i, ++alive)
                {
                    uint32 entry = DEMONS[urand(0, DEMONS.size() - 1)];
                    if (Creature* demon = Summon(entry, RandomPointAround(site.portal, 0.0f, 6.0f)))
                        SendTo(demon, RandomPointAround(site.target, 0.0f, 12.0f));
                }

                if (wave == DREADLORD_WAVE)
                {
                    if (Creature* dreadlord = Summon(NPC_LEGION_DREADLORD, site.portal))
                    {
                        dreadlord->Yell("Your world is ripe for conquest. The Legion has come to harvest it!", LANG_UNIVERSAL);
                        SendTo(dreadlord, site.target, false);
                    }
                }
            }

            void SpawnBoss()
            {
                _bossUp = true;
                Site const& site = SiteRef();
                if (Creature* boss = Summon(NPC_LEGION_PIT_COMMANDER, site.portal))
                {
                    boss->Yell("Kneel before the Burning Legion! Every one of you will burn!", LANG_UNIVERSAL);
                    SendTo(boss, site.target, false);
                }

                for (uint32 i = 0; i < 3; ++i)
                    if (Creature* guard = Summon(NPC_LEGION_FELGUARD, RandomPointAround(site.portal, 2.0f, 6.0f)))
                        SendTo(guard, RandomPointAround(site.target, 0.0f, 8.0f), false);
            }

            uint32 _site = 0;
            uint32 _wave = 0;
            uint32 _waveTimer = 0;
            bool _bossUp = false;
        };
    }

    std::unique_ptr<EraEvent> CreateLegionIncursionEvent()
    {
        return std::make_unique<LegionIncursionEvent>();
    }
}

using namespace EraEvents;

void AddEraEventLegionScripts()
{
    RegisterMob(NPC_LEGION_FELGUARD, {
        .spells = {
            { SPELL_CLEAVE, 6000, 9000, SpellTarget::Victim },
        },
    });

    RegisterMob(NPC_LEGION_INFERNAL, {
        .aura = SPELL_IMMOLATION,
    });

    RegisterMob(NPC_LEGION_FEL_STALKER, {
        .spells = {
            { SPELL_MANA_BURN, 10000, 15000, SpellTarget::RandomEnemy },
        },
    });

    RegisterMob(NPC_LEGION_VOIDWALKER, { });

    RegisterMob(NPC_LEGION_ANGUISHER, {
        .spells = {
            { SPELL_SHADOW_BOLT, 3000, 5000, SpellTarget::Victim },
        },
    });

    RegisterMob(NPC_LEGION_DREADLORD, {
        .spells = {
            { SPELL_SHADOW_VOLLEY, 10000, 14000, SpellTarget::Self },
            { SPELL_SLEEP,         20000, 25000, SpellTarget::RandomEnemy },
        },
    });

    RegisterMob(NPC_LEGION_PIT_COMMANDER, {
        .spells = {
            { SPELL_CLEAVE,     6000,  9000, SpellTarget::Victim },
            { SPELL_WAR_STOMP, 14000, 18000, SpellTarget::Self },
            { SPELL_KNOCK_AWAY, 18000, 24000, SpellTarget::Victim },
        },
    });
}
