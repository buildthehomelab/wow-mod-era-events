/*
 * mod-era-events: the Gates of Ahn'Qiraj (tier 4, after the Scarab Gong and before the war is won).
 *
 * The Qiraji swarm pours out of the Scarab Wall in waves against a line of Kaldorei infantry.
 * Midway the three dragons of the flashback join in, one after another, and after the last wave
 * Lieutenant General Nokhor leads the final push. Kill him to win.
 *
 * Besides the schedule, the war also breaks out the moment anyone completes Bang a Gong!, with no
 * minimum number of players: that's the ringer's moment, and anyone else of the era can join.
 *
 * Released under the MIT License.
 */

#include "EraEventMobs.h"
#include "EraEvents.h"

#include "Creature.h"
#include "CreatureAI.h"
#include "MotionMaster.h"
#include "Player.h"
#include "QuestDef.h"
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
            NPC_AQ_ANCHOR           = 9500810, // invisible trigger
            NPC_AQ_KALDOREI         = 9500811, // Kaldorei Infantry
            NPC_AQ_WASP             = 9500812, // Qiraji Wasp
            NPC_AQ_TANK             = 9500813, // Qiraji Tank
            NPC_AQ_CONQUEROR        = 9500814, // Anubisath Conqueror
            NPC_AQ_MERITHRA         = 9500815,
            NPC_AQ_ARYGOS           = 9500816,
            NPC_AQ_CAELESTRASZ      = 9500817,
            NPC_AQ_NOKHOR           = 9500818, // Lieutenant General Nokhor
        };

        enum Spells : uint32
        {
            SPELL_CLEAVE        = 15284,
            SPELL_THUNDERCLAP   = 15548,
            SPELL_KNOCK_AWAY    = 10101,
            SPELL_SUNDER_ARMOR  = 21081,
            SPELL_REND          = 16509,
            SPELL_STRIKE        = 11976,
        };

        enum Quests : uint32
        {
            QUEST_BANG_A_GONG        = 8743,
            QUEST_SIMPLY_BANG_A_GONG = 108743, // mod-individual-progression's repeatable version
        };

        constexpr uint32 MAP_KALIMDOR = 1;
        constexpr uint32 ZONE_SILITHUS = 1377;

        // In front of the Scarab Wall, where the flashback of A Pawn on the Eternal Board is fought.
        Position const ANCHOR_POS  = { -8064.17f, 1534.23f, 2.61f, 3.14159f };
        Position const GATE_MOUTH  = { -8112.0f, 1524.0f, 2.70f, 0.0f };
        Position const FRONT_LINE  = { -8078.0f, 1522.0f, 2.61f, 3.14159f };
        Position const DRAGON_SPOT = { -8045.0f, 1532.0f, 2.61f, 3.14159f };

        constexpr uint32 WAVE_COUNT = 8;
        constexpr uint32 WAVE_MS = 60 * IN_MILLISECONDS;
        constexpr uint32 FIRST_WAVE_MS = 15 * IN_MILLISECONDS;
        constexpr uint32 MAX_QIRAJI = 30;
        constexpr uint32 DEFENDERS = 12;

        // The flashback's dragons: which wave brings each, and what they say and cast.
        struct Dragon
        {
            uint32 entry;
            uint32 wave;
            uint8 yell;       // creature_text group copied from the original
            uint32 morph;     // human visage to dragon
            uint32 breath;    // the cast they make in the flashback, used here as the breath's look
        };

        std::array<Dragon, 3> const DRAGONS = {{
            { NPC_AQ_MERITHRA,    3, 2, 25105, 24818 },
            { NPC_AQ_ARYGOS,      5, 1, 25107, 50505 },
            { NPC_AQ_CAELESTRASZ, 7, 2, 25106, 54293 },
        }};

        std::array<uint32, 3> const QIRAJI = { NPC_AQ_WASP, NPC_AQ_TANK, NPC_AQ_CONQUEROR };

        class AQWarEvent : public EraEvent
        {
        public:
            AQWarEvent() : EraEvent(EVENT_AQ_WAR) { }

            char const* GetKey() const override { return "aq"; }
            char const* GetName() const override { return "Gates of Ahn'Qiraj"; }
            char const* GetConfigName() const override { return "AQWar"; }
            uint8 GetTier() const override { return 4; }

            bool Prepare(std::vector<Player*> const& /*cohort*/) override { return true; }

            std::string GetWarningText(uint32 minutes) const override
            {
                return Acore::StringFormat("The Scarab Wall trembles. The Qiraji will storm out of Ahn'Qiraj in {} minutes. "
                    "The Cenarion Circle calls every hero to Silithus!", minutes);
            }

            std::string GetStartText() const override
            {
                return "The gates of Ahn'Qiraj are open and the swarm is pouring out! Hold the line at the Scarab Wall in Silithus.";
            }

            std::string GetEndText(bool won) const override
            {
                return won
                    ? "Lieutenant General Nokhor has fallen and the swarm falls back behind the Scarab Wall. Silithus holds!"
                    : "The Qiraji have overrun the Kaldorei line and withdrawn into Ahn'Qiraj. They will come again.";
            }

            bool IsEventArea(uint32 mapId, uint32 zoneId, uint32 /*areaId*/) const override
            {
                return mapId == MAP_KALIMDOR && zoneId == ZONE_SILITHUS;
            }

            bool Start() override
            {
                if (!SpawnAnchor(MAP_KALIMDOR, ANCHOR_POS, NPC_AQ_ANCHOR))
                    return false;

                _wave = 0;
                _waveTimer = FIRST_WAVE_MS;
                _bossUp = false;

                // Two ranks of infantry across the front, facing the gate.
                uint32 spawned = 0;
                for (uint32 i = 0; i < DEFENDERS; ++i)
                {
                    float rank = (i % 2) ? 4.0f : 0.0f;
                    float across = (float(i / 2) - float(DEFENDERS / 4)) * 3.5f;
                    Position pos(FRONT_LINE.GetPositionX() + rank, FRONT_LINE.GetPositionY() + across,
                        FRONT_LINE.GetPositionZ(), FRONT_LINE.GetOrientation());
                    if (Summon(NPC_AQ_KALDOREI, pos))
                        ++spawned;
                }

                return spawned > 0;
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

                for (Dragon const& dragon : DRAGONS)
                    if (dragon.wave == _wave)
                        if (Creature* summon = Summon(dragon.entry, DRAGON_SPOT, 90 * IN_MILLISECONDS))
                            summon->AI()->SetData(0, 0);
            }

            void OnSummonDied(ObjectGuid /*guid*/, uint32 entry) override
            {
                if (entry == NPC_AQ_NOKHOR)
                    MarkWon();
            }

        private:
            void SpawnWave(uint32 wave)
            {
                uint32 alive = 0;
                for (uint32 entry : QIRAJI)
                    alive += CountAlive(entry);

                // Wasps from the start, tanks from wave 3, a conqueror or two from wave 5.
                std::vector<uint32> entries(3 + wave, NPC_AQ_WASP);
                if (wave >= 3)
                    for (uint32 i = 0; i < wave / 2; ++i)
                        entries[i] = NPC_AQ_TANK;
                if (wave >= 5)
                    entries.back() = NPC_AQ_CONQUEROR;
                if (wave >= 7)
                    entries.front() = NPC_AQ_CONQUEROR;

                for (uint32 entry : entries)
                {
                    if (alive++ >= MAX_QIRAJI)
                        break;

                    if (Creature* qiraji = Summon(entry, RandomPointAround(GATE_MOUTH, 0.0f, 8.0f)))
                        SendTo(qiraji, RandomPointAround(FRONT_LINE, 0.0f, 10.0f));
                }
            }

            void SpawnBoss()
            {
                _bossUp = true;
                if (Creature* boss = Summon(NPC_AQ_NOKHOR, GATE_MOUTH))
                {
                    SendTo(boss, FRONT_LINE, false);
                    boss->Yell("For the glory of C'Thun! Crush them beneath the sands!", LANG_UNIVERSAL);
                }

                // His honour guard.
                for (uint32 i = 0; i < 4; ++i)
                    if (Creature* guard = Summon(i < 2 ? NPC_AQ_CONQUEROR : NPC_AQ_TANK, RandomPointAround(GATE_MOUTH, 3.0f, 8.0f)))
                        SendTo(guard, RandomPointAround(FRONT_LINE, 0.0f, 8.0f), false);
            }

            uint32 _wave = 0;
            uint32 _waveTimer = 0;
            bool _bossUp = false;
        };
    }

    std::unique_ptr<EraEvent> CreateAQWarEvent()
    {
        return std::make_unique<AQWarEvent>();
    }
}

