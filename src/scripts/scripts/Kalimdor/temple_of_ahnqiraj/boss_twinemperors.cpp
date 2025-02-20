/* 
 * Copyright (C) 2006-2008 ScriptDev2 <https://scriptdev2.svn.sourceforge.net/>
 * Copyright (C) 2008-2015 Hellground <http://hellground.net/>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

/* ScriptData
SDName: Boss_Twinemperors
SD%Complete: 95
SDComment:
SDCategory: Temple of Ahn'Qiraj
EndScriptData */

#include "precompiled.h"
#include "def_temple_of_ahnqiraj.h"
#include "WorldPacket.h"

#include "Item.h"
#include "Spell.h"

#define SPELL_HEAL_BROTHER          7393
#define SPELL_TWIN_TELEPORT         800                     // CTRA watches for this spell to start its teleport timer
#define SPELL_TWIN_TELEPORT_VISUAL  26638                   // visual

#define SPELL_EXPLODEBUG            804
#define SPELL_MUTATE_BUG            802

#define SOUND_VN_DEATH              8660                    //8660 - Death - Feel
#define SOUND_VN_AGGRO              8661                    //8661 - Aggro - Let none
#define SOUND_VN_KILL               8662                    //8661 - Kill - your fate

#define SOUND_VL_AGGRO              8657                    //8657 - Aggro - To Late
#define SOUND_VL_KILL               8658                    //8658 - Kill - You will not
#define SOUND_VL_DEATH              8659                    //8659 - Death

#define PULL_RANGE                  50
#define ABUSE_BUG_RANGE             20
#define SPELL_BERSERK               26662
#define TELEPORTTIME                30000

#define SPELL_UPPERCUT              26007
#define SPELL_UNBALANCING_STRIKE    26613

#define VEKLOR_DIST                 20                      // VL will not come to melee when attacking

#define SPELL_SHADOWBOLT            26006
#define SPELL_BLIZZARD              26607
#define SPELL_ARCANEBURST           568

struct boss_twinemperorsAI : public ScriptedAI
{
    ScriptedInstance *pInstance;
    Timer Heal_Timer;
    Timer Teleport_Timer;
    Timer AfterTeleportTimer;
    Timer Abuse_Bug_Timer, BugsTimer;
    Timer EnrageTimer;
    bool AfterTeleport;
    bool DontYellWhenDead;
    bool tspellCast;


    virtual bool IAmVeklor() = 0;
    virtual void Reset() = 0;
    virtual void CastSpellOnBug(Creature *target) = 0;

    boss_twinemperorsAI(Creature *c): ScriptedAI(c)
    {
        pInstance = (c->GetInstanceData());
    }

    void TwinReset()
    {
        Heal_Timer = 0;                                     // first heal immediately when they get close together
        Teleport_Timer = TELEPORTTIME;
        AfterTeleport = false;
        tspellCast = false;
        AfterTeleportTimer = 0;
        Abuse_Bug_Timer = 10000 + rand()%7000;
        BugsTimer = 2000;
        m_creature->ClearUnitState(UNIT_STAT_STUNNED);
        DontYellWhenDead = false;
        EnrageTimer = 15*60000;

        if (pInstance)
            pInstance->SetData(DATA_TWIN_EMPERORS, NOT_STARTED);
    }

    Creature *GetOtherBoss()
    {
        if(pInstance)
        {
            return (Creature *)Unit::GetUnit((*m_creature), pInstance->GetData64(IAmVeklor() ? DATA_VEKNILASH : DATA_VEKLOR));
        }
        else
        {
            return (Creature *)0;
        }
    }

    void DamageTaken(Unit *done_by, uint32 &damage)
    {
        Creature* pOtherBoss = GetOtherBoss();
        if (pOtherBoss)
        {
            float dPercent = ((float)damage) / ((float)m_creature->GetMaxHealth());
            int odmg = (int)(dPercent * ((float)pOtherBoss->GetMaxHealth()));
            int ohealth = pOtherBoss->GetHealth()-odmg;
            pOtherBoss->SetHealth(ohealth > 0 ? ohealth : 0);
            if (ohealth <= 0)
            {
                done_by->Kill(pOtherBoss);
                pOtherBoss->SetLootRecipient(done_by);
            }
        }
    }

