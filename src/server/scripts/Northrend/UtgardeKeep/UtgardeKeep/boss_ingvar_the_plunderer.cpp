/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/* ScriptData
SDName: Boss_Ingvar_The_Plunderer
SD%Complete: 95
SDComment: Blizzlike Timers (just shadow axe summon needs a new timer)
SDCategory: Utgarde Keep
EndScriptData */

#include "ScriptMgr.h"
#include "InstanceScript.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "ScriptedCreature.h"
#include "SpellScript.h"
#include "utgarde_keep.h"

enum Yells
{
    // Ingvar (Human/Undead)
    SAY_AGGRO                   = 0,
    SAY_SLAY                    = 1,
    SAY_DEATH                   = 2,

    // Annhylde The Caller
    YELL_RESURRECT              = 0
};

enum Events
{
    EVENT_CLEAVE = 1,
    EVENT_SMASH,
    EVENT_STAGGERING_ROAR,
    EVENT_ENRAGE,

    EVENT_DARK_SMASH,
    EVENT_DREADFUL_ROAR,
    EVENT_WOE_STRIKE,
    EVENT_SHADOW_AXE,
    EVENT_JUST_TRANSFORMED,
    EVENT_SUMMON_BANSHEE,

    EVENT_RESURRECT_1,
    EVENT_RESURRECT_2
};

enum Phases
{
    PHASE_HUMAN = 1,
    PHASE_UNDEAD,
    PHASE_EVENT
};

enum Spells
{
    // Ingvar Spells human form
    SPELL_CLEAVE                                = 42724,
    SPELL_SMASH                                 = 42669,
    SPELL_STAGGERING_ROAR                       = 42708,
    SPELL_ENRAGE                                = 42705,

    SPELL_INGVAR_FEIGN_DEATH                    = 42795,
    SPELL_SUMMON_BANSHEE                        = 42912,
    SPELL_SCOURG_RESURRECTION                   = 42863, // Spawn resurrect effect around Ingvar

    // Ingvar Spells undead form
    SPELL_DARK_SMASH                            = 42723,
    SPELL_DREADFUL_ROAR                         = 42729,
    SPELL_WOE_STRIKE                            = 42730,
    SPELL_WOE_STRIKE_EFFECT                     = 42739,

    SPELL_SHADOW_AXE_SUMMON                     = 42748,
    SPELL_SHADOW_AXE_PERIODIC_DAMAGE            = 42750,

    // Spells for Annhylde
    SPELL_SCOURG_RESURRECTION_HEAL              = 42704, // Heal Max + DummyAura
    SPELL_SCOURG_RESURRECTION_BEAM              = 42857, // Channeling Beam of Annhylde
    SPELL_SCOURG_RESURRECTION_DUMMY             = 42862, // Some Emote Dummy?
    SPELL_INGVAR_TRANSFORM                      = 42796
};

enum Misc
{
    ACTION_START_PHASE_2
};

struct boss_ingvar_the_plunderer : public BossAI
{
    boss_ingvar_the_plunderer(Creature* creature) : BossAI(creature, DATA_INGVAR) { }

    void Reset() override
    {
        if (me->GetEntry() != NPC_INGVAR)
            me->UpdateEntry(NPC_INGVAR);
        me->RemoveUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
        me->SetImmuneToPC(false);
        me->SetUninteractible(false);

        _Reset();
    }

    void DamageTaken(Unit* /*doneBy*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo = nullptr*/) override
    {
        if (damage >= me->GetHealth() && events.IsInPhase(PHASE_HUMAN))
        {
            events.SetPhase(PHASE_EVENT);
            events.ScheduleEvent(EVENT_SUMMON_BANSHEE, 3s, 0, PHASE_EVENT);

            me->RemoveAllAuras();
            me->StopMoving();
            DoCast(me, SPELL_INGVAR_FEIGN_DEATH, true);

            me->SetUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
            me->SetImmuneToPC(true, true);
            me->SetUninteractible(true);

            Talk(SAY_DEATH);
        }

        if (events.IsInPhase(PHASE_EVENT))
            damage = 0;
    }

    void DoAction(int32 actionId) override
    {
        if (actionId == ACTION_START_PHASE_2)
            StartZombiePhase();
    }

    void StartZombiePhase()
    {
        me->RemoveAura(SPELL_INGVAR_FEIGN_DEATH);
        DoCast(me, SPELL_INGVAR_TRANSFORM, true);
        me->UpdateEntry(NPC_INGVAR_UNDEAD);
        events.ScheduleEvent(EVENT_JUST_TRANSFORMED, 500ms, 0, PHASE_EVENT);
    }

