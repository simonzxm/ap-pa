#include "unit.h"

QVector<QString> allHeroIds()
{
    return {
        "warrior",
        "mage",
        "priest",
        "ranger",
        "guardian"
    };
}

static void applyStarStat(UnitData &u)
{
    if (u.star >= 2) {
        u.maxHp = static_cast<int>(u.maxHp * 1.7);
        u.hp = u.maxHp;
        u.atk = static_cast<int>(u.atk * 1.6);
    }
}

UnitData makeHero(const QString &heroId, Owner owner)
{
    UnitData u;
    u.heroId = heroId;
    u.owner = owner;
    u.star = 1;

    if (heroId == "warrior") {
        u.heroName = "Warrior";
        u.maxHp = 520;
        u.hp = u.maxHp;
        u.atk = 35;
        u.range = 1;
        u.maxMana = 60;
        u.traits = {"Warrior", "Guardian"};
    } else if (heroId == "mage") {
        u.heroName = "Mage";
        u.maxHp = 340;
        u.hp = u.maxHp;
        u.atk = 45;
        u.range = 3;
        u.maxMana = 50;
        u.traits = {"Mage"};
    } else if (heroId == "priest") {
        u.heroName = "Priest";
        u.maxHp = 380;
        u.hp = u.maxHp;
        u.atk = 25;
        u.range = 2;
        u.maxMana = 55;
        u.traits = {"Healer"};
    } else if (heroId == "ranger") {
        u.heroName = "Ranger";
        u.maxHp = 360;
        u.hp = u.maxHp;
        u.atk = 38;
        u.range = 3;
        u.maxMana = 60;
        u.traits = {"Ranger", "Assassin"};
    } else {
        u.heroId = "guardian";
        u.heroName = "Guardian";
        u.maxHp = 600;
        u.hp = u.maxHp;
        u.atk = 28;
        u.range = 1;
        u.maxMana = 65;
        u.traits = {"Guardian"};
    }

    if (owner == Owner::Enemy) {
        u.maxHp = static_cast<int>(u.maxHp * 0.95);
        u.hp = u.maxHp;
        u.atk = static_cast<int>(u.atk * 0.95);
    }

    applyStarStat(u);
    return u;
}

QVector<Equipment> defaultEquipmentCatalog()
{
    return {
        Equipment{"Iron Sword", 15, 0, 1.0, 0},
        Equipment{"Chain Vest", 0, 150, 1.0, 0},
        Equipment{"Rapid Gloves", 0, 0, 1.2, 0},
        Equipment{"Blue Crystal", 0, 0, 1.0, -30}
    };
}
