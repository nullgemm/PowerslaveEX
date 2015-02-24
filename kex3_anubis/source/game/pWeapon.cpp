//
// Copyright(C) 2014-2015 Samuel Villarreal
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//      Player Weapon Behavior
//

#include "kexlib.h"
#include "game.h"
#include "renderMain.h"

//
// kexPlayerWeapon::kexPlayerWeapon
//

kexPlayerWeapon::kexPlayerWeapon(void)
{
    this->owner = NULL;
    this->anim = NULL;
    this->state = WS_IDLE;
    this->bob_x = 0;
    this->bob_y = 0;
}

//
// kexPlayerWeapon::~kexPlayerWeapon
//

kexPlayerWeapon::~kexPlayerWeapon(void)
{
}

//
// kexPlayerWeapon::Update
//

void kexPlayerWeapon::Update(void)
{
    UpdateBob();
    UpdateSprite();
}

//
// kexPlayerWeapon::ChangeAnim
//

void kexPlayerWeapon::ChangeAnim(spriteAnim_t *changeAnim)
{
    if(changeAnim == NULL)
    {
        return;
    }
    
    anim = changeAnim;
    frameID = 0;
    ticks = 0;

    if(anim == kexGame::cLocal->WeaponInfo(owner->CurrentWeapon())->idle)
    {
        state = WS_IDLE;
    }

    for(unsigned int i = 0; i < anim->frames[0].actions.Length(); ++i)
    {
        anim->frames[0].actions[i]->Execute(owner->Actor());
    }
}

//
// kexPlayerWeapon::ChangeAnim
//

void kexPlayerWeapon::ChangeAnim(const char *animName)
{
    ChangeAnim(kexGame::cLocal->SpriteAnimManager()->Get(animName));
}

//
// kexPlayerWeapon::ChangeAnim
//

void kexPlayerWeapon::ChangeAnim(const weaponState_t changeState)
{
    const kexGameLocal::weaponInfo_t *weaponInfo = kexGame::cLocal->WeaponInfo(owner->CurrentWeapon());

    switch(changeState)
    {
    case WS_IDLE:
        ChangeAnim(weaponInfo->idle);
        state = WS_IDLE;
        break;

    case WS_RAISE:
        ChangeAnim(weaponInfo->raise);
        state = WS_RAISE;
        break;

    case WS_LOWER:
        ChangeAnim(weaponInfo->lower);
        state = WS_LOWER;
        break;

    case WS_FIRE:
        ChangeAnim(weaponInfo->fire);
        state = WS_FIRE;
        break;

    default:
        break;
    }
}

//
// kexPlayerWeapon::UpdateBob
//

void kexPlayerWeapon::UpdateBob(void)
{
    kexActor *actor = owner->Actor();
    float movement = actor->Velocity().ToVec2().Unit();

    if((movement > 0 || owner->LandTime() < 0) &&
        (actor->Origin().z <= actor->FloorHeight() || actor->Flags() & AF_INWATER) &&
        state != WS_FIRE)
    {
        float t = (float)bobTime;
        float speed;
        float x, y;

        movement = movement / 16.0f;
        kexMath::Clamp(movement, 0, 1);

        speed = (actor->Flags() & AF_INWATER) ? 0.025f : 0.075f;

        x = kexMath::Sin(t * speed) * (16*movement);
        y = kexMath::Fabs(kexMath::Sin(t * speed) * (16*movement)) - owner->LandTime();

        bob_x = (x - bob_x) * 0.25f + bob_x;
        bob_y = (y - bob_y) * 0.25f + bob_y;

        bobTime++;
    }
    else
    {
        bobTime = 0;
        bob_x = (0 - bob_x) * 0.25f + bob_x;
        bob_y = (0 - bob_y) * 0.25f + bob_y;
    }
}

//
// kexPlayerWeapon::UpdateSprite
//

