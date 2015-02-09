#include "ActorMon.h"

#include <vector>
#include <assert.h>

#include "Init.h"
#include "Item.h"
#include "ActorPlayer.h"
#include "GameTime.h"
#include "Attack.h"
#include "Reload.h"
#include "Inventory.h"
#include "FeatureTrap.h"
#include "FeatureMob.h"
#include "Properties.h"
#include "Render.h"
#include "Sound.h"
#include "Utils.h"
#include "Map.h"
#include "Log.h"
#include "MapParsing.h"
#include "Ai.h"
#include "LineCalc.h"
#include "Gods.h"
#include "ItemFactory.h"
#include "ActorFactory.h"
#include "Knockback.h"
#include "Explosion.h"
#include "Popup.h"

using namespace std;

Mon::Mon() :
    Actor                   (),
    awareCounter_           (0),
    playerAwareOfMeCounter_ (0),
    isMsgMonInViewPrinted_  (false),
    lastDirTravelled_       (Dir::center),
    spellCoolDownCur_       (0),
    isRoamingAllowed_       (true),
    isStealth_              (false),
    leader_                 (nullptr),
    tgt_                    (nullptr),
    waiting_                (false),
    shockCausedCur_         (0.0),
    hasGivenXpForSpotting_  (false),
    nrTurnsUntilUnsummoned_ (-1) {}

Mon::~Mon()
{
    for (Spell* const spell : spellsKnown_) {delete spell;}
}

