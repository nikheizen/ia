#include "player_bon.hpp"

#include "init.hpp"
#include "text_format.hpp"
#include "actor_player.hpp"
#include "dungeon_master.hpp"
#include "item_factory.hpp"
#include "inventory.hpp"
#include "player_spells_handling.hpp"
#include "map.hpp"
#include "map_parsing.hpp"
#include "create_character.hpp"
#include "save_handling.hpp"

namespace player_bon
{

bool traits[(size_t)Trait::END];

namespace
{

Bg bg_ = Bg::END;

bool is_trait_blocked_for_bg(const Trait trait, const Bg bg)
{
    switch (trait)
    {
    case Trait::adept_melee_fighter:
        break;

    case Trait::expert_melee_fighter:
        break;

    case Trait::master_melee_fighter:
        break;

    case Trait::adept_marksman:
        break;

    case Trait::expert_marksman:
        break;

    case Trait::master_marksman:
        break;

    case Trait::steady_aimer:
        break;

    case Trait::sharpshooter:
        break;

    case Trait::fast_shooter:
        break;

    case Trait::dem_expert:
        //Too much character theme mismatch
        return bg == Bg::occultist || bg == Bg::ghoul;

    case Trait::cool_headed:
        break;

    case Trait::courageous:
        break;

    case Trait::warlock:
        break;

    case Trait::summoner:
        break;

    case Trait::blood_sorcerer:
        break;

    case Trait::seer:
        break;

    case Trait::tough:
        break;

    case Trait::rugged:
        break;

    case Trait::unbreakable:
        break;

    case Trait::strong_backed:
        break;

    case Trait::dexterous:
        break;

    case Trait::lithe:
        break;

    case Trait::mobile:
        break;

    case Trait::fearless:
        break;

    case Trait::healer:
        //Cannot use Medial Bag
        return bg == Bg::ghoul;

    case Trait::observant:
        break;

    case Trait::perceptive:
        break;

    case Trait::vigilant:
        break;

    case Trait::rapid_recoverer:
        //Cannot regen HP passively
        return bg == Bg::ghoul;

    case Trait::survivalist:
        //Has RDISEASE already + a bit of theme mismatch
        return bg == Bg::ghoul;

    case Trait::perseverant:
        break;

    case Trait::self_aware:
        break;

    case Trait::stout_spirit:
        break;

    case Trait::strong_spirit:
    case Trait::mighty_spirit:
        //Have very little use for spirit, aside from Spell Resistance
        return bg == Bg::ghoul || bg == Bg::war_vet;

    case Trait::stealthy:
        break;

    case Trait::imperceptible:
        break;

    case Trait::vicious:
        break;

    case Trait::treasure_hunter:
        break;

    case Trait::undead_bane:
        break;

    case Trait::ravenous:
        break;

    case Trait::foul:
        break;

    case Trait::toxic:
        break;

    case Trait::indomitable_fury:
        break;

    case Trait::END:
        break;
    }

    return false;
}

} //namespace

void init()
{
    for (int i = 0; i < (int)Trait::END; ++i)
    {
        traits[i] = false;
    }

    bg_ = Bg::END;
}

void save()
{
    save_handling::put_int((int)bg_);

    for (int i = 0; i < (int)Trait::END; ++i)
    {
        save_handling::put_bool(traits[i]);
    }
}

void load()
{
    bg_ = Bg(save_handling::get_int());

    for (int i = 0; i < (int)Trait::END; ++i)
    {
        traits[i] = save_handling::get_bool();
    }
}

std::string bg_title(const Bg id)
{
    switch (id)
    {
    case Bg::ghoul:
        return "Ghoul";

    case Bg::occultist:
        return "Occultist";

    case Bg::rogue:
        return "Rogue";

    case Bg::war_vet:
        return "War Veteran";

    case Bg::END:
        break;
    }

    ASSERT(false);

    return "[BG TITLE MISSING]";
}

std::string trait_title(const Trait id)
{
    switch (id)
    {
    case Trait::adept_melee_fighter:
        return "Adept Melee Fighter";

    case Trait::expert_melee_fighter:
        return "Expert Melee Fighter";

    case Trait::master_melee_fighter:
        return "Master Melee Fighter";

    case Trait::cool_headed:
        return "Cool-headed";

    case Trait::courageous:
        return "Courageous";

    case Trait::warlock:
        return "Warlock";

    case Trait::summoner:
        return "Summoner";

    case Trait::blood_sorcerer:
        return "Blood Sorcerer";

    case Trait::seer:
        return "Seer";

    case Trait::dexterous:
        return "Dexterous";

    case Trait::lithe:
        return "Lithe";

    case Trait::mobile:
        return "Mobile";

    case Trait::fearless:
        return "Fearless";

    case Trait::healer:
        return "Healer";

    case Trait::adept_marksman:
        return "Adept Marksman";

    case Trait::expert_marksman:
        return "Expert Marksman";

    case Trait::master_marksman:
        return "Master Marksman";

    case Trait::steady_aimer:
        return "Steady Aimer";

    case Trait::sharpshooter:
        return "Sharpshooter";

    case Trait::fast_shooter:
        return "Fast Shooter";

    case Trait::observant:
        return "Observant";

    case Trait::perceptive:
        return "Perceptive";

    case Trait::vigilant:
        return "Vigilant";

    case Trait::rapid_recoverer:
        return "Rapid Recoverer";

    case Trait::survivalist:
        return "Survivalist";

    case Trait::perseverant:
        return "Perseverant";

    case Trait::self_aware:
        return "Self-aware";

    case Trait::stout_spirit:
        return "Stout Spirit";

    case Trait::strong_spirit:
        return "Strong Spirit";

    case Trait::mighty_spirit:
        return "Mighty Spirit";

    case Trait::stealthy:
        return "Stealthy";

    case Trait::imperceptible:
        return "Imperceptible";

    case Trait::vicious:
        return "Vicious";

    case Trait::strong_backed:
        return "Strong-backed";

    case Trait::tough:
        return "Tough";

    case Trait::rugged:
        return "Rugged";

    case Trait::unbreakable:
        return "Unbreakable";

    case Trait::treasure_hunter:
        return "Treasure Hunter";

    case Trait::dem_expert:
        return "Demolition Expert";

    case Trait::undead_bane:
        return "Bane of the Undead";

    case Trait::ravenous:
        return "Ravenous";

    case Trait::foul:
        return "Foul";

    case Trait::toxic:
        return "Toxic";

    case Trait::indomitable_fury:
        return "Indomitable Fury";

    case Trait::END:
        break;
    }

    ASSERT(false);

    return "[TRAIT TITLE MISSING]";
}

std::vector<std::string> bg_descr(const Bg id)
{
    switch (id)
    {
    case Bg::ghoul:
    {
        return
        {
            "Does not regenerate Hit Points and cannot use medical equipment - must "
            "instead heal by feeding on corpses (press [q])",
            "",
            "Has an arcane ability to incite Frenzy at will (increased speed, "
            "+10% melee hit chance, +1 melee damage, must move towards enemies).",
            "",
            "Does not become Weakened when Frenzy ends",
            "",
            "Has powerful claws to attack with",
            "",
            "-15% hit chance with firearms and thrown weapons",
            "",
            "+10 Hit Points",
            "",
            "Is immune to Disease and Infections",
            "",
            "Has Infravision",
            "",
            "-50% shock taken from seeing monsters",
            "",
            "All Ghouls are allied"
        };
    }
    break;

    case Bg::occultist:
        return
        {
            "Can learn spells by heart when casting from manuscripts",
            "",
            "-50% shock taken from using and identifying strange items (e.g. potions)",
            "",
            "Can dispel magic traps",
            "",
            "-2 Hit Points",
            "",
            "Starts with the following trait(s):",
            "",
            "* " + trait_title(Trait::stout_spirit),
            trait_descr(Trait::stout_spirit)
        };
        break;

    case Bg::rogue:
        return
        {
            "Has an arcane ability to cloud the minds of enemies, causing them to "
            "forget their pursuit",
            "",
            "The rate of shock received passively over time is reduced by half",
            "",
            "Starts with the following trait(s):",
            "",
            "* " + trait_title(Trait::observant),
            trait_descr(Trait::observant),
            "",
            "* " + trait_title(Trait::stealthy),
            trait_descr(Trait::stealthy)
        };
        break;

    case Bg::war_vet:
        return
        {
            "Can switch to prepared weapon instantly",
            "",
            "Maintains armor twice as long before it breaks",
            "",
            "Starts with 10% insanity",
            "",
            "Starts with the following trait(s):",
            "",
            "* " + trait_title(Trait::adept_marksman),
            trait_descr(Trait::adept_marksman),
            "",
            "* " + trait_title(Trait::adept_melee_fighter),
            trait_descr(Trait::adept_melee_fighter),
            "",
            "* " + trait_title(Trait::tough),
            trait_descr(Trait::tough)
        };
        break;

    case Bg::END:
        break;
    }

    ASSERT(false);

    return {};
}

std::string trait_descr(const Trait id)
{
    switch (id)
    {
    case Trait::adept_melee_fighter:
        return "+10% hit chance and +1 damage with melee attacks";

    case Trait::expert_melee_fighter:
        return "+10% hit chance and +1 damage with melee attacks";

    case Trait::master_melee_fighter:
        return "+10% hit chance and +1 damage with melee attacks";

    case Trait::adept_marksman:
        return "+10% hit chance with firearms and thrown weapons, you occasionally "
               "reload instantly";

    case Trait::expert_marksman:
        return "+10% hit chance with firearms and thrown weapons, you often "
               "reload instantly";

    case Trait::master_marksman:
        return "+10% hit chance and +1 damage with firearms and thrown weapons, you usually "
               "reload instantly";

    case Trait::steady_aimer:
        return "Standing still gives ranged attacks +20% hit chance on the following turn";

    case Trait::sharpshooter:
        return "Standing still for three turns gives ranged attacks maximum hit chance "
               "and damage on the following turn";

    case Trait::fast_shooter:
        return "Every second consecutive shot is a free action (not applicable to thrown weapons)";

    case Trait::dem_expert:
        return "+1 radius for explosives (be careful), you are not harmed by your own "
               "Molotov Cocktails, you occasionally light explosives instantly";

    case Trait::cool_headed:
        return "+20% shock resistance";

    case Trait::courageous:
        return "+20% shock resistance";

    case Trait::warlock:
        return "-1 Spirit cost for damage dealing spells, casting any spell has a chance "
               "to make you \"Charged\" for one turn, causing attack spells to do "
               "maximum damage";

    case Trait::summoner:
        return "-1 Spirit cost for summoning spells, halved risk that called creatures are hostile";

    case Trait::blood_sorcerer:
        return "-1 Spirit cost for all spells, casting a spell drains 2 Hit Points";

    case Trait::seer:
        return "Detection spells have decreased Spirit costs, and the spell "
               "\"Detect Monsters\" has triple duration";

    case Trait::tough:
        return "+2 hit points, +10% carry weight limit, better results for object "
               "interactions requiring strength (e.g. bashing doors or pushing a lid)";

    case Trait::rugged:
        return "+2 hit points, +10% carry weight limit, better results for object "
               "interactions requiring strength (such as bashing doors, or moving the "
               "lid from a stone coffin)";

    case Trait::unbreakable:
        return "+2 hit points, +10% carry weight limit, better results for object "
               "interactions requiring strength (such as bashing doors, or moving the "
               "lid from a stone coffin)";

    case Trait::strong_backed:
        return "+30% carry weight limit";

    case Trait::dexterous:
        return "+25% chance to dodge melee attacks, better chances to evade traps, "
               "every fifth step is a free action";

    case Trait::lithe:
        return "+25% chance to dodge melee attacks, better chances to evade traps, "
               "every fourth step is a free action";

    case Trait::mobile:
        return "Every third step is a free action";

    case Trait::fearless:
        return "You cannot become terrified, +5% shock resistance";

    case Trait::healer:
        return "Using medical equipment takes half the normal time and resources";

    case Trait::observant:
        return "You can spot hidden traps and doors from two cells away, and you are "
               "more likely to spot hidden monsters and objects";

    case Trait::perceptive:
        return "You can spot hidden traps and doors from three cells away, and you are "
               "more likely to spot hidden monsters and objects";

    case Trait::vigilant:
        return "You cannot be backstabbed - monsters that you are unaware of gets no melee attack "
               "bonus against you, and their attacks can be dodged";

    case Trait::rapid_recoverer:
        return "Double Hit Point regeneration rate";

    case Trait::survivalist:
        return "You cannot become diseased, negative effects from wounds reduced by 50%";

    case Trait::perseverant:
        return "When your Hit Points are reduced to 30% or less, you gain +50% chance to "
               "dodge melee attacks, and +30% hit chance with melee and ranged attacks";

    case Trait::self_aware:
        return "You cannot become confused, the number of remaining turns for status "
               "effects are displayed";

    case Trait::stout_spirit:
        return "+2 Spirit Points, increased Spirit regeneration rate, you can defy harmful "
               "spells (it takes a certain number of turns to regain spell resistance)";

    case Trait::strong_spirit:
        return "+2 Spirit Points, increased Spirit regeneration rate, you can defy harmful spells "
               "- the number of turns to regain spell resistance is reduced";

    case Trait::mighty_spirit:
        return "+2 Spirit Points, increased Spirit regeneration rate, you can defy harmful spells "
               "- the number of turns to regain spell resistance is reduced";

    case Trait::stealthy:
        return "You are more likely to avoid detection";

    case Trait::imperceptible:
        return "You are more likely to avoid detection";

    case Trait::vicious:
        return "+50% backstab damage";

    case Trait::treasure_hunter:
        return "You tend to find more items";

    case Trait::undead_bane:
        return "+2 melee and ranged attack damage against undead monsters. Attacks "
               "against ethereal undead monsters (e.g. Ghosts) never pass through them "
               "(although you can still miss in the ordinary way)";

    case Trait::ravenous:
        return "You occasionally feed on living victims when attacking with your claws.";

    case Trait::foul:
        return "You have particularly filthy and verminous claws - vicious worms occasionally "
               "burst out from the corpses of your victims to attack your enemies.";

    case Trait::toxic:
        return "Attacks with your claws occasionally poisons your victims. "
               "You are immune to Poison.";

    case Trait::indomitable_fury:
        return "While Frenzied, you are immune to Wounds, and your attacks causes fear.";

    case Trait::END:
        break;
    }

    ASSERT(false);

    return "[TRAIT DESCRIPTION MISSING]";
}

void trait_prereqs(const Trait trait,
                   const Bg bg,
                   std::vector<Trait>& traits_out,
                   Bg& bg_out)
{
    traits_out.clear();
    bg_out = Bg::END;

    switch (trait)
    {
    case Trait::adept_melee_fighter:
        break;

    case Trait::expert_melee_fighter:
        traits_out.push_back(Trait::adept_melee_fighter);
        break;

    case Trait::master_melee_fighter:
        traits_out.push_back(Trait::expert_melee_fighter);
        break;

    case Trait::adept_marksman:
        break;

    case Trait::expert_marksman:
        traits_out.push_back(Trait::adept_marksman);
        break;

    case Trait::master_marksman:
        traits_out.push_back(Trait::expert_marksman);
        bg_out = Bg::war_vet;
        break;

    case Trait::steady_aimer:
        bg_out = Bg::war_vet;
        break;

    case Trait::sharpshooter:
        traits_out.push_back(Trait::steady_aimer);
        traits_out.push_back(Trait::expert_marksman);
        bg_out = Bg::war_vet;
        break;

    case Trait::fast_shooter:
        traits_out.push_back(Trait::expert_marksman);
        traits_out.push_back(Trait::dexterous);
        bg_out = Bg::war_vet;
        break;

    case Trait::dem_expert:
        break;

    case Trait::cool_headed:
        break;

    case Trait::courageous:
        traits_out.push_back(Trait::cool_headed);
        bg_out = Bg::war_vet;
        break;

    case Trait::warlock:
        traits_out.push_back(Trait::fearless);
        traits_out.push_back(Trait::strong_spirit);
        bg_out = Bg::occultist;
        break;

    case Trait::summoner:
        traits_out.push_back(Trait::fearless);
        traits_out.push_back(Trait::strong_spirit);
        bg_out = Bg::occultist;
        break;

    case Trait::blood_sorcerer:
        traits_out.push_back(Trait::tough);
        bg_out = Bg::occultist;
        break;

    case Trait::seer:
        traits_out.push_back(Trait::observant);
        bg_out = Bg::occultist;
        break;

    case Trait::tough:
        break;

    case Trait::rugged:
        traits_out.push_back(Trait::tough);
        break;

    case Trait::unbreakable:
        traits_out.push_back(Trait::rugged);
        break;

    case Trait::strong_backed:
        traits_out.push_back(Trait::tough);
        break;

    case Trait::dexterous:
        break;

    case Trait::lithe:
        traits_out.push_back(Trait::dexterous);
        break;

    case Trait::mobile:
        traits_out.push_back(Trait::lithe);
        break;

    case Trait::fearless:
        traits_out.push_back(Trait::cool_headed);
        break;

    case Trait::healer:
        break;

    case Trait::observant:
        break;

    case Trait::perceptive:
        traits_out.push_back(Trait::observant);
        break;

    case Trait::vigilant:
        traits_out.push_back(Trait::observant);
        break;

    case Trait::rapid_recoverer:
        traits_out.push_back(Trait::tough);
        traits_out.push_back(Trait::healer);
        break;

    case Trait::survivalist:
        traits_out.push_back(Trait::tough);
        traits_out.push_back(Trait::healer);
        break;

    case Trait::perseverant:
        traits_out.push_back(Trait::tough);
        traits_out.push_back(Trait::fearless);
        break;

    case Trait::self_aware:
        traits_out.push_back(Trait::stout_spirit);
        traits_out.push_back(Trait::observant);
        break;

    case Trait::stout_spirit:
        break;

    case Trait::strong_spirit:
        traits_out.push_back(Trait::stout_spirit);
        break;

    case Trait::mighty_spirit:
        traits_out.push_back(Trait::strong_spirit);
        break;

    case Trait::stealthy:
        break;

    case Trait::imperceptible:
        traits_out.push_back(Trait::stealthy);
        break;

    case Trait::vicious:
        traits_out.push_back(Trait::stealthy);
        traits_out.push_back(Trait::dexterous);
        break;

    case Trait::treasure_hunter:
        traits_out.push_back(Trait::observant);
        break;

    case Trait::undead_bane:
        traits_out.push_back(Trait::tough);
        traits_out.push_back(Trait::fearless);
        traits_out.push_back(Trait::stout_spirit);
        break;

    case Trait::ravenous:
        traits_out.push_back(Trait::adept_melee_fighter);
        bg_out = Bg::ghoul;
        break;

    case Trait::foul:
        bg_out = Bg::ghoul;
        break;

    case Trait::toxic:
        traits_out.push_back(Trait::foul);
        bg_out = Bg::ghoul;
        break;

    case Trait::indomitable_fury:
        traits_out.push_back(Trait::adept_melee_fighter);
        traits_out.push_back(Trait::tough);
        bg_out = Bg::ghoul;
        break;

    case Trait::END:
        break;
    }

    //Remove traits which are blocked for this background (prerequisites are
    //considered fulfilled)
    for (auto it = begin(traits_out); it != end(traits_out); /* No increment */)
    {
        const Trait trait = *it;

        if (is_trait_blocked_for_bg(trait, bg))
        {
            it = traits_out.erase(it);
        }
        else //Not blocked
        {
            ++it;
        }
    }

    //Sort lexicographically
    sort(traits_out.begin(), traits_out.end(), [](const Trait & t1, const Trait & t2)
    {
        const std::string str1 = trait_title(t1);
        const std::string str2 = trait_title(t2);
        return str1 < str2;
    });
}

bool is_prereqs_ok(const Trait id)
{
    //If the trait is already picked, of course the prerequisites are OK!
    if (traits[(size_t)id])
    {
        return true;
    }

    std::vector<Trait> prereq_traits;

    Bg prereq_bg = Bg::END;

    trait_prereqs(id,
                  bg_,
                  prereq_traits,
                  prereq_bg);

    //Background OK?
    bool is_ok = bg_== prereq_bg || prereq_bg == Bg::END;

    if (is_ok)
    {
        //Background is OK, time to check the traits
        for (Trait prereq_trait : prereq_traits)
        {
            if (!traits[(size_t)prereq_trait])
            {
                is_ok = false;
                break;
            }
        }
    }

    return is_ok;
}

Bg bg()
{
    return bg_;
}

void pickable_bgs(std::vector<Bg>& bgs_out)
{
    bgs_out.clear();

    for (int i = 0; i < (int)Bg::END; ++i)
    {
        bgs_out.push_back(Bg(i));
    }

    //Sort lexicographically
    sort(bgs_out.begin(), bgs_out.end(), [](const Bg & bg1, const Bg & bg2)
    {
        const std::string str1 = bg_title(bg1);
        const std::string str2 = bg_title(bg2);
        return str1 < str2;
    });
}

void trait_list_for_bg(const Bg bg, std::vector<Trait>& traits_out)
{
    traits_out.clear();

    for (size_t i = 0; i < (size_t)Trait::END; ++i)
    {
        const Trait trait = Trait(i);

        const bool IS_BLOCKED_FOR_BG = is_trait_blocked_for_bg(trait, bg);

        if (!IS_BLOCKED_FOR_BG)
        {
            //Check trait prerequisites (traits and background)
            //NOTE: Traits blocked for the current background are not
            //considered prerequisites

            std::vector<Trait> trait_prereq_list;
            Bg bg_prereq = Bg::END;

            trait_prereqs(trait, bg, trait_prereq_list, bg_prereq);

            bool is_pickable = (bg_ == bg_prereq) || (bg_prereq == Bg::END);

            if (is_pickable)
            {
                traits_out.push_back(trait);
            }
        }
    }

    //Sort lexicographically
    sort(traits_out.begin(), traits_out.end(), [](const Trait & t1, const Trait & t2)
    {
        const std::string str1 = trait_title(t1);
        const std::string str2 = trait_title(t2);
        return str1 < str2;
    });
}

void pick_bg(const Bg bg)
{
    ASSERT(bg != Bg::END);

    bg_ = bg;

    switch (bg_)
    {
    case Bg::ghoul:
        map::player->prop_handler().try_add(new Prop_rDisease(Prop_turns::indefinite),
                                            Prop_src::intr,
                                            true,
                                            Verbosity::silent);

        map::player->prop_handler().try_add(new Prop_infravis(Prop_turns::indefinite),
                                            Prop_src::intr,
                                            true,
                                            Verbosity::silent);

        player_spells_handling::learn_spell_if_not_known(Spell_id::frenzy);

        map::player->change_max_hp(10, Verbosity::silent);
        break;

    case Bg::occultist:
        pick_trait(Trait::stout_spirit);
        map::player->change_max_hp(-2, Verbosity::silent);
        break;

    case Bg::rogue:
        player_spells_handling::learn_spell_if_not_known(Spell_id::cloud_minds);
        pick_trait(Trait::observant);
        pick_trait(Trait::stealthy);
        break;

    case Bg::war_vet:
        pick_trait(Trait::adept_melee_fighter);
        pick_trait(Trait::adept_marksman);
        pick_trait(Trait::tough);
        map::player->ins_ += 10;
        break;

    case Bg::END:
        break;
    }
}

void set_all_traits_to_picked()
{
    for (int i = 0; i < (int)Trait::END; ++i)
    {
        traits[i] = true;
    }
}

void pick_trait(const Trait id)
{
    ASSERT(id != Trait::END);

    traits[(size_t)id] = true;

    switch (id)
    {
    case Trait::tough:
        map::player->change_max_hp(2, Verbosity::silent);
        break;

    case Trait::rugged:
        map::player->change_max_hp(2, Verbosity::silent);
        break;

    case Trait::unbreakable:
        map::player->change_max_hp(2, Verbosity::silent);
        break;

    case Trait::stout_spirit:
        map::player->change_max_spi(2, Verbosity::silent);

        map::player->prop_handler().try_add(new Prop_rSpell(Prop_turns::indefinite),
                                            Prop_src::intr,
                                            true,
                                            Verbosity::silent);
        break;

    case Trait::strong_spirit:
        map::player->change_max_spi(2, Verbosity::silent);
        break;

    case Trait::mighty_spirit:
        map::player->change_max_spi(2, Verbosity::silent);
        break;

    case Trait::self_aware:
        map::player->prop_handler().try_add(new Prop_rConf(Prop_turns::indefinite),
                                            Prop_src::intr,
                                            true,
                                            Verbosity::silent);
        break;

    case Trait::survivalist:
        map::player->prop_handler().try_add(new Prop_rDisease(Prop_turns::indefinite),
                                            Prop_src::intr,
                                            true,
                                            Verbosity::silent);
        break;

    case Trait::fearless:
        map::player->prop_handler().try_add(new Prop_rFear(Prop_turns::indefinite),
                                            Prop_src::intr,
                                            true,
                                            Verbosity::silent);
        break;

    case Trait::toxic:
        map::player->prop_handler().try_add(new Prop_rPoison(Prop_turns::indefinite),
                                            Prop_src::intr,
                                            true,
                                            Verbosity::silent);

    default:
        break;
    }
}

std::string all_picked_traits_titles_line()
{
    std::string out = "";

    for (int i = 0; i < (int)Trait::END; ++i)
    {
        if (traits[i])
        {
            const std::string title = trait_title(Trait(i));

            out += (out.empty() ? "" : ", ") + title;
        }
    }

    return out;
}

int spi_occultist_can_cast_at_lvl(const int LVL)
{
    ASSERT(LVL > 0);
    const int SPI_FROM_START_TRAIT  = 2;
    const int SPI_FROM_LVLS         = (LVL - 1) * SPI_PER_LVL;
    return PLAYER_START_SPI + SPI_FROM_LVLS + SPI_FROM_START_TRAIT - 1;
}

bool gets_undead_bane_bon(const Actor_data_t& actor_data)
{
    return player_bon::traits[(size_t)Trait::undead_bane] && actor_data.is_undead;
}

} //player_bon