void kexPlayerWeapon::UpdateSprite(void)
{
    spriteFrame_t *frame;

    if(anim == NULL)
    {
        return;
    }

    frame = &anim->frames[frameID];
    ticks += (1.0f / (float)frame->delay) * 0.5f;
    
    // handle advancing to next frame
    if(ticks >= 1)
    {
        ticks = 0;

        // handle re-fire
        if(frame->HasRefireFrame() && owner->Cmd().Buttons() & BC_ATTACK)
        {
            ChangeAnim(frame->refireFrame);
            return;
        }
        // handle goto jumps
        else if(frame->HasNextFrame())
        {
            ChangeAnim(frame->nextFrame);
            return;
        }
        
        // reached the end of the frame?
        if(++frameID >= (int16_t)anim->NumFrames())
        {
            // loop back
            frameID = 0;

            // if lowering weapon, then switch to pending weapon
            if(state == WS_LOWER)
            {
                owner->ChangeWeapon();
                ChangeAnim(WS_RAISE);
                return;
            }
        }

        for(unsigned int i = 0; i < anim->frames[frameID].actions.Length(); ++i)
        {
            anim->frames[frameID].actions[i]->Execute(owner->Actor());
        }
    }

    switch(state)
    {
    case WS_IDLE:
        if(owner->PendingWeapon() != owner->CurrentWeapon())
        {
            ChangeAnim(WS_LOWER);
        }
        if(owner->Cmd().Buttons() & BC_ATTACK)
        {
            ChangeAnim(WS_FIRE);
        }
        break;

    default:
        break;
    }
}

//
// kexPlayerWeapon::DrawAnimFrame
//

void kexPlayerWeapon::DrawAnimFrame(spriteAnim_t *sprAnim)
{
    int frm = frameID;

    if(!sprAnim)
    {
        return;
    }

    const kexGameLocal::weaponInfo_t *weaponInfo = kexGame::cLocal->WeaponInfo(owner->CurrentWeapon());

    if(sprAnim == weaponInfo->flame && sprAnim->NumFrames() > 0)
    {
        frm = (kex::cSession->GetTicks() >> 1) % sprAnim->NumFrames();
    }

    kexCpuVertList  *vl = kexRender::cVertList;
    spriteFrame_t   *frame = &sprAnim->frames[frm];
    spriteSet_t     *spriteSet;
    kexSprite       *sprite;
    spriteInfo_t    *info;

    for(unsigned int i = 0; i < frame->spriteSet[0].Length(); ++i)
    {
        spriteSet = &frame->spriteSet[0][i];
        sprite = spriteSet->sprite;
        info = &sprite->InfoList()[spriteSet->index];

        float x = (float)spriteSet->x;
        float y = (float)spriteSet->y;
        float w = (float)info->atlas.w;
        float h = (float)info->atlas.h;
        word c = 0xff;

        float u1, u2, v1, v2;
        
        u1 = info->u[0 ^ spriteSet->bFlipped];
        u2 = info->u[1 ^ spriteSet->bFlipped];
        v1 = info->v[0];
        v2 = info->v[1];

        kexRender::cScreen->SetAspectDimentions(x, y, w, h);

        sprite->Texture()->Bind();

        x += bob_x + weaponInfo->offsetX;
        y += bob_y + weaponInfo->offsetY;

        if(!(frame->flags & SFF_FULLBRIGHT))
        {
            c = (owner->Actor()->Sector()->lightLevel << 1);

            if(c > 255)
            {
                c = 255;
            }
        }

        vl->AddQuad(x, y + 8, 0, w, h, u1, v1, u2, v2, (byte)c, (byte)c, (byte)c, 255);
        vl->DrawElements();
    }
}

//
// kexPlayerWeapon::Draw
//

void kexPlayerWeapon::Draw(void)
{
    const kexGameLocal::weaponInfo_t *weaponInfo = kexGame::cLocal->WeaponInfo(owner->CurrentWeapon());
    int which, ammo;
    
    kexRender::cScreen->SetOrtho();
    kexRender::cBackend->SetState(GLSTATE_DEPTHTEST, false);
    kexRender::cBackend->SetState(GLSTATE_SCISSOR, true);
    kexRender::cBackend->SetBlend(GLSRC_SRC_ALPHA, GLDST_ONE_MINUS_SRC_ALPHA);

    kexRender::cVertList->BindDrawPointers();
    ammo = owner->GetAmmo(owner->CurrentWeapon());

    if(state == WS_IDLE)
    {
        DrawAnimFrame(weaponInfo->flame);
    }

    if(ammo <= 0)
    {
        which = 2;
    }
    else if(ammo == 1)
    {
        which = 1;
    }
    else
    {
        which = 0;
    }

    if(state == WS_RAISE && weaponInfo->ammoRaise[which] && which != 2)
    {
        // nothing
    }
    else
    {
        DrawAnimFrame(anim);
    }

    switch(state)
    {
    case WS_IDLE:
        DrawAnimFrame(weaponInfo->ammoIdle[which]);
        break;

    case WS_RAISE:
        DrawAnimFrame(weaponInfo->ammoRaise[which]);
        break;

    case WS_LOWER:
        DrawAnimFrame(weaponInfo->ammoLower[which]);
        break;

    case WS_FIRE:
        DrawAnimFrame(weaponInfo->ammoFire[which]);
        break;
            
    default:
        break;
    }
}