void Mon::onActorTurn()
{
#ifndef NDEBUG
    //Verify that monster is not outside the map
    if (!Utils::isPosInsideMap(pos, false))
    {
        TRACE << "Monster outside map" << endl;
        assert(false);
    }

    //Verify that monster's leader does not have a leader (never allowed)
    if (leader_ && !isActorMyLeader(Map::player) && static_cast<Mon*>(leader_)->leader_)
    {
        TRACE << "Two (or more) steps of leader is never allowed" << endl;
        assert(false);
    }
#endif // NDEBUG

    if (awareCounter_ <= 0 && !isActorMyLeader(Map::player))
    {
        waiting_ = !waiting_;

        if (waiting_)
        {
            GameTime::tick();
            return;
        }
    }
    else //Is aware, or player is leader
    {
        waiting_ = false;
    }

    //Pick a target
    vector<Actor*> tgtBucket;

    bool props[size_t(PropId::END)];
    propHandler_->getPropIds(props);

    if (props[size_t(PropId::conflict)])
    {
        //Monster is conflicted (e.g. by player ring/amulet)
        tgtBucket = GameTime::actors_;

        bool blockedLos[MAP_W][MAP_H];

        MapParse::run(CellCheck::BlocksLos(), blockedLos);

        //Remove self and all unseen actors from vector
        for (auto it = begin(tgtBucket); it != end(tgtBucket);)
        {
            if (*it == this || !canSeeActor(**it, blockedLos))
            {
                tgtBucket.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }
    else //Not conflicted
    {
        getSeenFoes(tgtBucket);

        // If not aware, remove player from target bucket
        if (awareCounter_ <= 0)
        {
            for (auto it = begin(tgtBucket); it != end(tgtBucket); ++it)
            {
                if (*it == Map::player)
                {
                    tgtBucket.erase(it);
                    break;
                }
            }
        }
    }

    tgt_ = Utils::getRandomClosestActor(pos, tgtBucket);

    if (spellCoolDownCur_ != 0) {spellCoolDownCur_--;}

    if (awareCounter_ > 0)
    {
        isRoamingAllowed_ = true;
        if (leader_)
        {
            if (leader_->isAlive() && !isActorMyLeader(Map::player))
            {
                static_cast<Mon*>(leader_)->awareCounter_ =
                    leader_->getData().nrTurnsAware;
            }
        }
        else //Monster does not have a leader
        {
            if (isAlive() && Rnd::oneIn(14))
            {
                speakPhrase();
            }
        }
    }

    isStealth_ = !isActorMyLeader(Map::player)                                  &&
                 data_->abilityVals.getVal(AbilityId::stealth, true, *this) > 0 &&
                 !Map::player->canSeeActor(*this, nullptr);

    //Array used for AI purposes, e.g. to prevent tactically bad positions,
    //or prevent certain monsters from walking on a certain type of cells, etc.
    //This is checked in all AI movement functions. Cells set to true are
    //totally forbidden for the monster to move into.
    bool aiSpecialBlockers[MAP_W][MAP_H];
    Ai::Info::setSpecialBlockedCells(*this, aiSpecialBlockers);

    //------------------------------ SPECIAL MONSTER ACTIONS
    //                               (ZOMBIES RISING, WORMS MULTIPLYING...)
    if (leader_ != Map::player && (tgt_ == nullptr || tgt_ == Map::player))
    {
        if (onActorTurn_())
        {
            return;
        }
    }

    //------------------------------ COMMON ACTIONS
    //                               (MOVING, ATTACKING, CASTING SPELLS...)
    //Looking is as an action if monster not aware before, and became aware from looking.
    //(This is to give the monsters some reaction time, and not instantly attack)
    if (
        data_->ai[int(AiId::looks)] &&
        leader_ != Map::player      &&
        (tgt_ == nullptr || tgt_ == Map::player))
    {
        if (Ai::Info::lookBecomePlayerAware(*this))
        {
            return;
        }
    }

    if (data_->ai[int(AiId::makesRoomForFriend)] && tgt_ == Map::player)
    {
        if (Ai::Action::makeRoomForFriend(*this))
        {
            return;
        }
    }

    if (Rnd::oneIn(6))
    {
        if (Ai::Action::tryCastRandomSpell(*this))
        {
            return;
        }
    }

    if (data_->ai[int(AiId::attacks)] && tgt_)
    {
        if (tryAttack(*tgt_))
        {
            return;
        }
    }

    if (Ai::Action::tryCastRandomSpell(*this))
    {
        return;
    }

    int erraticMovePct = int(data_->erraticMovePct);

    //Never move erratically if frenzied
    if (props[size_t(PropId::frenzied)])
    {
        erraticMovePct = 0;
    }

    //Move less erratically if allied to player
    if (isActorMyLeader(Map::player))
    {
        erraticMovePct /= 2;
    }

    //Move more erratically if confused
    if (props[size_t(PropId::confused)])
    {
        erraticMovePct *= 2;
    }

    constrInRange(0, erraticMovePct, 95);

    if (data_->ai[int(AiId::movesToRandomWhenUnaware)] && Rnd::percent(erraticMovePct))
    {
        if (Ai::Action::moveToRandomAdjCell(*this))
        {
            return;
        }
    }

    if (data_->ai[int(AiId::movesToTgtWhenLos)])
    {
        if (Ai::Action::moveToTgtSimple(*this))
        {
            return;
        }
    }

    vector<Pos> path;

    if (data_->ai[int(AiId::pathsToTgtWhenAware)] && leader_ != Map::player)
    {
        Ai::Info::setPathToPlayerIfAware(*this, path);
    }

    if (leader_ != Map::player)
    {
        if (Ai::Action::handleClosedBlockingDoor(*this, path))
        {
            return;
        }
    }

    if (Ai::Action::stepPath(*this, path))
    {
        return;
    }

    if (data_->ai[int(AiId::movesToLeader)])
    {
        Ai::Info::setPathToLeaderIfNoLosToleader(*this, path);

        if (Ai::Action::stepPath(*this, path))
        {
            return;
        }
    }

    if (
        data_->ai[int(AiId::movesToLair)]   &&
        leader_ != Map::player              &&
        (tgt_ == nullptr || tgt_ == Map::player))
    {
        if (Ai::Action::stepToLairIfLos(*this, lairCell_))
        {
            return;
        }
        else //No LOS to lair
        {
            //Try to use pathfinder to travel to lair
            Ai::Info::setPathToLairIfNoLos(*this, path, lairCell_);

            if (Ai::Action::stepPath(*this, path))
            {
                return;
            }
        }
    }

    if (data_->ai[int(AiId::movesToRandomWhenUnaware)])
    {
        if (Ai::Action::moveToRandomAdjCell(*this))
        {
            return;
        }
    }

    GameTime::tick();
}

void Mon::onStdTurn()
{
    if (nrTurnsUntilUnsummoned_ > 0)
    {
        --nrTurnsUntilUnsummoned_;

        if (nrTurnsUntilUnsummoned_ <= 0)
        {
            if (Map::player->canSeeActor(*this, nullptr))
            {
                Log::addMsg(getNameThe() + " suddenly disappears!");
            }
            state_ = ActorState::destroyed;
            return;
        }
    }
    onStdTurn_();
}

void Mon::onHit(int& dmg)
{
    (void)dmg;
    awareCounter_ = data_->nrTurnsAware;
}

void Mon::moveDir(Dir dir)
{
#ifndef NDEBUG
    if (dir == Dir::END)
    {
        TRACE << "Illegal direction parameter" << endl;
        assert(false);
    }

    if (!Utils::isPosInsideMap(pos, false))
    {
        TRACE << "Monster outside map" << endl;
        assert(false);
    }
#endif // NDEBUG

    getPropHandler().changeMoveDir(pos, dir);

    //Trap affects leaving?
    if (dir != Dir::center)
    {
        auto* f = Map::cells[pos.x][pos.y].rigid;
        if (f->getId() == FeatureId::trap)
        {
            dir = static_cast<Trap*>(f)->actorTryLeave(*this, dir);
            if (dir == Dir::center)
            {
                TRACE_VERBOSE << "Monster move prevented by trap" << endl;
                GameTime::tick();
                return;
            }
        }
    }

    // Movement direction is stored for AI purposes
    lastDirTravelled_ = dir;

    const Pos targetCell(pos + DirUtils::getOffset(dir));

    if (dir != Dir::center && Utils::isPosInsideMap(targetCell, false))
    {
        pos = targetCell;

        //Bump features in target cell (i.e. to trigger traps)
        vector<Mob*> mobs;
        GameTime::getMobsAtPos(pos, mobs);
        for (auto* m : mobs) {m->bump(*this);}
        Map::cells[pos.x][pos.y].rigid->bump(*this);
    }

    GameTime::tick();
}

void Mon::hearSound(const Snd& snd)
{
    if (isAlive())
    {
        if (snd.isAlertingMon())
        {
            becomeAware(false);
        }
    }
}

void Mon::speakPhrase()
{
    const bool IS_SEEN_BY_PLAYER = Map::player->canSeeActor(*this, nullptr);
    const string msg = IS_SEEN_BY_PLAYER ?
                       getAggroPhraseMonSeen() :
                       getAggroPhraseMonHidden();
    const SfxId sfx = IS_SEEN_BY_PLAYER ?
                      getAggroSfxMonSeen() :
                      getAggroSfxMonHidden();

    Snd snd(msg, sfx, IgnoreMsgIfOriginSeen::no, pos, this,
            SndVol::low, AlertsMon::yes);
    SndEmit::emitSnd(snd);
}

void Mon::becomeAware(const bool IS_FROM_SEEING)
{
    if (isAlive())
    {
        const int AWARENESS_CNT_BEFORE  = awareCounter_;
        awareCounter_                   = data_->nrTurnsAware;
        if (AWARENESS_CNT_BEFORE <= 0)
        {
            if (IS_FROM_SEEING && Map::player->canSeeActor(*this, nullptr))
            {
                Map::player->updateFov();
                Render::drawMapAndInterface(true);
                Log::addMsg(getNameThe() + " sees me!");
            }
            if (Rnd::coinToss())
            {
                speakPhrase();
            }
        }
    }
}

void Mon::playerBecomeAwareOfMe(const int DURATION_FACTOR)
{
    const int LOWER         = 4 * DURATION_FACTOR;
    const int UPPER         = 6 * DURATION_FACTOR;
    const int ROLL          = Rnd::range(LOWER, UPPER);
    playerAwareOfMeCounter_ = max(playerAwareOfMeCounter_, ROLL);
}

bool Mon::tryAttack(Actor& defender)
{
    if (state_ != ActorState::alive || (awareCounter_ <= 0 && leader_ != Map::player))
    {
        return false;
    }

    AiAvailAttacksData availAttacks;
    getAvailAttacks(defender, availAttacks);

    const AiAttData att = getAtt(availAttacks);

    if (!att.weapon)
    {
        return false;
    }

    if (att.isMelee)
    {
        if (att.weapon->getData().melee.isMeleeWpn)
        {
            Attack::melee(*this, *att.weapon, defender);
            return true;
        }
        return false;
    }

    if (att.weapon->getData().ranged.isRangedWpn)
    {
        if (availAttacks.isTimeToReload)
        {
            Reload::reloadWieldedWpn(*this);
            return true;
        }

        //Check if friend is in the way (with a small chance to ignore this)
        bool isBlockedByFriend = false;
        if (Rnd::fraction(4, 5))
        {
            vector<Pos> line;
            LineCalc::calcNewLine(pos, defender.pos, true, 9999, false, line);
            for (Pos& linePos : line)
            {
                if (linePos != pos && linePos != defender.pos)
                {
                    Actor* const actorHere = Utils::getActorAtPos(linePos);
                    if (actorHere)
                    {
                        isBlockedByFriend = true;
                        break;
                    }
                }
            }
        }

        if (isBlockedByFriend) {return false;}

        const int NR_TURNS_NO_RANGED = data_->rangedCooldownTurns;

        PropDisabledRanged* rangedCooldownProp =
            new PropDisabledRanged(PropTurns::specific, NR_TURNS_NO_RANGED);

        propHandler_->tryApplyProp(rangedCooldownProp);

        Attack::ranged(*this, *att.weapon, defender.pos);

        return true;
    }

    return false;
}

void Mon::getAvailAttacks(Actor& defender, AiAvailAttacksData& availAttacksRef)
{
    if (propHandler_->allowAttack(false))
    {
        availAttacksRef.isMelee = Utils::isPosAdj(pos, defender.pos, false);

        Wpn* weapon = nullptr;
        const size_t nrIntrinsics = inv_->getIntrinsicsSize();

        if (availAttacksRef.isMelee)
        {
            if (propHandler_->allowAttackMelee(false))
            {

                //Melee weapon in wielded slot?
                weapon = static_cast<Wpn*>(inv_->getItemInSlot(SlotId::wielded));

                if (weapon)
                {
                    if (weapon->getData().melee.isMeleeWpn)
                    {
                        availAttacksRef.weapons.push_back(weapon);
                    }
                }

                //Intrinsic melee attacks?
                for (size_t i = 0; i < nrIntrinsics; ++i)
                {
                    weapon = static_cast<Wpn*>(inv_->getIntrinsicInElement(i));
                    if (weapon->getData().melee.isMeleeWpn)
                    {
                        availAttacksRef.weapons.push_back(weapon);
                    }
                }
            }
        }
        else //Ranged attack
        {
            if (propHandler_->allowAttackRanged(false))
            {
                //Ranged weapon in wielded slot?
                weapon =
                    static_cast<Wpn*>(inv_->getItemInSlot(SlotId::wielded));

                if (weapon)
                {
                    if (weapon->getData().ranged.isRangedWpn)
                    {
                        availAttacksRef.weapons.push_back(weapon);

                        //Check if reload time instead
                        if (
                            weapon->nrAmmoLoaded == 0 &&
                            !weapon->getData().ranged.hasInfiniteAmmo)
                        {
                            if (inv_->hasAmmoForFirearmInInventory())
                            {
                                availAttacksRef.isTimeToReload = true;
                            }
                        }
                    }
                }

                //Intrinsic ranged attacks?
                for (size_t i = 0; i < nrIntrinsics; ++i)
                {
                    weapon = static_cast<Wpn*>(inv_->getIntrinsicInElement(i));
                    if (weapon->getData().ranged.isRangedWpn)
                    {
                        availAttacksRef.weapons.push_back(weapon);
                    }
                }
            }
        }
    }
}

AiAttData Mon::getAtt(const AiAvailAttacksData& availAttacks)
{
    AiAttData att(nullptr, availAttacks.isMelee);

    if (availAttacks.weapons.empty())
    {
        return att;
    }

    const size_t IDX = Rnd::range(0, availAttacks.weapons.size() - 1);

    att.weapon = availAttacks.weapons[IDX];

    return att;
}

bool Mon::isLeaderOf(const Actor* const actor) const
{
    if (!actor || actor->isPlayer())
        return false;

    //Actor is a monster
    return static_cast<const Mon*>(actor)->leader_ == this;
}

bool Mon::isActorMyLeader(const Actor* const actor) const
{
    return leader_ == actor;
}

int Mon::getGroupSize()
{
    const Actor* const groupLeader = leader_ ? leader_ : this;

    int ret = 1; //Starting at one to include leader

    for (const Actor* const actor : GameTime::actors_)
    {
        if (actor->isActorMyLeader(groupLeader))
        {
            ++ret;
        }
    }

    return ret;
}

//--------------------------------------------------------- SPECIFIC MONSTERS
string Cultist::getCultistPhrase()
{
    vector<string> phraseBucket;

    const God* const god = Gods::getCurGod();

    if (god && Rnd::coinToss())
    {
        const string name   = god->getName();
        const string descr  = god->getDescr();
        phraseBucket.push_back(name + " save us!");
        phraseBucket.push_back(descr + " will save us!");
        phraseBucket.push_back(name + ", guide us!");
        phraseBucket.push_back(descr + " guides us!");
        phraseBucket.push_back("For " + name + "!");
        phraseBucket.push_back("For " + descr + "!");
        phraseBucket.push_back("Blood for " + name + "!");
        phraseBucket.push_back("Blood for " + descr + "!");
        phraseBucket.push_back("Perish for " + name + "!");
        phraseBucket.push_back("Perish for " + descr + "!");
        phraseBucket.push_back("In the name of " + name + "!");
    }
    else
    {
        phraseBucket.push_back("Apigami!");
        phraseBucket.push_back("Bhuudesco invisuu!");
        phraseBucket.push_back("Bhuuesco marana!");
        phraseBucket.push_back("Crudux cruo!");
        phraseBucket.push_back("Cruento paashaeximus!");
        phraseBucket.push_back("Cruento pestis shatruex!");
        phraseBucket.push_back("Cruo crunatus durbe!");
        phraseBucket.push_back("Cruo lokemundux!");
        phraseBucket.push_back("Cruo-stragaraNa!");
        phraseBucket.push_back("Gero shay cruo!");
        phraseBucket.push_back("In marana domus-bhaava crunatus!");
        phraseBucket.push_back("Caecux infirmux!");
        phraseBucket.push_back("Malax sayti!");
        phraseBucket.push_back("Marana pallex!");
        phraseBucket.push_back("Marana malax!");
        phraseBucket.push_back("Pallex ti!");
        phraseBucket.push_back("Peroshay bibox malax!");
        phraseBucket.push_back("Pestis Cruento!");
        phraseBucket.push_back("Pestis cruento vilomaxus pretiacruento!");
        phraseBucket.push_back("Pretaanluxis cruonit!");
        phraseBucket.push_back("Pretiacruento!");
        phraseBucket.push_back("StragarNaya!");
        phraseBucket.push_back("Vorox esco marana!");
        phraseBucket.push_back("Vilomaxus!");
        phraseBucket.push_back("Prostragaranar malachtose!");
        phraseBucket.push_back("Apigami!");
    }

    return phraseBucket[Rnd::range(0, phraseBucket.size() - 1)];
}

void Cultist::mkStartItems()
{
    //If we are on a low-ish dlvl, let the vast majority of cultists carry pistols
    const bool  IS_LOW_DLVL   = Map::dlvl < 4;

    const int   PISTOL        = IS_LOW_DLVL   ? 20 : 6;
    const int   PUMP_SHOTGUN  = PISTOL        + 3;
    const int   SAWN_SHOTGUN  = PUMP_SHOTGUN  + 3;
    const int   MG            = SAWN_SHOTGUN  + 1;

    const int   TOT           = MG;
    const int   RND           = Map::dlvl == 0 ? PISTOL : Rnd::range(1, TOT);

    if (RND <= PISTOL)
    {
        Item*     item                        = ItemFactory::mk(ItemId::pistol);
        const int AMMO_CAP                    = static_cast<Wpn*>(item)->AMMO_CAP;
        static_cast<Wpn*>(item)->nrAmmoLoaded = Rnd::range(AMMO_CAP / 4, AMMO_CAP);

        inv_->putInSlot(SlotId::wielded, item);

        if (Rnd::oneIn(5))
        {
            inv_->putInGeneral(ItemFactory::mk(ItemId::pistolClip));
        }
    }
    else if (RND <= PUMP_SHOTGUN)
    {
        Item*     item                        = ItemFactory::mk(ItemId::pumpShotgun);
        const int AMMO_CAP                    = static_cast<Wpn*>(item)->AMMO_CAP;
        static_cast<Wpn*>(item)->nrAmmoLoaded = Rnd::range(AMMO_CAP / 4, AMMO_CAP);

        inv_->putInSlot(SlotId::wielded, item);

        if (Rnd::oneIn(4))
        {
            item            = ItemFactory::mk(ItemId::shotgunShell);
            item->nrItems_  = Rnd::range(1, 8);
            inv_->putInGeneral(item);
        }
    }
    else if (RND <= SAWN_SHOTGUN)
    {
        inv_->putInSlot(SlotId::wielded, ItemFactory::mk(ItemId::sawedOff));

        if (Rnd::oneIn(4))
        {
            Item* item      = ItemFactory::mk(ItemId::shotgunShell);
            item->nrItems_  = Rnd::range(1, 8);
            inv_->putInGeneral(item);
        }
    }
    else //Machine gun
    {
        //Number of machine gun bullets loaded needs to be a multiple of the number of
        //projectiles fired in each burst
        Item*       item        = ItemFactory::mk(ItemId::machineGun);
        Wpn* const  wpn         = static_cast<Wpn*>(item);
        const int   CAP_SCALED  = wpn->AMMO_CAP / NR_MG_PROJECTILES;
        const int   MIN_SCALED  = CAP_SCALED / 4;
        wpn->nrAmmoLoaded       = Rnd::range(MIN_SCALED, CAP_SCALED) * NR_MG_PROJECTILES;
        inv_->putInSlot(SlotId::wielded, item);
    }

    if (Rnd::oneIn(3))
    {
        inv_->putInGeneral(ItemFactory::mkRandomScrollOrPotion(true, true));
    }

    if (Rnd::oneIn(12))
    {
        spellsKnown_.push_back(SpellHandling::getRandomSpellForMon());
    }
}

void CultistElectric::mkStartItems()
{
    Item*       item        = ItemFactory::mk(ItemId::miGoGun);
    const int   AMMO_CAP    = static_cast<Wpn*>(item)->AMMO_CAP;

    static_cast<Wpn*>(item)->nrAmmoLoaded = Rnd::range(AMMO_CAP / 4, AMMO_CAP);

    inv_->putInSlot(SlotId::wielded, item);

    if (Rnd::oneIn(3))
    {
        inv_->putInGeneral(ItemFactory::mkRandomScrollOrPotion(true, true));
    }

    if (Rnd::oneIn(3))
    {
        spellsKnown_.push_back(SpellHandling::getRandomSpellForMon());
    }
}

void CultistSpikeGun::mkStartItems()
{
    Item* item = ItemFactory::mk(ItemId::spikeGun);
    const int AMMO_CAP = static_cast<Wpn*>(item)->AMMO_CAP;
    static_cast<Wpn*>(item)->nrAmmoLoaded = Rnd::range(AMMO_CAP / 4, AMMO_CAP);
    inv_->putInSlot(SlotId::wielded, item);

    item = ItemFactory::mk(ItemId::ironSpike);
    item->nrItems_ = Rnd::range(4, 12);
    inv_->putInGeneral(item);
}

void CultistPriest::mkStartItems()
{
    Item* item = ItemFactory::mk(ItemId::dagger);
    item->meleeDmgPlus_ = 2;
    inv_->putInSlot(SlotId::wielded, item);

    inv_->putInGeneral(ItemFactory::mkRandomScrollOrPotion(true, true));
    inv_->putInGeneral(ItemFactory::mkRandomScrollOrPotion(true, true));

    const int NR_SPELLS = 3;

    for (int i = 0; i < NR_SPELLS; ++i)
    {
        spellsKnown_.push_back(SpellHandling::getRandomSpellForMon());
    }
}

void FireHound::mkStartItems()
{
    inv_->putInIntrinsics(ItemFactory::mk(ItemId::fireHoundBreath));
    inv_->putInIntrinsics(ItemFactory::mk(ItemId::fireHoundBite));
}

void FrostHound::mkStartItems()
{
    inv_->putInIntrinsics(ItemFactory::mk(ItemId::frostHoundBreath));
    inv_->putInIntrinsics(ItemFactory::mk(ItemId::frostHoundBite));
}

void Zuul::place_()
{
    if (ActorData::data[size_t(ActorId::zuul)].nrLeftAllowedToSpawn > 0)
    {
        //NOTE: Do not call die() here - that would have side effects such as player
        //getting XP. Instead, simply set the dead state to destroyed.
        state_              = ActorState::destroyed;

        Actor* actor        = ActorFactory::mk(ActorId::cultistPriest, pos);
        auto& propHandler   = actor->getPropHandler();

        propHandler.tryApplyProp(new PropPossByZuul(PropTurns::indefinite), true);
        actor->restoreHp(999, false);
    }
}

void Zuul::mkStartItems()
{
    inv_->putInIntrinsics(ItemFactory::mk(ItemId::zuulBite));
}

bool Vortex::onActorTurn_()
{
    if (!isAlive())
    {
        return false;
    }

    if (pullCooldown > 0)
    {
        --pullCooldown;
    }

    if (awareCounter_ <= 0 || pullCooldown > 0)
    {
        return false;
    }

    const Pos& playerPos = Map::player->pos;

    if (!Utils::isPosAdj(pos, playerPos, true) && Rnd::oneIn(4))
    {
        TRACE << "Vortex attempting to pull player" << endl;

        const Pos   delta               = playerPos - pos;
        Pos         knockBackFromPos    = playerPos;
        if (delta.x >  1) {knockBackFromPos.x++;}
        if (delta.x < -1) {knockBackFromPos.x--;}
        if (delta.y >  1) {knockBackFromPos.y++;}
        if (delta.y < -1) {knockBackFromPos.y--;}

        if (knockBackFromPos != playerPos)
        {
            TRACE << "Good pos found to knockback player from (";
            TRACE << knockBackFromPos.x << ",";
            TRACE << knockBackFromPos.y << ")" << endl;
            TRACE << "Player position: ";
            TRACE << playerPos.x << "," << playerPos.y << ")" << endl;
            bool blockedLos[MAP_W][MAP_H];
            MapParse::run(CellCheck::BlocksLos(), blockedLos);
            if (canSeeActor(*(Map::player), blockedLos))
            {
                TRACE << "I am seeing the player" << endl;
                if (Map::player->canSeeActor(*this, nullptr))
                {
                    Log::addMsg("The Vortex attempts to pull me in!");
                }
                else
                {
                    Log::addMsg("A powerful wind is pulling me!");
                }
                TRACE << "Attempt pull (knockback)" << endl;
                KnockBack::tryKnockBack(*(Map::player), knockBackFromPos,
                                        false, false);
                pullCooldown = 5;
                GameTime::tick();
                return true;
            }
        }
    }
    return false;
}

void DustVortex::onDeath()
{
    Explosion::runExplosionAt(pos, ExplType::applyProp, ExplSrc::misc, 0, SfxId::END,
                              new PropBlind(PropTurns::std), &clrGray);
}

void DustVortex::mkStartItems()
{
    inv_->putInIntrinsics(ItemFactory::mk(ItemId::dustVortexEngulf));
}

void FireVortex::onDeath()
{
    Explosion::runExplosionAt(
        pos, ExplType::applyProp, ExplSrc::misc, 0, SfxId::END,
        new PropBurning(PropTurns::std), &clrRedLgt);
}

void FireVortex::mkStartItems()
{
    inv_->putInIntrinsics(ItemFactory::mk(ItemId::fireVortexEngulf));
}

void FrostVortex::onDeath()
{
    //TODO: Add explosion with cold damage
}

void FrostVortex::mkStartItems()
{
    inv_->putInIntrinsics(ItemFactory::mk(ItemId::frostVortexEngulf));
}

bool Ghost::onActorTurn_()
{
    if (
        isAlive()                                     &&
        awareCounter_ > 0                             &&
        Utils::isPosAdj(pos, Map::player->pos, false) &&
        Rnd::percent() < 30)
    {
        bool blocked[MAP_W][MAP_H];
        MapParse::run(CellCheck::BlocksLos(), blocked);

        const bool PLAYER_SEES_ME = Map::player->canSeeActor(*this, blocked);
        const string refer        = PLAYER_SEES_ME ? getNameThe() : "It";

        Log::addMsg(refer + " reaches for me... ");

        const AbilityRollResult rollResult =
            AbilityRoll::roll(Map::player->getData().abilityVals.getVal(
                                  AbilityId::dodgeAtt, true, *this));
        const bool PLAYER_DODGES = rollResult >= successSmall;
        if (PLAYER_DODGES)
        {
            Log::addMsg("I dodge!", clrMsgGood);
        }
        else
        {
            Map::player->getPropHandler().tryApplyProp(
                new PropSlowed(PropTurns::std));
        }
        GameTime::tick();
        return true;
    }
    return false;
}

void Ghost::mkStartItems()
{
    inv_->putInIntrinsics(ItemFactory::mk(ItemId::ghostClaw));
}

void Phantasm::mkStartItems()
{
    inv_->putInIntrinsics(ItemFactory::mk(ItemId::phantasmSickle));
}

void Wraith::mkStartItems()
{
    inv_->putInIntrinsics(ItemFactory::mk(ItemId::wraithClaw));
    spellsKnown_.push_back(SpellHandling::getRandomSpellForMon());
    spellsKnown_.push_back(SpellHandling::getRandomSpellForMon());
}

void MiGo::mkStartItems()
{
    Item*       item        = ItemFactory::mk(ItemId::miGoGun);
    const int   AMMO_CAP    = static_cast<Wpn*>(item)->AMMO_CAP;

    static_cast<Wpn*>(item)->nrAmmoLoaded = Rnd::range(AMMO_CAP / 4, AMMO_CAP);

    inv_->putInSlot(SlotId::wielded, item);

    if (getId() == ActorId::miGo)
    {
        inv_->putInIntrinsics(ItemFactory::mk(ItemId::miGoSting));
    }
    else if (getId() == ActorId::miGoCommander)
    {
        inv_->putInIntrinsics(ItemFactory::mk(ItemId::miGoCommanderSting));

        if (Rnd::oneIn(3))
        {
            inv_->putInSlot(SlotId::body, ItemFactory::mk(ItemId::armorMiGo));
        }
    }

    if (Rnd::oneIn(9))
    {
        inv_->putInGeneral(ItemFactory::mk(ItemId::miGoGunAmmo));
    }

    spellsKnown_.push_back(new SpellTeleport);
    spellsKnown_.push_back(new SpellMiGoHypno);
    spellsKnown_.push_back(new SpellHealSelf);

    if (Rnd::coinToss())
    {
        spellsKnown_.push_back(SpellHandling::getRandomSpellForMon());
    }
}

void SentryDrone::mkStartItems()
{
    spellsKnown_.push_back(new SpellTeleport);
    spellsKnown_.push_back(new SpellHealSelf);
    spellsKnown_.push_back(new SpellDarkbolt);
    spellsKnown_.push_back(new SpellBurn);
}

void FlyingPolyp::mkStartItems()
{
    inv_->putInIntrinsics(ItemFactory::mk(ItemId::polypTentacle));
}

void GreaterPolyp::mkStartItems()
{
    inv_->putInIntrinsics(ItemFactory::mk(ItemId::greaterPolypTentacle));
}

void Rat::mkStartItems()
{
    Item* item = nullptr;
    if (Rnd::percent() < 15)
    {
        item = ItemFactory::mk(ItemId::ratBiteDiseased);
    }
    else
    {
        item = ItemFactory::mk(ItemId::ratBite);
    }
    inv_->putInIntrinsics(item);
}

void RatThing::mkStartItems()
{
    inv_->putInIntrinsics(ItemFactory::mk(ItemId::ratThingBite));
}

void BrownJenkin::mkStartItems()
{
    inv_->putInIntrinsics(ItemFactory::mk(ItemId::brownJenkinBite));
    spellsKnown_.push_back(new SpellTeleport);
}

void Shadow::mkStartItems()
{
    inv_->putInIntrinsics(ItemFactory::mk(ItemId::shadowClaw));
}

void Ghoul::mkStartItems()
{
    inv_->putInIntrinsics(ItemFactory::mk(ItemId::ghoulClaw));
}

void Mummy::mkStartItems()
{
    inv_->putInIntrinsics(ItemFactory::mk(ItemId::mummyMaul));

    spellsKnown_.push_back(SpellHandling::mkSpellFromId(SpellId::disease));

    const int NR_SPELLS = 3;

    for (int i = 0; i < NR_SPELLS; ++i)
    {
        spellsKnown_.push_back(SpellHandling::getRandomSpellForMon());
    }
}

bool Mummy::onActorTurn_()
{
    //TODO: Below is an implementation for mummies turning friendly if player is wielding
    //the Staff of the Pharoh. It is commented out at least until after v17.0 is released.
    //It is probably too powerful and unbalanced. Perhaps add this power as a separate
    //spell to the Staff (something like "Pharohs Command")?

//    if (
//        !isAlive()          ||
//        data_->isUnique     ||
//        awareCounter_ <= 0  ||
//        isActorMyLeader(Map::player))
//    {
//        return false;
//    }
//
//    const Item* const playerWpn = Map::player->getInv().getItemInSlot(SlotId::wielded);
//
//    if (playerWpn && playerWpn->getId() == ItemId::pharaohStaff)
//    {
//        bool blockedLos[MAP_W][MAP_H];
//        MapParse::run(CellCheck::BlocksLos(), blockedLos);
//
//        if (canSeeActor(*Map::player, blockedLos))
//        {
//            if (Map::player->canSeeActor(*this, nullptr))
//            {
//                const string name = getNameThe();
//
//                Log::addMsg(name + " bows before me.", clrWhite, false, true);
//            }
//
//            leader_ = Map::player;
//
//            GameTime::tick();
//            return true;
//        }
//    }

    return false;
}

void MummyCrocHead::mkStartItems()
{
    inv_->putInIntrinsics(ItemFactory::mk(ItemId::crocHeadMummySpear));
}

void MummyUnique::mkStartItems()
{
    inv_->putInIntrinsics(ItemFactory::mk(ItemId::mummyMaul));

    spellsKnown_.push_back(SpellHandling::mkSpellFromId(SpellId::disease));

    spellsKnown_.push_back(SpellHandling::getRandomSpellForMon());
    spellsKnown_.push_back(SpellHandling::getRandomSpellForMon());
    spellsKnown_.push_back(SpellHandling::getRandomSpellForMon());
}

bool Khephren::onActorTurn_()
{
    if (isAlive() && awareCounter_ > 0 && !hasSummonedLocusts)
    {
        bool blocked[MAP_W][MAP_H];
        MapParse::run(CellCheck::BlocksLos(), blocked);

        if (canSeeActor(*(Map::player), blocked))
        {
            MapParse::run(CellCheck::BlocksMoveCmn(true), blocked);

            const int SPAWN_AFTER_X = Map::player->pos.x + FOV_STD_RADI_INT + 1;

            for (int y = 0; y  < MAP_H; ++y)
            {
                for (int x = 0; x <= SPAWN_AFTER_X; ++x)
                {
                    blocked[x][y] = true;
                }
            }

            vector<Pos> freeCells;
            Utils::mkVectorFromBoolMap(false, blocked, freeCells);

            sort(begin(freeCells), end(freeCells), IsCloserToPos(pos));

            const size_t NR_OF_SPAWNS = 15;
            if (freeCells.size() >= NR_OF_SPAWNS + 1)
            {
                Log::addMsg("Khephren calls a plague of Locusts!");
                Map::player->incrShock(ShockLvl::heavy, ShockSrc::misc);
                for (size_t i = 0; i < NR_OF_SPAWNS; ++i)
                {
                    Actor* const actor  = ActorFactory::mk(ActorId::locust, freeCells[0]);
                    Mon* const mon      = static_cast<Mon*>(actor);
                    mon->awareCounter_  = 999;
                    mon->leader_        = leader_ ? leader_ : this;
                    freeCells.erase(begin(freeCells));
                }
                Render::drawMapAndInterface();
                hasSummonedLocusts = true;
                GameTime::tick();
                return true;
            }
        }
    }

    return false;
}

void DeepOne::mkStartItems()
{
    inv_->putInIntrinsics(ItemFactory::mk(ItemId::deepOneJavelinAtt));
    inv_->putInIntrinsics(ItemFactory::mk(ItemId::deepOneSpearAtt));
}

void Ape::mkStartItems()
{
    inv_->putInIntrinsics(ItemFactory::mk(ItemId::apeMaul));
}

bool Ape::onActorTurn_()
{
    if (frenzyCoolDown_ > 0)
    {
        --frenzyCoolDown_;
    }

    if (
        frenzyCoolDown_ <= 0    &&
        tgt_                    &&
        (getHp() <= getHpMax(true) / 2))
    {
        frenzyCoolDown_ = 30;

        const int NR_FRENZY_TURNS = Rnd::range(4, 6);

        propHandler_->tryApplyProp(
            new PropFrenzied(PropTurns::specific, NR_FRENZY_TURNS));
    }
    return false;
}

void GiantBat::mkStartItems()
{
    inv_->putInIntrinsics(ItemFactory::mk(ItemId::giantBatBite));
}

void Byakhee::mkStartItems()
{
    inv_->putInIntrinsics(ItemFactory::mk(ItemId::byakheeClaw));
}

void GiantMantis::mkStartItems()
{
    inv_->putInIntrinsics(ItemFactory::mk(ItemId::giantMantisClaw));
}

void Chthonian::mkStartItems()
{
    inv_->putInIntrinsics(ItemFactory::mk(ItemId::chthonianBite));
}

void HuntingHorror::mkStartItems()
{
    inv_->putInIntrinsics(ItemFactory::mk(ItemId::huntingHorrorBite));
}

bool KeziahMason::onActorTurn_()
{
    if (isAlive() && awareCounter_ > 0 && !hasSummonedJenkin)
    {
        bool blockedLos[MAP_W][MAP_H];
        MapParse::run(CellCheck::BlocksLos(), blockedLos);

        if (canSeeActor(*(Map::player), blockedLos))
        {
            MapParse::run(CellCheck::BlocksMoveCmn(true), blockedLos);

            vector<Pos> line;
            LineCalc::calcNewLine(pos, Map::player->pos, true, 9999, false, line);

            const int LINE_SIZE = line.size();
            for (int i = 0; i < LINE_SIZE; ++i)
            {
                const Pos c = line[i];
                if (!blockedLos[c.x][c.y])
                {
                    //TODO: Use the generalized summoning functionality
                    Log::addMsg("Keziah summons Brown Jenkin!");
                    Actor* const actor    = ActorFactory::mk(ActorId::brownJenkin, c);
                    Mon* jenkin           = static_cast<Mon*>(actor);
                    Render::drawMapAndInterface();
                    hasSummonedJenkin     = true;
                    jenkin->awareCounter_ = 999;
                    jenkin->leader_       = leader_ ? leader_ : this;
                    GameTime::tick();
                    return true;
                }
            }
        }
    }

    return false;
}

void KeziahMason::mkStartItems()
{
    spellsKnown_.push_back(new SpellTeleport);
    spellsKnown_.push_back(new SpellHealSelf);
    spellsKnown_.push_back(new SpellSummonMon);
    spellsKnown_.push_back(new SpellPest);
    spellsKnown_.push_back(new SpellDarkbolt);
    spellsKnown_.push_back(SpellHandling::getRandomSpellForMon());

    for (int i = Rnd::range(2, 3); i > 0; --i)
        inv_->putInGeneral(ItemFactory::mkRandomScrollOrPotion(true, true));
}

void LengElder::onStdTurn_()
{
    if (isAlive())
    {
        awareCounter_ = 100;

        if (hasGivenItemToPlayer_)
        {
            bool blockedLos[MAP_W][MAP_H];
            MapParse::run(CellCheck::BlocksLos(), blockedLos);
            if (canSeeActor(*Map::player, blockedLos))
            {
                if (nrTurnsToHostile_ <= 0)
                {
                    Log::addMsg("I am ripped to pieces!!!", clrMsgBad);
                    Map::player->hit(999, DmgType::pure);
                }
                else
                {
                    --nrTurnsToHostile_;
                }
            }
        }
        else //Has not given item to player
        {
            const bool IS_PLAYER_SEE_ME = Map::player->canSeeActor(*this, nullptr);
            const bool IS_PLAYER_ADJ    = Utils::isPosAdj(pos, Map::player->pos, false);
            if (IS_PLAYER_SEE_ME && IS_PLAYER_ADJ)
            {
                Log::addMsg("I perceive a cloaked figure standing before me...",
                            clrWhite, false, true);
                Log::addMsg("It is the Elder Hierophant of the Leng monastery, ");
                Log::addMsg("the High Priest Not to Be Described.",
                            clrWhite, false, true);

                Popup::showMsg("", true, "");

                auto& inv = Map::player->getInv();
                //TODO: Which item to give?
                inv.putInGeneral(ItemFactory::mk(ItemId::hideousMask));

                hasGivenItemToPlayer_ = true;
                nrTurnsToHostile_     = Rnd::range(9, 11);
            }
        }
    }
}

void LengElder::mkStartItems()
{

}

void Ooze::onStdTurn_()
{
    restoreHp(1, false);
}

void OozeBlack::mkStartItems()
{
    inv_->putInIntrinsics(ItemFactory::mk(ItemId::oozeBlackSpewPus));
}

void OozeClear::mkStartItems()
{
    inv_->putInIntrinsics(ItemFactory::mk(ItemId::oozeClearSpewPus));
}

void OozePutrid::mkStartItems()
{
    inv_->putInIntrinsics(ItemFactory::mk(ItemId::oozePutridSpewPus));
}

void OozePoison::mkStartItems()
{
    inv_->putInIntrinsics(ItemFactory::mk(ItemId::oozePoisonSpewPus));
}

void ColorOOSpace::mkStartItems()
{
    inv_->putInIntrinsics(ItemFactory::mk(ItemId::colourOOSpaceTouch));
}

const Clr& ColorOOSpace::getClr()
{
    return curColor;
}

void ColorOOSpace::onStdTurn_()
{
    curColor.r = Rnd::range(40, 255);
    curColor.g = Rnd::range(40, 255);
    curColor.b = Rnd::range(40, 255);

    restoreHp(1, false);

    if (Map::player->canSeeActor(*this, nullptr))
    {
        Map::player->getPropHandler().tryApplyProp(new PropConfused(PropTurns::std));
    }
}

bool Spider::onActorTurn_()
{
    return false;
}

void GreenSpider::mkStartItems()
{
    inv_->putInIntrinsics(ItemFactory::mk(ItemId::greenSpiderBite));
}

void WhiteSpider::mkStartItems()
{
    inv_->putInIntrinsics(ItemFactory::mk(ItemId::whiteSpiderBite));
}

void RedSpider::mkStartItems()
{
    inv_->putInIntrinsics(ItemFactory::mk(ItemId::redSpiderBite));
}

void ShadowSpider::mkStartItems()
{
    inv_->putInIntrinsics(ItemFactory::mk(ItemId::shadowSpiderBite));
}

void LengSpider::mkStartItems()
{
    inv_->putInIntrinsics(ItemFactory::mk(ItemId::lengSpiderBite));
}

void Wolf::mkStartItems()
{
    inv_->putInIntrinsics(ItemFactory::mk(ItemId::wolfBite));
}

bool WormMass::onActorTurn_()
{
    if (
        isAlive()                                       &&
        awareCounter_ > 0                               &&
        GameTime::actors_.size() < MAX_NR_ACTORS_ON_MAP &&
        Rnd::oneIn(spawnNewOneInN))
    {
        bool blocked[MAP_W][MAP_H];
        MapParse::run(CellCheck::BlocksActor(*this, true), blocked,
                      MapParseMode::overwrite, Rect(pos - 1, pos + 1));

        for (const Pos& d : DirUtils::dirList)
        {
            const Pos pAdj(pos + d);

            if (!blocked[pAdj.x][pAdj.y])
            {
                Actor* const    actor   = ActorFactory::mk(data_->id, pAdj);
                WormMass* const worm    = static_cast<WormMass*>(actor);
                spawnNewOneInN += 3;
                worm->spawnNewOneInN    = spawnNewOneInN;
                worm->awareCounter_     = awareCounter_;
                worm->leader_           = leader_ ? leader_ : this;
                GameTime::tick();
                return true;
            }
        }
    }
    return false;
}

void WormMass::mkStartItems()
{
    inv_->putInIntrinsics(ItemFactory::mk(ItemId::wormMassBite));
}

bool GiantLocust::onActorTurn_()
{
    if (
        isAlive()                                       &&
        awareCounter_ > 0                               &&
        GameTime::actors_.size() < MAX_NR_ACTORS_ON_MAP &&
        Rnd::oneIn(spawnNewOneInN))
    {
        bool blocked[MAP_W][MAP_H];
        MapParse::run(CellCheck::BlocksActor(*this, true), blocked,
                      MapParseMode::overwrite, Rect(pos - 1, pos + 1));

        for (const Pos& d : DirUtils::dirList)
        {
            const Pos pAdj(pos + d);

            if (!blocked[pAdj.x][pAdj.y])
            {
                Actor* const    actor     = ActorFactory::mk(data_->id, pAdj);
                GiantLocust* const locust = static_cast<GiantLocust*>(actor);
                spawnNewOneInN += 3;
                locust->spawnNewOneInN    = spawnNewOneInN;
                locust->awareCounter_     = awareCounter_;
                locust->leader_           = leader_ ? leader_ : this;
                GameTime::tick();
                return true;
            }
        }
    }
    return false;
}

void GiantLocust::mkStartItems()
{
    inv_->putInIntrinsics(ItemFactory::mk(ItemId::giantLocustBite));
}

bool LordOfShadows::onActorTurn_()
{
    return false;
}

void LordOfShadows::mkStartItems()
{

}

bool LordOfSpiders::onActorTurn_()
{
    if (isAlive() && awareCounter_ > 0 && Rnd::coinToss())
    {
        const Pos playerPos = Map::player->pos;

        if (Map::player->canSeeActor(*this, nullptr))
        {
            Log::addMsg(data_->spellCastMsg);
        }

        for (int dx = -1; dx <= 1; ++dx)
        {
            for (int dy = -1; dy <= 1; ++dy)
            {
                if (Rnd::fraction(3, 4))
                {

                    const Pos p(playerPos + Pos(dx, dy));
                    const auto* const featureHere = Map::cells[p.x][p.y].rigid;

                    if (featureHere->canHaveRigid())
                    {
                        auto& d = FeatureData::getData(featureHere->getId());
                        const auto* const mimic = static_cast<Rigid*>(d.mkObj(p));
                        Trap* const f = new Trap(p, mimic, TrapId::web);
                        Map::put(f);
                        f->reveal(false);
                    }
                }
            }
        }
    }
    return false;
}

void LordOfSpiders::mkStartItems()
{

}

bool LordOfSpirits::onActorTurn_()
{
    return false;
}

void LordOfSpirits::mkStartItems()
{

}

bool LordOfPestilence::onActorTurn_()
{
    return false;
}

void LordOfPestilence::mkStartItems()
{

}

bool Zombie::onActorTurn_()
{
    return tryResurrect();
}

bool MajorClaphamLee::onActorTurn_()
{
    if (tryResurrect())
    {
        return true;
    }

    if (isAlive() && awareCounter_ > 0 && !hasSummonedTombLegions)
    {
        bool blockedLos[MAP_W][MAP_H];
        MapParse::run(CellCheck::BlocksLos(), blockedLos);

        if (canSeeActor(*(Map::player), blockedLos))
        {
            Log::addMsg("Major Clapham Lee calls forth his Tomb-Legions!");
            vector<ActorId> monIds;
            monIds.clear();

            monIds.push_back(ActorId::deanHalsey);

            const int NR_OF_EXTRA_SPAWNS = 4;

            for (int i = 0; i < NR_OF_EXTRA_SPAWNS; ++i)
            {
                const int ZOMBIE_TYPE = Rnd::range(1, 3);
                ActorId id = ActorId::zombie;
                switch (ZOMBIE_TYPE)
                {
                case 1: id = ActorId::zombie;        break;
                case 2: id = ActorId::zombieAxe;     break;
                case 3: id = ActorId::bloatedZombie; break;
                }
                monIds.push_back(id);
            }
            ActorFactory::summon(pos, monIds, true, this);
            Render::drawMapAndInterface();
            hasSummonedTombLegions = true;
            Map::player->incrShock(ShockLvl::heavy, ShockSrc::misc);
            GameTime::tick();
            return true;
        }
    }

    return false;
}

bool Zombie::tryResurrect()
{
    if (isCorpse() && !hasResurrected)
    {
        const int NR_TURNS_TO_CAN_RISE = 5;

        if (deadTurnCounter < NR_TURNS_TO_CAN_RISE)
        {
            ++deadTurnCounter;
        }
        if (deadTurnCounter >= NR_TURNS_TO_CAN_RISE)
        {
            if (pos != Map::player->pos && Rnd::oneIn(14))
            {
                state_  = ActorState::alive;
                hp_     = (getHpMax(true) * 3) / 4;
                glyph_  = data_->glyph;
                tile_   = data_->tile;
                clr_    = data_->color;
                hasResurrected = true;
                data_->nrKills--;
                if (Map::cells[pos.x][pos.y].isSeenByPlayer)
                {
                    Log::addMsg(getCorpseNameThe() + " rises again!!", clrWhite, true);
                    Map::player->incrShock(ShockLvl::some, ShockSrc::misc);
                }

                awareCounter_ = data_->nrTurnsAware * 2;
                GameTime::tick();
                return true;
            }
        }
    }
    return false;
}

void Zombie::onDeath()
{
    //If resurrected once and has corpse, blow up the corpse
    if (hasResurrected && isCorpse())
    {
        state_ = ActorState::destroyed;
        Map::mkBlood(pos);
        Map::mkGore(pos);
    }
}

void ZombieClaw::mkStartItems()
{
    Item* item = nullptr;
    if (Rnd::percent() < 20)
    {
        item = ItemFactory::mk(ItemId::zombieClawDiseased);
    }
    else
    {
        item = ItemFactory::mk(ItemId::zombieClaw);
    }
    inv_->putInIntrinsics(item);
}

void ZombieAxe::mkStartItems()
{
    inv_->putInIntrinsics(ItemFactory::mk(ItemId::zombieAxe));
}

void BloatedZombie::mkStartItems()
{
    inv_->putInIntrinsics(ItemFactory::mk(ItemId::bloatedZombiePunch));
    inv_->putInIntrinsics(ItemFactory::mk(ItemId::bloatedZombieSpit));
}

bool Mold::onActorTurn_()
{
    if (
        isAlive()                                       &&
        GameTime::actors_.size() < MAX_NR_ACTORS_ON_MAP &&
        Rnd::oneIn(spawnNewOneInN))
    {
        bool blocked[MAP_W][MAP_H];
        MapParse::run(CellCheck::BlocksActor(*this, true), blocked,
                      MapParseMode::overwrite, Rect(pos - 1, pos + 1));

        for (const Pos& d : DirUtils::dirList)
        {
            const Pos adjPos(pos + d);

            if (!blocked[adjPos.x][adjPos.y])
            {
                Actor* const  actor     = ActorFactory::mk(data_->id, adjPos);
                Mold* const   mold      = static_cast<Mold*>(actor);
                mold->awareCounter_     = awareCounter_;
                mold->leader_           = leader_ ? leader_ : this;
                GameTime::tick();
                return true;
            }
        }
    }
    return false;
}

void Mold::mkStartItems()
{
    inv_->putInIntrinsics(ItemFactory::mk(ItemId::moldSpores));
}

void GasSpore::onDeath()
{
    Explosion::runExplosionAt(pos, ExplType::expl);
}

TheDarkOne::TheDarkOne() :
    Mon                 (),
    hasGreetedPlayer_   (false),
    BIG_SPELL_COOLDOWN_ (15),
    bigSpellCounter_    (BIG_SPELL_COOLDOWN_) {}

void TheDarkOne::mkStartItems()
{
    inv_->putInIntrinsics(ItemFactory::mk(ItemId::theDarkOneClaw));

    spellsKnown_.push_back(new SpellTerrifyMon());
    spellsKnown_.push_back(new SpellDisease());
    spellsKnown_.push_back(new SpellBurn());
}

void TheDarkOne::onDeath()
{
    Log::addMsg("The ground rumbles...", clrWhite, false, true);

    const Pos stairPos(MAP_W - 2, 11);
    Map::put(new Stairs(stairPos));
    Map::put(new RubbleLow(stairPos - Pos(1, 0)));

    Map::player->updateFov();
    Render::drawMapAndInterface();
}

void TheDarkOne::onStdTurn_()
{

}

bool TheDarkOne::onActorTurn_()
{
    if (!isAlive())
    {
        return false;
    }

    if (!hasGreetedPlayer_)
    {
        Map::player->updateFov();
        Render::drawMapAndInterface();

        Log::addMsg("A booming voice echoes through the halls.", clrWhite, false, true);
        Audio::play(SfxId::bossVoice1);

        hasGreetedPlayer_ = true;
        awareCounter_     = data_->nrTurnsAware;
    }

    bool blockedLos[MAP_W][MAP_H];
    MapParse::run(CellCheck::BlocksLos(), blockedLos);

    if (bigSpellCounter_ > 0 && canSeeActor(*Map::player, blockedLos))
    {
        --bigSpellCounter_;
    }

    //Summon copies, change position with one of them.
    if (bigSpellCounter_ <= 0 && tgt_)
    {
        bool blocked[MAP_W][MAP_H];
        MapParse::run(CellCheck::BlocksMoveCmn(true), blocked);

        vector<Pos> freeCells;
        Utils::mkVectorFromBoolMap(false, blocked, freeCells);

        const int NR_SUMMONED = min(3, int(freeCells.size()));

        for (int i = 0; i < NR_SUMMONED; ++i)
        {
            const int CELL_IDX = Rnd::range(0, int(freeCells.size()));

            const Pos& p(CELL_IDX);

            vector<Mon*> summoned;
            ActorFactory::summon(p, {ActorId::theDarkOneCpy}, true, this, &summoned);

            assert(summoned.size() == 1);

            Mon* const mon = summoned.front();

            mon->nrTurnsUntilUnsummoned_ = Rnd::range(6, 12);

            if (i == 0)
            {
                pos.swap(mon->pos);
                assert(pos != mon->pos);
            }
        }

        GameTime::tick();

        bigSpellCounter_ = BIG_SPELL_COOLDOWN_ + Rnd::range(-5, 5);

        return true;
    }

    if (Rnd::coinToss())
    {
        Map::cells[pos.x][pos.y].rigid->mkBloody();
        Map::cells[pos.x][pos.y].rigid->tryPutGore();
    }

    return false;
}

void TheDarkOneCpy::mkStartItems()
{
    for (Actor* const actor : GameTime::actors_)
    {
        if (actor->getId() == ActorId::theDarkOne)
        {
            hpMax_  = max(2, actor->getHpMax(true)  / 2);
            hp_     = max(1, actor->getHp()         / 2);
        }
    }

    inv_->putInIntrinsics(ItemFactory::mk(ItemId::theDarkOneClaw));
}