    void JustDied(Unit* Killer)
    {
        Creature *pOtherBoss = GetOtherBoss();
        if (pOtherBoss && !DontYellWhenDead)
        {
            ((boss_twinemperorsAI *)pOtherBoss->AI())->DontYellWhenDead = true;
            Killer->Kill(pOtherBoss);
            pOtherBoss->SetHealth(0);
            pOtherBoss->SetLootRecipient(Killer);
        }

        if (!DontYellWhenDead)                              // I hope AI is not threaded
            DoPlaySoundToSet(m_creature, IAmVeklor() ? SOUND_VL_DEATH : SOUND_VN_DEATH);

        if (pInstance)
            pInstance->SetData(DATA_TWIN_EMPERORS, DONE);
    }

    void KilledUnit(Unit* victim)
    {
        DoPlaySoundToSet(m_creature, IAmVeklor() ? SOUND_VL_KILL : SOUND_VN_KILL);
    }

    void EnterCombat(Unit *who)
    {
        DoZoneInCombat();
        Creature *pOtherBoss = GetOtherBoss();
        if (pOtherBoss)
        {
            // TODO: we should activate the other boss location so he can start attackning even if nobody
            // is near I dont know how to do that
            ScriptedAI *otherAI = (ScriptedAI*)pOtherBoss->AI();
            if (!pOtherBoss->IsInCombat())
            {
                DoPlaySoundToSet(m_creature, IAmVeklor() ? SOUND_VL_AGGRO : SOUND_VN_AGGRO);
                otherAI->AttackStart(who);
                otherAI->DoZoneInCombat();
            }
        }
        if (pInstance)
            pInstance->SetData(DATA_TWIN_EMPERORS, IN_PROGRESS);
    }

    void SpellHit(Unit *caster, const SpellEntry *entry)
    {
        if (caster == m_creature)
            return;

        Creature *pOtherBoss = GetOtherBoss();
        if (entry->Id != SPELL_HEAL_BROTHER || !pOtherBoss)
            return;

        // add health so we keep same percentage for both brothers
        uint32 mytotal = m_creature->GetMaxHealth(), histotal = pOtherBoss->GetMaxHealth();
        float mult = ((float)mytotal) / ((float)histotal);
        if (mult < 1)
            mult = 1.0f/mult;
        #define HEAL_BROTHER_AMOUNT 30000.0f
        uint32 largerAmount = (uint32)((HEAL_BROTHER_AMOUNT * mult) - HEAL_BROTHER_AMOUNT);

        uint32 myh = m_creature->GetHealth();
        uint32 hish = pOtherBoss->GetHealth();
        if (mytotal > histotal)
        {
            uint32 h = m_creature->GetHealth()+largerAmount;
            m_creature->SetHealth(std::min(mytotal, h));
        }
        else
        {
            uint32 h = pOtherBoss->GetHealth()+largerAmount;
            pOtherBoss->SetHealth(std::min(histotal, h));
        }
    }

    void TryHealBrother(uint32 diff)
    {
        if (IAmVeklor())                                    // this spell heals caster and the other brother so let VN cast it
            return;

        if (Heal_Timer.Expired(diff))
        {
            Unit *pOtherBoss = GetOtherBoss();
            if (pOtherBoss && (pOtherBoss->IsWithinDistInMap(m_creature, 60)))
            {
                DoCast(pOtherBoss, SPELL_HEAL_BROTHER);
                Heal_Timer = 1000;
            }
        } 
    }

    void TeleportToMyBrother()
    {
        if (!pInstance)
            return;

        Teleport_Timer = TELEPORTTIME;

        if(IAmVeklor())
            return;                                         // mechanics handled by veknilash so they teleport exactly at the same time and to correct coordinates

        Creature *pOtherBoss = GetOtherBoss();
        if (pOtherBoss)
        {
            //m_creature->MonsterYell("Teleporting ...", LANG_UNIVERSAL, 0);
            float other_x = pOtherBoss->GetPositionX();
            float other_y = pOtherBoss->GetPositionY();
            float other_z = pOtherBoss->GetPositionZ();
            float other_o = pOtherBoss->GetOrientation();

            Map *thismap = m_creature->GetMap();
            thismap->CreatureRelocation(pOtherBoss, m_creature->GetPositionX(),
                m_creature->GetPositionY(),    m_creature->GetPositionZ(), m_creature->GetOrientation());
            thismap->CreatureRelocation(m_creature, other_x, other_y, other_z, other_o);

            SetAfterTeleport();
            ((boss_twinemperorsAI*) pOtherBoss->AI())->SetAfterTeleport();
        }
    }

