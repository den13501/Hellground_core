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
SDName: Boss_Highlord_Omokk
SD%Complete: 100
SDComment:
SDCategory: Blackrock Spire
EndScriptData */

#include "precompiled.h"

#define SPELL_WARSTOMP          24375
#define SPELL_CLEAVE            15579
#define SPELL_STRIKE            18368
#define SPELL_REND              18106
#define SPELL_SUNDERARMOR       24317
#define SPELL_KNOCKAWAY         20686
#define SPELL_SLOW              22356

struct boss_highlordomokkAI : public ScriptedAI
{
    boss_highlordomokkAI(Creature *c) : ScriptedAI(c) {}

    int32 WarStomp_Timer;
    int32 Cleave_Timer;
    int32 Strike_Timer;
    int32 Rend_Timer;
    int32 SunderArmor_Timer;
    int32 KnockAway_Timer;
    int32 Slow_Timer;

    void Reset()
    {
        WarStomp_Timer = 15000;
        Cleave_Timer = 6000;
        Strike_Timer = 10000;
        Rend_Timer = 14000;
        SunderArmor_Timer = 2000;
        KnockAway_Timer = 18000;
        Slow_Timer = 24000;
    }

    void EnterCombat(Unit *who)
    {
    }

    void UpdateAI(const uint32 diff)
    {
        //Return since we have no target
        if (!UpdateVictim() )
            return;

        WarStomp_Timer -= diff;
        if (WarStomp_Timer <= diff)
        {
            DoCast(m_creature->GetVictim(),SPELL_WARSTOMP);
            WarStomp_Timer += 14000;
        }

        Cleave_Timer -= diff;
        if (Cleave_Timer <= diff)
        {
            DoCast(m_creature->GetVictim(),SPELL_CLEAVE);
            Cleave_Timer += 8000;
        }

        Strike_Timer -= diff;
        if (Strike_Timer <= diff)
        {
            DoCast(m_creature->GetVictim(),SPELL_STRIKE);
            Strike_Timer += 10000;
        }

        Rend_Timer -= diff;
        if (Rend_Timer <= diff)
        {
            DoCast(m_creature->GetVictim(),SPELL_REND);
            Rend_Timer += 18000;
        }

        SunderArmor_Timer -= diff;
        if (SunderArmor_Timer <= diff)
        {
            DoCast(m_creature->GetVictim(),SPELL_SUNDERARMOR);
            SunderArmor_Timer += 25000;
        }

        KnockAway_Timer -= diff;
        if (KnockAway_Timer <= diff)
        {
            DoCast(m_creature->GetVictim(),SPELL_KNOCKAWAY);
            KnockAway_Timer += 12000;
        }

        Slow_Timer -= diff;
        if (Slow_Timer <= diff)
        {
            DoCast(m_creature->GetVictim(),SPELL_SLOW);
            Slow_Timer += 18000;
        }

        DoMeleeAttackIfReady();
    }
};
CreatureAI* GetAI_boss_highlordomokk(Creature *_Creature)
{
    return new boss_highlordomokkAI (_Creature);
}

void AddSC_boss_highlordomokk()
{
    Script *newscript;
    newscript = new Script;
    newscript->Name="boss_highlord_omokk";
    newscript->GetAI = &GetAI_boss_highlordomokk;
    newscript->RegisterSelf();
}