    void JustEngagedWith(Unit* who) override
    {
        if (events.IsInPhase(PHASE_EVENT) || events.IsInPhase(PHASE_UNDEAD)) // ingvar gets multiple JustEngagedWith calls
            return;
        BossAI::JustEngagedWith(who);

        Talk(SAY_AGGRO);
        events.SetPhase(PHASE_HUMAN);
        events.ScheduleEvent(EVENT_CLEAVE, 6s, 12s, 0, PHASE_HUMAN);
        events.ScheduleEvent(EVENT_STAGGERING_ROAR, 18s, 21s, 0, PHASE_HUMAN);
        events.ScheduleEvent(EVENT_ENRAGE, 7s, 14s, 0, PHASE_HUMAN);
        events.ScheduleEvent(EVENT_SMASH, 12s, 17s, 0, PHASE_HUMAN);
    }

    void AttackStart(Unit* who) override
    {
        if (events.IsInPhase(PHASE_EVENT)) // prevent ingvar from beginning to attack/chase during transition
            return;
        BossAI::AttackStart(who);
    }

    void JustDied(Unit* /*killer*/) override
    {
        _JustDied();
        Talk(SAY_DEATH);
    }

    void ScheduleSecondPhase()
    {
        events.SetPhase(PHASE_UNDEAD);
        events.ScheduleEvent(EVENT_DARK_SMASH, 14s, 18s, 0, PHASE_UNDEAD);
        events.ScheduleEvent(EVENT_DREADFUL_ROAR, 0ms, 0, PHASE_UNDEAD);
        events.ScheduleEvent(EVENT_WOE_STRIKE, 10s, 14s, 0, PHASE_UNDEAD);
        events.ScheduleEvent(EVENT_SHADOW_AXE, 30s, 0, PHASE_UNDEAD);
    }

    void KilledUnit(Unit* who) override
    {
        if (who->GetTypeId() == TYPEID_PLAYER)
            Talk(SAY_SLAY);
    }

    void UpdateAI(uint32 diff) override
    {
        if (!events.IsInPhase(PHASE_EVENT) && !UpdateVictim())
            return;

        events.Update(diff);

        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        while (uint32 eventId = events.ExecuteEvent())
        {
            switch (eventId)
            {
                // PHASE ONE
                case EVENT_CLEAVE:
                    DoCastVictim(SPELL_CLEAVE);
                    events.ScheduleEvent(EVENT_CLEAVE, 6s, 12s, 0, PHASE_HUMAN);
                    break;
                case EVENT_STAGGERING_ROAR:
                    DoCast(me, SPELL_STAGGERING_ROAR);
                    events.ScheduleEvent(EVENT_STAGGERING_ROAR, 18s, 22s, 0, PHASE_HUMAN);
                    break;
                case EVENT_ENRAGE:
                    DoCast(me, SPELL_ENRAGE);
                    events.ScheduleEvent(EVENT_ENRAGE, 7s, 14s, 0, PHASE_HUMAN);
                    break;
                case EVENT_SMASH:
                    DoCastAOE(SPELL_SMASH);
                    events.ScheduleEvent(EVENT_SMASH, 12s, 16s, 0, PHASE_HUMAN);
                    break;
                case EVENT_JUST_TRANSFORMED:
                    me->RemoveUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
                    me->SetImmuneToPC(false);
                    me->SetUninteractible(false);
                    ScheduleSecondPhase();
                    Talk(SAY_AGGRO);
                    DoZoneInCombat();
                    return;
                case EVENT_SUMMON_BANSHEE:
                    DoCast(me, SPELL_SUMMON_BANSHEE);
                    return;
                // PHASE TWO
                case EVENT_DARK_SMASH:
                    DoCastVictim(SPELL_DARK_SMASH);
                    events.ScheduleEvent(EVENT_DARK_SMASH, 12s, 16s, 0, PHASE_UNDEAD);
                    break;
                case EVENT_DREADFUL_ROAR:
                    DoCast(me, SPELL_DREADFUL_ROAR);
                    events.ScheduleEvent(EVENT_DREADFUL_ROAR, 18s, 22s, 0, PHASE_UNDEAD);
                    break;
                case EVENT_WOE_STRIKE:
                    DoCastVictim(SPELL_WOE_STRIKE);
                    events.ScheduleEvent(EVENT_WOE_STRIKE, 10s, 14s, 0, PHASE_UNDEAD);
                    break;
                case EVENT_SHADOW_AXE:
                    if (Unit* target = SelectTarget(SelectTargetMethod::Random, 1, 0.0f, true))
                        DoCast(target, SPELL_SHADOW_AXE_SUMMON);
                    events.ScheduleEvent(EVENT_SHADOW_AXE, 30s, 0, PHASE_UNDEAD);
                    break;
                default:
                    break;
            }

            if (me->HasUnitState(UNIT_STATE_CASTING))
                return;
        }

        if (!events.IsInPhase(PHASE_EVENT))
            DoMeleeAttackIfReady();
    }
};

struct npc_annhylde_the_caller : public ScriptedAI
{
    npc_annhylde_the_caller(Creature* creature) : ScriptedAI(creature)
    {
        x = 0.f;
        y = 0.f;
        z = 0.f;
        _instance = creature->GetInstanceScript();
    }

