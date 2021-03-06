/*
 * This file is part of the Colobot: Gold Edition source code
 * Copyright (C) 2001-2014, Daniel Roux, EPSITEC SA & TerranovaTeam
 * http://epsiteс.ch; http://colobot.info; http://github.com/colobot
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see http://gnu.org/licenses
 */

// motionworm.h

#pragma once


#include "object/motion/motion.h"



class CMotionWorm : public CMotion
{
public:
    CMotionWorm(CObject* object);
    ~CMotionWorm();

    void    DeleteObject(bool bAll=false);
    bool    Create(Math::Vector pos, float angle, ObjectType type, float power);
    bool    EventProcess(const Event &event);

    bool    SetParam(int rank, float value);
    float   GetParam(int rank);

protected:
    void    CreatePhysics();
    bool    EventFrame(const Event &event);

protected:
    float       m_timeUp;
    float       m_timeDown;
    float       m_armMember;
    float       m_armTimeAbs;
    float       m_armTimeMarch;
    float       m_armTimeAction;
    short       m_armAngles[3*3*3*3*10];
    int         m_armTimeIndex;
    int         m_armPartIndex;
    int         m_armMemberIndex;
    int         m_armLastAction;
    float       m_armLinSpeed;
    float       m_armCirSpeed;
    int         m_specAction;
    float       m_specTime;
    bool        m_bArmStop;
    float       m_lastParticle;
};