using namespace EraEvents;

// One of the flashback's dragons: shouts its battle cry, takes its true form, and strafes the swarm
// with its breath before flying off. SetData starts it once it's placed.
struct npc_era_aq_dragon : public ScriptedAI
{
    npc_era_aq_dragon(Creature* creature) : ScriptedAI(creature) { }

    void InitializeAI() override
    {
        me->SetReactState(REACT_PASSIVE);
        for (Dragon const& dragon : DRAGONS)
            if (dragon.entry == me->GetEntry())
                _dragon = &dragon;
    }

    void SetData(uint32 /*type*/, uint32 /*value*/) override
    {
        if (!_dragon)
            return;

        Talk(_dragon->yell);

        scheduler.Schedule(2s, [this](TaskContext /*context*/)
        {
            DoCastSelf(_dragon->morph, true);
        }).Schedule(5s, [this](TaskContext /*context*/)
        {
            me->HandleEmoteCommand(EMOTE_ONESHOT_LIFTOFF);
            me->SetDisableGravity(true);
            me->GetMotionMaster()->MoveTakeoff(0, FRONT_LINE.GetPositionX(), FRONT_LINE.GetPositionY(), FRONT_LINE.GetPositionZ() + 18.0f, 7.0f);
        }).Schedule(9s, [this](TaskContext context)
        {
            Breathe();
            if (context.GetRepeatCounter() < 7)
                context.Repeat(8s);
        }).Schedule(75s, [this](TaskContext /*context*/)
        {
            me->GetMotionMaster()->MoveCharge(-8030.0f, 1480.0f, 70.0f, 30.0f);
        });
    }