    void Reset() override
    {
        _events.Reset();

        me->GetPosition(x, y, z);
        me->GetMotionMaster()->MovePoint(1, x, y, z - 15.0f);
    }

    void MovementInform(uint32 type, uint32 id) override
    {
        if (type != POINT_MOTION_TYPE)
            return;

        switch (id)
        {
            case 1:
                Talk(YELL_RESURRECT);
                if (Creature* ingvar = ObjectAccessor::GetCreature(*me, _instance->GetGuidData(DATA_INGVAR)))
                {
                    ingvar->RemoveAura(SPELL_SUMMON_BANSHEE);
                    ingvar->CastSpell(ingvar, SPELL_SCOURG_RESURRECTION_DUMMY, true);
                    DoCast(ingvar, SPELL_SCOURG_RESURRECTION_BEAM);
                }
                _events.ScheduleEvent(EVENT_RESURRECT_1, 8s);
                break;
            case 2:
                me->DespawnOrUnsummon();
                break;
            default:
                break;
        }
    }

    void AttackStart(Unit* /*who*/) override { }
    void MoveInLineOfSight(Unit* /*who*/) override { }
    void JustEngagedWith(Unit* /*who*/) override { }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_RESURRECT_1:
                    if (Creature* ingvar = ObjectAccessor::GetCreature(*me, _instance->GetGuidData(DATA_INGVAR)))
                    {
                        ingvar->RemoveAura(SPELL_INGVAR_FEIGN_DEATH);
                        ingvar->CastSpell(ingvar, SPELL_SCOURG_RESURRECTION_HEAL, false);
                    }
                    _events.ScheduleEvent(EVENT_RESURRECT_2, 3s);
                    break;
                case EVENT_RESURRECT_2:
                    if (Creature* ingvar = ObjectAccessor::GetCreature(*me, _instance->GetGuidData(DATA_INGVAR)))
                    {
                        ingvar->RemoveAurasDueToSpell(SPELL_SCOURG_RESURRECTION_DUMMY);
                        ingvar->AI()->DoAction(ACTION_START_PHASE_2);
                    }

                    me->GetMotionMaster()->MovePoint(2, x, y, z + 15.0f);
                    break;
                default:
                    break;
            }
        }
    }

private:
    InstanceScript* _instance;
    EventMap _events;
    float x, y, z;
};

struct npc_ingvar_throw_dummy : public ScriptedAI
{
    npc_ingvar_throw_dummy(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        if (Creature* target = me->FindNearestCreature(NPC_THROW_TARGET, 200.0f))
        {
            float x, y, z;
            target->GetPosition(x, y, z);
            me->GetMotionMaster()->MoveCharge(x, y, z);
            target->DespawnOrUnsummon();
        }
        else
            me->DespawnOrUnsummon();
    }

    void MovementInform(uint32 type, uint32 id) override
    {
        if (type == EFFECT_MOTION_TYPE && id == EVENT_CHARGE)
        {
            me->CastSpell(me, SPELL_SHADOW_AXE_PERIODIC_DAMAGE, true);
            me->DespawnOrUnsummon(10s);
        }
    }
};

// 42912 - Summon Banshee
class spell_ingvar_summon_banshee : public SpellScript
{
    PrepareSpellScript(spell_ingvar_summon_banshee);

    void SetDest(SpellDestination& dest)
    {
        dest.RelocateOffset({ 0.0f, 0.0f, 30.0f, 0.0f });
    }

    void Register() override
    {
        OnDestinationTargetSelect += SpellDestinationTargetSelectFn(spell_ingvar_summon_banshee::SetDest, EFFECT_0, TARGET_DEST_CASTER_BACK);
    }
};

// 42730, 59735 - Woe Strike
class spell_ingvar_woe_strike : public AuraScript
{
    PrepareAuraScript(spell_ingvar_woe_strike);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_WOE_STRIKE_EFFECT });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        HealInfo* healInfo = eventInfo.GetHealInfo();
        if (!healInfo || !healInfo->GetHeal())
            return false;

        return true;
    }

    void HandleProc(AuraEffect* aurEff, ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();
        GetTarget()->CastSpell(eventInfo.GetActor(), SPELL_WOE_STRIKE_EFFECT, aurEff);
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_ingvar_woe_strike::CheckProc);
        OnEffectProc += AuraEffectProcFn(spell_ingvar_woe_strike::HandleProc, EFFECT_1, SPELL_AURA_PROC_TRIGGER_SPELL);
    }
};

void AddSC_boss_ingvar_the_plunderer()
{
    RegisterUtgardeKeepCreatureAI(boss_ingvar_the_plunderer);
    RegisterUtgardeKeepCreatureAI(npc_annhylde_the_caller);
    RegisterUtgardeKeepCreatureAI(npc_ingvar_throw_dummy);
    RegisterSpellScript(spell_ingvar_summon_banshee);
    RegisterSpellScript(spell_ingvar_woe_strike);
}