    void SetAfterTeleport()
    {
        m_creature->InterruptNonMeleeSpells(false);
        DoStopAttack();
        DoResetThreat();
        DoCast(m_creature, SPELL_TWIN_TELEPORT_VISUAL);
        m_creature->addUnitState(UNIT_STAT_STUNNED);
        AfterTeleport = true;
        AfterTeleportTimer = 2000;
        tspellCast = false;
    }

    bool TryActivateAfterTTelep(uint32 diff)
    {
        if (AfterTeleport)
        {
            if (!tspellCast)
            {
                m_creature->ClearUnitState(UNIT_STAT_STUNNED);
                DoCast(m_creature, SPELL_TWIN_TELEPORT);
                m_creature->addUnitState(UNIT_STAT_STUNNED);
            }

            tspellCast = true;

            if (AfterTeleportTimer.Expired(diff))
            {
                AfterTeleport = false;
                m_creature->ClearUnitState(UNIT_STAT_STUNNED);
                Unit *nearu = m_creature->SelectNearestTarget(100);
                //DoYell(nearu->GetName(), LANG_UNIVERSAL, 0);
                if(nearu)
                {
                    AttackStart(nearu);
                    m_creature->getThreatManager().addThreat(nearu, 10000);
                }
                return true;
            }
            else
            {
                // update important timers which would otherwise get skipped
                if (EnrageTimer.Expired(diff))
                    EnrageTimer = 0;
                if (Teleport_Timer.Expired(diff))
                    Teleport_Timer = 0;
                return false;
            }
        }
        else
        {
            return true;
        }
    }

    void MoveInLineOfSight(Unit *who)
    {
        if (!who || m_creature->GetVictim())
            return;

        if (who->isTargetableForAttack() && who->isInAccessiblePlacefor(m_creature) && m_creature->IsHostileTo(who))
        {
            float attackRadius = m_creature->GetAttackDistance(who);
            if (attackRadius < PULL_RANGE)
                attackRadius = PULL_RANGE;
            if (m_creature->IsWithinDistInMap(who, attackRadius) && m_creature->GetDistanceZ(who) <= /*CREATURE_Z_ATTACK_RANGE*/7 /*there are stairs*/)
            {
                //if(who->HasStealthAura())
                //    who->RemoveSpellsCausingAura(SPELL_AURA_MOD_STEALTH);
                AttackStart(who);
            }
        }
    }

    class AnyBugCheck
    {
        public:
            AnyBugCheck(WorldObject const* obj, float range) : i_obj(obj), i_range(range) {}
            bool operator()(Unit* u)
            {
                Creature *c = (Creature *)u;
                if (!i_obj->IsWithinDistInMap(c, i_range))
                    return false;
                return (c->GetEntry() == 15316 || c->GetEntry() == 15317);
            }
        private:
            WorldObject const* i_obj;
            float i_range;
    };

    Creature *RespawnNearbyBugsAndGetOne()
    {
        std::list<Creature*> unitList;

        AnyBugCheck u_check(m_creature, 150);
        Hellground::ObjectListSearcher<Creature, AnyBugCheck> searcher(unitList, u_check);
        Cell::VisitGridObjects(me, searcher, 150);

        Creature *nearb = NULL;

        for(std::list<Creature*>::iterator iter = unitList.begin(); iter != unitList.end(); ++iter)
        {
            Creature *c = (Creature *)(*iter);
            if (c && c->IsDead())
            {
                c->Respawn();
                c->setFaction(7);
                c->RemoveAllAuras();
            }
            if (c->IsWithinDistInMap(m_creature, ABUSE_BUG_RANGE))
            {
                if (!nearb || (rand()%4)==0)
                    nearb = c;
            }
        }
        return nearb;
    }