    // The breath's look, and a quarter of the health of up to four Qiraji near the front.
    void Breathe()
    {
        DoCastSelf(_dragon->breath, true);

        std::list<Creature*> qiraji;
        me->GetCreatureListWithEntryInGrid(qiraji, std::vector<uint32>(QIRAJI.begin(), QIRAJI.end()), 40.0f);
        qiraji.remove_if([](Creature* creature) { return !creature->IsAlive(); });

        uint32 hit = 0;
        for (Creature* target : qiraji)
        {
            if (hit++ >= 4)
                break;
            Unit::DealDamage(me, target, target->GetMaxHealth() / 4, nullptr, DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NATURE);
        }
    }

    void UpdateAI(uint32 diff) override
    {
        scheduler.Update(diff);
    }

private:
    Dragon const* _dragon = nullptr;
};

// The gong starts the war on the spot.
class EraEventsAQPlayerScript : public PlayerScript
{
public:
    EraEventsAQPlayerScript() : PlayerScript("EraEventsAQPlayerScript", { PLAYERHOOK_ON_PLAYER_COMPLETE_QUEST }) { }

    void OnPlayerCompleteQuest(Player* /*player*/, Quest const* quest) override
    {
        if (quest && (quest->GetQuestId() == QUEST_BANG_A_GONG || quest->GetQuestId() == QUEST_SIMPLY_BANG_A_GONG))
            RequestStart(EVENT_AQ_WAR);
    }
};

void AddEraEventAQScripts()
{
    RegisterMob(NPC_AQ_KALDOREI, {
        .spells = {
            { SPELL_STRIKE, 6000, 9000, SpellTarget::Victim },
        },
        .seekEnemies = true,
    });

    RegisterMob(NPC_AQ_WASP, {
        .seekEnemies = true,
    });

    RegisterMob(NPC_AQ_TANK, {
        .spells = {
            { SPELL_SUNDER_ARMOR, 6000, 10000, SpellTarget::Victim },
        },
        .seekEnemies = true,
    });

    RegisterMob(NPC_AQ_CONQUEROR, {
        .spells = {
            { SPELL_CLEAVE,      6000,  9000, SpellTarget::Victim },
            { SPELL_THUNDERCLAP, 12000, 16000, SpellTarget::Self },
        },
        .seekEnemies = true,
    });

    RegisterMob(NPC_AQ_NOKHOR, {
        .spells = {
            { SPELL_CLEAVE,      6000,  9000, SpellTarget::Victim },
            { SPELL_THUNDERCLAP, 12000, 16000, SpellTarget::Self },
            { SPELL_KNOCK_AWAY,  18000, 24000, SpellTarget::Victim },
            { SPELL_REND,        10000, 14000, SpellTarget::RandomEnemy },
        },
        .seekEnemies = true,
    });

    RegisterCreatureAI(npc_era_aq_dragon);
    new EraEventsAQPlayerScript();
}