    void HandleBugs(uint32 diff)
    {
        if (BugsTimer.Expired(diff) || Abuse_Bug_Timer.Expired(diff))
        {
            Creature *c = RespawnNearbyBugsAndGetOne();
            if (Abuse_Bug_Timer.Passed())
            {
                if (c)
                {
                    CastSpellOnBug(c);
                    Abuse_Bug_Timer = 10000 + rand()%7000;
                }
                else
                    Abuse_Bug_Timer = 1000;
            }
            BugsTimer = 2000;
        }
    }

    void CheckEnrage(uint32 diff)
    {
        if (EnrageTimer.Expired(diff))
        {
            if (!m_creature->IsNonMeleeSpellCast(true))
            {
                DoCast(m_creature, SPELL_BERSERK);
                EnrageTimer = 60*60000;
            }
            else EnrageTimer = 0;
        } 
    }
};

class BugAura : public Aura
{
    public:
        BugAura(SpellEntry *spell, uint32 eff, int32 *bp, Unit *target, Unit *caster) : Aura(spell, eff, bp, target, caster, NULL)
            {}
};

struct boss_veknilashAI : public boss_twinemperorsAI
{
    bool IAmVeklor() {return false;}
    boss_veknilashAI(Creature *c) : boss_twinemperorsAI(c) {}

    Timer UpperCut_Timer;
    Timer UnbalancingStrike_Timer;
    Timer Scarabs_Timer;
    int Rand;
    int RandX;
    int RandY;

    Creature* Summoned;

    void Reset()
    {
        TwinReset();
        UpperCut_Timer.Reset(14000 + rand() % 15000);
        UnbalancingStrike_Timer.Reset(8000 + rand() % 10000);
        Scarabs_Timer.Reset(7000 + rand() % 7000);

                                                            //Added. Can be removed if its included in DB.
        m_creature->ApplySpellImmune(0, IMMUNITY_DAMAGE, SPELL_SCHOOL_MASK_MAGIC, true);
    }

    void CastSpellOnBug(Creature *target)
    {
        target->setFaction(14);
        ((CreatureAI*)target->AI())->AttackStart(m_creature->getThreatManager().getHostilTarget());
        SpellEntry *spell = (SpellEntry *)GetSpellStore()->LookupEntry(SPELL_MUTATE_BUG);
        for (int i=0; i<3; i++)
        {
            if (!spell->Effect[i])
                continue;
            target->AddAura(new BugAura(spell, i, NULL, target, target));
        }
        target->SetHealth(target->GetMaxHealth());
    }

    void UpdateAI(const uint32 diff)
    {
        //Return since we have no target
        if (!UpdateVictim())
            return;

        if (!TryActivateAfterTTelep(diff))
            return;

        //UnbalancingStrike_Timer
        if (UnbalancingStrike_Timer.Expired(diff))
        {
            DoCast(m_creature->GetVictim(),SPELL_UNBALANCING_STRIKE);
            UnbalancingStrike_Timer = 8000+rand()%12000;
        }

        if (UpperCut_Timer.Expired(diff))
        {
            Unit* randomMelee = SelectUnit(SELECT_TARGET_RANDOM, 0, NOMINAL_MELEE_RANGE, true);
            if (randomMelee)
                DoCast(randomMelee,SPELL_UPPERCUT);
            UpperCut_Timer = 15000+rand()%15000;
        }

        HandleBugs(diff);

        //Heal brother when 60yrds close
        TryHealBrother(diff);

        //Teleporting to brother
        if (Teleport_Timer.Expired(diff))
        {
            TeleportToMyBrother();
        }

        CheckEnrage(diff);

        DoMeleeAttackIfReady();
    }
};

struct boss_veklorAI : public boss_twinemperorsAI
{
    bool IAmVeklor() {return true;}
    boss_veklorAI(Creature *c) : boss_twinemperorsAI(c) {}

    Timer ShadowBolt_Timer;
    Timer Blizzard_Timer;
    Timer ArcaneBurst_Timer;
    Timer Scorpions_Timer;
    int Rand;
    int RandX;
    int RandY;

    Creature* Summoned;

    void Reset()
    {
        TwinReset();
        ShadowBolt_Timer = 0;
        Blizzard_Timer.Reset(15000 + rand() % 5000);
        ArcaneBurst_Timer.Reset(1000);
        Scorpions_Timer.Reset(7000 + rand() % 7000);

        //Added. Can be removed if its included in DB.
        m_creature->ApplySpellImmune(0, IMMUNITY_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, true);
        m_creature->SetBaseWeaponDamage(BASE_ATTACK, MINDAMAGE, 0);
        m_creature->SetBaseWeaponDamage(BASE_ATTACK, MAXDAMAGE, 0);
    }

    void CastSpellOnBug(Creature *target)
    {
        target->setFaction(14);
        SpellEntry *spell = (SpellEntry *)GetSpellStore()->LookupEntry(SPELL_EXPLODEBUG);
        for (int i=0; i<3; i++)
        {
            if (!spell->Effect[i])
                continue;
            target->AddAura(new BugAura(spell, i, NULL, target, target));
        }
        target->SetHealth(target->GetMaxHealth());
    }

    void UpdateAI(const uint32 diff)
    {
        //Return since we have no target
        if (!UpdateVictim())
            return;

        // reset arcane burst after teleport - we need to do this because
        // when VL jumps to VN's location there will be a warrior who will get only 2s to run away
        // which is almost impossible
        if (AfterTeleport)
            ArcaneBurst_Timer = 5000;
        if (!TryActivateAfterTTelep(diff))
            return;

        //ShadowBolt_Timer
        if (ShadowBolt_Timer.Expired(diff))
        {
            if (m_creature->IsWithinDistInMap(m_creature, 45))
                m_creature->GetMotionMaster()->MoveChase(m_creature->GetVictim(), VEKLOR_DIST, 0);
            else
                DoCast(m_creature->GetVictim(),SPELL_SHADOWBOLT);
            ShadowBolt_Timer = 2000;
        }

        //Blizzard_Timer
        if (Blizzard_Timer.Expired(diff))
        {
            Unit* target = NULL;
            target = SelectUnit(SELECT_TARGET_RANDOM, 0, GetSpellMaxRange(SPELL_BLIZZARD), true);
            if (target)
                DoCast(target,SPELL_BLIZZARD);
            Blizzard_Timer = 15000+rand()%15000;
        }

        if (ArcaneBurst_Timer.Expired(diff))
        {
            Unit *mvic;
            if ((mvic=SelectUnit(SELECT_TARGET_NEAREST, 0, NOMINAL_MELEE_RANGE, true))!=NULL)
            {
                DoCast(mvic,SPELL_ARCANEBURST);
                ArcaneBurst_Timer = 5000;
            }
        }

        HandleBugs(diff);

        //Heal brother when 60yrds close
        TryHealBrother(diff);

        //Teleporting to brother
        if (Teleport_Timer.Expired(diff))
        {
            TeleportToMyBrother();
        }

        CheckEnrage(diff);

        //VL doesn't melee
        //DoMeleeAttackIfReady();
    }

    void AttackStart(Unit* who)
    {
        if (!who)
            return;

        if (who->isTargetableForAttack())
        {
            // VL doesn't melee
            if ( m_creature->Attack(who, false) )
            {
                m_creature->GetMotionMaster()->MoveChase(who, VEKLOR_DIST, 0);
                m_creature->AddThreat(who, 0.0f);
            }

            if (!m_creature->IsInCombat())
                EnterCombat(who);
        }
    }
};

CreatureAI* GetAI_boss_veknilash(Creature *_Creature)
{
    return new boss_veknilashAI (_Creature);
}

CreatureAI* GetAI_boss_veklor(Creature *_Creature)
{
    return new boss_veklorAI (_Creature);
}

void AddSC_boss_twinemperors()
{
    Script *newscript;

    newscript = new Script;
    newscript->Name="boss_veknilash";
    newscript->GetAI = &GetAI_boss_veknilash;
    newscript->RegisterSelf();

    newscript = new Script;
    newscript->Name="boss_veklor";
    newscript->GetAI = &GetAI_boss_veklor;
    newscript->RegisterSelf();
}

